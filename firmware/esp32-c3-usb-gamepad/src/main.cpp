#include <Arduino.h>
#include "pinmap.h"

#include <Wire.h>
#include <esp_rom_sys.h>

#if defined(ESP32C3_STATUS_OLED)
#include <Wire.h>
#include <U8g2lib.h>
#endif

static constexpr uint32_t USB_SERIAL_FRAME_INTERVAL_MS = 10;
static constexpr uint8_t ESP32C3_LED_PIN = 8;

// MCP23017 register map used for simple polled button inputs.
static constexpr uint8_t MCP_REG_IODIRA = 0x00;
static constexpr uint8_t MCP_REG_IODIRB = 0x01;
static constexpr uint8_t MCP_REG_GPPUA = 0x0C;
static constexpr uint8_t MCP_REG_GPPUB = 0x0D;
static constexpr uint8_t MCP_REG_GPIOA = 0x12;
static constexpr uint8_t MCP_REG_GPIOB = 0x13;

static bool isReservedDisplayPin(uint8_t pin)
{
#if defined(ESP32C3_STATUS_OLED)
  if (STATUS_DISPLAY_ENABLED && (pin == STATUS_OLED_SDA_PIN || pin == STATUS_OLED_SCL_PIN))
  {
    return true;
  }
#endif
  return false;
}

int16_t mapAxis(int raw, int minV, int centerV, int maxV, int deadzone);
uint32_t readButtons();
void initMcp23017();
uint16_t readMcp23017Inputs();
static uint8_t axesToHat(int16_t x, int16_t y);

static void blinkLed(uint8_t pulses, uint16_t onMs, uint16_t offMs)
{
  for (uint8_t i = 0; i < pulses; i++)
  {
    digitalWrite(ESP32C3_LED_PIN, LOW); // Active-low onboard LED.
    delay(onMs);
    digitalWrite(ESP32C3_LED_PIN, HIGH);
    delay(offMs);
  }
}

#if defined(ESP32C3_STATUS_OLED)
static U8G2_SSD1306_72X40_ER_F_HW_I2C statusDisplaySsd(
  U8G2_R0,
  U8X8_PIN_NONE,  // reset
  U8X8_PIN_NONE,  // clock  — Wire already initialised in setup()
  U8X8_PIN_NONE   // data   — Wire already initialised in setup()
);
static U8G2_SH1106_72X40_WISE_F_HW_I2C statusDisplaySh1106(
  U8G2_R0,
  U8X8_PIN_NONE,  // reset
  U8X8_PIN_NONE,  // clock  — Wire already initialised in setup()
  U8X8_PIN_NONE   // data   — Wire already initialised in setup()
);
static U8G2_SSD1306_72X40_ER_F_SW_I2C statusDisplaySsdSw(
  U8G2_R0,
  STATUS_OLED_SCL_PIN,
  STATUS_OLED_SDA_PIN,
  U8X8_PIN_NONE
);
static U8G2_SH1106_72X40_WISE_F_SW_I2C statusDisplaySh1106Sw(
  U8G2_R0,
  STATUS_OLED_SCL_PIN,
  STATUS_OLED_SDA_PIN,
  U8X8_PIN_NONE
);
static bool statusDisplayReady = false;
static uint8_t statusDisplayAddr = STATUS_OLED_I2C_ADDR;
static uint8_t statusDisplaySdaPin = STATUS_OLED_SDA_PIN;
static uint8_t statusDisplaySclPin = STATUS_OLED_SCL_PIN;
static constexpr uint8_t STATUS_VISIBLE_WIDTH = 72;
static constexpr uint8_t STATUS_VISIBLE_HEIGHT = 40;
static constexpr uint8_t STATUS_X_OFFSET = 30;
static constexpr uint8_t STATUS_Y_OFFSET = 12;
static uint32_t statusLastMs = 0;
static bool statusWasConnected = false;
static uint32_t statusConnectedSinceMs = 0;
static uint32_t statusLastInputChangeMs = 0;
static uint32_t statusPrevButtons = 0;
static int16_t statusPrevX = 0;
static int16_t statusPrevY = 0;
static uint32_t statusDiagLastMs = 0;
static uint32_t statusLedDiagLastMs = 0;
static uint8_t statusI2cDiagCode = 0;
static int16_t statusRx = 0;
static int16_t statusRy = 0;

