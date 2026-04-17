#!/usr/bin/env python3
import argparse
import glob
import logging
import signal
import sys
import time

import serial
from evdev import AbsInfo, UInput, ecodes as e

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
    by_id_candidates = sorted(glob.glob('/dev/serial/by-id/*'))
    if by_id_candidates:
        return by_id_candidates[0]

    candidates = sorted(glob.glob('/dev/ttyACM*') + glob.glob('/dev/ttyUSB*'))
    if not candidates:
        raise RuntimeError('No USB serial device found at /dev/serial/by-id/*, /dev/ttyACM*, or /dev/ttyUSB*')
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


def open_serial(port_arg, baud):
    port = autodetect_port() if port_arg == 'auto' else port_arg
    ser = serial.Serial(port, baud, timeout=0.2, rtscts=False, dsrdtr=False)
    # Some USB CDC devices reset on DTR toggles; keep it low and stable.
    ser.dtr = False
    return port, ser


def main():
    parser = argparse.ArgumentParser(description='Bridge CyberBrick ESP32-C3 USB serial to Linux virtual gamepad')
    parser.add_argument('--port', default='auto', help='Serial port, e.g. /dev/ttyACM0 (default: auto)')
    parser.add_argument('--baud', type=int, default=115200, help='Serial baud rate')
    parser.add_argument('--name', default='CyberBrick USB Bridge Gamepad', help='Virtual gamepad name')
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')

    caps = {
        e.EV_KEY: BUTTON_CODES,
        e.EV_ABS: [
            (e.ABS_X, AbsInfo(value=0, min=-32767, max=32767, fuzz=0, flat=0, resolution=0)),
            (e.ABS_Y, AbsInfo(value=0, min=-32767, max=32767, fuzz=0, flat=0, resolution=0)),
        ],
    }

    ui = UInput(caps, name=args.name, version=0x0001)
    ser = None

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    logging.info('Virtual gamepad created and bridge started')

    try:
        while not STOP:
            if ser is None:
                try:
                    port, ser = open_serial(args.port, args.baud)
                    logging.info('Using serial port: %s', port)

                    # Ignore initial banner lines and settle connection.
                    start = time.time()
                    while time.time() - start < 1.0:
                        ser.readline()
                except Exception as exc:
                    logging.warning('Serial open failed, retrying in 1s: %s', exc)
                    time.sleep(1.0)
                    continue

            try:
                raw = ser.readline()
            except serial.SerialException as exc:
                logging.warning('Serial read failed, reconnecting: %s', exc)
                try:
                    ser.close()
                except Exception:
                    pass
                ser = None
                time.sleep(0.5)
                continue

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
        if ser is not None:
            ser.close()
        ui.close()


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'bridge error: {exc}', file=sys.stderr)
        sys.exit(1)
