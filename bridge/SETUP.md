# MT5 -> Pi Sender Bridge Setup

This is the recommended stable stack for your current architecture:

- MT5 EA exports prices to JSONL file
- Python sender reads file and pushes events to Pi `/ingest/events`
- Pi evaluates alerts and sends Telegram

## 1) Why this path is best now

- Stable: avoids MQL5 socket/DLL complexity
- Reliable: sender persists sequence and outbox in SQLite
- Compatible: already matches your ingest protocol in `cloud/INGEST_PROTOCOL.md`
- Easy migration: same event schema can later move to TCP/ZeroMQ

## 2) Deploy MT5 exporter EA

1. Copy `bridge/MT5PriceExporter.mq5` to:
   - `<MT5 Data Folder>/MQL5/Experts/`
2. Compile in MetaEditor
3. Attach EA to any chart
4. Set inputs:
   - `OutputFileName=ledger_ticks.jsonl`
   - `SymbolsCSV=EURUSD,GBPUSD,USDJPY,XAUUSD` (edit to your watchlist)
   - `PublishEveryMs=500`

EA writes into `FILE_COMMON` path, shared across terminals.

## 3) Run sender on MT5 machine (Windows)

From your bridge folder:

```bash
python -m venv .venv
.venv\\Scripts\\activate
pip install -r requirements.txt
```

Set environment (PowerShell):

```powershell
$env:LEDGER_PI_URL="http://<PI_IP>:8000"
$env:LEDGER_INGEST_KEY="<YOUR_INGEST_SHARED_KEY>"
$env:LEDGER_SOURCE_ID="mt5-main-01"
$env:LEDGER_INPUT_FILE="$env:APPDATA\\MetaQuotes\\Terminal\\Common\\Files\\ledger_ticks.jsonl"
$env:LEDGER_STATE_DB=".\\bridge_state.db"
python .\\mt5_sender.py
```

## 4) Verify on Pi

- Source health:
  - `GET /ingest/status`
- Expected state:
  - `ACTIVE` while sender is running
- Sequence should increase continuously

## 5) Reliability behavior

- Dedupe: server dedupes by `(source_id, sequence)`
- Replay: sender periodically checks missing sequences and resends
- Heartbeat: sent every ~5 seconds by default
- Failover states on Pi:
  - `ACTIVE` -> `STALE` -> `OFFLINE`

## 6) Next upgrade (optional)

When you want lower latency and bigger scale:

1. Keep `mt5_sender.py` logic and protocol unchanged
2. Replace file intake with local TCP/ZeroMQ receiver
3. Keep same payload and ingest endpoints
