# CyberBrick Breakout — Net List & Wiring Reference

## Input connectors (all SH1.0)

Pin 1 is the **locking tab side** on all SH1.0 housings.

### J1 — Left Joystick X  (SM03B-SRSS-TB, SH1.0 3-pin)
| Pin | Net | Destination |
|-----|-----|-------------|
| 1 | VCC | 3.3 V rail |
| 2 | ADC_LX | J5 pin 2 → ESP32-C3 GPIO0 |
| 3 | GND | GND rail |

### J2 — Left Joystick Y  (SM03B-SRSS-TB, SH1.0 3-pin)
| Pin | Net | Destination |
|-----|-----|-------------|
| 1 | VCC | 3.3 V rail |
| 2 | ADC_LY | J5 pin 3 → ESP32-C3 GPIO1 |
| 3 | GND | GND rail |

### J3 — Right Joystick X  (SM03B-SRSS-TB, SH1.0 3-pin)
| Pin | Net | Destination |
|-----|-----|-------------|
| 1 | VCC | 3.3 V rail |
| 2 | ADC_RX | J5 pin 4 → ESP32-C3 GPIO2 |
| 3 | GND | GND rail |

### J4 — Right Joystick Y  (SM03B-SRSS-TB, SH1.0 3-pin)
| Pin | Net | Destination |
|-----|-----|-------------|
| 1 | VCC | 3.3 V rail |
| 2 | ADC_RY | J5 pin 5 → ESP32-C3 GPIO3 |
| 3 | GND | GND rail |

### J9–J24 — Button inputs  (SM02B-SRSS-TB, SH1.0 2-pin)
Each button gets its own 2-pin SH1.0 connector.

| Ref | Net | MCP23017 | Button |
|-----|-----|----------|--------|
| J9  | BTN_DL_UP    | GPA0 | D-pad Left — Up |
| J10 | BTN_DL_LEFT  | GPA1 | D-pad Left — Left |
| J11 | BTN_DL_RIGHT | GPA2 | D-pad Left — Right |
| J12 | BTN_DL_DOWN  | GPA3 | D-pad Left — Down |
| J13 | BTN_DR_UP    | GPA4 | D-pad Right — Up (Y/△) |
| J14 | BTN_DR_LEFT  | GPA5 | D-pad Right — Left (X/□) |
| J15 | BTN_DR_RIGHT | GPA6 | D-pad Right — Right (B/○) |
| J16 | BTN_DR_DOWN  | GPA7 | D-pad Right — Down (A/✕) |
| J17 | BTN_SHL1     | GPB0 | Left Shoulder 1 (L) |
| J18 | BTN_SHL2     | GPB1 | Left Shoulder 2 (L2) |
| J19 | BTN_SHR1     | GPB2 | Right Shoulder 1 (R) |
| J20 | BTN_SHR2     | GPB3 | Right Shoulder 2 (R2) |
| J21 | BTN_START    | GPB4 | Start |
| J22 | BTN_SELECT   | GPB5 | Select |
| J23 | BTN_HOTKEY   | GPB6 | Hotkey |
| J24 | BTN_COIN     | GPB7 | Coin |

Pinout for each 2-pin button connector:
| Pin | Signal |
|-----|--------|
| 1 | BTN_xxx (to MCP23017 GPIO) |
| 2 | GND |

Buttons are active LOW — firmware enables internal pull-ups. No external resistors needed.

---

## Output connectors (2.54 mm headers to MCU)

### J5 — ADC Output Header (2.54 mm, 1×6)
| Pin | Net | Connect to ESP32-C3 |
|-----|-----|---------------------|
| 1 | VCC | 3V3 (ref only, optional) |
| 2 | ADC_LX | GPIO0 |
| 3 | ADC_LY | GPIO1 |
| 4 | ADC_RX | GPIO2 |
| 5 | ADC_RY | GPIO3 |
| 6 | GND | GND |

---

### J6 — MCP23017 Button Header (2.54 mm, 2×9)
Signal side of each SH1.0 button connector fans out here to the MCP23017.

| Pin | Net | MCP23017 | Button |
|-----|-----|----------|--------|
| 1  | BTN_DL_UP    | GPA0 | D-pad Left Up |
| 2  | BTN_DR_UP    | GPA4 | D-pad Right Up (Y/△) |
| 3  | BTN_DL_LEFT  | GPA1 | D-pad Left Left |
| 4  | BTN_DR_LEFT  | GPA5 | D-pad Right Left (X/□) |
| 5  | BTN_DL_RIGHT | GPA2 | D-pad Left Right |
| 6  | BTN_DR_RIGHT | GPA6 | D-pad Right Right (B/○) |
| 7  | BTN_DL_DOWN  | GPA3 | D-pad Left Down |
| 8  | BTN_DR_DOWN  | GPA7 | D-pad Right Down (A/✕) |
| 9  | BTN_SHL1     | GPB0 | Left Shoulder 1 (L) |
| 10 | BTN_SHR1     | GPB2 | Right Shoulder 1 (R) |
| 11 | BTN_SHL2     | GPB1 | Left Shoulder 2 (L2) |
| 12 | BTN_SHR2     | GPB3 | Right Shoulder 2 (R2) |
| 13 | BTN_START    | GPB4 | Start |
| 14 | BTN_SELECT   | GPB5 | Select |
| 15 | BTN_HOTKEY   | GPB6 | Hotkey |
| 16 | BTN_COIN     | GPB7 | Coin |
| 17 | GND | — | Common GND |
| 18 | GND | — | Common GND |

---

### J7 — I2C Bus Header (2.54 mm, 1×4)
| Pin | Net | Connect to |
|-----|-----|-----------|
| 1 | I2C_SDA | ESP32-C3 GPIO5 + MCP23017 SDA + OLED SDA |
| 2 | I2C_SCL | ESP32-C3 GPIO6 + MCP23017 SCL + OLED SCL |
| 3 | VCC | 3.3 V |
| 4 | GND | GND |

---

### J8 — Power In (2.54 mm, 1×2)
| Pin | Net |
|-----|-----|
| 1 | VCC (3.3 V) |
| 2 | GND |

---

## Net summary

| Net | Type | Sources → Sinks |
|-----|------|----------------|
| VCC | Power | J8.1 → J1.1, J2.1, J3.1, J4.1, C1–C4+, J5.1, J7.3 |
| GND | Power | J8.2 → J1.3, J2.3, J3.3, J4.3, J9–J24 pin 2, C1–C4−, J5.6, J6.17, J6.18, J7.4 |
| ADC_LX | Analog | J1.2 → J5.2 |
| ADC_LY | Analog | J2.2 → J5.3 |
| ADC_RX | Analog | J3.2 → J5.4 |
| ADC_RY | Analog | J4.2 → J5.5 |
| I2C_SDA | Digital | J7.1 (bidirectional) |
| I2C_SCL | Digital | J7.2 (bidirectional) |
| BTN_* | Digital | J9–J24 pin 1 → J6 pins 1–16 (each unique net) |