static bool statusUseSh1106()
{
  return STATUS_OLED_USE_SH1106_72X40;
}

static bool i2cDevicePresent(uint8_t addr)
{
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

static void blinkDiagCode()
{
  // Codes: 1=0x3C found, 2=0x3D found, 3=both, 4=none.
  uint8_t pulses = statusI2cDiagCode;
  if (pulses == 0)
  {
    pulses = 4;
  }
  blinkLed(pulses, 70, 90);
}

static void logDisplayProbe()
{
  Serial.println("[display] probing I2C bus...");

  const uint8_t pinPairs[][2] = {
    { STATUS_OLED_SDA_PIN, STATUS_OLED_SCL_PIN },
    { 8, 9 },
    { 9, 8 },
    { 6, 5 },
  };

  for (size_t i = 0; i < (sizeof(pinPairs) / sizeof(pinPairs[0])); i++)
  {
    Wire.begin(pinPairs[i][0], pinPairs[i][1]);
    delay(8);
    Serial.printf("[display] scan SDA=%u SCL=%u -> ", pinPairs[i][0], pinPairs[i][1]);

    bool any = false;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
      if (i2cDevicePresent(addr))
      {
        Serial.printf("0x%02X ", addr);
        any = true;
      }
    }

    if (!any)
    {
      Serial.print("none");
    }
    Serial.println();
  }
}

