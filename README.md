# Ledger

Ledger is a Qt desktop trading journal and execution assistant focused on discretionary FX/CFD workflows.

It combines:

- trade journaling
- calendar and account analytics
- MT5 signal export
- checklist-driven setup scoring
- local MT5 price alerts with desktop and Discord notifications
- in-app auto-update from GitHub Releases

This README is the single source of truth for how to build, run, configure, test, and release this project.

## Contents

- Project goals
- Current feature set
- Application architecture
- Directory structure
- Build and run with CMake
- MT5 integration
- Local price alert system
- Development and testing workflow
- Release workflow (GitHub)
- Configuration and data files
- Troubleshooting
- Documentation maintenance policy

## Project goals

- Keep a fast desktop-first trading workflow.
- Store all journal data locally in SQLite.
- Support one-click execution signal writing for MT5 bridge EA.
- Support free, local, low-latency price alerts without paid market-data APIs.
- Keep deployment simple for Windows users.

## Current feature set

### 1) Trades tab

- Add, edit, delete trades
- Search and filter trades
- Export trades to CSV
- Track result metrics such as R and USD

### 2) Calendar tab

- Month view with daily trade summaries
- Year selector and month navigation
- Daily trade count and PnL aggregation

### 3) Accounts tab

- Per-account analytics view for:
  - Funded 1
  - Funded 2
  - Live
- Balance and performance rollups from stored trades

### 4) Execute tab

- Pair/direction/entry/SL/TP input
- Multi-account risk controls
- Lot-size calculations
- MT5 signal queue writing for bridge EA consumption

### 5) Checklist tab

- 9-condition, multi-timeframe checklist
- Weighted scoring (HTF/MTF/LTF)
- Setup quality state:
  - Not Ready
  - Simple Setup
  - 2-Step Setup
  - Perfect Setup

### 6) Alerts tab

- Price alerts stored in local database
- Two alert types:
  - NEAR (within selectable pip range)
  - TOUCH (touch level)
- Current price display and alert status tracking
- Feed health indicator:
  - connected
  - stale
  - waiting for file / path not set

### 7) Notifications

- Desktop tray popup alerts
- Desktop beep on trigger (configurable)
- Discord webhook notifications with retry/backoff
- Discord mute toggle (desktop-only mode)
- Test Discord button in Settings
- Alert event log file creation/opening from Settings

### 8) Updater

- Check for updates from latest GitHub Release
- Download zip package
- Apply update via PowerShell script on Windows

## Application architecture

### High-level flow

- UI layer in Qt Widgets
- Core domain helpers for lot sizing, instrument formatting, database, and alert feed parsing
- SQLite persistence in ledger.db
- MT5 integration through file-based signal and tick flows

### Price alert flow (current)

- MT5PriceExporter.mq5 writes JSONL ticks into MT5 common files
- Ledger PriceFetcher polls local JSONL file
- Ledger evaluates NEAR and TOUCH conditions
- Ledger triggers desktop popup/beep and optional Discord webhook
- Ledger logs events to alert log file

### Persistence

SQLite database file:

- ledger.db

Tables:

- trades
- alerts

## Directory structure

- core: domain and infrastructure code
- ui: Qt widget screens and interactions
- bridge: MT5 bridge assets and replay tool
- build: local build output
- cloud: legacy Pi alert server assets removed from active flow

Important files:

- CMakeLists.txt
- main.cpp
- core/database.cpp
- core/pricefetcher.h
- core/pricealert.h
- core/signalwriter.h
- ui/mainwindow.cpp
- ui/alertswidget.cpp
- ui/checklistwidget.cpp
- bridge/MT5PriceExporter.mq5
- bridge/tick_replay.py
- WINDOWS_SETUP.md
- MT5_LOCAL_FEED_SETUP.md

## Build and run with CMake

## Prerequisites

- CMake 3.16+
- C++17 compiler
- Qt 6 (Core, Widgets, Sql, Network)

## Linux build

```bash
cmake -S . -B build
cmake --build build -j
./build/Ledger
```

## Windows build

Use your preferred generator/toolchain with Qt 6 configured.

