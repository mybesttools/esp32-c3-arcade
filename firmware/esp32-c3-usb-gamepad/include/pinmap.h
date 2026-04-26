#pragma once

// BLE device identity.
static const char *USB_MANUFACTURER = "BLE Everywhere";
static const char *USB_PRODUCT = "BLE Everywhere Controller";

static const unsigned long SERIAL_BAUD_RATE = 115200;

// Optional MCP23017 button expander.
static const bool USE_MCP23017 = true;
static const uint8_t MCP23017_I2C_ADDR = 0x20;
static const uint32_t MCP_I2C_CLOCK_HZ = 50000;

// MCP23017 button plan (GPA=port A bits 0..7, GPB=port B bits 0..7):
//   GPA0=A  GPA1=B  GPA2=X   GPA3=Y  GPA4=L  GPA5=R  GPA6=START GPA7=SELECT
//   GPB0=HOTKEY GPB1=COIN GPB2=L2 GPB3=R2 GPB4=DPAD_UP GPB5=DPAD_DOWN GPB6=DPAD_LEFT GPB7=DPAD_RIGHT
static const uint8_t MCP_EXTRA_LEFT_BIT = 0;   // GPA0
static const uint8_t MCP_EXTRA_RIGHT_BIT = 1;  // GPA1

// Optional tiny OLED status display (SSD1306 over I2C).
// Set STATUS_DISPLAY_ENABLED to true once you confirm your display type.
// Typical values: address 0x3C, width 128, height 64.
static const bool STATUS_DISPLAY_ENABLED = true;
static const uint8_t STATUS_OLED_I2C_ADDR = 0x3C;    // confirmed address
static const uint8_t STATUS_OLED_ALT_I2C_ADDR = 0x3D; // fallback
static const int STATUS_OLED_WIDTH = 128;
static const int STATUS_OLED_HEIGHT = 64;

// ABrobot ESP32-C3 OLED uses SH1106-compatible 72x40 at an internal offset.
// Expected onboard I2C pins on this module family: SDA=GPIO5, SCL=GPIO6.
static const bool STATUS_OLED_USE_SH1106_72X40 = false;
static const uint8_t STATUS_OLED_SDA_PIN = 5;
static const uint8_t STATUS_OLED_SCL_PIN = 6;

// Analog joystick used as d-pad hat switch (update to your wiring).
// Positive X = right, positive Y = up. Invert JOY_INVERT_Y if your
// stick reports down as positive.
static const uint8_t JOY_X_PIN  = A0;  // GPIO0, JOY1_X
static const uint8_t JOY_Y_PIN  = A1;  // GPIO1, JOY1_Y
static const uint8_t JOY2_X_PIN = A2;  // GPIO2, JOY2_X
static const uint8_t JOY2_Y_PIN = A3;  // GPIO3, JOY2_Y
static const bool    JOY_INVERT_Y = false;

// Calibrate these after your first test run.
static const int JOY_MIN = 0;
static const int JOY_MAX = 4095;
static const int JOY_CENTER_X = 2048;
static const int JOY_CENTER_Y = 2048;
static const int JOY_DEADZONE = 35;
// Right stick often needs a larger deadzone due to board/layout noise.
static const int JOY2_DEADZONE = 120;
// If true, MCP d-pad presses force right-stick output to neutral to avoid
// analog crosstalk artifacts on compact handheld wiring.
static const bool MCP_DPAD_LOCKS_RIGHT_STICK = true;

// Threshold (in signed 16-bit units) at which a joystick tilt counts
// as a d-pad press. 8192 ≈ 25% of full deflection.
static const int16_t HAT_THRESHOLD = 8192;

// NDS-style buttons, active LOW (wired to GND when pressed).
// Bit index in the button bitmask matches array index.
static const uint8_t BUTTON_PINS[] = {
  2,  // A
  3,  // B
  4,  // X
  8,  // Y
  10, // L
  7,  // R
  9,  // START
  20, // SELECT
  21, // HOTKEY
  // COIN removed — no free GPIO
};

static const size_t BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);
static const size_t MCP_TOTAL_BUTTON_COUNT = 16;
static const size_t TOTAL_BUTTON_COUNT = USE_MCP23017 ? MCP_TOTAL_BUTTON_COUNT : BUTTON_COUNT;
