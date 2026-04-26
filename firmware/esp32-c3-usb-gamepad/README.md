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

## Pinouts

### Config B — single joystick, direct GPIO buttons (default)

Environments: `esp32c3_esp32c3_ble`, `esp32c3_esp32c3_usbserial`

**Joystick** (analog, connect to 3.3 V, GND, and wiper):

| Signal  | GPIO  | Arduino pin |
|---------|-------|-------------|
| JOY X   | GPIO0 | A0          |
| JOY Y   | GPIO1 | A1          |

**Buttons** (active LOW — wire between GPIO pin and GND, internal pull-up enabled):

| Button | GPIO |
|--------|------|
| A      | 2    |
| B      | 3    |
| X      | 4    |
| Y      | 5    |
| L      | 6    |
| R      | 7    |
| START  | 9    |
| SELECT | 10   |
| HOTKEY | 20   |
| COIN   | 21   |

**OLED status display** (I2C, shared with Config A):

| Signal | GPIO |
|--------|------|
| SDA    | 5    |
| SCL    | 6    |

> Note: GPIO5/6 are shared between the OLED and button Y/L in Config B.
> If the display is connected, remove buttons Y and L from the `BUTTON_PINS[]`
> array in `include/pinmap.h`, or remap them to unused GPIOs.

---

### Config A — dual joystick, MCP23017 expander board

Environments: `esp32c3_ble_mcp`

All buttons are wired to the MCP23017 I/O expander (no direct GPIO buttons).
The MCP23017 connects over I2C with A0/A1/A2 address pins all tied to GND
(address `0x20`).

**Joysticks** (analog, connect to 3.3 V, GND, and wiper):

| Signal       | GPIO  | Arduino pin | Role                  |
|--------------|-------|-------------|-----------------------|
| Left JOY X   | GPIO0 | A0          | Hat switch            |
| Left JOY Y   | GPIO1 | A1          | Hat switch            |
| Right JOY X  | GPIO2 | A2          | BLE Rx axis           |
| Right JOY Y  | GPIO3 | A3          | BLE Ry axis           |

**I2C bus** (MCP23017 + OLED share the same bus):

| Signal | GPIO | Connected to         |
|--------|------|----------------------|
| SDA    | 5    | MCP23017 SDA, OLED SDA |
| SCL    | 6    | MCP23017 SCL, OLED SCL |

**MCP23017 button mapping** (active LOW — wire each button between the
MCP pin and GND; internal pull-ups are enabled by firmware):

| Button    | MCP port | MCP bit | Board label |
|-----------|----------|---------|-------------|
| A (✕)     | GPA      | 0       | A0          |
| B (○)     | GPA      | 1       | A1          |
| X (□)     | GPA      | 2       | A2          |
| Y (△)     | GPA      | 3       | A3          |
| L shoulder| GPA      | 4       | A4          |
| R shoulder| GPA      | 5       | A5          |
| Start     | GPA      | 6       | A6          |
| Select    | GPA      | 7       | A7          |
| Hotkey    | GPB      | 0       | B0          |
| Coin      | GPB      | 1       | B1          |
| *(spare)* | GPB      | 2       | B2          |
| *(spare)* | GPB      | 3       | B3          |
| D-pad Up  | GPB      | 4       | B4          |
| D-pad Down| GPB      | 5       | B5          |
| D-pad Left| GPB      | 6       | B6          |
| D-pad Right| GPB     | 7       | B7          |

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

## Environments

| Environment | Config | Description |
|---|---|---|
| `esp32c3_esp32c3_ble` | B | BLE gamepad, single joystick, direct GPIO buttons (default) |
| `esp32c3_ble_mcp` | A | BLE gamepad, dual joystick, MCP23017 expander buttons |
| `esp32c3_esp32c3_usbserial` | B | USB CDC serial bridge for Pi-side uinput script, single joystick |
| `esp32c3_rp2040_usbhid` | — | Wired USB HID gamepad on Raspberry Pi Pico |