static void statusWakeCycle()
{
  for (uint8_t i = 0; i < 4; i++)
  {
    bool fill = ((i % 2) == 0);
    if (statusUseSh1106())
    {
      statusDisplaySh1106.clearBuffer();
      if (fill)
      {
        statusDisplaySh1106.drawBox(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
      }
      statusDisplaySh1106.sendBuffer();
    }
    else
    {
      statusDisplaySsd.clearBuffer();
      if (fill)
      {
        statusDisplaySsd.drawBox(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
      }
      statusDisplaySsd.sendBuffer();
    }
    delay(120);
  }
}

static void statusBruteForceProbe()
{
  const uint8_t addrs[2] = { STATUS_OLED_I2C_ADDR, STATUS_OLED_ALT_I2C_ADDR };

  for (uint8_t ai = 0; ai < 2; ai++)
  {
    uint8_t addr = addrs[ai];

    Serial.printf("[display] brute probe SSD1306 @0x%02X\n", addr);
    statusDisplaySsd.setI2CAddress((uint8_t)(addr << 1));
    statusDisplaySsd.setBusClock(400000);
    statusDisplaySsd.begin();
    statusDisplaySsd.setPowerSave(0);
    statusDisplaySsd.setContrast(255);
    for (uint8_t i = 0; i < 2; i++)
    {
      statusDisplaySsd.clearBuffer();
      if ((i % 2) == 0)
      {
        statusDisplaySsd.drawBox(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
      }
      else
      {
        statusDisplaySsd.drawFrame(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
        statusDisplaySsd.setFont(u8g2_font_5x7_tr);
        statusDisplaySsd.drawStr(0, 20, "SSD1306");
      }
      statusDisplaySsd.sendBuffer();
      delay(180);
    }

    Serial.printf("[display] brute probe SH1106 @0x%02X\n", addr);
    statusDisplaySh1106.setI2CAddress((uint8_t)(addr << 1));
    statusDisplaySh1106.setBusClock(400000);
    statusDisplaySh1106.begin();
    statusDisplaySh1106.setPowerSave(0);
    statusDisplaySh1106.setContrast(255);
    for (uint8_t i = 0; i < 2; i++)
    {
      statusDisplaySh1106.clearBuffer();
      if ((i % 2) == 0)
      {
        statusDisplaySh1106.drawBox(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
      }
      else
      {
        statusDisplaySh1106.drawFrame(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
        statusDisplaySh1106.setFont(u8g2_font_5x7_tr);
        statusDisplaySh1106.drawStr(4, 20, "SH1106");
      }
      statusDisplaySh1106.sendBuffer();
      delay(180);
    }

    Serial.printf("[display] brute probe SSD1306-SW @0x%02X\n", addr);
    statusDisplaySsdSw.setI2CAddress((uint8_t)(addr << 1));
    statusDisplaySsdSw.begin();
    statusDisplaySsdSw.setContrast(255);
    for (uint8_t i = 0; i < 2; i++)
    {
      statusDisplaySsdSw.clearBuffer();
      if ((i % 2) == 0)
      {
        statusDisplaySsdSw.drawBox(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
      }
      else
      {
        statusDisplaySsdSw.drawFrame(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
        statusDisplaySsdSw.setFont(u8g2_font_5x7_tr);
        statusDisplaySsdSw.drawStr(0, 20, "SSD1306 SW");
      }
      statusDisplaySsdSw.sendBuffer();
      delay(180);
    }

    Serial.printf("[display] brute probe SH1106-SW @0x%02X\n", addr);
    statusDisplaySh1106Sw.setI2CAddress((uint8_t)(addr << 1));
    statusDisplaySh1106Sw.begin();
    statusDisplaySh1106Sw.setContrast(255);
    for (uint8_t i = 0; i < 2; i++)
    {
      statusDisplaySh1106Sw.clearBuffer();
      if ((i % 2) == 0)
      {
        statusDisplaySh1106Sw.drawBox(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
      }
      else
      {
        statusDisplaySh1106Sw.drawFrame(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
        statusDisplaySh1106Sw.setFont(u8g2_font_5x7_tr);
        statusDisplaySh1106Sw.drawStr(4, 20, "SH1106 SW");
      }
      statusDisplaySh1106Sw.sendBuffer();
      delay(180);
    }
  }
}

static const char *hatToText(uint8_t hat)
{
  // Hat values follow ESP32-BLE-Gamepad convention:
  // 0=center, 1=up, 2=up-right, 3=right, 4=down-right,
  // 5=down, 6=down-left, 7=left, 8=up-left.
  switch (hat)
  {
    case 1: return "UP";
    case 2: return "UP-RIGHT";
    case 3: return "RIGHT";
    case 4: return "DOWN-RIGHT";
    case 5: return "DOWN";
    case 6: return "DOWN-LEFT";
    case 7: return "LEFT";
    case 8: return "UP-LEFT";
    default: return "CENTER";
  }
}

static uint8_t countPressedButtons(uint32_t buttons)
{
  uint8_t count = 0;
  for (size_t i = 0; i < TOTAL_BUTTON_COUNT; i++)
  {
    if ((buttons & (1UL << i)) != 0)
    {
      count++;
    }
  }
  return count;
}

// Draw a joystick circle (r=8) with a filled dot showing stick position.
static void drawStick(U8G2 &disp, uint8_t cx, uint8_t cy, int16_t sx, int16_t sy)
{
  disp.drawCircle(cx, cy, 8);
  int8_t dx = (int8_t)((int32_t)sx * 6 / 32767);
  int8_t dy = (int8_t)((int32_t)sy * 6 / 32767);
  disp.drawDisc((uint8_t)(cx + dx), (uint8_t)(cy + dy), 2);
}

static void statusDrawAdvertising()
{
  if (!statusDisplayReady)
  {
    return;
  }

  uint8_t phase = (uint8_t)((millis() / 350) % 4);
  char dots[5] = "    ";
  for (uint8_t i = 0; i < phase; i++)
  {
    dots[i] = '.';
  }
  dots[4] = '\0';

  char line3[24];
  snprintf(line3, sizeof(line3), "WAIT HOST%s", dots);

  U8G2 &disp = statusUseSh1106() ? (U8G2 &)statusDisplaySh1106 : (U8G2 &)statusDisplaySsd;
  disp.clearBuffer();
  disp.setFont(u8g2_font_5x7_tr);
  disp.drawFrame(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
  disp.drawStr(0, 8, "BLE Everywhere");
  disp.drawStr(0, 16, "ADVERTISING");
  disp.drawStr(0, 24, line3);
  disp.drawStr(0, 32, "PAIR NOW");
  disp.drawStr(0, 40, "BLE Everywhere");
  disp.sendBuffer();
}

static void statusDrawConnected(int16_t x, int16_t y, uint8_t hat, uint32_t buttons)
{
  if (!statusDisplayReady)
  {
    return;
  }

  U8G2 &disp = statusUseSh1106() ? (U8G2 &)statusDisplaySh1106 : (U8G2 &)statusDisplaySsd;
  disp.clearBuffer();

  // Left stick — centered at (9, 20), drives hat.
  drawStick(disp, 9, 20, x, y);

#if defined(USE_MCP23017_EXPANDER)
  // Right stick — centered at (63, 20), drives Rx/Ry axes.
  // Negate X for display since JOY2_INVERT_X is already applied to statusRx.
  drawStick(disp, 63, 20, JOY2_INVERT_X ? -statusRx : statusRx, JOY2_INVERT_Y ? -statusRy : statusRy);
#endif

  // Middle area (x 18..53): show lowest pressed button index, or "---".
  char btnStr[6];
  if (buttons != 0)
  {
    uint8_t idx = 0;
    uint32_t tmp = buttons;
    while ((tmp & 1) == 0) { tmp >>= 1; idx++; }
    snprintf(btnStr, sizeof(btnStr), "B%u", (unsigned)(idx + 1));
  }
  else
  {
    snprintf(btnStr, sizeof(btnStr), "---");
  }
  disp.setFont(u8g2_font_7x13_tr);
  uint8_t tw = (uint8_t)disp.getStrWidth(btnStr);
  uint8_t tx = (uint8_t)(18 + (36 - tw) / 2);
  disp.drawStr(tx, 27, btnStr);

  disp.sendBuffer();
}

static void statusInit()
{
  if (!STATUS_DISPLAY_ENABLED)
  {
    return;
  }

  statusDisplaySdaPin = STATUS_OLED_SDA_PIN;
  statusDisplaySclPin = STATUS_OLED_SCL_PIN;
  statusDisplayAddr = STATUS_OLED_I2C_ADDR;

  // Wire was already started on OLED pins in setup().
  delay(1000);
  delay(8);

  if (!i2cDevicePresent(statusDisplayAddr) && i2cDevicePresent(STATUS_OLED_ALT_I2C_ADDR))
  {
    statusDisplayAddr = STATUS_OLED_ALT_I2C_ADDR;
  }

  bool found3C = i2cDevicePresent(0x3C);
  bool found3D = i2cDevicePresent(0x3D);
  if (found3C)
  {
    blinkLed(1, 220, 140);
  }
  if (found3D)
  {
    blinkLed(2, 220, 140);
  }
  if (!found3C && !found3D)
  {
    blinkLed(4, 70, 70);
  }

  statusI2cDiagCode = 0;
  if (found3C) statusI2cDiagCode |= 0x01;
  if (found3D) statusI2cDiagCode |= 0x02;
  if (statusI2cDiagCode == 0x01) statusI2cDiagCode = 1;
  else if (statusI2cDiagCode == 0x02) statusI2cDiagCode = 2;
  else if (statusI2cDiagCode == 0x03) statusI2cDiagCode = 3;
  else statusI2cDiagCode = 4;

  esp_rom_printf("[display] SDA=%u SCL=%u ADDR=0x%02X found3C=%u found3D=%u diagCode=%u\n",
                statusDisplaySdaPin, statusDisplaySclPin, statusDisplayAddr,
                found3C ? 1U : 0U, found3D ? 1U : 0U, statusI2cDiagCode);

  // Full I2C bus scan for debugging.
  esp_rom_printf("[display] I2C scan: ");
  for (uint8_t addr = 0x08; addr <= 0x77; addr++)
  {
    if (i2cDevicePresent(addr))
    {
      esp_rom_printf("0x%02X ", addr);
    }
  }
  esp_rom_printf("\n");

  if (statusUseSh1106())
  {
    esp_rom_printf("[display] calling SH1106 begin()...\n");
    statusDisplaySh1106.setI2CAddress((uint8_t)(statusDisplayAddr << 1));
    statusDisplaySh1106.setBusClock(400000);
    statusDisplaySh1106.begin();
    statusDisplaySh1106.setPowerSave(0);
    statusDisplaySh1106.setContrast(255);
    esp_rom_printf("[display] SH1106 begin() done\n");
  }
  else
  {
    esp_rom_printf("[display] calling SSD1306 begin()...\n");
    statusDisplaySsd.setI2CAddress((uint8_t)(statusDisplayAddr << 1));
    statusDisplaySsd.setBusClock(400000);
    statusDisplaySsd.begin();
    statusDisplaySsd.setPowerSave(0);
    statusDisplaySsd.setContrast(255);
    esp_rom_printf("[display] SSD1306 begin() done\n");
  }

  // 3 quick blinks = begin() completed without hanging.
  blinkLed(3, 60, 60);

  statusDisplayReady = true;
  statusDrawAdvertising();
  esp_rom_printf("[display] statusInit() complete, display ready\n");
}

static void statusUpdate(bool connected, int16_t x, int16_t y, uint8_t hat, uint32_t buttons)
{
  if (!statusDisplayReady)
  {
    return;
  }

  if (!statusWasConnected && connected)
  {
    statusConnectedSinceMs = millis();
    statusLastInputChangeMs = millis();
  }

  if (connected && (buttons != statusPrevButtons || x != statusPrevX || y != statusPrevY))
  {
    statusLastInputChangeMs = millis();
    statusPrevButtons = buttons;
    statusPrevX = x;
    statusPrevY = y;
  }

  uint32_t now = millis();
  if ((now - statusLastMs) < 100 && connected == statusWasConnected)
  {
    return;
  }
  statusLastMs = now;
  statusWasConnected = connected;

  if (!connected)
  {
    statusDrawAdvertising();
    return;
  }

  statusDrawConnected(x, y, hat, buttons);
}
#endif

#if defined(ESP32C3_DIAGNOSTIC_INPUTS)
static void readControllerState(int16_t &x, int16_t &y, uint32_t &buttons)
{
  uint32_t phase = (millis() / 20) % 400;
  int32_t wave = (phase < 200) ? phase : (399 - phase);
  x = (int16_t)((wave * 32767 / 199) - 16384);
  y = (int16_t)(-x);
  buttons = ((millis() / 1000) % 2 == 0) ? 0x1 : 0x0;
}
#else
static void readControllerState(int16_t &x, int16_t &y, uint32_t &buttons)
{
  int rawX = analogRead(JOY_X_PIN);
  int rawY = analogRead(JOY_Y_PIN);
  x = mapAxis(rawX, JOY_MIN, JOY_CENTER_X, JOY_MAX, JOY_DEADZONE);
  y = mapAxis(rawY, JOY_MIN, JOY_CENTER_Y, JOY_MAX, JOY_DEADZONE);
  buttons = readButtons();
}
#endif

#if defined(CONTROLLER_MODE_BLE)
#include <BleGamepad.h>
BleGamepadConfiguration bleGamepadConfig;
BleGamepad bleGamepad("BLE Everywhere Controller", "BLE Everywhere", 100);

// Convert signed joystick axes to an 8-way hat direction.
static uint8_t axesToHat(int16_t x, int16_t y)
{
  bool up    = (y >  HAT_THRESHOLD);
  bool down  = (y < -HAT_THRESHOLD);
  bool right = (x >  HAT_THRESHOLD);
  bool left  = (x < -HAT_THRESHOLD);

  if (up   && right) return HAT_UP_RIGHT;
  if (up   && left)  return HAT_UP_LEFT;
  if (down && right) return HAT_DOWN_RIGHT;
  if (down && left)  return HAT_DOWN_LEFT;
  if (up)            return HAT_UP;
  if (down)          return HAT_DOWN;
  if (right)         return HAT_RIGHT;
  if (left)          return HAT_LEFT;
  return HAT_CENTERED;
}
#elif defined(CONTROLLER_MODE_USB_HID)
#include <Adafruit_TinyUSB.h>

// HID report descriptor for a simple gamepad with 2 axes and 16 buttons
#define TUD_HID_REPORT_DESC_SIMPLE_GAMEPAD(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP      )                 , \
  HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD   )                 , \
  HID_COLLECTION ( HID_COLLECTION_APPLICATION  )                 , \
    __VA_ARGS__ \
    /* 2 axes (x, y) */ \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP      )                 , \
    HID_USAGE      ( HID_USAGE_DESKTOP_X         )                 , \
    HID_USAGE      ( HID_USAGE_DESKTOP_Y         )                 , \
    HID_LOGICAL_MIN  ( -127                                      ) , \
    HID_LOGICAL_MAX  ( 127                                       ) , \
    HID_REPORT_SIZE  ( 8                                         ) , \
    HID_REPORT_COUNT ( 2                                         ) , \
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE    ) , \
    /* 16 buttons */ \
    HID_USAGE_PAGE ( HID_USAGE_PAGE_BUTTON       )                 , \
    HID_USAGE_MIN    ( 1                                         ) , \
    HID_USAGE_MAX    ( 16                                        ) , \
    HID_LOGICAL_MIN  ( 0                                         ) , \
    HID_LOGICAL_MAX  ( 1                                         ) , \
    HID_REPORT_SIZE  ( 1                                         ) , \
    HID_REPORT_COUNT ( 16                                        ) , \
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE    ) , \
  HID_COLLECTION_END

uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_SIMPLE_GAMEPAD(HID_REPORT_ID(1)) };
Adafruit_USBD_HID usb_hid;
#endif

// Convert analog joystick reading to signed 16-bit range with center deadzone.
int16_t mapAxis(int raw, int minV, int centerV, int maxV, int deadzone)
{
  if (raw < centerV - deadzone)
  {
    long v = map(raw, minV, centerV - deadzone, -32767, 0);
    return (int16_t)constrain(v, -32767, 0);
  }

  if (raw > centerV + deadzone)
  {
    long v = map(raw, centerV + deadzone, maxV, 0, 32767);
    return (int16_t)constrain(v, 0, 32767);
  }

  return 0;
}

uint32_t readButtons()
{
  uint32_t bits = 0;

#if defined(USE_MCP23017_EXPANDER)
  // All buttons on MCP23017; read entire port word and map by bit position.
  uint16_t mcpInputs = readMcp23017Inputs();
  for (size_t i = 0; i < MCP_BUTTON_COUNT; i++)
  {
    if ((mcpInputs & (1U << MCP_BUTTON_BITS[i])) == 0U) // active LOW
    {
      bits |= (1UL << i);
    }
  }
#else
  for (size_t i = 0; i < BUTTON_COUNT; i++)
  {
    if (isReservedDisplayPin(BUTTON_PINS[i]))
    {
      continue;
    }

    bool pressed = (digitalRead(BUTTON_PINS[i]) == LOW);
    if (pressed)
    {
      bits |= (1UL << i);
    }
  }

  if (USE_MCP23017)
  {
    uint16_t mcpInputs = readMcp23017Inputs();
    bool extraLeftPressed  = ((mcpInputs & (1U << MCP_EXTRA_LEFT_BIT))  == 0U);
    bool extraRightPressed = ((mcpInputs & (1U << MCP_EXTRA_RIGHT_BIT)) == 0U);

    if (extraLeftPressed)
    {
      bits |= (1UL << BUTTON_COUNT);
    }
    if (extraRightPressed)
    {
      bits |= (1UL << (BUTTON_COUNT + 1));
    }
  }
#endif

  return bits;
}

void initMcp23017()
{
  if (!USE_MCP23017)
  {
    return;
  }

  // All pins as inputs.
  Wire.beginTransmission(MCP23017_I2C_ADDR);
  Wire.write(MCP_REG_IODIRA);
  Wire.write(0xFF);
  Wire.write(0xFF);
  Wire.endTransmission();

  // Enable pull-ups on all MCP pins.
  Wire.beginTransmission(MCP23017_I2C_ADDR);
  Wire.write(MCP_REG_GPPUA);
  Wire.write(0xFF);
  Wire.write(0xFF);
  Wire.endTransmission();
}

uint16_t readMcp23017Inputs()
{
  if (!USE_MCP23017)
  {
    return 0xFFFF;
  }

  Wire.beginTransmission(MCP23017_I2C_ADDR);
  Wire.write(MCP_REG_GPIOA);
  if (Wire.endTransmission(false) != 0)
  {
    return 0xFFFF;
  }

  if (Wire.requestFrom((int)MCP23017_I2C_ADDR, 2) != 2)
  {
    return 0xFFFF;
  }

  uint8_t gpioA = (uint8_t)Wire.read();
  uint8_t gpioB = (uint8_t)Wire.read();
  return (uint16_t)gpioA | ((uint16_t)gpioB << 8);
}

void setup()
{
  // Give USB-Serial/JTAG time to re-enumerate after reset so host can attach.
  Serial.begin(SERIAL_BAUD_RATE);
  delay(2000);
  esp_rom_printf("\n\n[boot] === BLE Everywhere boot ===\n");
  Serial.println("\n[boot] === BLE Everywhere boot ===");

  pinMode(ESP32C3_LED_PIN, OUTPUT);
  digitalWrite(ESP32C3_LED_PIN, HIGH);
  blinkLed(1, 80, 80);

#if !defined(ESP32C3_DIAGNOSTIC_INPUTS)
  analogReadResolution(12);

  // I2C bus recovery: if a previous reset interrupted a transaction the OLED
  // may hold SDA low, preventing Wire.begin() from working. Clock SCL 9 times
  // to flush any stuck byte, then send a STOP condition to free the bus.
  pinMode(STATUS_OLED_SCL_PIN, OUTPUT);
  pinMode(STATUS_OLED_SDA_PIN, INPUT_PULLUP);
  for (uint8_t i = 0; i < 9; i++) {
    digitalWrite(STATUS_OLED_SCL_PIN, HIGH); delayMicroseconds(5);
    digitalWrite(STATUS_OLED_SCL_PIN, LOW);  delayMicroseconds(5);
  }
  // STOP: SDA low→high while SCL high.
  pinMode(STATUS_OLED_SDA_PIN, OUTPUT);
  digitalWrite(STATUS_OLED_SDA_PIN, LOW);
  digitalWrite(STATUS_OLED_SCL_PIN, HIGH); delayMicroseconds(5);
  digitalWrite(STATUS_OLED_SDA_PIN, HIGH); delayMicroseconds(5);

  // Use the OLED I2C pins explicitly. Wire.begin() with no args defaults to
  // GPIO8/9 on ESP32-C3, which conflicts with the onboard LED on GPIO8.
  esp_rom_printf("[boot] Wire.begin(SDA=%u, SCL=%u)\n", STATUS_OLED_SDA_PIN, STATUS_OLED_SCL_PIN);
  Wire.begin(STATUS_OLED_SDA_PIN, STATUS_OLED_SCL_PIN);
  initMcp23017();

  // Reclaim GPIO8 for LED after Wire.begin() in case it was touched.
  pinMode(ESP32C3_LED_PIN, OUTPUT);
  digitalWrite(ESP32C3_LED_PIN, HIGH);
  blinkLed(2, 60, 60); // checkpoint 2: Wire.begin + MCP done
  esp_rom_printf("[boot] checkpoint 2: Wire + MCP done\n");

  for (size_t i = 0; i < BUTTON_COUNT; i++)
  {
    if (isReservedDisplayPin(BUTTON_PINS[i]))
    {
      esp_rom_printf("[warn] skip button pin %u (reserved for OLED I2C)\n", BUTTON_PINS[i]);
      continue;
    }

    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

  pinMode(JOY_X_PIN, INPUT);
  pinMode(JOY_Y_PIN, INPUT);
#if defined(USE_MCP23017_EXPANDER)
#if defined(USE_MCP23017_EXPANDER)
  pinMode(JOY2_X_PIN, INPUT);
  pinMode(JOY2_Y_PIN, INPUT);
#endif
#endif
#endif

#if defined(CONTROLLER_MODE_BLE)
  // Test OLED first, before BLE (which may take a long time).
  blinkLed(3, 60, 60); // checkpoint 3: about to init display
  esp_rom_printf("[boot] checkpoint 3: starting statusInit()\n");
  statusInit();
  blinkLed(5, 60, 60); // checkpoint 5: display init done, starting BLE
  esp_rom_printf("[boot] checkpoint 5: statusInit() done, starting BLE\n");
  delay(500);
  bleGamepadConfig.setButtonCount(TOTAL_BUTTON_COUNT);
  bleGamepadConfig.setHatSwitchCount(1);
#if defined(USE_MCP23017_EXPANDER)
  // Enable Rx/Ry for right analog stick. Left stick is sent as hat switch.
  bleGamepadConfig.setWhichAxes(false, false, false, false, true, true, false, false);
#else
  bleGamepadConfig.setWhichAxes(false, false, false, false, false, false, false, false);
#endif
  bleGamepad.begin(&bleGamepadConfig);
  blinkLed(6, 60, 60); // checkpoint 6: BLE started
  esp_rom_printf("[boot] checkpoint 6: BLE started, entering loop\n");
#elif defined(CONTROLLER_MODE_USB_HID)
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();
  // For debug printing
  Serial.begin(SERIAL_BAUD_RATE);
#else
#if defined(ESP32C3_STATUS_OLED)
  statusInit();
  statusUpdate(false, 0, 0, 0, 0);
#endif
  delay(100);
  Serial.println("ESP32C3_SERIAL_GAMEPAD_V1");
#endif
}

void loop()
{
#if defined(CONTROLLER_MODE_BLE)
  if (!bleGamepad.isConnected())
  {
    uint32_t now = millis();
    if ((now - statusDiagLastMs) >= 1000)
    {
      statusDiagLastMs = now;
      Serial.printf("[diag] adv sda=%u scl=%u addr=0x%02X ready=%u\n",
                    statusDisplaySdaPin,
                    statusDisplaySclPin,
                    statusDisplayAddr,
                    statusDisplayReady ? 1U : 0U);
      esp_rom_printf("[diag] adv sda=%u scl=%u addr=0x%02X ready=%u\n",
                     statusDisplaySdaPin,
                     statusDisplaySclPin,
                     statusDisplayAddr,
                     statusDisplayReady ? 1U : 0U);
    }

    if ((now - statusLedDiagLastMs) >= 2500)
    {
      statusLedDiagLastMs = now;
      blinkDiagCode();
    }

    statusUpdate(false, 0, 0, HAT_CENTERED, 0);
    delay(10);
    return;
  }

  int16_t x = 0;
  int16_t y = 0;
  uint32_t buttons = 0;
  readControllerState(x, y, buttons);
  uint8_t hat = axesToHat(JOY_INVERT_X ? -x : x, JOY_INVERT_Y ? -y : y);
#if defined(USE_MCP23017_EXPANDER)
  int16_t rx = mapAxis(analogRead(JOY2_X_PIN), JOY_MIN, JOY2_CENTER_X, JOY_MAX, JOY_DEADZONE);
  int16_t ry = mapAxis(analogRead(JOY2_Y_PIN), JOY_MIN, JOY2_CENTER_Y, JOY_MAX, JOY_DEADZONE);
  if (JOY2_INVERT_X) rx = -rx;
  if (JOY2_INVERT_Y) ry = -ry;
#endif
  for (size_t i = 0; i < TOTAL_BUTTON_COUNT; i++)
  {
    uint8_t buttonIndex = (uint8_t)(i + 1);
    bool pressed = ((buttons & (1UL << i)) != 0);

    if (pressed)
    {
      bleGamepad.press(buttonIndex);
    }
    else
    {
      bleGamepad.release(buttonIndex);
    }
  }

  bleGamepad.setHat1(hat);
#if defined(USE_MCP23017_EXPANDER)
  bleGamepad.setRX(rx);
  bleGamepad.setRY(ry);
  statusRx = rx;
  statusRy = ry;
#endif
  statusUpdate(true, x, y, hat, buttons);
#elif defined(CONTROLLER_MODE_USB_HID)
  // Wait for host to be ready
  if ( !usb_hid.ready() ) {
    delay(10);
    return;
  }

  int16_t x = 0;
  int16_t y = 0;
  uint32_t buttons = 0;
  readControllerState(x, y, buttons);

  struct {
    int8_t x;
    int8_t y;
    uint16_t buttons;
  } report = { 0 };

  report.x = map(x, -32767, 32767, -127, 127);
  report.y = map(y, -32767, 32767, -127, 127);
  report.buttons = buttons;

  usb_hid.sendReport(1, &report, sizeof(report));
#else
  static uint32_t lastSerialFrameMs = 0;
  uint32_t now = millis();
  bool serialHostReady = Serial.isConnected();
  if ((now - lastSerialFrameMs) < USB_SERIAL_FRAME_INTERVAL_MS)
  {
#if defined(ESP32C3_STATUS_OLED)
    statusUpdate(serialHostReady, statusPrevX, statusPrevY,
                 axesToHat(JOY_INVERT_X ? -statusPrevX : statusPrevX,
                           JOY_INVERT_Y ? -statusPrevY : statusPrevY),
                 statusPrevButtons);
#endif
    delay(1);
    return;
  }
  lastSerialFrameMs = now;

  int16_t x = 0;
  int16_t y = 0;
  uint32_t buttons = 0;
  readControllerState(x, y, buttons);
  uint8_t hat = axesToHat(JOY_INVERT_X ? -x : x, JOY_INVERT_Y ? -y : y);

#if defined(ESP32C3_STATUS_OLED)
  statusUpdate(serialHostReady, x, y, hat, buttons);
#endif

  // CSV frame for Pi bridge: R,<x>,<y>,<buttons_hex>
  Serial.print("R,");
  Serial.print(x);
  Serial.print(',');
  Serial.print(y);
  Serial.print(',');
  Serial.println(buttons, HEX);
#endif

  delay(1);
}
