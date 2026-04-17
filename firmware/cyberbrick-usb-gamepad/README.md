# CyberBrick Controller Firmware (PlatformIO)

This project now defaults to wired USB for ESP32-C3 by using USB CDC serial reports and a Raspberry Pi uinput bridge.

ESP32-C3 note: it does not support native USB HID gamepad device mode. The wired path here is:

1. ESP32-C3 sends controller state over USB serial
2. Raspberry Pi bridge script creates a virtual Linux gamepad via uinput

## Project layout

- `src/main.cpp`: dual-mode firmware (USB serial bridge mode + optional BLE mode)
- `include/pinmap.h`: joystick/button pins and calibration
- `platformio.ini`: build and upload config
- `tools/pi_usb_serial_to_uinput.py`: Pi bridge from serial to virtual gamepad

## What it does

- Reads one analog joystick (`JOY_X_PIN`, `JOY_Y_PIN`)
- Reads up to 10 digital buttons using `INPUT_PULLUP`
- USB mode: sends controller frames over USB serial for Pi bridge
- BLE mode: sends controller state as BLE gamepad

## Requirements

- VS Code + PlatformIO extension, or PlatformIO CLI
- ESP32-C3 CyberBrick core board connected by USB
- Raspberry Pi with `/dev/uinput` enabled
- Pi packages: `python3-evdev` and `python3-serial`

## Configure pins

Edit `include/pinmap.h`:

- Set `JOY_X_PIN` / `JOY_Y_PIN`
- Set each pin in `BUTTON_PINS[]`
- Tune `JOY_CENTER_X`, `JOY_CENTER_Y`, and `JOY_DEADZONE`

## Build and upload (PlatformIO)

Default environment is wired USB serial bridge mode.

Run inside this folder:

```bash
pio run
pio run -t upload
pio device monitor
```

If your board is not detected automatically, upload with an explicit port:

```bash
pio run -t upload --upload-port /dev/tty.usbmodemXXXX
```

## Raspberry Pi USB bridge setup

Install dependencies on Pi:

```bash
sudo apt update
sudo apt install -y python3-evdev python3-serial joystick
```

Run the bridge script as root (required for `/dev/uinput`):

```bash
sudo python3 tools/pi_usb_serial_to_uinput.py --port /dev/ttyACM0
```

Then validate:

```bash
jstest /dev/input/js0
```

If joystick drifts, increase `JOY_DEADZONE` or adjust center values.

## Optional BLE mode build

If you want BLE later:

```bash
pio run -e cyberbrick_esp32c3_ble
pio run -e cyberbrick_esp32c3_ble -t upload
```

## RetroPie mapping

In EmulationStation:

1. Hold any button to start controller setup.
2. Map joystick directions and action buttons.
3. Map Start/Select and Hotkey.
4. Skip unused controls by holding a button.
