# ESP32-C3 Controller Firmware

This project targets **ESP32-C3 BLE gamepad mode** by default.

The board advertises as `ESP32-C3 Arcade` and pairs directly with RetroPie over Bluetooth.

It also includes an optional tiny OLED status display (SSD1306 I2C) showing:

- boot status
- advertising / connected state
- live axis and button activity

## BLE and USB support

Yes, both are supported from the same codebase:

- BLE gamepad mode on ESP32-C3 (`esp32c3_ble`)
- USB serial bridge mode on ESP32-C3 (`esp32c3_usbserial`)

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
- `JOY2_X_PIN` = `A2`
- `JOY2_Y_PIN` = `A3`

Buttons (active LOW, wire each button between GPIO pin and GND):

| Button | GPIO | Notes |
|--------|------|-------|
| A      | 2    | |
| B      | 3    | |
| X      | 4    | |
| Y      | 8    | Moved from GPIO5 (OLED SDA conflict) |
| L      | 10   | Moved from GPIO6 (OLED SCL conflict) |
| R      | 7    | |
| START  | 9    | |
| SELECT | 20   | |
| HOTKEY | 21   | |

**Avoid GPIO5 (OLED SDA) and GPIO6 (OLED SCL)** — pulling them LOW corrupts the display.
**Avoid GPIO18/GPIO19** — USB D-/D+ pins, always driven by hardware.
**GPIO8** is also the onboard LED; pressing Y will briefly light it — harmless.

## Optional tiny OLED display

The BLE environment enables status output (`ESP32_C3_STATUS_OLED`).

For ABrobot ESP32-C3 OLED boards, the onboard 0.42" display is a 72x40 pixel
OLED driven over I2C on GPIO5 (SDA) / GPIO6 (SCL).

**Reference article:**
[ESP32-C3 0.42" OLED — emalliab](https://emalliab.wordpress.com/2025/02/12/esp32-c3-0-42-oled/)

Key findings from debugging:
- Board I2C address is **0x3C** (firmware also probes 0x3D as fallback).
- Display is **SSD1306 128×64** — uses `U8G2_SSD1306_128X64_NONAME_F_SW_I2C`.
- Hardware I2C (`Wire.begin()`) hangs indefinitely on wrong init commands — firmware uses **software I2C** (bit-bang) to avoid this entirely.
- Physical bezel clips ~28 px on each side and ~12 px at the top. All UI is
  drawn within the safe zone `x=28..99, y=12..63`.

Config in `include/pinmap.h`:

- `STATUS_DISPLAY_ENABLED = true`
- `STATUS_OLED_I2C_ADDR = 0x3C` (confirmed; 0x3D kept as fallback)
- `STATUS_OLED_USE_SH1106_72X40 = false`
- `STATUS_OLED_SDA_PIN = 5`
- `STATUS_OLED_SCL_PIN = 6`

If your display has different dimensions or address, change those constants.

## Setup — step by step

### 1. Build and upload to ESP32-C3

```bash
cd firmware/esp32-c3-usb-gamepad
pio run -e esp32c3_ble
pio run -e esp32c3_ble -t upload

# USB serial bridge firmware
pio run -e esp32c3_usbserial
pio run -e esp32c3_usbserial -t upload
```

If needed, upload with explicit port:

```bash
pio run -e esp32c3_ble -t upload --upload-port /dev/tty.usbmodemXXXX
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

## Future expansion — MCP23017 I2C GPIO expander

When the MCP23017 arrives, the controller gains: all remaining buttons, a second
analog stick, and enough spare pins for future additions.

### I2C bus

Put the MCP23017 on the same I2C bus as the OLED (**GPIO5 SDA / GPIO6 SCL**).

| Address | Device     |
|---------|------------|
| 0x3C    | OLED SSD1306 (SW I2C, already working) |
| 0x20    | MCP23017 (HW Wire) |

To enable: set `USE_MCP23017 = true` in `pinmap.h`.
`Wire.begin(5, 6)` is called in `setup()` before `initMcp23017()`.

### Button allocation (MCP23017 — 16 pins)

All current direct-GPIO buttons move to the MCP, freeing GPIOs for the second
analog stick.

| MCP pin | Button   | Current GPIO |
|---------|----------|--------------|
| GPA0    | A        | GPIO2 |
| GPA1    | B        | GPIO3 |
| GPA2    | X        | GPIO4 |
| GPA3    | Y        | GPIO8 |
| GPA4    | L        | GPIO10 |
| GPA5    | R        | GPIO7 |
| GPA6    | START    | GPIO9 |
| GPA7    | SELECT   | GPIO20 |
| GPB0    | HOTKEY   | GPIO21 |
| GPB1    | COIN     | new |
| GPB2    | L2       | new (PSP style) |
| GPB3    | R2       | new (PSP style) |
| GPB4    | D-Pad Up    | new |
| GPB5    | D-Pad Down  | new |
| GPB6    | D-Pad Left  | new |
| GPB7    | D-Pad Right | new |

### Second analog stick (GPIOs freed after button migration)

| GPIO | Function |
|------|----------|
| 0    | JOY1_X (existing) |
| 1    | JOY1_Y (existing) |
| 2    | JOY2_X (new) |
| 3    | JOY2_Y (new) |

The second stick maps to BLE axes **RX + RY** and the D-pad is read from
**MCP GPB4..GPB7** into the BLE hat switch.

### Migration steps

1. Wire MCP23017 SDA→GPIO5, SCL→GPIO6, VCC→3.3 V, GND→GND, A0/A1/A2→GND (address 0x20).
2. Move button wires from direct GPIOs to the corresponding MCP pins above.
3. Wire second joystick X→GPIO2, Y→GPIO3.
4. In `pinmap.h`: set `USE_MCP23017 = true`, clear `BUTTON_PINS[]` (buttons now read from MCP).
5. Add `JOY2_X_PIN = 2`, `JOY2_Y_PIN = 3` to `pinmap.h`.
6. Enable second axis reporting in `main.cpp` (`gamepad.setAxes(..., rx, ry, ...)`).

