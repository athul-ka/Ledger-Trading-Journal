# Windows-Local Price Alert Plan (No Raspberry Pi)

## Goal

Run alerting fully on the Windows machine where Ledger is open, using live broker-grade prices with no third-party API credit limits.

## Recommended Option (Best Fit)

Use MT5 terminal prices locally:

1. MT5 EA exports ticks to a local JSONL file (`bridge/MT5PriceExporter.mq5`)
2. Ledger app reads the latest prices from that file every 200-1000 ms
3. Existing local alert logic evaluates NEAR/TOUCH and triggers desktop notifications

Why this is best:

- Free: no paid API required
- No request limits: local broker feed, no REST credits
- Reliable: no Pi/network dependency for alert evaluation
- Low latency: near real-time while app is running

## Other Options

### Option A: Broker WebSocket API directly (if broker supports)

- Pros: true streaming prices, low latency
- Cons: broker-specific auth/protocol, more implementation complexity
- Use when: you want architecture independent of MT5 terminal process

### Option B: Lightweight local bridge process (Python) from MT5 file to local HTTP

- Pros: decouples file parsing from Qt UI thread, easy retries/health checks
- Cons: one extra local process to run
- Use when: you want observability and process isolation

### Option C: Public market-data API polling (free tiers)

- Pros: easy to integrate
- Cons: limits, throttling, occasional delays/inconsistency
- Not recommended for your requirements

## Implementation Phases

### Phase 1: Remove old Pi/cloud alert path (done)

- Remove Pi sync helper (`core/alertsync.h`)
- Remove Pi settings and sync actions from UI
- Delete cloud alert server artifacts under `cloud/`

### Phase 2: Add local MT5 feed adapter in Ledger

1. Add settings key for local tick file path:
   - Default: `%APPDATA%/MetaQuotes/Terminal/Common/Files/ledger_ticks.jsonl`
2. Add parser in `PriceFetcher` for last line JSON:
   - Accept `{symbol,bid,ask,ts_ms}`
   - Use `mid=(bid+ask)/2` as price
3. Replace Twelve Data polling as primary source with local-file polling (1s default)
4. Keep alert evaluation logic unchanged (NEAR/TOUCH/BOTH)

### Phase 3: Reliability hardening

1. Detect file rotation/truncation safely
2. Ignore malformed lines without stopping polling
3. Add stale-price indicator if no update > N seconds
4. Add optional fallback provider toggle (off by default)

### Phase 4: UX and validation

1. Add "Data source" section in Settings:
   - Source: `MT5 Local File`
   - Tick file path picker
   - Poll interval (200-2000 ms)
2. Show source health in Alerts tab:
   - `Connected / Stale / Waiting for file`
3. Validate with EURUSD and XAUUSD test alerts

## Data Freshness Expectations

- MT5 export interval: 500 ms (configurable in EA)
- Ledger poll interval: 200-1000 ms
- Effective alert latency target: < 1-2 seconds while app is open

## Acceptance Criteria

1. Alerts fire without any Pi/cloud service running
2. Works with internet outages as long as MT5 remains connected to broker
3. No Twelve Data key or Telegram key required
4. Existing add/delete/reset alert UX remains functional
