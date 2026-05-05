"""
Ledger Alert Server  —  runs on your Raspberry Pi 24 x 7
=========================================================
Keeps a local SQLite database of price alerts.
- REST API  : Ledger Windows app syncs alerts here.
- WebSocket : real-time price feed from Twelve Data (no API credit cost).
- REST poll : fallback every 5 min for indices (NAS100, US30) not on free WS plan.
- Telegram  : fires a message when NEAR or TOUCH condition is met.

Environment variables (set in .env or export before running):
  TWELVE_DATA_KEY   — free key from twelvedata.com
  TELEGRAM_TOKEN    — bot token from @BotFather
  TELEGRAM_CHAT_ID  — your personal chat/group ID
  PORT              — HTTP port (default 8000)
"""

import asyncio
import json
import os
import sqlite3
from datetime import datetime, timezone

import httpx
import websockets
from fastapi import FastAPI
from pydantic import BaseModel

# ── Config ────────────────────────────────────────────────────────────────────

TWELVE_DATA_KEY  = os.getenv("TWELVE_DATA_KEY",  "")
TELEGRAM_TOKEN   = os.getenv("TELEGRAM_TOKEN",   "")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID", "")
DB_PATH          = os.path.join(os.path.dirname(__file__), "alerts.db")

# Pairs that Twelve Data does NOT stream via WebSocket on the free plan.
# These are polled via REST every 5 minutes instead.
INDEX_PAIRS = {"NAS100", "US100", "NASDAQ100", "US30", "DJ30", "SP500", "US500"}

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
        conn.commit()


# ── Symbol helpers ────────────────────────────────────────────────────────────

# Map Ledger pair name → Twelve Data symbol
SYMBOL_MAP = {
    "BTCUSD": "BTC/USD", "ETHUSD": "ETH/USD",
    "XAUUSD": "XAU/USD", "XAGUSD": "XAG/USD",
    "NAS100": "IXIC",    "US100":  "IXIC",  "NASDAQ100": "IXIC",
    "US30":   "DJI",     "DJ30":   "DJI",
    "SP500":  "SPX",     "US500":  "SPX",
}

# Map Twelve Data WS symbol → Ledger pair (for reverse lookup)
REVERSE_MAP = {
    "BTC/USD": "BTCUSD", "ETH/USD": "ETHUSD",
    "XAU/USD": "XAUUSD", "XAG/USD": "XAGUSD",
}


def to_td(pair: str) -> str:
    p = pair.upper().strip()
    if p in SYMBOL_MAP:
        return SYMBOL_MAP[p]
    if len(p) == 6:
        return p[:3] + "/" + p[3:]
    return p


def from_td(symbol: str) -> str:
    if symbol in REVERSE_MAP:
        return REVERSE_MAP[symbol]
    return symbol.replace("/", "")  # EUR/USD → EURUSD


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
        return
    url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
    async with httpx.AsyncClient(timeout=10) as client:
        try:
            await client.post(url, json={
                "chat_id":    TELEGRAM_CHAT_ID,
                "text":       message,
                "parse_mode": "HTML",
            })
        except Exception as exc:
            print(f"[Telegram] Error: {exc}")


# ── Alert evaluation ──────────────────────────────────────────────────────────

