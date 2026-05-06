#!/usr/bin/env bash
# install_pi.sh  —  One-shot setup for Ledger Alert Server on Raspberry Pi
# Usage:  bash install_pi.sh
# Run as the normal pi/user account (not root).  sudo is used where needed.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_DIR="$HOME/ledger-alerts"
SERVICE_NAME="ledger-alerts"
VENV_DIR="$INSTALL_DIR/venv"
ENV_FILE="$INSTALL_DIR/.env"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

echo ""
echo "=== Ledger Alert Server — Raspberry Pi installer ==="
echo ""

# ── 1. System packages ────────────────────────────────────────────────────────
echo "▸ Installing system packages …"
sudo apt-get update -qq
sudo apt-get install -y python3 python3-pip python3-venv git > /dev/null

# ── 2. Create install directory ───────────────────────────────────────────────
mkdir -p "$INSTALL_DIR"
cp "$SCRIPT_DIR/server.py"       "$INSTALL_DIR/server.py"
cp "$SCRIPT_DIR/requirements.txt" "$INSTALL_DIR/requirements.txt"

# ── 3. Python virtual environment ─────────────────────────────────────────────
echo "▸ Creating Python virtual environment …"
python3 -m venv "$VENV_DIR"
"$VENV_DIR/bin/pip" install --upgrade pip -q
"$VENV_DIR/bin/pip" install -r "$INSTALL_DIR/requirements.txt" -q
echo "  ✓ Dependencies installed"

# ── 4. .env file (config) ─────────────────────────────────────────────────────
if [ ! -f "$ENV_FILE" ]; then
    cat > "$ENV_FILE" << 'EOF'
# Twelve Data free API key — https://twelvedata.com/register
TWELVE_DATA_KEY=YOUR_KEY_HERE

# Telegram Bot — get token from @BotFather, get chat id from @userinfobot
TELEGRAM_TOKEN=YOUR_BOT_TOKEN_HERE
TELEGRAM_CHAT_ID=YOUR_CHAT_ID_HERE

# Port the server listens on  (default 8000)
PORT=8000
EOF
    echo "  ✓ Created $ENV_FILE — edit it with your keys before starting!"
else
    echo "  ℹ  $ENV_FILE already exists — skipping"
fi

# ── 5. systemd service ────────────────────────────────────────────────────────
echo "▸ Installing systemd service …"
sudo tee "$SERVICE_FILE" > /dev/null << EOF
[Unit]
Description=Ledger Alert Server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$USER
WorkingDirectory=$INSTALL_DIR
EnvironmentFile=$ENV_FILE
ExecStart=$VENV_DIR/bin/uvicorn server:app --host 0.0.0.0 --port \${PORT:-8000}
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"
echo "  ✓ Service enabled"

# ── 6. Done ───────────────────────────────────────────────────────────────────
echo ""
echo "=== Setup complete ==="
echo ""
echo "  1. Edit your API keys:  nano $ENV_FILE"
echo "  2. Start the server  :  sudo systemctl start $SERVICE_NAME"
echo "  3. View live logs    :  journalctl -u $SERVICE_NAME -f"
echo ""
echo "  In Ledger Settings → Pi Server URL, enter:  http://$(hostname -I | awk '{print $1}'):8000"
echo ""
