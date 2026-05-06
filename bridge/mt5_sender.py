#!/usr/bin/env python3
"""
Resilient sender bridge:
MT5 JSONL tick file -> Ledger Pi ingest API

Reads JSON lines written by MT5 exporter, assigns monotonic sequence IDs,
and posts batches to /ingest/events with optional shared-key auth.
"""

from __future__ import annotations

import json
import os
import sqlite3
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import httpx


@dataclass
class Config:
    pi_base_url: str
    source_id: str
    ingest_key: str
    input_file: Path
    state_db: Path
    batch_size: int
    flush_interval_s: float
    heartbeat_interval_s: float
    replay_window: int


CFG = Config(
    pi_base_url=os.getenv("LEDGER_PI_URL", "http://127.0.0.1:8000").rstrip("/"),
    source_id=os.getenv("LEDGER_SOURCE_ID", "mt5-main-01").strip(),
    ingest_key=os.getenv("LEDGER_INGEST_KEY", "").strip(),
    input_file=Path(os.getenv("LEDGER_INPUT_FILE", "./ledger_ticks.jsonl")),
    state_db=Path(os.getenv("LEDGER_STATE_DB", "./bridge_state.db")),
    batch_size=max(10, int(os.getenv("LEDGER_BATCH_SIZE", "200"))),
    flush_interval_s=max(0.05, float(os.getenv("LEDGER_FLUSH_INTERVAL", "0.30"))),
    heartbeat_interval_s=max(1.0, float(os.getenv("LEDGER_HEARTBEAT_INTERVAL", "5.0"))),
    replay_window=max(50, int(os.getenv("LEDGER_REPLAY_WINDOW", "500"))),
)


def now_ms() -> int:
    return int(time.time() * 1000)


