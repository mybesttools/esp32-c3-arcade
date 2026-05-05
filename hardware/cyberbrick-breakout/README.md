# CyberBrick Controller Breakout

Single-board controller PCB: the ABrobot ESP32-C3 OLED module sockets directly
on the board. All CyberBrick joystick and button cables plug straight in via SH1.0
connectors. The MCP23017 button expander connects via a 2.54mm I2C header.

## Files

```
cyberbrick-breakout/
├── docs/
│   ├── DESIGN.md       ← full design spec, BOM, connector details, ESP32-C3 socket pinout
│   └── NETLIST.md      ← pin-by-pin wiring reference
├── kicad/
│   └── cyberbrick-breakout.kicad_sch   ← KiCad 7/8 schematic (rev 1.2)
└── pinmap.h            ← drop-in for firmware/esp32c3-usb-gamepad/include/pinmap.h
```

## Connector overview

| Side | Connector | Qty | Purpose |
|------|-----------|-----|---------|
| Input (controls) | SH1.0 3-pin (SM03B-SRSS-TB) | 4 | Joystick axes |
| Input (controls) | SH1.0 2-pin (SM02B-SRSS-TB) | 16 | Buttons |
| MCU (socketed) | 2× female 1×8 socket, 2.54mm | 1 module | ABrobot ESP32-C3 OLED |
| Output (MCP) | 2.54mm male header 2×9 | 1 | MCP23017 button signals |
| Output (I2C) | 2.54mm male header 1×4 | 1 | I2C bus to MCP23017 |
| Power | 2.54mm male header 1×2 | 1 | Optional 3.3V in |

## Joystick connectors (SH1.0 3-pin, SM03B-SRSS-TB)

CyberBrick dual-axis joystick = **two separate cables** per stick.

| Pin | Signal | Notes |
|-----|--------|-------|
| 1 | VCC (3.3V) | Locking tab side |
| 2 | Analog signal | → ESP32-C3 ADC via PCB trace |
| 3 | GND | |

## Button connectors (SH1.0 2-pin, SM02B-SRSS-TB)

| Pin | Signal |
|-----|--------|
| 1 | Signal → MCP23017 GPIO |
| 2 | GND |

Active LOW. Firmware enables internal pull-ups. No external resistors needed.

## Quick start

1. Open `kicad/cyberbrick-breakout.kicad_sch` in KiCad 7 or 8
2. **Measure your actual ESP32-C3 module** row-to-row spacing before placing footprints
3. Run ERC, lay out PCB (target ~80×65mm), export Gerbers
4. Copy `pinmap.h` to `firmware/esp32c3-usb-gamepad/include/pinmap.h`
5. Build: `pio run -e esp32c3_ble_mcp`

## ⚠️ GPIO2 note

GPIO2 is a boot strapping pin on ESP32-C3 — used for Right X joystick (ADC_RX).
The joystick wiper floats high-ish at power-up so boot is fine, but **never add
a pull-down resistor on this line**.
