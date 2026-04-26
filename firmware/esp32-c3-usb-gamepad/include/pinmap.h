#pragma once

// BLE device identity.
static const char *USB_MANUFACTURER = "BLE Everywhere";
static const char *USB_PRODUCT = "BLE Everywhere Controller";

static const unsigned long SERIAL_BAUD_RATE = 115200;

// ── Config A: MCP23017 expander board connected ────────────────────────────
// Build with: pio run -e esp32c3_ble_mcp  (adds -D USE_MCP23017_EXPANDER)
//
// Two analog joysticks on ESP32-C3 ADC pins.
// All digital buttons wired to MCP23017 GPA/GPB (active LOW, no resistors).
// Left stick  → hat switch 1.   Right stick → BLE Rx/Ry axes.
// L3/R3 (joystick clicks) → GPB2 / GPB3.
#if defined(USE_MCP23017_EXPANDER)

static const bool USE_MCP23017 = true;
static const uint8_t MCP23017_I2C_ADDR = 0x20; // A0/A1/A2 all tied to GND

// Left joystick (hat switch).
static const uint8_t JOY_X_PIN = A0;  // GPIO0
static const uint8_t JOY_Y_PIN = A1;  // GPIO1
static const bool    JOY_INVERT_X = true;
static const bool    JOY_INVERT_Y = false;

// Right joystick (BLE Rx/Ry axes).
static const uint8_t JOY2_X_PIN = A2; // GPIO2
static const uint8_t JOY2_Y_PIN = A3; // GPIO3
static const bool    JOY2_INVERT_X = true;
static const bool    JOY2_INVERT_Y = false;

// Calibration — adjust after first test run.
static const int JOY_MIN      = 0;
static const int JOY_MAX      = 4095;
static const int JOY_CENTER_X = 2048;
static const int JOY_CENTER_Y = 2048;
static const int JOY2_CENTER_X = 2048;
static const int JOY2_CENTER_Y = 2048;
static const int JOY_DEADZONE = 35;

static const int16_t HAT_THRESHOLD = 8192;

// No direct-GPIO buttons — all on MCP23017.
static const uint8_t BUTTON_PINS[] = {};
static const size_t  BUTTON_COUNT  = 0;

// MCP23017 bit positions for each button (Port A bits 0-7, Port B bits 8-15), active LOW.
// Board right-side labels: rows B0/A0 … B7/A7.  Use the Ax pin for Port A, Bx for Port B.
// Button index in the array = button number sent in the BLE report.
static const uint8_t MCP_BUTTON_BITS[] = {
  0,   // A         board pin A0  (Port A bit 0)
  1,   // B         board pin A1  (Port A bit 1)
  2,   // X         board pin A2  (Port A bit 2)
  3,   // Y         board pin A3  (Port A bit 3)
  4,   // L         board pin A4  (Port A bit 4)
  5,   // R         board pin A5  (Port A bit 5)
  6,   // START     board pin A6  (Port A bit 6)
  7,   // SELECT    board pin A7  (Port A bit 7)
  8,   // HOTKEY    board pin B0  (Port B bit 0)
  9,   // COIN      board pin B1  (Port B bit 1)
  10,  // spare     board pin B2  (Port B bit 2, formerly L3)
  11,  // spare     board pin B3  (Port B bit 3, formerly R3)
  12,  // D-UP      board pin B4  (Port B bit 4)
  13,  // D-DOWN    board pin B5  (Port B bit 5)
  14,  // D-LEFT    board pin B6  (Port B bit 6)
  15,  // D-RIGHT   board pin B7  (Port B bit 7)
};
static const size_t MCP_BUTTON_COUNT  = sizeof(MCP_BUTTON_BITS) / sizeof(MCP_BUTTON_BITS[0]);
static const size_t TOTAL_BUTTON_COUNT = MCP_BUTTON_COUNT;

#else

// ── Config B: no expander, single joystick, direct GPIO buttons ────────────
// Build with: pio run -e esp32c3_esp32c3_ble  (default)

static const bool USE_MCP23017 = false;
static const uint8_t MCP23017_I2C_ADDR = 0x20;

// Extra MCP buttons (L3/R3) accessible via the two-bit legacy path.
static const uint8_t MCP_EXTRA_LEFT_BIT  = 0;  // GPA0 -> L3
static const uint8_t MCP_EXTRA_RIGHT_BIT = 1;  // GPA1 -> R3

static const uint8_t JOY_X_PIN = A0;
static const uint8_t JOY_Y_PIN = A1;
static const bool    JOY_INVERT_X = false;
static const bool    JOY_INVERT_Y = false;

static const int JOY_MIN      = 0;
static const int JOY_MAX      = 4095;
static const int JOY_CENTER_X = 2048;
static const int JOY_CENTER_Y = 2048;
static const int JOY_DEADZONE = 35;

static const int16_t HAT_THRESHOLD = 8192;

// NDS-style buttons, active LOW (wired to GND when pressed).
static const uint8_t BUTTON_PINS[] = {
  2,  // A
  3,  // B
  4,  // X
  5,  // Y
  6,  // L
  7,  // R
  9,  // START
  10, // SELECT
  20, // HOTKEY
  21, // COIN
};
static const size_t BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

static const size_t MCP_EXTRA_BUTTON_COUNT = 2;
static const size_t TOTAL_BUTTON_COUNT = BUTTON_COUNT + MCP_EXTRA_BUTTON_COUNT;

#endif // USE_MCP23017_EXPANDER

// ── OLED status display (shared by both configs) ───────────────────────────
// I2C on GPIO5 (SDA) / GPIO6 (SCL) — same bus as MCP23017.
static const bool STATUS_DISPLAY_ENABLED = true;
static const uint8_t STATUS_OLED_I2C_ADDR     = 0x3D;
static const uint8_t STATUS_OLED_ALT_I2C_ADDR = 0x3C;
static const int STATUS_OLED_WIDTH  = 128;
static const int STATUS_OLED_HEIGHT = 64;

// ABrobot ESP32-C3 OLED: set false for SSD1306 (confirmed working), true for SH1106.
static const bool STATUS_OLED_USE_SH1106_72X40 = false;
static const uint8_t STATUS_OLED_SDA_PIN = 5;
static const uint8_t STATUS_OLED_SCL_PIN = 6;
