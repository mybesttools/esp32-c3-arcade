#!/usr/bin/env python3
"""
Raspberry Pi ESP-NOW → uinput bridge

Reads gamepad frames from the ESP8266 D1 dongle over USB serial and
creates a virtual Linux gamepad via uinput.

Usage:
    sudo python3 tools/pi_espnow_to_uinput.py [--port /dev/ttyUSB0]

Frame format (from D1 dongle):
    GP,<hat>,<buttons_decimal>

    hat      : 0-7 (N NE E SE S SW W NW) or 255 (center)
    buttons  : decimal integer, bit mask:
               bit 0  = A
               bit 1  = B
               bit 2  = X
               bit 3  = Y
               bit 4  = L
               bit 5  = R
               bit 6  = START
               bit 7  = SELECT
               bit 8  = HOTKEY
               bit 9  = COIN

Requirements (Raspberry Pi):
    sudo apt install python3-evdev python3-serial
"""

import argparse
import sys
import serial
import evdev
from evdev import UInput, AbsInfo, ecodes as e

# Hat switch direction → (ABS_HAT0X, ABS_HAT0Y)
HAT_MAP = {
    0:    ( 0, -1),   # N
    1:    ( 1, -1),   # NE
    2:    ( 1,  0),   # E
    3:    ( 1,  1),   # SE
    4:    ( 0,  1),   # S
    5:    (-1,  1),   # SW
    6:    (-1,  0),   # W
    7:    (-1, -1),   # NW
    0xFF: ( 0,  0),   # center
}

BUTTON_MAP = [
    e.BTN_A,       # bit 0  – A
    e.BTN_B,       # bit 1  – B
    e.BTN_X,       # bit 2  – X
    e.BTN_Y,       # bit 3  – Y
    e.BTN_TL,      # bit 4  – L
    e.BTN_TR,      # bit 5  – R
    e.BTN_START,   # bit 6  – START
    e.BTN_SELECT,  # bit 7  – SELECT
    e.BTN_MODE,    # bit 8  – HOTKEY
    e.BTN_THUMBL,  # bit 9  – COIN
]

BUTTON_COUNT = len(BUTTON_MAP)


def make_uinput():
    cap = {
        e.EV_KEY: BUTTON_MAP,
        e.EV_ABS: [
            (e.ABS_HAT0X, AbsInfo(value=0, min=-1, max=1, fuzz=0, flat=0, resolution=0)),
            (e.ABS_HAT0Y, AbsInfo(value=0, min=-1, max=1, fuzz=0, flat=0, resolution=0)),
        ],
    }
    return UInput(cap, name="ESP32-C3 Arcade", version=0x1)


def find_port():
    import glob
    for pattern in ["/dev/ttyUSB*", "/dev/ttyACM*"]:
        ports = glob.glob(pattern)
        if ports:
            return sorted(ports)[0]
    raise RuntimeError("No serial port found. Pass --port explicitly.")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default=None)
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    port = args.port or find_port()
    print(f"Opening {port} at {args.baud} baud")

    ui = make_uinput()
    print("Virtual gamepad created: ESP32-C3 Arcade")

    prev_hat = (0, 0)
    prev_buttons = 0

    with serial.Serial(port, args.baud, timeout=1) as ser:
        print("Waiting for frames...")
        while True:
            try:
                line = ser.readline().decode("ascii", errors="ignore").strip()
            except Exception:
                continue

            if not line.startswith("GP,"):
                continue

            parts = line.split(",")
            if len(parts) != 3:
                continue

            try:
                hat_val  = int(parts[1])
                buttons  = int(parts[2])
            except ValueError:
                continue

            hat_xy = HAT_MAP.get(hat_val, (0, 0))

            if hat_xy != prev_hat:
                ui.write(e.EV_ABS, e.ABS_HAT0X, hat_xy[0])
                ui.write(e.EV_ABS, e.ABS_HAT0Y, hat_xy[1])
                prev_hat = hat_xy

            changed = buttons ^ prev_buttons
            for bit in range(BUTTON_COUNT):
                if changed & (1 << bit):
                    ui.write(e.EV_KEY, BUTTON_MAP[bit], 1 if (buttons & (1 << bit)) else 0)
            prev_buttons = buttons

            ui.syn()


if __name__ == "__main__":
    main()
