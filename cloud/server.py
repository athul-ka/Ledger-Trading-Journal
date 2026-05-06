"""
Ledger Alert Server  —  runs on your Raspberry Pi 24 x 7
=========================================================
Keeps a local SQLite database of price alerts.
- REST API   : Ledger Windows app syncs alerts here.
- Ingest API : bridge process submits price/heartbeat events with sequence numbers.
- Telegram   : fires a message when NEAR or TOUCH condition is met.

Environment variables (set in .env or export before running):
    TELEGRAM_TOKEN            — bot token from @BotFather
    TELEGRAM_CHAT_ID          — your personal chat/group ID
    INGEST_SHARED_KEY         — optional auth key for /ingest/* endpoints
    HEARTBEAT_STALE_SECONDS   — source state changes to STALE after this many seconds
    HEARTBEAT_OFFLINE_SECONDS — source state changes to OFFLINE after this many seconds
    PORT                      — HTTP port (default 8000)
"""

import asyncio
import json
import os
import sqlite3
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Literal
from zoneinfo import ZoneInfo

import httpx
from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel, Field

# ── Config ────────────────────────────────────────────────────────────────────

TELEGRAM_TOKEN   = os.getenv("TELEGRAM_TOKEN",   "")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID", "")
TWELVE_DATA_KEY  = os.getenv("TWELVE_DATA_KEY", "").strip()
DB_PATH          = os.path.join(os.path.dirname(__file__), "alerts.db")
CONFIG_PATH      = Path(os.path.dirname(__file__)) / "runtime_config.json"
INGEST_SHARED_KEY = os.getenv("INGEST_SHARED_KEY", "").strip()
HEARTBEAT_STALE_SECONDS = max(5, int(os.getenv("HEARTBEAT_STALE_SECONDS", "15")))
HEARTBEAT_OFFLINE_SECONDS = max(
    HEARTBEAT_STALE_SECONDS + 5,
    int(os.getenv("HEARTBEAT_OFFLINE_SECONDS", "45")),
)
ENABLE_TWELVE_DATA_POLL = os.getenv("ENABLE_TWELVE_DATA_POLL", "0").strip() in ("1", "true", "TRUE", "yes", "YES")
ALERT_TIMEZONE = os.getenv("ALERT_TIMEZONE", "Europe/Berlin")
POLL_START_HOUR = max(0, min(23, int(os.getenv("POLL_START_HOUR", "7"))))
POLL_END_HOUR = max(1, min(24, int(os.getenv("POLL_END_HOUR", "23"))))
POLL_INTERVAL_SECONDS = max(60, int(os.getenv("POLL_INTERVAL_SECONDS", "300")))
MAX_DAILY_CREDITS = max(1, int(os.getenv("MAX_DAILY_CREDITS", "800")))
CREDITS_PER_SYMBOL_REQUEST = max(1, int(os.getenv("CREDITS_PER_SYMBOL_REQUEST", "1")))

try:
    APP_TZ = ZoneInfo(ALERT_TIMEZONE)
except Exception:
    APP_TZ = timezone.utc

_td_round_robin_cursor = 0

app = FastAPI(title="Ledger Alert Server", version="1.0")


# ── Database ──────────────────────────────────────────────────────────────────

