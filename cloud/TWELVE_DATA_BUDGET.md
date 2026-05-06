# Twelve Data Budget Mode (800 credits/day)

This mode is designed for delayed HTF alerts where 3 to 5 minute lag is acceptable.

## Behavior

- Polling window only: `07:00` to `23:00` (local timezone)
- Poll interval default: `300s` (5 minutes)
- Poll only active alert symbols
- Daily hard cap: `MAX_DAILY_CREDITS=800`
- Round-robin sampling when active symbols are too many for budget
- Alert logic supports:
  - `NEAR` alerts
  - `TOUCH/BOTH` alerts with crossed-level detection between two polls

## Why round-robin

At 5-minute polling over 16 hours:

- 16h = 192 polling cycles/day
- Budget per cycle around: `800 / 192 = 4.16`

With `CREDITS_PER_SYMBOL_REQUEST=1`, system can safely check about 4 symbols per cycle on average.
If you track more symbols, polling rotates them fairly.

## Config (.env)

```env
TWELVE_DATA_KEY=...
ENABLE_TWELVE_DATA_POLL=1
ALERT_TIMEZONE=Europe/Berlin
POLL_START_HOUR=7
POLL_END_HOUR=23
POLL_INTERVAL_SECONDS=300
MAX_DAILY_CREDITS=800
CREDITS_PER_SYMBOL_REQUEST=1
```

## Monitoring endpoint

`GET /twelvedata/status`

Returns:

- current window status
- used and remaining credits for local day
- poll settings in effect
