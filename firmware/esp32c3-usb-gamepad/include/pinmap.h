#pragma once

// =============================================================================
// pinmap.h — CyberBrick v2 controller (dual joystick + MCP23017 buttons)
// Config A — esp32c3_ble_mcp environment
// =============================================================================

// ─── Joystick ADC pins ────────────────────────────────────────────────────────
// CyberBrick SH1.0 joystick signal wiper → ESP32-C3 GPIO (ADC1)
// Connect: 3.3V → VCC pin, wiper → signal pin, GND → GND pin

#define JOY_LEFT_X_PIN   0   // GPIO0 / A0 — Left stick X
#define JOY_LEFT_Y_PIN   1   // GPIO1 / A1 — Left stick Y  (also hat switch)
#define JOY_RIGHT_X_PIN  2   // GPIO2 / A2 — Right stick X (BLE Rx axis)
#define JOY_RIGHT_Y_PIN  3   // GPIO3 / A3 — Right stick Y (BLE Ry axis)

// ─── Joystick calibration ────────────────────────────────────────────────────
#define JOY_CENTER_X    2048
#define JOY_CENTER_Y    2048
#define JOY_DEADZONE     150
#define JOY_MIN           50
#define JOY_MAX         4000

// ─── I2C bus (MCP23017 + OLED) ───────────────────────────────────────────────
#define I2C_SDA_PIN      5
#define I2C_SCL_PIN      6

// ─── MCP23017 ────────────────────────────────────────────────────────────────
// A0=A1=A2=GND → address 0x20
#define MCP_I2C_ADDR    0x20

// MCP23017 pin assignments (active LOW, internal pull-ups enabled)
// GPA = port A (0x00), GPB = port B (0x01)
//
//  GPA0 → D-pad Left  — Up
//  GPA1 → D-pad Left  — Left
//  GPA2 → D-pad Left  — Right
//  GPA3 → D-pad Left  — Down
//  GPA4 → D-pad Right — Up    (Y / Triangle △)
//  GPA5 → D-pad Right — Left  (X / Square   □)
//  GPA6 → D-pad Right — Right (B / Circle   ○)
//  GPA7 → D-pad Right — Down  (A / Cross    ✕)
//
//  GPB0 → Left  Shoulder 1 (L)
//  GPB1 → Left  Shoulder 2 (L2)
//  GPB2 → Right Shoulder 1 (R)
//  GPB3 → Right Shoulder 2 (R2)
//  GPB4 → Start
//  GPB5 → Select
//  GPB6 → Hotkey
//  GPB7 → Coin

// Button IDs used in firmware button array (GPA bits 0-7, GPB bits 0-7)
#define BTN_DL_UP       0    // GPA0
#define BTN_DL_LEFT     1    // GPA1
#define BTN_DL_RIGHT    2    // GPA2
#define BTN_DL_DOWN     3    // GPA3
#define BTN_DR_UP       4    // GPA4
#define BTN_DR_LEFT     5    // GPA5
#define BTN_DR_RIGHT    6    // GPA6
#define BTN_DR_DOWN     7    // GPA7
#define BTN_SHL1        8    // GPB0
#define BTN_SHL2        9    // GPB1
#define BTN_SHR1       10    // GPB2
#define BTN_SHR2       11    // GPB3
#define BTN_START      12    // GPB4
#define BTN_SELECT     13    // GPB5
#define BTN_HOTKEY     14    // GPB6
#define BTN_COIN       15    // GPB7

#define MCP_BUTTON_COUNT 16

// ─── OLED status display ─────────────────────────────────────────────────────
#define STATUS_DISPLAY_ENABLED        true
#define STATUS_OLED_I2C_ADDR          0x3C   // probe 0x3D if blank
#define STATUS_OLED_USE_SH1106_72X40  true   // ABrobot 0.42" panel
#define STATUS_OLED_SDA_PIN           I2C_SDA_PIN
#define STATUS_OLED_SCL_PIN           I2C_SCL_PIN
