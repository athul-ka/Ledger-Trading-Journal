#!/usr/bin/env bash
# Deploy latest Ledger alert-server changes to Raspberry Pi.
# Run from this repository root on your local machine.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <pi-host-or-ip> [pi-user]"
  echo "Example: $0 192.168.1.55 pi"
  exit 1
fi

PI_HOST="$1"
PI_USER="${2:-pi}"
REMOTE_DIR="/home/${PI_USER}/ledger-alerts"
REMOTE_VENV="${REMOTE_DIR}/venv"
SERVICE_NAME="ledger-alerts"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

FILES=(
  "server.py"
  "requirements.txt"
  "install_pi.sh"
  "TWELVE_DATA_BUDGET.md"
  "INGEST_PROTOCOL.md"
)

echo "[1/4] Checking SSH connectivity to ${PI_USER}@${PI_HOST}"
ssh -o BatchMode=yes -o ConnectTimeout=8 "${PI_USER}@${PI_HOST}" "echo 'SSH OK'" >/dev/null

echo "[2/4] Uploading updated files to ${REMOTE_DIR}"
for file in "${FILES[@]}"; do
  scp "${SCRIPT_DIR}/${file}" "${PI_USER}@${PI_HOST}:${REMOTE_DIR}/${file}" >/dev/null
  echo "  - synced ${file}"
done

echo "[3/4] Installing dependencies and restarting ${SERVICE_NAME}"
ssh "${PI_USER}@${PI_HOST}" bash -s <<EOF
set -euo pipefail
REMOTE_DIR="${REMOTE_DIR}"
REMOTE_VENV="${REMOTE_VENV}"
SERVICE_NAME="${SERVICE_NAME}"

if [[ ! -d "$REMOTE_DIR" ]]; then
  echo "Remote directory not found: $REMOTE_DIR"
  exit 1
fi

if [[ ! -x "$REMOTE_VENV/bin/pip" ]]; then
  echo "Virtual environment not found: $REMOTE_VENV"
  echo "Run install_pi.sh once on the Pi first."
  exit 1
fi

"$REMOTE_VENV/bin/pip" install -r "$REMOTE_DIR/requirements.txt"
sudo systemctl daemon-reload
sudo systemctl restart "$SERVICE_NAME"
sudo systemctl is-active "$SERVICE_NAME"
EOF

echo "[4/4] Verifying API endpoints"
ssh "${PI_USER}@${PI_HOST}" bash -s <<'EOF'
set -euo pipefail
if command -v curl >/dev/null 2>&1; then
  curl -fsS "http://127.0.0.1:8000/health" | sed -n '1,1p'
  curl -fsS "http://127.0.0.1:8000/twelvedata/status" | sed -n '1,1p'
else
  echo "curl not installed on Pi; skipping endpoint checks"
fi
EOF

echo "Deployment complete."
