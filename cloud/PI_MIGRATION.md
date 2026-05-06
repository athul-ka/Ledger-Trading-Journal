# Pi Migration Guide (One Command)

Use this when you need to push the latest alert-server changes to your Raspberry Pi.

## Prerequisites

- Pi setup already completed once with `install_pi.sh`
- SSH access from your machine to Pi user account
- Pi service name is `ledger-alerts`

## 1) Set German-time window and budget on Pi

Edit `/home/pi/ledger-alerts/.env` on the Pi and confirm:

```env
TWELVE_DATA_KEY=YOUR_TWELVE_DATA_KEY
ENABLE_TWELVE_DATA_POLL=1
ALERT_TIMEZONE=Europe/Berlin
POLL_START_HOUR=7
POLL_END_HOUR=23
POLL_INTERVAL_SECONDS=300
MAX_DAILY_CREDITS=800
CREDITS_PER_SYMBOL_REQUEST=1
```

## 2) Run one-command deploy from this repo

```bash
bash cloud/deploy_pi_update.sh <PI_IP_OR_HOSTNAME> [PI_USER]
```

Examples:

```bash
bash cloud/deploy_pi_update.sh 192.168.1.55
bash cloud/deploy_pi_update.sh raspberrypi.local pi
```

## 3) Verify on Pi

- Health: `http://PI_IP:8000/health`
- Budget status: `http://PI_IP:8000/twelvedata/status`
- Alerts: `http://PI_IP:8000/alerts`

## Notes

- The script syncs these files:
  - `cloud/server.py`
  - `cloud/requirements.txt`
  - `cloud/install_pi.sh`
  - `cloud/TWELVE_DATA_BUDGET.md`
  - `cloud/INGEST_PROTOCOL.md`
- Then it installs requirements, restarts systemd service, and checks endpoints.
