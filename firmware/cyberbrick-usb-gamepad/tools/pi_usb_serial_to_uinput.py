#!/usr/bin/env python3
import argparse
import glob
import logging
import signal
import sys
import time

import serial
from evdev import UInput, ecodes as e

BUTTON_CODES = [
    e.BTN_SOUTH,   # B1
    e.BTN_EAST,    # B2
    e.BTN_WEST,    # B3
    e.BTN_NORTH,   # B4
    e.BTN_TL,      # L1
    e.BTN_TR,      # R1
    e.BTN_START,   # START
    e.BTN_SELECT,  # SELECT
    e.BTN_MODE,    # HOTKEY
    e.BTN_THUMBL,  # COIN
]

STOP = False


def on_signal(_sig, _frame):
    global STOP
    STOP = True


def autodetect_port():
    candidates = sorted(glob.glob('/dev/ttyACM*') + glob.glob('/dev/ttyUSB*'))
    if not candidates:
        raise RuntimeError('No USB serial device found at /dev/ttyACM* or /dev/ttyUSB*')
    return candidates[0]


def parse_frame(line):
    # Frame format from firmware: R,<x>,<y>,<buttons_hex>
    parts = line.strip().split(',')
    if len(parts) != 4 or parts[0] != 'R':
        return None

    try:
        x = int(parts[1])
        y = int(parts[2])
        buttons = int(parts[3], 16)
    except ValueError:
        return None

    return x, y, buttons


def main():
    parser = argparse.ArgumentParser(description='Bridge CyberBrick ESP32-C3 USB serial to Linux virtual gamepad')
    parser.add_argument('--port', default='auto', help='Serial port, e.g. /dev/ttyACM0 (default: auto)')
    parser.add_argument('--baud', type=int, default=115200, help='Serial baud rate')
    parser.add_argument('--name', default='CyberBrick USB Bridge Gamepad', help='Virtual gamepad name')
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')

    port = autodetect_port() if args.port == 'auto' else args.port
    logging.info('Using serial port: %s', port)

    caps = {
        e.EV_KEY: BUTTON_CODES,
        e.EV_ABS: [
            (e.ABS_X, (-(32767), 32767, 0, 0)),
            (e.ABS_Y, (-(32767), 32767, 0, 0)),
        ],
    }

    ui = UInput(caps, name=args.name, version=0x0001)
    ser = serial.Serial(port, args.baud, timeout=0.2)

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    # Ignore initial banner lines and settle connection.
    start = time.time()
    while time.time() - start < 1.0:
        ser.readline()

    logging.info('Virtual gamepad created and bridge started')

    try:
        while not STOP:
            raw = ser.readline()
            if not raw:
                continue

            try:
                line = raw.decode('utf-8', errors='ignore')
            except Exception:
                continue

            frame = parse_frame(line)
            if frame is None:
                continue

            x, y, buttons = frame
            ui.write(e.EV_ABS, e.ABS_X, x)
            ui.write(e.EV_ABS, e.ABS_Y, y)

            for i, code in enumerate(BUTTON_CODES):
                pressed = 1 if (buttons & (1 << i)) else 0
                ui.write(e.EV_KEY, code, pressed)

            ui.syn()
    finally:
        ser.close()
        ui.close()


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'bridge error: {exc}', file=sys.stderr)
        sys.exit(1)
