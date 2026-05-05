# CyberBrick Controller Breakout Board — Design Doc

## Purpose

Single-board controller PCB that:
- Sockets the ABrobot ESP32-C3 OLED module directly (no flying wires to MCU)
- Accepts all CyberBrick SH1.0 joystick and button cables
- Breaks out MCP23017 button signals via 2.54mm header
- Powers everything from the ESP32-C3 3V3 pin

## Board layout concept

```
┌─────────────────────────────────────────────┐
│  [J1 LX] [J2 LY] [J3 RX] [J4 RY]  SH1.0   │  ← joystick connectors, top edge
│                                              │
│  ┌──────────────────────┐                   │
│  │  ABrobot ESP32-C3    │  (socketed)        │
│  │  OLED on-board       │                   │
│  │  USB-C left side     │                   │
│  └──────────────────────┘                   │
│                                              │
│  [J6 MCP23017 2×9]  [J7 I2C]  [J8 PWR]     │  ← 2.54mm output headers
│                                              │
│  [J9..J24  button SH1.0 connectors]         │  ← button connectors, bottom/sides
└─────────────────────────────────────────────┘
```

## ESP32-C3 Module socket

Module: ABrobot ESP32-C3 0.42" OLED
- Dimensions: ~25 mm × 20 mm
- 2× 8-pin rows, 2.54 mm pitch
- Row-to-row spacing: ~15.24 mm (600 mil) — **measure your board before ordering PCBs**
- USB-C connector overhangs the left edge

### Pin assignment (8 pins per row)

Top row (left → right on module):
| Module pin | Label | Signal | Breakout net |
|------------|-------|--------|-------------|
| T1 | IO8  | GPIO8 (onboard LED) | — (NC) |
| T2 | 9    | GPIO9 | — (NC) |
| T3 | 8    | GPIO8 / I2C SDA* | — |
| T4 | 7    | GPIO7 | — (NC) |
| T5 | 6    | GPIO6 / I2C SCL | I2C_SCL |
| T6 | 5    | GPIO5 / I2C SDA | I2C_SDA |
| T7 | 4    | GPIO4 | — (NC) |
| T8 | 3    | GPIO3 / ADC1_CH3 | ADC_RY |

Bottom row (left → right on module):
| Module pin | Label | Signal | Breakout net |
|------------|-------|--------|-------------|
| B1 | V5   | 5V (USB) | — (NC / optional power in) |
| B2 | GD   | GND | GND |
| B3 | V3   | 3.3V out | VCC |
| B4 | RX   | GPIO20 / UART RX | — (NC) |
| B5 | TX   | GPIO21 / UART TX | — (NC) |
| B6 | 2    | GPIO2 / ADC1_CH2 ⚠️ | ADC_RX |
| B7 | 1    | GPIO1 / ADC1_CH1 | ADC_LY |
| B8 | 0    | GPIO0 / ADC1_CH0 | ADC_LX |

> ⚠️ **GPIO2 (ADC_RX):** GPIO2 is a boot strapping pin on ESP32-C3. It must be
> HIGH (or floating with internal pull-up) during reset for normal flash boot.
> The joystick wiper is high-impedance at power-up, so this is safe in practice,
> but do NOT pull GPIO2 LOW with an external resistor or load during boot.
> The SH1.0 joystick module does not drive the line low at rest — it outputs a
> mid-rail analog voltage — so no issue expected.

### Socket footprints

| Ref | Part | Footprint |
|-----|------|-----------|
| U1 top | Female 1×8 socket, 2.54mm | PinSocket_1x08_P2.54mm_Vertical |
| U1 bot | Female 1×8 socket, 2.54mm | PinSocket_1x08_P2.54mm_Vertical |

Place the two sockets so the module's USB-C connector clears the PCB edge.
Add a PCB cutout or leave at least 10mm clearance on the left side for the connector body.

---

## Connector types

### Input side — all SH1.0 (cables from CyberBrick parts)

| Connector | Usage | Qty |
|---|---|---|
| JST SH 1.0 mm, 3-pin female RA (SM03B-SRSS-TB) | Each joystick axis (X and Y separate) | 4 |
| JST SH 1.0 mm, 2-pin female RA (SM02B-SRSS-TB) | Each button (signal + GND) | 16 |