async def evaluate_alerts(pair: str, price: float):
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

            # ── TOUCH: within 1 pip ─────────────────────────────────────────
            if (dist_pips <= 1.0
                    and not row["touch_triggered"]
                    and atype in ("TOUCH", "BOTH")):
                conn.execute(
                    "UPDATE alerts SET touch_triggered = 1 WHERE id = ?", (row["id"],)
                )
                conn.commit()
                dec = max(1, round(-1 * (ps - 1)))  # rough decimal count
                fmt = f"{{:.{len(str(target).rstrip('0').split('.')[-1])}f}}"
                msg = (
                    f"🎯 <b>TOUCH ALERT — {pair}</b>\n"
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


# ── WebSocket price worker (real-time, no credit cost) ───────────────────────

async def ws_price_worker():
    """
    Connects to Twelve Data WebSocket and subscribes to all forex/metals/crypto
    pairs that have active alerts.  Reconnects automatically on any error.
    """
    while True:
        if not TWELVE_DATA_KEY:
            await asyncio.sleep(30)
            continue

        # Collect streaming pairs (exclude indices — those use REST)
        with db_conn() as conn:
            rows = conn.execute(
                "SELECT DISTINCT pair FROM alerts WHERE active = 1"
            ).fetchall()

        ws_pairs   = [r["pair"] for r in rows if r["pair"].upper() not in INDEX_PAIRS]
        rest_pairs = [r["pair"] for r in rows if r["pair"].upper() in INDEX_PAIRS]

        # Launch REST poller for indices
        if rest_pairs:
            asyncio.create_task(index_rest_poller(rest_pairs))

        if not ws_pairs:
            await asyncio.sleep(30)
            continue

        symbols = ",".join(to_td(p) for p in ws_pairs)
        uri = f"wss://ws.twelvedata.com/v1/quotes/price?apikey={TWELVE_DATA_KEY}"

        try:
            async with websockets.connect(uri, ping_interval=20, ping_timeout=10) as ws:
                await ws.send(json.dumps({
                    "action": "subscribe",
                    "params": {"symbols": symbols},
                }))
                print(f"[WS] Subscribed to: {symbols}")

                async for raw in ws:
                    try:
                        data = json.loads(raw)
                    except json.JSONDecodeError:
                        continue

                    if data.get("event") == "price":
                        sym   = data.get("symbol", "")
                        price = float(data.get("price", 0))
                        if price > 0:
                            await evaluate_alerts(from_td(sym), price)

        except Exception as exc:
            print(f"[WS] Disconnected: {exc}  — reconnecting in 15 s …")
            await asyncio.sleep(15)


# ── REST poller for indices (not available on free WS plan) ──────────────────

async def index_rest_poller(pairs: list[str]):
    """Poll Twelve Data REST every 5 minutes for NAS100, US30, etc."""
    if not TWELVE_DATA_KEY:
        return
    symbols_str = ",".join(to_td(p) for p in pairs)
    url = (f"https://api.twelvedata.com/price"
           f"?symbol={symbols_str}&apikey={TWELVE_DATA_KEY}")
    async with httpx.AsyncClient(timeout=10) as client:
        for _ in range(12):            # run for 1 hour; ws_worker restarts it
            try:
                resp = await client.get(url)
                data = resp.json()
                for pair in pairs:
                    sym = to_td(pair)
                    if sym in data and "price" in data[sym]:
                        price = float(data[sym]["price"])
                        await evaluate_alerts(pair, price)
                    elif "price" in data:      # single-symbol response
                        price = float(data["price"])
                        await evaluate_alerts(pairs[0], price)
            except Exception as exc:
                print(f"[REST] Error: {exc}")
            await asyncio.sleep(300)   # 5 minutes


# ── FastAPI startup / shutdown ────────────────────────────────────────────────

@app.on_event("startup")
async def on_startup():
    init_db()
    asyncio.create_task(ws_price_worker())
    print("[Server] Ledger Alert Server started")


# ── REST endpoints ────────────────────────────────────────────────────────────

class AlertIn(BaseModel):
    pair:         str
    target_price: float
    near_pips:    float = 10.0
    alert_type:   str   = "BOTH"
    notes:        str   = ""


@app.get("/health")
def health():
    return {"status": "ok", "time": datetime.now(timezone.utc).isoformat()}


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
    return {"synced": len(alerts)}


@app.get("/prices")
async def current_prices():
    """Fetch current prices for all watched pairs on demand."""
    if not TWELVE_DATA_KEY:
        return {"error": "TWELVE_DATA_KEY not set"}
    with db_conn() as conn:
        rows = conn.execute(
            "SELECT DISTINCT pair FROM alerts WHERE active = 1"
        ).fetchall()
    pairs = [r["pair"] for r in rows]
    if not pairs:
        return {}
    symbols = ",".join(to_td(p) for p in pairs)
    async with httpx.AsyncClient(timeout=10) as client:
        resp = await client.get(
            f"https://api.twelvedata.com/price?symbol={symbols}&apikey={TWELVE_DATA_KEY}"
        )
    return resp.json()
