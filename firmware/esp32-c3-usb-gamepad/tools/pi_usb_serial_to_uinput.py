#!/usr/bin/env python3
"""
ESP32-C3 USB Serial -> Linux uinput gamepad bridge for RetroPie.

Reads lines from firmware in format:
  R,<x>,<y>,<buttons_hex>

Where:
  x,y = signed int16 in range -32767..32767
  buttons_hex = hex bitmask (bit 0 = A, bit 1 = B, ...)

Usage:
  sudo python3 tools/pi_usb_serial_to_uinput.py --port /dev/ttyACM0
  sudo python3 tools/pi_usb_serial_to_uinput.py --port auto
"""

import argparse
import glob
import serial
from evdev import UInput, AbsInfo, ecodes as e

BUTTON_MAP = [
    e.BTN_A,       # 0
    e.BTN_B,       # 1
    e.BTN_X,       # 2
    e.BTN_Y,       # 3
    e.BTN_TL,      # 4 (L)
    e.BTN_TR,      # 5 (R)
    e.BTN_START,   # 6
    e.BTN_SELECT,  # 7
    e.BTN_MODE,    # 8 (HOTKEY)
    e.BTN_THUMBL,  # 9 (COIN)
]


def auto_port():
    for pattern in ("/dev/ttyACM*", "/dev/ttyUSB*", "/dev/tty.usbmodem*"):
        ports = sorted(glob.glob(pattern))
        if ports:
            return ports[0]
    raise RuntimeError("No serial device found. Use --port explicitly.")


def make_device():
    return UInput(
        {
            e.EV_ABS: [
                (e.ABS_X, AbsInfo(0, -32767, 32767, 0, 0, 0)),
                (e.ABS_Y, AbsInfo(0, -32767, 32767, 0, 0, 0)),
            ],
            e.EV_KEY: BUTTON_MAP,
        },
        name="BLE Everywhere USB Bridge",
        version=0x1,
    )


def parse_line(line):
    if not line.startswith("R,"):
        return None

    parts = line.strip().split(",")
    if len(parts) != 4:
        return None

    try:
        x = int(parts[1])
        y = int(parts[2])
        buttons = int(parts[3], 16)
    except ValueError:
        return None

    x = max(-32767, min(32767, x))
    y = max(-32767, min(32767, y))
    return x, y, buttons


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="auto")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    port = auto_port() if args.port == "auto" else args.port
    print(f"Using serial port: {port}")

    ui = make_device()
    print("uinput gamepad created: BLE Everywhere USB Bridge")

    last_buttons = 0

    with serial.Serial(port, args.baud, timeout=0.2) as ser:
        while True:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="ignore")
            parsed = parse_line(line)
            if parsed is None:
                continue

            x, y, buttons = parsed

            ui.write(e.EV_ABS, e.ABS_X, x)
            ui.write(e.EV_ABS, e.ABS_Y, y)

            changed = buttons ^ last_buttons
            for bit, key in enumerate(BUTTON_MAP):
                if changed & (1 << bit):
                    pressed = 1 if (buttons & (1 << bit)) else 0
                    ui.write(e.EV_KEY, key, pressed)

            ui.syn()
            last_buttons = buttons


if __name__ == "__main__":
    main()
