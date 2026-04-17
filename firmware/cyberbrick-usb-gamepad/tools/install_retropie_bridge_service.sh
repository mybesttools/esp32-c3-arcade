#!/usr/bin/env bash
set -euo pipefail

# Install CyberBrick USB serial -> uinput bridge as a systemd service on RetroPie.
# Usage:
#   ./tools/install_retropie_bridge_service.sh [PI_USER] [PROJECT_DIR] [PORT]
# Examples:
#   ./tools/install_retropie_bridge_service.sh pi /home/pi/cyberbrick-usb-gamepad auto
#   ./tools/install_retropie_bridge_service.sh pi /home/pi/cyberbrick-usb-gamepad /dev/ttyACM0

PI_USER="${1:-pi}"
PROJECT_DIR="${2:-/home/${PI_USER}/cyberbrick-usb-gamepad}"
PORT="${3:-auto}"

SERVICE_PATH="/etc/systemd/system/cyberbrick-bridge.service"
RUNNER_PATH="${PROJECT_DIR}/tools/run_bridge.sh"

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root: sudo $0 [PI_USER] [PROJECT_DIR] [PORT]" >&2
  exit 1
fi

if [[ ! -f "${PROJECT_DIR}/tools/pi_usb_serial_to_uinput.py" ]]; then
  echo "Bridge script not found at ${PROJECT_DIR}/tools/pi_usb_serial_to_uinput.py" >&2
  exit 1
fi

apt update
apt install -y python3-evdev python3-serial joystick

cat > "${RUNNER_PATH}" <<EOF
#!/usr/bin/env bash
set -euo pipefail
exec /usr/bin/python3 "${PROJECT_DIR}/tools/pi_usb_serial_to_uinput.py" --port "${PORT}"
EOF
chmod +x "${RUNNER_PATH}"
chown "${PI_USER}:${PI_USER}" "${RUNNER_PATH}"

cat > "${SERVICE_PATH}" <<EOF
[Unit]
Description=CyberBrick USB serial to uinput bridge
After=multi-user.target
Wants=multi-user.target

[Service]
Type=simple
User=root
WorkingDirectory=${PROJECT_DIR}
ExecStart=${RUNNER_PATH}
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable cyberbrick-bridge.service
systemctl restart cyberbrick-bridge.service

echo "Service installed and started."
echo "Check status: sudo systemctl status cyberbrick-bridge.service"
echo "Check logs:   sudo journalctl -u cyberbrick-bridge.service -f"
