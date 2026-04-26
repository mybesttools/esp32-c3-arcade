"""
ESP32-C3 ESP-NOW Gamepad Transmitter – boot.py

Reads controller state via rc_module and broadcasts it over ESP-NOW
to the ESP8266 D1 dongle plugged into the Raspberry Pi.

Upload with:
    mpremote connect /dev/cu.usbmodem1401 cp mpy/boot.py :boot.py
    mpremote connect /dev/cu.usbmodem1401 reset

Frame format (3 bytes):
    Byte 0 : hat  (0-7 = N/NE/E/SE/S/SW/W/NW, 0xFF = center)
    Byte 1 : buttons low byte  (A B X Y L R START SELECT)
    Byte 2 : buttons high byte (HOTKEY COIN)

Channel mapping (Transmitter Shield XA005):
    L1 (0) – joystick X -> d-pad hat
    L2 (1) – joystick Y -> d-pad hat
    L3 (2) – 3-pos switch: low = L, high = R
    R1 (3) – 3-pos switch: low = START, high = SELECT
    R2 (4) – 3-pos switch: low = HOTKEY, high = COIN
    K1 (6) – digital -> A
    K2 (7) – digital -> B
    K3 (8) – digital -> X
    K4 (9) – digital -> Y
"""

import sys
import utime

sys.path.append('/bbl')

# ESP-NOW must be initialised before rc_module takes the WiFi stack.
# rc_module.rc_master_init() activates STA internally; we attach to the
# same ESPNow instance it sets up rather than creating a competing one.
import network
import espnow

DONGLE_MAC = b'\x8C\xAA\xB5\x76\x70\xBC'  # ESP8266 D1 dongle

_HAT_THRESHOLD    = 8192
_SWITCH_THRESHOLD = 8192


def _axes_to_hat(x, y):
    up    = y >  _HAT_THRESHOLD
    down  = y < -_HAT_THRESHOLD
    right = x >  _HAT_THRESHOLD
    left  = x < -_HAT_THRESHOLD
    if up    and right: return 1
    if right and down:  return 3
    if down  and left:  return 5
    if left  and up:    return 7
    if up:              return 0
    if right:           return 2
    if down:            return 4
    if left:            return 6
    return 0xFF


def _read_state(rc_data):
    hat = _axes_to_hat(rc_data[0], rc_data[1])
    b = 0
    if rc_data[6]:                        b |= (1 << 0)  # A
    if rc_data[7]:                        b |= (1 << 1)  # B
    if rc_data[8]:                        b |= (1 << 2)  # X
    if rc_data[9]:                        b |= (1 << 3)  # Y
    if rc_data[2] < -_SWITCH_THRESHOLD:   b |= (1 << 4)  # L
    if rc_data[2] >  _SWITCH_THRESHOLD:   b |= (1 << 5)  # R
    if rc_data[3] < -_SWITCH_THRESHOLD:   b |= (1 << 6)  # START
    if rc_data[3] >  _SWITCH_THRESHOLD:   b |= (1 << 7)  # SELECT
    if rc_data[4] < -_SWITCH_THRESHOLD:   b |= (1 << 8)  # HOTKEY
    if rc_data[4] >  _SWITCH_THRESHOLD:   b |= (1 << 9)  # COIN
    return hat, b


def main():
    # Activate WiFi in STA mode so ESP-NOW can use the radio.
    w = network.WLAN(network.STA_IF)
    w.active(True)
    w.disconnect()

    en = espnow.ESPNow()
    en.active(True)
    en.add_peer(DONGLE_MAC)

    # Init rc_module after ESP-NOW is already active so they share the stack.
    import rc_module
    rc_module.rc_master_init()

    prev_hat = 0xFF
    prev_buttons = 0

    while True:
        rc_data = rc_module.rc_master_data()
        if rc_data is not None:
            hat, buttons = _read_state(rc_data)
            if hat != prev_hat or buttons != prev_buttons:
                try:
                    en.send(DONGLE_MAC, bytes([hat, buttons & 0xFF, (buttons >> 8) & 0xFF]))
                except Exception:
                    pass
                prev_hat = hat
                prev_buttons = buttons
        utime.sleep_ms(10)


main()