def db_conn() -> sqlite3.Connection:
    conn = sqlite3.connect(CFG.state_db)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    CFG.state_db.parent.mkdir(parents=True, exist_ok=True)
    with db_conn() as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS bridge_state (
                id INTEGER PRIMARY KEY CHECK (id = 1),
                last_sequence INTEGER NOT NULL DEFAULT 0,
                last_offset INTEGER NOT NULL DEFAULT 0,
                last_inode INTEGER NOT NULL DEFAULT 0,
                last_heartbeat_ms INTEGER NOT NULL DEFAULT 0,
                updated_at TEXT
            )
            """
        )
        conn.execute(
            """
            INSERT INTO bridge_state(id) VALUES (1)
            ON CONFLICT(id) DO NOTHING
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS outbox (
                sequence INTEGER PRIMARY KEY,
                event_json TEXT NOT NULL,
                sent INTEGER NOT NULL DEFAULT 0,
                created_ms INTEGER NOT NULL
            )
            """
        )
        conn.commit()


def get_state() -> dict[str, int]:
    with db_conn() as conn:
        row = conn.execute("SELECT * FROM bridge_state WHERE id = 1").fetchone()
    return {
        "last_sequence": int(row["last_sequence"]),
        "last_offset": int(row["last_offset"]),
        "last_inode": int(row["last_inode"]),
        "last_heartbeat_ms": int(row["last_heartbeat_ms"]),
    }


def save_state(**kwargs: int) -> None:
    allowed = {"last_sequence", "last_offset", "last_inode", "last_heartbeat_ms"}
    fields = [k for k in kwargs.keys() if k in allowed]
    if not fields:
        return

    assignments = ", ".join(f"{f} = ?" for f in fields)
    values = [int(kwargs[f]) for f in fields]
    with db_conn() as conn:
        conn.execute(
            f"UPDATE bridge_state SET {assignments}, updated_at = ? WHERE id = 1",
            (*values, time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())),
        )
        conn.commit()


def put_outbox_event(event: dict[str, Any], sequence: int) -> None:
    with db_conn() as conn:
        conn.execute(
            "INSERT OR REPLACE INTO outbox(sequence, event_json, sent, created_ms) VALUES (?, ?, 0, ?)",
            (sequence, json.dumps(event, separators=(",", ":")), now_ms()),
        )
        conn.commit()


def get_unsent(limit: int) -> list[sqlite3.Row]:
    with db_conn() as conn:
        rows = conn.execute(
            "SELECT sequence, event_json FROM outbox WHERE sent = 0 ORDER BY sequence ASC LIMIT ?",
            (limit,),
        ).fetchall()
    return rows


def mark_sent_up_to(sequence: int) -> None:
    with db_conn() as conn:
        conn.execute("UPDATE outbox SET sent = 1 WHERE sequence <= ?", (sequence,))
        conn.execute(
            "DELETE FROM outbox WHERE sent = 1 AND sequence < ?",
            (max(0, sequence - 5000),),
        )
        conn.commit()


def mark_unsent_sequences(sequences: list[int]) -> None:
    if not sequences:
        return
    with db_conn() as conn:
        conn.executemany("UPDATE outbox SET sent = 0 WHERE sequence = ?", [(s,) for s in sequences])
        conn.commit()


def next_sequence() -> int:
    state = get_state()
    new_seq = state["last_sequence"] + 1
    save_state(last_sequence=new_seq)
    return new_seq


def valid_tick(payload: dict[str, Any]) -> bool:
    symbol = str(payload.get("symbol", "")).strip().upper()
    if not symbol:
        return False
    bid = payload.get("bid")
    ask = payload.get("ask")
    mid = payload.get("mid")
    if mid is None and bid is None and ask is None:
        return False
    return True


def tick_to_event(payload: dict[str, Any], sequence: int) -> dict[str, Any]:
    return {
        "source_id": CFG.source_id,
        "sequence": sequence,
        "event_type": "price",
        "symbol": str(payload.get("symbol", "")).strip().upper(),
        "bid": payload.get("bid"),
        "ask": payload.get("ask"),
        "mid": payload.get("mid"),
        "ts_ms": int(payload.get("ts_ms") or now_ms()),
        "checksum": "",
    }


def heartbeat_event(sequence: int) -> dict[str, Any]:
    return {
        "source_id": CFG.source_id,
        "sequence": sequence,
        "event_type": "heartbeat",
        "symbol": "",
        "ts_ms": now_ms(),
        "checksum": "",
    }


def read_new_lines() -> int:
    if not CFG.input_file.exists():
        return 0

    stat = CFG.input_file.stat()
    inode = int(getattr(stat, "st_ino", 0))
    size = int(stat.st_size)

    state = get_state()
    offset = state["last_offset"]
    last_inode = state["last_inode"]

    if inode != last_inode or offset > size:
        offset = 0

    added = 0
    with CFG.input_file.open("r", encoding="utf-8") as f:
        f.seek(offset)
        while True:
            line = f.readline()
            if not line:
                break
            offset = f.tell()
            line = line.strip()
            if not line:
                continue
            try:
                payload = json.loads(line)
            except json.JSONDecodeError:
                continue

            if not isinstance(payload, dict) or not valid_tick(payload):
                continue

            seq = next_sequence()
            put_outbox_event(tick_to_event(payload, seq), seq)
            added += 1

    save_state(last_offset=offset, last_inode=inode)
    return added


def maybe_enqueue_heartbeat() -> bool:
    state = get_state()
    now = now_ms()
    if now - state["last_heartbeat_ms"] < int(CFG.heartbeat_interval_s * 1000):
        return False

    seq = next_sequence()
    put_outbox_event(heartbeat_event(seq), seq)
    save_state(last_heartbeat_ms=now)
    return True


def build_headers() -> dict[str, str]:
    headers = {"Content-Type": "application/json"}
    if CFG.ingest_key:
        headers["X-Ledger-Key"] = CFG.ingest_key
    return headers


def post_batch(client: httpx.Client, events: list[dict[str, Any]]) -> dict[str, Any]:
    payload = {
        "protocol_version": 1,
        "transport_id": "mt5-file-bridge",
        "events": events,
    }
    resp = client.post(
        f"{CFG.pi_base_url}/ingest/events",
        headers=build_headers(),
        json=payload,
        timeout=10.0,
    )
    resp.raise_for_status()
    return resp.json()


def pull_replay_gap(client: httpx.Client) -> None:
    state = get_state()
    upper = state["last_sequence"]
    if upper <= 0:
        return
    lower = max(1, upper - CFG.replay_window)

    req = {
        "source_id": CFG.source_id,
        "from_sequence": lower,
        "to_sequence": upper,
    }
    try:
        resp = client.post(
            f"{CFG.pi_base_url}/ingest/replay-request",
            headers=build_headers(),
            json=req,
            timeout=10.0,
        )
        resp.raise_for_status()
        data = resp.json()
        missing = [int(x) for x in data.get("missing_sequences", [])]
        mark_unsent_sequences(missing)
    except Exception:
        return


def flush_outbox(client: httpx.Client) -> int:
    rows = get_unsent(CFG.batch_size)
    if not rows:
        return 0

    events = [json.loads(r["event_json"]) for r in rows]
    try:
        result = post_batch(client, events)
    except Exception:
        return 0

    source = result.get("sources", {}).get(CFG.source_id, {})
    contiguous = int(source.get("highest_contiguous_sequence", 0))
    missing = [int(x) for x in source.get("missing_sequences", [])]

    if contiguous > 0:
        mark_sent_up_to(contiguous)
    if missing:
        mark_unsent_sequences(missing)

    return int(result.get("accepted", 0))


def print_banner() -> None:
    print("Ledger MT5 Sender Bridge")
    print(f"  source_id     : {CFG.source_id}")
    print(f"  pi_url        : {CFG.pi_base_url}")
    print(f"  input_file    : {CFG.input_file}")
    print(f"  state_db      : {CFG.state_db}")
    print(f"  batch_size    : {CFG.batch_size}")
    print(f"  flush_interval: {CFG.flush_interval_s}s")
    print(f"  heartbeat_int : {CFG.heartbeat_interval_s}s")


def main() -> None:
    init_db()
    print_banner()

    with httpx.Client() as client:
        last_replay_ms = 0
        while True:
            read_new_lines()
            maybe_enqueue_heartbeat()
            flush_outbox(client)

            now = now_ms()
            if now - last_replay_ms > 15000:
                pull_replay_gap(client)
                last_replay_ms = now

            time.sleep(CFG.flush_interval_s)


if __name__ == "__main__":
    main()