def db_conn():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    with db_conn() as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS alerts (
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                pair            TEXT    NOT NULL,
                target_price    REAL    NOT NULL,
                near_pips       REAL    DEFAULT 10,
                alert_type      TEXT    DEFAULT 'BOTH',
                active          INTEGER DEFAULT 1,
                touch_triggered INTEGER DEFAULT 0,
                near_triggered  INTEGER DEFAULT 0,
                created_at      TEXT,
                notes           TEXT
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS ingest_sources (
                source_id                   TEXT PRIMARY KEY,
                highest_sequence            INTEGER DEFAULT 0,
                highest_contiguous_sequence INTEGER DEFAULT 0,
                last_event_at              TEXT,
                last_heartbeat_at          TEXT,
                state                      TEXT DEFAULT 'INIT',
                updated_at                 TEXT
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS ingest_events (
                source_id   TEXT NOT NULL,
                sequence    INTEGER NOT NULL,
                event_type  TEXT NOT NULL,
                symbol      TEXT,
                price       REAL,
                event_time  TEXT,
                payload_hash TEXT,
                received_at TEXT,
                PRIMARY KEY (source_id, sequence)
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS ingest_deadletter (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                source_id   TEXT,
                sequence    INTEGER,
                reason      TEXT,
                payload     TEXT,
                received_at TEXT
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS pair_prices (
                pair TEXT PRIMARY KEY,
                last_price REAL,
                updated_at TEXT
            )
        """)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS td_credit_usage (
                day_key TEXT PRIMARY KEY,
                used_credits INTEGER DEFAULT 0,
                updated_at TEXT
            )
        """)
        conn.commit()


def load_runtime_config():
    global TELEGRAM_TOKEN, TELEGRAM_CHAT_ID, TWELVE_DATA_KEY

    if not CONFIG_PATH.exists():
        return

    try:
        config = json.loads(CONFIG_PATH.read_text())
    except Exception as exc:
        print(f"[Config] Failed to read {CONFIG_PATH.name}: {exc}")
        return

    TELEGRAM_TOKEN = str(config.get("telegram_token", TELEGRAM_TOKEN)).strip()
    TELEGRAM_CHAT_ID = str(config.get("telegram_chat_id", TELEGRAM_CHAT_ID)).strip()
    TWELVE_DATA_KEY = str(config.get("twelve_data_key", TWELVE_DATA_KEY)).strip()
    print("[Config] Loaded runtime settings")


def save_runtime_config(telegram_token: str, telegram_chat_id: str, twelve_data_key: str):
    config = {
        "telegram_token": telegram_token.strip(),
        "telegram_chat_id": telegram_chat_id.strip(),
        "twelve_data_key": twelve_data_key.strip(),
    }
    CONFIG_PATH.write_text(json.dumps(config, indent=2))
    print(f"[Config] Saved runtime settings to {CONFIG_PATH.name}")


SYMBOL_MAP = {
    "BTCUSD": "BTC/USD", "ETHUSD": "ETH/USD",
    "XAUUSD": "XAU/USD", "XAGUSD": "XAG/USD",
    "NAS100": "IXIC",    "US100":  "IXIC",  "NASDAQ100": "IXIC",
    "US30":   "DJI",     "DJ30":   "DJI",
    "SP500":  "SPX",     "US500":  "SPX",
}


def to_td(pair: str) -> str:
    p = pair.upper().strip()
    if p in SYMBOL_MAP:
        return SYMBOL_MAP[p]
    if len(p) == 6:
        return p[:3] + "/" + p[3:]
    return p


PIP_SIZES: dict[str, float] = {
    "XAUUSD": 0.01,  "XAGUSD": 0.001,
    "NAS100": 0.01,  "US100":  0.01,
    "US30":   1.0,   "DJ30":   1.0,
    "SP500":  0.1,   "US500":  0.1,
    "BTCUSD": 1.0,   "ETHUSD": 0.1,
}


def pip_size(pair: str) -> float:
    p = pair.upper()
    if p in PIP_SIZES:
        return PIP_SIZES[p]
    if p.endswith("JPY"):
        return 0.01
    return 0.0001


# ── Telegram ──────────────────────────────────────────────────────────────────

async def send_telegram(message: str):
    if not TELEGRAM_TOKEN or not TELEGRAM_CHAT_ID:
        print("[Telegram] Skipped: TELEGRAM_TOKEN or TELEGRAM_CHAT_ID missing")
        return
    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
    async with httpx.AsyncClient(timeout=10) as client:
        try:
            response = await client.post(url, json={
                "chat_id":    TELEGRAM_CHAT_ID,
                "text":       message,
                "parse_mode": "HTML",
            })
            if response.is_success:
                print("[Telegram] Delivered")
            else:
                print(f"[Telegram] API error {response.status_code}: {response.text}")
        except Exception as exc:
            print(f"[Telegram] Error: {exc}")


# ── Alert evaluation ──────────────────────────────────────────────────────────

def get_last_price(pair: str) -> float | None:
    with db_conn() as conn:
        row = conn.execute(
            "SELECT last_price FROM pair_prices WHERE pair = ?",
            (pair.upper(),),
        ).fetchone()
    if not row:
        return None
    return float(row["last_price"]) if row["last_price"] is not None else None


def set_last_price(pair: str, price: float):
    with db_conn() as conn:
        conn.execute(
            """
            INSERT INTO pair_prices (pair, last_price, updated_at)
            VALUES (?, ?, ?)
            ON CONFLICT(pair) DO UPDATE SET
                last_price = excluded.last_price,
                updated_at = excluded.updated_at
            """,
            (pair.upper(), price, datetime.now(timezone.utc).isoformat()),
        )
        conn.commit()


async def evaluate_alerts(pair: str, price: float, previous_price: float | None = None):
    """Check all active alerts for this pair and fire Telegram if condition met."""
    pair = pair.upper()
    ps   = pip_size(pair)
    now  = datetime.now(timezone.utc).strftime("%H:%M:%S UTC")

    with db_conn() as conn:
        rows = conn.execute(
            "SELECT * FROM alerts WHERE pair = ? AND active = 1", (pair,)
        ).fetchall()

        for row in rows:
            target    = row["target_price"]
            near_pips = row["near_pips"]
            atype     = row["alert_type"]
            dist_pips = abs(price - target) / ps
            crossed = previous_price is not None and ((previous_price - target) * (price - target) <= 0)

            # ── CROSSED/TOUCH: level crossed between polls or current price very close ─
            if ((crossed or dist_pips <= 1.0)
                    and not row["touch_triggered"]
                    and atype in ("TOUCH", "BOTH")):
                label = "CROSSED" if crossed and dist_pips > 1.0 else "TOUCH"
                print(f"[Alert] {label} {pair} target={target} price={price} dist={dist_pips:.2f}p")
                conn.execute(
                    "UPDATE alerts SET touch_triggered = 1 WHERE id = ?", (row["id"],)
                )
                conn.commit()
                msg = (
                    f"🎯 <b>{label} ALERT — {pair}</b>\n"
                    f"Your level : <b>{target}</b>\n"
                    f"Price now  : {price}\n"
                    f"Time       : {now}\n"
                    f"Note       : {row['notes'] or '—'}"
                )
                asyncio.create_task(send_telegram(msg))

            # ── NEAR: within nearPips but not touching ───────────────────────
            elif (dist_pips <= near_pips
                    and dist_pips > 1.0
                    and not row["near_triggered"]
                    and atype in ("NEAR", "BOTH")):
                print(f"[Alert] NEAR {pair} target={target} price={price} dist={dist_pips:.2f}p")
                conn.execute(
                    "UPDATE alerts SET near_triggered = 1 WHERE id = ?", (row["id"],)
                )
                conn.commit()
                msg = (
                    f"🔔 <b>NEAR ALERT — {pair}</b>\n"
                    f"Your level : <b>{target}</b>\n"
                    f"Price now  : {price}  ({dist_pips:.1f} pips away)\n"
                    f"Time       : {now}\n"
                    f"Note       : {row['notes'] or '—'}"
                )
                asyncio.create_task(send_telegram(msg))

    set_last_price(pair, price)


def in_poll_window_local(now_local: datetime) -> bool:
    if POLL_START_HOUR < POLL_END_HOUR:
        return POLL_START_HOUR <= now_local.hour < POLL_END_HOUR
    return now_local.hour >= POLL_START_HOUR or now_local.hour < POLL_END_HOUR


def day_key_local(now_local: datetime) -> str:
    return now_local.strftime("%Y-%m-%d")


def get_used_credits(day_key: str) -> int:
    with db_conn() as conn:
        row = conn.execute(
            "SELECT used_credits FROM td_credit_usage WHERE day_key = ?",
            (day_key,),
        ).fetchone()
    return int(row["used_credits"]) if row else 0


def add_used_credits(day_key: str, credits: int):
    with db_conn() as conn:
        conn.execute(
            """
            INSERT INTO td_credit_usage (day_key, used_credits, updated_at)
            VALUES (?, ?, ?)
            ON CONFLICT(day_key) DO UPDATE SET
                used_credits = td_credit_usage.used_credits + excluded.used_credits,
                updated_at = excluded.updated_at
            """,
            (day_key, credits, datetime.now(timezone.utc).isoformat()),
        )
        conn.commit()


def active_pairs() -> list[str]:
    with db_conn() as conn:
        rows = conn.execute(
            "SELECT DISTINCT pair FROM alerts WHERE active = 1 ORDER BY pair ASC"
        ).fetchall()
    return [r["pair"].upper() for r in rows]


def credits_left_today(now_local: datetime) -> int:
    used = get_used_credits(day_key_local(now_local))
    return max(0, MAX_DAILY_CREDITS - used)


def remaining_window_cycles(now_local: datetime) -> int:
    if not in_poll_window_local(now_local):
        return 0
    end_hour_mod = POLL_END_HOUR % 24
    end_dt = now_local.replace(hour=end_hour_mod, minute=0, second=0, microsecond=0)
    if POLL_START_HOUR >= POLL_END_HOUR and now_local.hour >= POLL_START_HOUR:
        end_dt = end_dt + timedelta(days=1)
    remaining = max(0, int((end_dt - now_local).total_seconds()))
    return max(1, remaining // POLL_INTERVAL_SECONDS)


async def poll_twelve_data_pairs(pairs: list[str], now_local: datetime):
    if not pairs or not TWELVE_DATA_KEY:
        return

    symbols = ",".join(to_td(p) for p in pairs)
    url = f"https://api.twelvedata.com/price?symbol={symbols}&apikey={TWELVE_DATA_KEY}"

    try:
        async with httpx.AsyncClient(timeout=15) as client:
            resp = await client.get(url)
            data = resp.json()
    except Exception as exc:
        print(f"[TD] Poll error: {exc}")
        return

    for pair in pairs:
        sym = to_td(pair)
        price = None
        if isinstance(data, dict) and sym in data and isinstance(data[sym], dict) and "price" in data[sym]:
            price = float(data[sym]["price"])
        elif isinstance(data, dict) and len(pairs) == 1 and "price" in data:
            price = float(data["price"])

        if price and price > 0:
            prev = get_last_price(pair)
            await evaluate_alerts(pair, price, prev)

    add_used_credits(day_key_local(now_local), len(pairs) * CREDITS_PER_SYMBOL_REQUEST)


def choose_pairs_for_cycle(all_pairs: list[str], now_local: datetime) -> list[str]:
    global _td_round_robin_cursor
    if not all_pairs:
        return []

    credits_left = credits_left_today(now_local)
    if credits_left <= 0:
        return []

    cycles_left = remaining_window_cycles(now_local)
    budget_this_cycle = max(1, credits_left // max(1, cycles_left))
    max_symbols = max(1, budget_this_cycle // CREDITS_PER_SYMBOL_REQUEST)
    max_symbols = min(max_symbols, len(all_pairs))

    start = _td_round_robin_cursor % len(all_pairs)
    picked = [all_pairs[(start + i) % len(all_pairs)] for i in range(max_symbols)]
    _td_round_robin_cursor = (start + max_symbols) % len(all_pairs)
    return picked


async def twelve_data_poll_worker():
    while True:
        now_local = datetime.now(APP_TZ)

        if not ENABLE_TWELVE_DATA_POLL or not TWELVE_DATA_KEY:
            await asyncio.sleep(30)
            continue

        if not in_poll_window_local(now_local):
            await asyncio.sleep(30)
            continue

        pairs = active_pairs()
        cycle_pairs = choose_pairs_for_cycle(pairs, now_local)
        if cycle_pairs:
            await poll_twelve_data_pairs(cycle_pairs, now_local)
            used = get_used_credits(day_key_local(now_local))
            print(f"[TD] Polled {len(cycle_pairs)} symbols | credits used today: {used}/{MAX_DAILY_CREDITS}")

        await asyncio.sleep(POLL_INTERVAL_SECONDS)


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def source_state_for(last_heartbeat_at: str | None) -> str:
    if not last_heartbeat_at:
        return "INIT"
    try:
        delta = datetime.now(timezone.utc) - datetime.fromisoformat(last_heartbeat_at)
    except ValueError:
        return "INIT"
    seconds = delta.total_seconds()
    if seconds >= HEARTBEAT_OFFLINE_SECONDS:
        return "OFFLINE"
    if seconds >= HEARTBEAT_STALE_SECONDS:
        return "STALE"
    return "ACTIVE"


def ensure_source(source_id: str):
    with db_conn() as conn:
        conn.execute(
            """
            INSERT INTO ingest_sources (source_id, updated_at)
            VALUES (?, ?)
            ON CONFLICT(source_id) DO UPDATE SET updated_at = excluded.updated_at
            """,
            (source_id, now_iso()),
        )
        conn.commit()


def recalc_sequences(source_id: str) -> tuple[int, int, list[int]]:
    with db_conn() as conn:
        row = conn.execute(
            "SELECT highest_contiguous_sequence FROM ingest_sources WHERE source_id = ?",
            (source_id,),
        ).fetchone()
        highest_contiguous = int(row["highest_contiguous_sequence"]) if row else 0

        while True:
            next_seq = highest_contiguous + 1
            exists = conn.execute(
                "SELECT 1 FROM ingest_events WHERE source_id = ? AND sequence = ?",
                (source_id, next_seq),
            ).fetchone()
            if not exists:
                break
            highest_contiguous = next_seq

        row = conn.execute(
            "SELECT COALESCE(MAX(sequence), 0) AS max_sequence FROM ingest_events WHERE source_id = ?",
            (source_id,),
        ).fetchone()
        highest_sequence = int(row["max_sequence"]) if row else 0

        window_end = min(highest_sequence, highest_contiguous + 256)
        missing_rows = conn.execute(
            """
            WITH RECURSIVE seq(x) AS (
                SELECT ?
                UNION ALL
                SELECT x + 1 FROM seq WHERE x < ?
            )
            SELECT x AS sequence
            FROM seq
            WHERE x > 0
            AND NOT EXISTS (
                SELECT 1
                FROM ingest_events e
                WHERE e.source_id = ? AND e.sequence = x
            )
            """,
            (highest_contiguous + 1, window_end, source_id),
        ).fetchall()
        missing = [int(r["sequence"]) for r in missing_rows]

        hb_row = conn.execute(
            "SELECT last_heartbeat_at FROM ingest_sources WHERE source_id = ?",
            (source_id,),
        ).fetchone()
        heartbeat_at = hb_row["last_heartbeat_at"] if hb_row else None
        state = source_state_for(heartbeat_at)

        conn.execute(
            """
            UPDATE ingest_sources
            SET highest_sequence = ?,
                highest_contiguous_sequence = ?,
                state = ?,
                updated_at = ?
            WHERE source_id = ?
            """,
            (highest_sequence, highest_contiguous, state, now_iso(), source_id),
        )
        conn.commit()

    return highest_sequence, highest_contiguous, missing


async def source_watchdog_worker():
    while True:
        with db_conn() as conn:
            rows = conn.execute(
                "SELECT source_id, last_heartbeat_at FROM ingest_sources"
            ).fetchall()
            for row in rows:
                state = source_state_for(row["last_heartbeat_at"])
                conn.execute(
                    "UPDATE ingest_sources SET state = ?, updated_at = ? WHERE source_id = ?",
                    (state, now_iso(), row["source_id"]),
                )
            conn.commit()
        await asyncio.sleep(5)


def check_ingest_auth(x_ledger_key: str | None):
    if not INGEST_SHARED_KEY:
        return
    if (x_ledger_key or "").strip() != INGEST_SHARED_KEY:
        raise HTTPException(status_code=401, detail="Unauthorized ingest key")


# ── FastAPI startup / shutdown ────────────────────────────────────────────────

@app.on_event("startup")
async def on_startup():
    init_db()
    load_runtime_config()
    asyncio.create_task(source_watchdog_worker())
    asyncio.create_task(twelve_data_poll_worker())
    print("[Server] Ledger Alert Server started")


# ── REST endpoints ────────────────────────────────────────────────────────────

class AlertIn(BaseModel):
    pair:         str
    target_price: float
    near_pips:    float = 10.0
    alert_type:   str   = "BOTH"
    notes:        str   = ""


class SettingsIn(BaseModel):
    telegram_token: str = ""
    telegram_chat_id: str = ""
    twelve_data_key: str = ""


class IngestEvent(BaseModel):
    source_id: str = Field(min_length=1, max_length=128)
    sequence: int = Field(ge=1)
    event_type: Literal["price", "heartbeat"]
    symbol: str = ""
    bid: float | None = None
    ask: float | None = None
    mid: float | None = None
    ts_ms: int | None = None
    checksum: str = ""


class IngestBatch(BaseModel):
    protocol_version: int = 1
    transport_id: str = ""
    events: list[IngestEvent]


class ReplayRequest(BaseModel):
    source_id: str = Field(min_length=1, max_length=128)
    from_sequence: int = Field(ge=1)
    to_sequence: int = Field(ge=1)


@app.get("/health")
def health():
    return {"status": "ok", "time": now_iso()}


@app.get("/alerts")
def list_alerts():
    with db_conn() as conn:
        rows = conn.execute("SELECT * FROM alerts ORDER BY id DESC").fetchall()
    return [dict(r) for r in rows]


@app.post("/alerts", status_code=201)
def create_alert(alert: AlertIn):
    with db_conn() as conn:
        cur = conn.execute(
            "INSERT INTO alerts (pair, target_price, near_pips, alert_type, active, created_at, notes) "
            "VALUES (?,?,?,?,1,?,?)",
            (alert.pair.upper(), alert.target_price, alert.near_pips,
             alert.alert_type, datetime.now(timezone.utc).isoformat(), alert.notes),
        )
        conn.commit()
    return {"id": cur.lastrowid}


@app.delete("/alerts/{alert_id}")
def delete_alert(alert_id: int):
    with db_conn() as conn:
        conn.execute("DELETE FROM alerts WHERE id = ?", (alert_id,))
        conn.commit()
    return {"deleted": alert_id}


@app.patch("/alerts/{alert_id}/reset")
def reset_alert(alert_id: int):
    """Reset triggered flags so the alert can fire again."""
    with db_conn() as conn:
        conn.execute(
            "UPDATE alerts SET touch_triggered = 0, near_triggered = 0 WHERE id = ?",
            (alert_id,),
        )
        conn.commit()
    return {"reset": alert_id}


@app.post("/alerts/sync")
def sync_alerts(alerts: list[AlertIn]):
    """
    Full replace — called by the Ledger app whenever its alert list changes.
    Preserves triggered state for alerts that already exist (matched by pair + target_price).
    """
    with db_conn() as conn:
        # Preserve triggered state for existing alerts
        existing = {
            (r["pair"], r["target_price"]): dict(r)
            for r in conn.execute("SELECT * FROM alerts").fetchall()
        }
        conn.execute("DELETE FROM alerts")
        for a in alerts:
            key = (a.pair.upper(), a.target_price)
            old = existing.get(key)
            conn.execute(
                "INSERT INTO alerts "
                "(pair, target_price, near_pips, alert_type, active, "
                "touch_triggered, near_triggered, created_at, notes) "
                "VALUES (?,?,?,?,1,?,?,?,?)",
                (
                    a.pair.upper(), a.target_price, a.near_pips, a.alert_type,
                    old["touch_triggered"] if old else 0,
                    old["near_triggered"]  if old else 0,
                    old["created_at"]      if old else datetime.now(timezone.utc).isoformat(),
                    a.notes,
                ),
            )
        conn.commit()
    print(f"[Sync] Loaded {len(alerts)} alerts from app")
    return {"synced": len(alerts)}


@app.post("/settings/sync")
def sync_settings(settings: SettingsIn):
    global TELEGRAM_TOKEN, TELEGRAM_CHAT_ID, TWELVE_DATA_KEY

    TELEGRAM_TOKEN = settings.telegram_token.strip()
    TELEGRAM_CHAT_ID = settings.telegram_chat_id.strip()
    TWELVE_DATA_KEY = settings.twelve_data_key.strip() or TWELVE_DATA_KEY
    save_runtime_config(TELEGRAM_TOKEN, TELEGRAM_CHAT_ID, TWELVE_DATA_KEY)
    print("[Sync] Applied remote settings from app")
    return {
        "synced": True,
        "has_telegram_token": bool(TELEGRAM_TOKEN),
        "has_telegram_chat_id": bool(TELEGRAM_CHAT_ID),
        "has_twelve_data_key": bool(TWELVE_DATA_KEY),
    }


@app.post("/ingest/events")
async def ingest_events(batch: IngestBatch, x_ledger_key: str | None = Header(default=None)):
    """
    Ingests ordered events from one or more sources.
    Uses source_id + sequence as idempotency key.
    """
    check_ingest_auth(x_ledger_key)

    if batch.protocol_version != 1:
        raise HTTPException(status_code=400, detail="Unsupported protocol_version")

    accepted = 0
    duplicates = 0
    rejected = 0
    source_ids: set[str] = set()

    with db_conn() as conn:
        for event in batch.events:
            source_ids.add(event.source_id)
            ensure_source(event.source_id)

            if event.event_type == "price" and not event.symbol.strip():
                rejected += 1
                conn.execute(
                    """
                    INSERT INTO ingest_deadletter (source_id, sequence, reason, payload, received_at)
                    VALUES (?, ?, ?, ?, ?)
                    """,
                    (event.source_id, event.sequence, "missing-symbol", event.model_dump_json(), now_iso()),
                )
                continue

            price = event.mid
            if price is None and event.bid is not None and event.ask is not None:
                price = (event.bid + event.ask) / 2.0
            if price is None and event.bid is not None:
                price = event.bid
            if price is None and event.ask is not None:
                price = event.ask

            if event.event_type == "price" and (price is None or price <= 0):
                rejected += 1
                conn.execute(
                    """
                    INSERT INTO ingest_deadletter (source_id, sequence, reason, payload, received_at)
                    VALUES (?, ?, ?, ?, ?)
                    """,
                    (event.source_id, event.sequence, "invalid-price", event.model_dump_json(), now_iso()),
                )
                continue

            payload_hash = event.checksum.strip() or f"{event.source_id}:{event.sequence}:{event.event_type}"
            try:
                conn.execute(
                    """
                    INSERT INTO ingest_events (
                        source_id, sequence, event_type, symbol, price, event_time, payload_hash, received_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        event.source_id,
                        event.sequence,
                        event.event_type,
                        event.symbol.upper().strip() if event.symbol else None,
                        price,
                        str(event.ts_ms) if event.ts_ms else None,
                        payload_hash,
                        now_iso(),
                    ),
                )
            except sqlite3.IntegrityError:
                duplicates += 1
                continue

            accepted += 1
            if event.event_type == "heartbeat":
                conn.execute(
                    """
                    UPDATE ingest_sources
                    SET last_heartbeat_at = ?,
                        last_event_at = ?,
                        state = 'ACTIVE',
                        updated_at = ?
                    WHERE source_id = ?
                    """,
                    (now_iso(), now_iso(), now_iso(), event.source_id),
                )
            else:
                conn.execute(
                    """
                    UPDATE ingest_sources
                    SET last_event_at = ?,
                        updated_at = ?
                    WHERE source_id = ?
                    """,
                    (now_iso(), now_iso(), event.source_id),
                )

        conn.commit()

    source_acks = {}
    for source_id in source_ids:
        highest_sequence, highest_contiguous, missing = recalc_sequences(source_id)
        source_acks[source_id] = {
            "highest_sequence": highest_sequence,
            "highest_contiguous_sequence": highest_contiguous,
            "missing_sequences": missing,
        }

    for event in batch.events:
        if event.event_type != "price":
            continue
        pair = event.symbol.upper().strip()
        price = event.mid
        if price is None and event.bid is not None and event.ask is not None:
            price = (event.bid + event.ask) / 2.0
        if price is None and event.bid is not None:
            price = event.bid
        if price is None and event.ask is not None:
            price = event.ask
        if pair and price and price > 0:
            await evaluate_alerts(pair, price)

    return {
        "protocol_version": 1,
        "accepted": accepted,
        "duplicates": duplicates,
        "rejected": rejected,
        "sources": source_acks,
        "server_time": now_iso(),
    }


@app.post("/ingest/replay-request")
def replay_request(req: ReplayRequest, x_ledger_key: str | None = Header(default=None)):
    check_ingest_auth(x_ledger_key)
    if req.to_sequence < req.from_sequence:
        raise HTTPException(status_code=400, detail="to_sequence must be >= from_sequence")

    max_span = 2000
    to_seq = min(req.to_sequence, req.from_sequence + max_span - 1)

    with db_conn() as conn:
        existing_rows = conn.execute(
            """
            SELECT sequence
            FROM ingest_events
            WHERE source_id = ? AND sequence BETWEEN ? AND ?
            """,
            (req.source_id, req.from_sequence, to_seq),
        ).fetchall()
        existing = {int(r["sequence"]) for r in existing_rows}

    missing = [s for s in range(req.from_sequence, to_seq + 1) if s not in existing]
    highest_sequence, highest_contiguous, _ = recalc_sequences(req.source_id)
    return {
        "source_id": req.source_id,
        "from_sequence": req.from_sequence,
        "to_sequence": to_seq,
        "missing_sequences": missing,
        "highest_sequence": highest_sequence,
        "highest_contiguous_sequence": highest_contiguous,
        "server_time": now_iso(),
    }


@app.get("/ingest/status")
def ingest_status():
    with db_conn() as conn:
        rows = conn.execute(
            """
            SELECT source_id, highest_sequence, highest_contiguous_sequence,
                   last_event_at, last_heartbeat_at, state, updated_at
            FROM ingest_sources
            ORDER BY source_id ASC
            """
        ).fetchall()

    out = []
    for row in rows:
        state = source_state_for(row["last_heartbeat_at"])
        out.append(
            {
                "source_id": row["source_id"],
                "state": state,
                "highest_sequence": row["highest_sequence"],
                "highest_contiguous_sequence": row["highest_contiguous_sequence"],
                "last_event_at": row["last_event_at"],
                "last_heartbeat_at": row["last_heartbeat_at"],
                "updated_at": row["updated_at"],
            }
        )

    return {
        "stale_after_seconds": HEARTBEAT_STALE_SECONDS,
        "offline_after_seconds": HEARTBEAT_OFFLINE_SECONDS,
        "sources": out,
        "server_time": now_iso(),
    }


@app.get("/twelvedata/status")
def twelvedata_status():
    now_local = datetime.now(APP_TZ)
    day = day_key_local(now_local)
    used = get_used_credits(day)
    return {
        "enabled": ENABLE_TWELVE_DATA_POLL,
        "has_key": bool(TWELVE_DATA_KEY),
        "timezone": str(ALERT_TIMEZONE),
        "window": f"{POLL_START_HOUR:02d}:00-{POLL_END_HOUR:02d}:00",
        "poll_interval_seconds": POLL_INTERVAL_SECONDS,
        "credits_per_symbol_request": CREDITS_PER_SYMBOL_REQUEST,
        "max_daily_credits": MAX_DAILY_CREDITS,
        "used_today": used,
        "remaining_today": max(0, MAX_DAILY_CREDITS - used),
        "in_window_now": in_poll_window_local(now_local),
        "server_time": now_iso(),
    }
