# ESP32-C3 Controller Firmware

This project targets **ESP32-C3 BLE gamepad mode** by default.

The board advertises as `ESP32-C3 Arcade` and pairs directly with RetroPie over Bluetooth.

It also includes an optional tiny OLED status display (SSD1306 I2C) showing:

- boot status
- advertising / connected state
- live axis and button activity

## BLE and USB support

Yes, both are supported from the same codebase:

- BLE gamepad mode on ESP32-C3 (`esp32c3_esp32c3_ble`)
- USB serial bridge mode on ESP32-C3 (`esp32c3_esp32c3_usbserial`)

Note: ESP32-C3 does not expose native USB HID gamepad device mode in this project.
USB mode here is CDC serial output for a Pi-side bridge.

## Project layout

- `src/main.cpp`: controller firmware (BLE + USB HID + serial bridge modes)
- `include/pinmap.h`: joystick/button mapping and calibration
- `platformio.ini`: PlatformIO build config
- `tools/flash_our_firmware.sh`: upload helper for default ESP32-C3 BLE env

## Wiring

Joystick:

- `JOY_X_PIN` = `A0`
- `JOY_Y_PIN` = `A1`

Buttons (active LOW, wire each button between GPIO pin and GND):

- A = `2`
- B = `3`
- X = `4`
- Y = `5`
- L = `6`
- R = `7`
- START = `9`
- SELECT = `10`
- HOTKEY = `20`
- COIN = `21`

## Optional tiny OLED display

The BLE environment enables status output (`ESP32C3_STATUS_OLED`).

For ABrobot ESP32-C3 OLED boards, the onboard 0.42" display is a 72x40 pixel
OLED driven over I2C on GPIO5 (SDA) / GPIO6 (SCL).

**Reference article:**
[ESP32-C3 0.42" OLED — emalliab](https://emalliab.wordpress.com/2025/02/12/esp32-c3-0-42-oled/)

Key findings from debugging:
- Board I2C address may be **0x3C** or **0x3D** depending on variant — firmware probes both.
- Display controller may be SSD1306 or SH1106 — both U8g2 drivers are included.
- `Wire.begin()` with no arguments on ESP32-C3 defaults to GPIO8/GPIO9, which
  conflicts with the onboard LED on GPIO8. Always pass explicit SDA/SCL pins.
- The article uses `U8G2_SSD1306_72X40_ER_F_HW_I2C` which works without offset.
  The `U8G2_SSD1306_128X64_NONAME` variant also works with offset (30, 12).

Default config is in `include/pinmap.h`:

- `STATUS_DISPLAY_ENABLED = false`
- `STATUS_OLED_I2C_ADDR = 0x3C`
- `STATUS_OLED_USE_SH1106_72X40 = true`
- `STATUS_OLED_SDA_PIN = 5`
- `STATUS_OLED_SCL_PIN = 6`

Note: ABrobot SH1106 72x40 panels are not SSD1306-compatible and need SH1106
command mode.

If your display has different dimensions or address, change those constants.

## Setup — step by step

### 1. Build and upload to ESP32-C3

```bash
cd firmware/esp32c3-usb-gamepad
pio run -e esp32c3_esp32c3_ble
pio run -e esp32c3_esp32c3_ble -t upload

# USB serial bridge firmware
pio run -e esp32c3_esp32c3_usbserial
pio run -e esp32c3_esp32c3_usbserial -t upload
```

If needed, upload with explicit port:

```bash
pio run -e esp32c3_esp32c3_ble -t upload --upload-port /dev/tty.usbmodemXXXX
```

Or use the helper script:

```bash
./tools/flash_our_firmware.sh
```

### 2. Pair with RetroPie

In RetroPie Bluetooth settings:

1. Register and connect new Bluetooth device
2. Select `ESP32-C3 Arcade`

### 3. Configure in EmulationStation

In RetroPie/EmulationStation:

1. Hold any button to start controller setup.
2. Map joystick directions and action buttons.
3. Map Start / Select / Hotkey.
4. Skip unused controls by holding a button.

## Verify on Pi (optional)

Install joystick tools to inspect Linux input events:

```bash
sudo apt install joystick
```

Test:

```bash
jstest /dev/input/js0
```

## Tuning

Edit `include/pinmap.h` and adjust:

- `JOY_CENTER_X`, `JOY_CENTER_Y`
- `JOY_DEADZONE`
- `JOY_MIN`, `JOY_MAX`
- `BUTTON_PINS[]` order if you want a different button layout

## Alternate environments

- `esp32c3_esp32c3_ble` (default)
- `esp32c3_esp32c3_usbserial` (USB CDC serial bridge mode on ESP32-C3)
- `esp32c3_rp2040_usbhid` (wired USB HID on Pico)
- `esp32c3_d1_dongle` (ESP8266 ESP-NOW dongle utility)
