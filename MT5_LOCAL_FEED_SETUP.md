# MT5 EA Local Feed Setup (Windows)

This guide configures Ledger price alerts to use MT5 live prices locally with no API limits.

## What changed in Ledger

- Price alerts now read local MT5 tick data from a JSONL file.
- New Settings fields:
  - Tick File Path
  - Poll Interval (ms)
   - Stale Threshold (s)
   - Desktop alert sound toggle
   - Discord enable, mute toggle, webhook URL, and Test Discord button

- Alert types:
   - `NEAR`: trigger when price enters your selected pip range
   - `TOUCH`: trigger when price touches your target level

## Architecture

MT5 Terminal (running)
-> MT5PriceExporter.mq5 (EA)
-> local tick file (JSONL)
-> Ledger PriceFetcher
-> Alerts tab + desktop popup + sound + Discord

## Step-by-step setup

### 1) Install MT5 exporter EA

1. Copy `bridge/MT5PriceExporter.mq5` into your MT5 Experts folder.
2. Open MetaEditor and compile the EA.
3. Attach `MT5PriceExporter` to any open chart in MT5.
4. In EA inputs:
   - `OutputFileName = ledger_ticks.jsonl`
   - `SymbolsCSV = EURUSD,GBPUSD,USDJPY,XAUUSD` (edit to your watchlist)
   - `PublishEveryMs = 500`

### 2) Confirm tick file is being written

The exporter writes into MT5 FILE_COMMON path.

Typical Windows location:

`C:\Users\<YourUser>\AppData\Roaming\MetaQuotes\Terminal\Common\Files\ledger_ticks.jsonl`

Open the file and verify lines are being appended like:

{"symbol":"EURUSD","bid":1.08231,"ask":1.08235,"ts_ms":1770000000123}

### 3) Configure Ledger settings

1. Open Ledger -> Settings.
2. In "MT5 Local Price Feed":
   - Set Tick File Path to your `ledger_ticks.jsonl`
   - Set Poll Interval to `1000 ms` (or `500 ms`)
   - Set Stale Threshold to `15 s` (recommended)
3. In "Alert Notifications":
   - Enable "Play desktop sound on alerts"
   - Enable "Send Discord notifications"
   - Optional: enable "Mute Discord (desktop only)" if you want popup/sound without Discord
   - Paste your Discord webhook URL
   - Click "Test Discord" to verify webhook delivery
4. Open Alerts tab.
5. Add one test alert on an active symbol from your `SymbolsCSV` list.

### 4) Validate end-to-end

1. Check MT5 is running and connected.
2. Watch Alerts tab "Current Price" update and feed status show `Feed: connected`.
3. Move/choose a nearby alert level and confirm:
   - desktop popup appears
   - desktop sound plays
   - Discord message is delivered

## Linux dev simulation (no MT5 required)

Use the local replay tool to simulate MT5 ticks on Linux before production.

### 1) Start replay generator

From repo root:

```bash
python bridge/tick_replay.py \
   --output ./ledger_ticks_dev.jsonl \
   --symbols EURUSD,GBPUSD,USDJPY,XAUUSD \
   --interval-ms 500 \
   --truncate
```

### 2) Point Ledger to replay file

In Ledger Settings -> MT5 Local Price Feed:

- Tick File Path: absolute path to `ledger_ticks_dev.jsonl`
- Poll Interval: `500-1000 ms`
- Stale Threshold: `15 s`

### 3) Validate stale detection in dev

Run with stale simulation:

```bash
python bridge/tick_replay.py \
   --output ./ledger_ticks_dev.jsonl \
   --symbols EURUSD,GBPUSD \
   --interval-ms 500 \
   --truncate \
   --stale-after-steps 20 \
   --stale-duration-sec 25
```

Expected behavior:

- Alerts feed status becomes stale after threshold
- Stale desktop warning appears once
- Status recovers automatically when ticks resume

### 4) Validate Discord in dev

1. Enable Discord notifications in Settings.
2. Add a nearby test alert.
3. Confirm message delivery.
4. Optionally disconnect network briefly and trigger alert again to observe retry behavior.

## Recommended defaults

- EA `PublishEveryMs`: `500`
- Ledger poll interval: `500-1000 ms`
- Stale threshold: `15 s`
- Keep symbols list small and focused (only what you trade)

## Troubleshooting

### No prices in Ledger

- Verify EA is attached and "AutoTrading" is enabled in MT5.
- Confirm Tick File Path points to the exact file under FILE_COMMON.
- Ensure alert symbol matches exporter symbol format (example: `EURUSD`).

### Prices are stale

- Reduce Ledger poll interval (for example from 1000 to 500 ms).
- Reduce EA publish interval (for example from 1000 to 500 ms).
- Confirm MT5 market is open for that symbol.
- Increase stale threshold if your market/source is quiet (for example 20 to 30 s).

### Discord notification missing

- Confirm "Send Discord notifications" is enabled in Settings.
- Confirm "Mute Discord (desktop only)" is disabled.
- Verify webhook URL is valid and not revoked.
- Check network/firewall rules for Discord webhook access.
- Delivery retries are automatic with backoff.

### Alert did not trigger

- Confirm alert type (`NEAR` or `TOUCH`).
- Use Reset Selected in Alerts tab to clear prior trigger state.
- Verify symbol appears in `SymbolsCSV` so data exists for that pair.

### Where logs are written

Ledger writes alert/Discord event logs to `alert_events.log` under the app data location.

## Possible upgrades

### 1) File tail optimization for huge logs

Read only trailing block when file grows very large to minimize IO.

### 2) Dual-source fallback

Allow optional fallback source only when MT5 file is stale.

### 3) Socket mode

Upgrade EA output from file to local socket stream for lower latency and less disk IO.