### Output side — 2.54 mm headers (to MCP23017 only; ESP32-C3 is socketed)

| Connector | Usage | Qty |
|---|---|---|
| 2.54 mm pin header (male) | MCP23017 GPA / GPB button signals | 1× 18-pin (2×9) |
| 2.54 mm pin header (male) | I2C bus (SDA/SCL/VCC/GND to MCP23017) | 1× 4-pin |
| 2.54 mm pin header (male) | Power in / battery (3.3 V + GND) | 1× 2-pin |

> J5 (ADC out) and the old I2C header to the MCU are now internal PCB traces —
> the ESP32-C3 is socketed directly on the board.

---

## Signal routing

### Analog — joystick SH1.0 → PCB trace → ESP32-C3 socket pin

| SH1.0 connector | Signal net | ESP32-C3 socket pin |
|---|---|---|
| J1 (LX) pin 2 | ADC_LX | U1-B8 (GPIO0) |
| J2 (LY) pin 2 | ADC_LY | U1-B7 (GPIO1) |
| J3 (RX) pin 2 | ADC_RX | U1-B6 (GPIO2) ⚠️ |
| J4 (RY) pin 2 | ADC_RY | U1-T8 (GPIO3) |

All SH1.0 VCC pins → VCC (from U1-B3, 3V3)
All SH1.0 GND pins → GND (from U1-B2)

### I2C — PCB trace → ESP32-C3 socket + J6 I2C header

| Net | ESP32-C3 socket | J7 header pin | Connected to |
|---|---|---|---|
| I2C_SDA | U1-T6 (GPIO5) | J7 pin 1 | MCP23017 SDA |
| I2C_SCL | U1-T5 (GPIO6) | J7 pin 2 | MCP23017 SCL |

### Buttons → MCP23017 via J6

Same as before — 16 SH1.0 2-pin button connectors fan out to J6 (2×9 header).
MCP23017 connects over I2C via J7.

---

## Power

- 3.3 V from ESP32-C3 V3 pin (U1-B3) → VCC rail
- OR feed 3.3 V in via J8 (use when programming without USB)
- All SH1.0 VCC pins powered from VCC rail
- 100 nF decoupling caps C1–C4 on each joystick VCC line

---

## PCB notes

- Target size: ~80 × 65 mm (allows room for 16 SH1.0 button connectors + ESP32-C3 socket + joystick connectors)
- 4× M3 mounting holes, 3 mm from corners
- ESP32-C3 module sockets centered on board
- Joystick SH1.0 connectors on top edge
- Button SH1.0 connectors on bottom edge and sides (8 per side or 16 along bottom)
- 2.54mm output headers (J6, J7, J8) on right edge
- Leave PCB edge clearance on the left side of U1 for the USB-C overhang

---

## BOM

| Ref | Part | Value | Package | Qty |
|---|---|---|---|---|
| U1 | ABrobot ESP32-C3 OLED (socketed) | — | 2× PinSocket_1x08_P2.54mm | 1 |
| J1–J4 | JST SH 1.0 mm 3-pin female RA | Joystick axis | SM03B-SRSS-TB | 4 |
| J9–J24 | JST SH 1.0 mm 2-pin female RA | Button inputs | SM02B-SRSS-TB | 16 |
| J6 | 2.54 mm male header 18-pin | MCP buttons | 2.54mm 2×9 | 1 |
| J7 | 2.54 mm male header 4-pin | I2C bus | 2.54mm 1×4 | 1 |
| J8 | 2.54 mm male header 2-pin | Power in | 2.54mm 1×2 | 1 |
| C1–C4 | Capacitor 100 nF | Decoupling | 0402 | 4 |

Total: 7 line items. J5 (old ADC header) eliminated — now internal traces.

> SM02B-SRSS-TB / SM03B-SRSS-TB: standard Mouser/LCSC SH1.0 RA SMD connectors.
> Matching cable-side housing: SHR-02V-S / SHR-03V-S with SSHL-002T-P0.2 crimps.
> These are exactly what CyberBrick pre-crimped 20 cm cables use.