Example pattern:

```bash
cmake -S . -B build -G "Ninja"
cmake --build build --config Release
```

Notes:

- On Windows, the target is configured as a GUI executable to avoid opening an extra console window.
- Release packaging and deploy are covered in WINDOWS_SETUP.md.

## MT5 integration

### Signal writing (Execute tab)

Ledger writes one or more JSON signal objects to the configured signal file path for MT5 bridge EA usage.

Setting:

- mt5SignalPath

### Live tick input for alerts

MT5 exporter EA:

- bridge/MT5PriceExporter.mq5

It writes JSONL lines like:

```json
{"symbol":"EURUSD","bid":1.08231,"ask":1.08235,"ts_ms":1770000000123}
```

Ledger reads this file from path:

- mt5TickFilePath

## Local price alert system

### Alert types

- NEAR: trigger when price is within near_pips distance from target
- TOUCH: trigger when price touches target within touch threshold

### Runtime settings

In Settings -> MT5 Local Price Feed:

- Tick File Path
- Poll Interval (ms)
- Stale Threshold (s)

In Settings -> Alert Notifications:

- Desktop sound toggle
- Discord enable toggle
- Discord mute toggle
- Discord webhook URL
- Test Discord button
- Open Alert Log button

### Event log

Ledger writes alert/Discord events to:

- alert_events.log under app data location

## Development and testing workflow

## 1) Fast local simulation without MT5

Use replay tool:

```bash
python bridge/tick_replay.py \
  --output ./ledger_ticks_dev.jsonl \
  --symbols EURUSD,GBPUSD,USDJPY,XAUUSD \
  --interval-ms 500 \
  --truncate
```

Then set Tick File Path to the generated file and validate alert behavior.

## 2) Stale-feed simulation

```bash
python bridge/tick_replay.py \
  --output ./ledger_ticks_dev.jsonl \
  --symbols EURUSD,GBPUSD \
  --interval-ms 500 \
  --truncate \
  --stale-after-steps 20 \
  --stale-duration-sec 25
```

Expected:

- feed status transitions to stale
- stale warning notification appears once
- status recovers when ticks resume

## 3) Discord testing

- Set webhook URL
- Click Test Discord button
- Optionally disable network briefly to observe retry/backoff behavior

## Release workflow (GitHub)

Refer to:

- WINDOWS_SETUP.md

Summary:

- push version tags
- GitHub Actions builds Windows artifact
- release zip is published
- in-app updater downloads and applies release

## Configuration and data files

Common runtime artifacts:

- ledger.db
- ledger_signal.json (or configured signal path)
- ledger_ticks.jsonl (MT5 exporter output)
- alert_events.log (app data location)

## Troubleshooting

### CMake configure/build fails

- Verify Qt 6 is discoverable by CMake
- Verify compiler toolchain is installed
- Confirm required Qt components: Core, Widgets, Sql, Network

### No prices in Alerts tab

- Confirm MT5 exporter EA is running
- Confirm Tick File Path points to active JSONL file
- Confirm symbol names match alert pairs

### Feed shows stale

- Check MT5/exporter is still writing
- Increase stale threshold for slower symbols
- Reduce poll interval for faster refresh

### Discord not delivering

- Ensure Discord enabled and not muted
- Verify webhook URL is valid
- Check firewall/proxy/network restrictions

### Tray icon warning or missing icon

- Ensure OS tray is available
- Ensure app icon/theme icon resources are available on target platform

## Documentation maintenance policy

This repository follows docs-with-code updates.

Rule:

- Every feature add/change/remove must include README updates in the same commit.

Minimum checklist for every feature PR/commit:

- Update feature list section
- Update configuration keys/paths if changed
- Update build/run instructions if changed
- Update testing section if changed
- Update related setup docs (for example MT5_LOCAL_FEED_SETUP.md, WINDOWS_SETUP.md)

Suggested commit style:

- feat: add <feature>
- docs: update README for <feature>

If code and docs diverge, treat README updates as required before release.

## License

No license file is currently defined in repository root.
Add a LICENSE file if you plan to distribute publicly on GitHub.
