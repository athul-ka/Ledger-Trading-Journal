#!/usr/bin/env python3
"""
Local tick replay generator for Ledger MT5 feed testing.

Writes JSONL lines compatible with MT5PriceExporter.mq5 output format:
{"symbol":"EURUSD","bid":1.08231,"ask":1.08235,"ts_ms":1770000000123}

Use this on Linux/macOS/Windows to simulate live ticks without MT5.
"""

from __future__ import annotations

import argparse
import json
import random
import time
from pathlib import Path


def pip_size(symbol: str) -> float:
    s = symbol.upper()
    if s in {"XAUUSD"}:
        return 0.01
    if s in {"XAGUSD"}:
        return 0.001
    if s in {"NAS100", "US100", "US30"}:
        return 0.1
    if s in {"BTCUSD", "ETHUSD"}:
        return 1.0
    if s.endswith("JPY"):
        return 0.01
    return 0.0001


def base_price(symbol: str) -> float:
    defaults = {
        "EURUSD": 1.0800,
        "GBPUSD": 1.2600,
        "USDJPY": 155.00,
        "XAUUSD": 2350.00,
        "NAS100": 18500.0,
        "US30": 39000.0,
        "BTCUSD": 62000.0,
        "ETHUSD": 3000.0,
    }
    s = symbol.upper()
    if s in defaults:
        return defaults[s]
    if s.endswith("JPY"):
        return 150.0
    return 1.0000


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate simulated MT5 JSONL ticks for Ledger.")
    parser.add_argument(
        "--output",
        default="./ledger_ticks_dev.jsonl",
        help="Output JSONL file path (default: ./ledger_ticks_dev.jsonl)",
    )
    parser.add_argument(
        "--symbols",
        default="EURUSD,GBPUSD,USDJPY,XAUUSD",
        help="Comma-separated symbol list",
    )
    parser.add_argument(
        "--interval-ms",
        type=int,
        default=500,
        help="Interval between publish cycles in ms",
    )
    parser.add_argument(
        "--steps",
        type=int,
        default=0,
        help="Number of cycles to emit; 0 means run forever",
    )
    parser.add_argument(
        "--spread-pips",
        type=float,
        default=0.8,
        help="Bid/ask spread in pips",
    )
    parser.add_argument(
        "--drift-pips",
        type=float,
        default=0.15,
        help="Per-cycle directional drift in pips",
    )
    parser.add_argument(
        "--jitter-pips",
        type=float,
        default=0.6,
        help="Random per-cycle jitter in pips",
    )
    parser.add_argument(
        "--truncate",
        action="store_true",
        help="Truncate output file before writing",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for deterministic replay",
    )
    parser.add_argument(
        "--stale-after-steps",
        type=int,
        default=0,
        help="After this many cycles, pause writing once to simulate stale feed (0 disables)",
    )
    parser.add_argument(
        "--stale-duration-sec",
        type=float,
        default=20.0,
        help="Pause duration in seconds when stale simulation triggers",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    random.seed(args.seed)

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    symbols = [s.strip().upper() for s in args.symbols.split(",") if s.strip()]
    if not symbols:
        raise SystemExit("No symbols configured")

    prices = {sym: base_price(sym) for sym in symbols}
    stale_triggered = False

    mode = "w" if args.truncate else "a"
    interval_s = max(0.05, args.interval_ms / 1000.0)

    print(f"[tick_replay] writing to: {out_path}")
    print(f"[tick_replay] symbols: {', '.join(symbols)}")
    print(f"[tick_replay] interval: {args.interval_ms} ms")

    with out_path.open(mode, encoding="utf-8") as f:
        cycle = 0
        while True:
            cycle += 1

            if (
                args.stale_after_steps > 0
                and cycle >= args.stale_after_steps
                and not stale_triggered
            ):
                stale_triggered = True
                print(
                    f"[tick_replay] simulating stale feed for {args.stale_duration_sec:.1f}s "
                    f"after cycle {cycle}"
                )
                time.sleep(max(0.0, args.stale_duration_sec))

            for sym in symbols:
                pip = pip_size(sym)
                drift = args.drift_pips * pip
                jitter = random.uniform(-args.jitter_pips * pip, args.jitter_pips * pip)

                prices[sym] = max(pip, prices[sym] + drift + jitter)

                spread = max(pip * 0.1, args.spread_pips * pip)
                bid = prices[sym] - (spread / 2.0)
                ask = prices[sym] + (spread / 2.0)

                payload = {
                    "symbol": sym,
                    "bid": round(bid, 10),
                    "ask": round(ask, 10),
                    "ts_ms": int(time.time() * 1000),
                }
                f.write(json.dumps(payload, separators=(",", ":")) + "\n")

            f.flush()

            if args.steps > 0 and cycle >= args.steps:
                print(f"[tick_replay] completed {cycle} cycles")
                break

            time.sleep(interval_s)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
