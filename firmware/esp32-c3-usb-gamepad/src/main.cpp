#include <Arduino.h>
#include "pinmap.h"

#include <Wire.h>
#include <esp_rom_sys.h>

#if defined(ESP32_C3_STATUS_OLED)
#include <Wire.h>
#include <U8g2lib.h>
#endif

static constexpr uint32_t USB_SERIAL_FRAME_INTERVAL_MS = 10;
static constexpr uint8_t ESP32_C3_LED_PIN = 8;

// MCP23017 register map used for simple polled button inputs.
static constexpr uint8_t MCP_REG_IODIRA = 0x00;
static constexpr uint8_t MCP_REG_IODIRB = 0x01;
static constexpr uint8_t MCP_REG_IOCONA = 0x0A;
static constexpr uint8_t MCP_REG_IOCONB = 0x0B;
static constexpr uint8_t MCP_REG_GPPUA = 0x0C;
static constexpr uint8_t MCP_REG_GPPUB = 0x0D;
static constexpr uint8_t MCP_REG_GPIOA = 0x12;
static constexpr uint8_t MCP_REG_GPIOB = 0x13;
static int gJoy2CenterX = JOY_CENTER_X;
static int gJoy2CenterY = JOY_CENTER_Y;
static int gJoy2FiltX = JOY_CENTER_X;
static int gJoy2FiltY = JOY_CENTER_Y;
static uint16_t gLastMcpRaw = 0xFFFF;
static uint32_t gLastMcpErr = 0;
static int gLastMcpPtrErr = 0;
static int gLastMcpReqErr = 0;
static bool gMcpReady = false;
static uint32_t gMcpLastInitAttemptMs = 0;
static void mcpPressedSummary(uint16_t raw, char *out, size_t outLen);

static bool isReservedDisplayPin(uint8_t pin)
{
#if defined(ESP32_C3_STATUS_OLED)
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
static uint8_t hatFromButtons(uint32_t buttons);
static void calibrateJoy2Center();
static bool mcpWriteReg(uint8_t reg, uint8_t value);
static int mcpReadReg(uint8_t reg);
static int readAnalogMedian5(uint8_t pin);

#if defined(ESP32_C3_STATUS_OLED)
// Use hardware I2C so OLED and MCP23017 share one coordinated Wire bus.
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C statusDisplaySsd(U8G2_R0, U8X8_PIN_NONE);
static U8G2_SH1106_128X64_NONAME_F_HW_I2C statusDisplaySh1106(U8G2_R0, U8X8_PIN_NONE);
// Single pointer used by all drawing helpers — set in statusInit().
static U8G2 *disp = nullptr;
static bool statusDisplayReady = false;
static uint8_t statusDisplayAddr = STATUS_OLED_I2C_ADDR;
static uint8_t statusDisplaySdaPin = STATUS_OLED_SDA_PIN;
static uint8_t statusDisplaySclPin = STATUS_OLED_SCL_PIN;
// Full framebuffer dimensions.
static constexpr uint8_t STATUS_VISIBLE_WIDTH  = 128;
static constexpr uint8_t STATUS_VISIBLE_HEIGHT = 64;
// Physically visible zone measured via bezel calibration.
// Sides clip at ~28px each; top clips ~12px; bottom has no clipping.
static constexpr uint8_t SAFE_X1 = 28;   // first visible column
static constexpr uint8_t SAFE_X2 = 99;   // last  visible column
static constexpr uint8_t SAFE_Y1 = 12;   // first visible row
static constexpr uint8_t SAFE_Y2 = 63;   // last  visible row
static constexpr uint8_t SAFE_W  = SAFE_X2 - SAFE_X1 + 1;  // 72
static constexpr uint8_t SAFE_H  = SAFE_Y2 - SAFE_Y1 + 1;  // 52
static constexpr uint8_t SAFE_CX = (SAFE_X1 + SAFE_X2) / 2; // 63
static uint32_t statusLastMs = 0;
static bool statusWasConnected = false;
static uint32_t statusConnectedSinceMs = 0;
static uint32_t statusLastInputChangeMs = 0;
static uint32_t statusPrevButtons = 0;
static int16_t statusPrevX = 0;
static int16_t statusPrevY = 0;
static int16_t statusPrevRx = 0;
static int16_t statusPrevRy = 0;
static uint32_t statusDiagLastMs = 0;
static uint32_t statusLedDiagLastMs = 0;
static uint8_t statusI2cDiagCode = 0;

static bool statusUseSh1106()
{
  return STATUS_OLED_USE_SH1106_72X40;
}

static bool i2cDevicePresent(uint8_t addr)
{
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

static void blinkLed(uint8_t pulses, uint16_t onMs, uint16_t offMs)
{
  for (uint8_t i = 0; i < pulses; i++)
  {
    digitalWrite(ESP32_C3_LED_PIN, LOW); // Active-low onboard LED.
    delay(onMs);
    digitalWrite(ESP32_C3_LED_PIN, HIGH);
    delay(offMs);
  }
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

// -- Shared drawing helpers --------------------------------------------------

// Draw a string horizontally centred within the physically visible zone.
static void drawCenteredStr(uint8_t y, const char *str)
{
  uint8_t w = (uint8_t)disp->getStrWidth(str);
  // Centre within [SAFE_X1 .. SAFE_X2], clamp to SAFE_X1 if too wide.
  uint8_t x = (SAFE_W >= w) ? (SAFE_X1 + (SAFE_W - w) / 2) : SAFE_X1;
  disp->drawStr(x, y, str);
}

// Draw a 4-way D-pad cross centred at (cx, cy) with the active direction filled.
static void drawDpad(uint8_t cx, uint8_t cy, uint8_t hat)
{
  // Swap left/right to match physical wiring.
  bool up    = (hat == 1 || hat == 2 || hat == 8);
  bool right = (hat == 6 || hat == 7 || hat == 8);
  bool down  = (hat == 4 || hat == 5 || hat == 6);
  bool left  = (hat == 2 || hat == 3 || hat == 4);

  const uint8_t R = 9; // distance from centre to direction-box centre
  const uint8_t S = 6; // box size
  const uint8_t H = 3; // S / 2

  if (up)    disp->drawBox  (cx - H, cy - R - H, S, S);
  else       disp->drawFrame(cx - H, cy - R - H, S, S);
  if (down)  disp->drawBox  (cx - H, cy + R - H, S, S);
  else       disp->drawFrame(cx - H, cy + R - H, S, S);
  if (left)  disp->drawBox  (cx - R - H, cy - H, S, S);
  else       disp->drawFrame(cx - R - H, cy - H, S, S);
  if (right) disp->drawBox  (cx + R - H, cy - H, S, S);
  else       disp->drawFrame(cx + R - H, cy - H, S, S);

  // Thin cross connectors between the four boxes.
  disp->drawHLine(cx - R + H, cy, 2 * R - S);
  disp->drawVLine(cx, cy - R + H, 2 * R - S);
}

// Draw a 13x10 labelled button box, filled + colour-inverted when pressed.
static void drawButton(uint8_t bx, uint8_t by, const char *label, bool pressed)
{
  const uint8_t W = 13, BH = 10;
  if (pressed)
  {
    disp->drawBox(bx, by, W, BH);
    disp->setDrawColor(0);
  }
  else
  {
    disp->drawFrame(bx, by, W, BH);
  }
  uint8_t lw = (uint8_t)disp->getStrWidth(label);
  disp->drawStr(bx + (W - lw) / 2, by + BH - 2, label);
  disp->setDrawColor(1);
}

// ---------------------------------------------------------------------------
static void statusWakeCycle()
{
  if (!disp) return;
  for (uint8_t i = 0; i < 4; i++)
  {
    disp->clearBuffer();
    if ((i % 2) == 0)
      disp->drawBox(0, 0, STATUS_VISIBLE_WIDTH, STATUS_VISIBLE_HEIGHT);
    disp->sendBuffer();
    delay(120);
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

static void statusDrawAdvertising()
{
  if (!statusDisplayReady) return;

  uint8_t phase = (uint8_t)((millis() / 400) % 5); // 0-3 progressive fill, 4=all filled

  disp->clearBuffer();

  // Inverted title bar across the full visible width.
  // Bar spans SAFE_X1..SAFE_X2, height 11px, top at SAFE_Y1.
  disp->setDrawColor(1);
  disp->drawBox(SAFE_X1, SAFE_Y1, SAFE_W, 11);
  disp->setDrawColor(0);
  disp->setFont(u8g2_font_6x10_tr);
  drawCenteredStr(SAFE_Y1 + 9, "BLE EVERYWHERE");
  disp->setDrawColor(1);

  // 4 dots evenly spaced across visible width, centred vertically.
  // Spacing: 72px / 4 = 18px each; centres at +9, +27, +45, +63 from SAFE_X1.
  const uint8_t dotCx[4] = {
    (uint8_t)(SAFE_X1 +  9),
    (uint8_t)(SAFE_X1 + 27),
    (uint8_t)(SAFE_X1 + 45),
    (uint8_t)(SAFE_X1 + 63),
  };
  const uint8_t dotCy = SAFE_Y1 + 28; // ~midpoint of safe zone
  for (uint8_t i = 0; i < 4; i++)
  {
    if (i <= phase)
      disp->drawDisc(dotCx[i], dotCy, 4);
    else
      disp->drawCircle(dotCx[i], dotCy, 4);
  }

  disp->setFont(u8g2_font_5x8_tr);
  drawCenteredStr(SAFE_Y1 + 38, "WAIT FOR HOST");
  drawCenteredStr(SAFE_Y1 + 46, "PAIR YOUR DEVICE");

  disp->sendBuffer();
}

static void statusDrawConnected(int16_t x, int16_t y, int16_t rx, int16_t ry, uint8_t hat, uint32_t buttons)
{
  if (!statusDisplayReady) return;

  disp->clearBuffer();

  // Layout: safe zone x=28..99 (72px wide), y=12..63 (52px tall).
  // Three equal columns: ~24px each.
  //   Left  col centre: 28 + 12 = 40
  //   Mid   col centre: 28 + 36 = 64  (= SAFE_CX)
  //   Right col centre: 28 + 60 = 88

  // --- Left stick box (14x14, dot inside) ---
  const uint8_t SBW = 14, SBH = 14;
  const uint8_t lCx = 40, lCy = SAFE_Y1 + SAFE_H / 2;  // ~38
  const uint8_t lBx = lCx - SBW / 2;
  const uint8_t lBy = lCy - SBH / 2;
  disp->drawFrame(lBx, lBy, SBW, SBH);
  int8_t ldx = (int8_t)((int32_t)x *  5 / 32767);
  int8_t ldy = (int8_t)((int32_t)y * -5 / 32767);  // screen Y inverted
  disp->drawDisc((uint8_t)(lCx + ldx), (uint8_t)(lCy + ldy), 1);

  // --- Right stick box (14x14, dot inside) ---
  const uint8_t rCx = 88, rCy = lCy;
  const uint8_t rBx = rCx - SBW / 2;
  const uint8_t rBy = rCy - SBH / 2;
  disp->drawFrame(rBx, rBy, SBW, SBH);
  int8_t rdx = (int8_t)((int32_t)rx * 5 / 32767);
  int8_t rdy = (int8_t)((int32_t)ry * -5 / 32767);  // screen Y inverted
  disp->drawDisc((uint8_t)(rCx + rdx), (uint8_t)(rCy + rdy), 1);

  // --- Middle: button code indication ---
  char code[16];
  snprintf(code, sizeof(code), "B:%04lX", (unsigned long)(buttons & 0xFFFF));
  disp->setFont(u8g2_font_5x8_tr);
  uint8_t cw = (uint8_t)disp->getStrWidth(code);
  disp->drawStr((uint8_t)(SAFE_CX - cw / 2), (uint8_t)(SAFE_Y1 + 26), code);

  const char *hatLabel = hatToText(hat);
  uint8_t hw = (uint8_t)disp->getStrWidth(hatLabel);
  disp->drawStr((uint8_t)(SAFE_CX - hw / 2), (uint8_t)(SAFE_Y1 + 36), hatLabel);

  if (USE_MCP23017)
  {
    char mcpState[20];
    snprintf(mcpState, sizeof(mcpState), "M:%c E:%lu",
             gMcpReady ? 'Y' : 'N',
             (unsigned long)gLastMcpErr);
    uint8_t mw = (uint8_t)disp->getStrWidth(mcpState);
    disp->drawStr((uint8_t)(SAFE_CX - mw / 2), (uint8_t)(SAFE_Y1 + 46), mcpState);

    char pressed[80] = {0};
    mcpPressedSummary(gLastMcpRaw, pressed, sizeof(pressed));

    char mcpPins[22];
    snprintf(mcpPins, sizeof(mcpPins), "P:%s", pressed);
    if (strlen(mcpPins) > 18)
    {
      mcpPins[18] = '.';
      mcpPins[19] = '.';
      mcpPins[20] = '.';
      mcpPins[21] = '\0';
    }
    uint8_t pw = (uint8_t)disp->getStrWidth(mcpPins);
    disp->drawStr((uint8_t)(SAFE_CX - pw / 2), (uint8_t)(SAFE_Y1 + 54), mcpPins);
  }

  disp->sendBuffer();
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

  // Give the display power-rail time to settle (can take up to 3 s on some
  // boards). Poll in 500 ms steps so we don't wait longer than needed.
  bool found3C = false;
  bool found3D = false;
  for (uint8_t attempt = 0; attempt < 6 && !found3C && !found3D; attempt++)
  {
    delay(500);
    found3C = i2cDevicePresent(0x3C);
    found3D = i2cDevicePresent(0x3D);
    esp_rom_printf("[display] probe attempt %u: 3C=%u 3D=%u\n",
                   attempt, found3C ? 1U : 0U, found3D ? 1U : 0U);
  }

  // Pick the address that actually responded.
  if (found3C)
  {
    statusDisplayAddr = 0x3C;
    blinkLed(1, 220, 140);
  }
  else if (found3D)
  {
    statusDisplayAddr = 0x3D;
    blinkLed(2, 220, 140);
  }
  else
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
    statusDisplaySh1106.begin();
    statusDisplaySh1106.setPowerSave(0);
    statusDisplaySh1106.setContrast(255);
    disp = &statusDisplaySh1106;
    esp_rom_printf("[display] SH1106 begin() done\n");
  }
  else
  {
    esp_rom_printf("[display] calling SSD1306 begin()...\n");
    statusDisplaySsd.begin();
    statusDisplaySsd.setPowerSave(0);
    statusDisplaySsd.setContrast(255);
    disp = &statusDisplaySsd;
    esp_rom_printf("[display] SSD1306 begin() done\n");
  }

  // 3 quick blinks = begin() completed without hanging.
  blinkLed(3, 60, 60);

  statusDisplayReady = true;

  // Calibration complete. Proceed to normal advertising screen.

  statusDrawAdvertising();
  esp_rom_printf("[display] statusInit() complete, display ready\n");
}

static void statusUpdate(bool connected, int16_t x, int16_t y, int16_t rx, int16_t ry, uint8_t hat, uint32_t buttons)
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

  if (connected && (buttons != statusPrevButtons || x != statusPrevX || y != statusPrevY ||
                    rx != statusPrevRx || ry != statusPrevRy))
  {
    statusLastInputChangeMs = millis();
    statusPrevButtons = buttons;
    statusPrevX = x;
    statusPrevY = y;
    statusPrevRx = rx;
    statusPrevRy = ry;
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

  statusDrawConnected(x, y, rx, ry, hat, buttons);
}
#endif

#if defined(ESP32_C3_DIAGNOSTIC_INPUTS)
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
  int16_t mx = mapAxis(rawX, JOY_MIN, JOY_CENTER_X, JOY_MAX, JOY_DEADZONE);
  int16_t my = mapAxis(rawY, JOY_MIN, JOY_CENTER_Y, JOY_MAX, JOY_DEADZONE);
  // Current stick orientation is mirrored on X only.
  x = -mx;
  y = my;
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

static uint8_t hatFromButtons(uint32_t buttons)
{
  // In MCP mode, GPB4..GPB7 are d-pad UP/DOWN/LEFT/RIGHT.
  // We map them to logical button bits 12..15 in readButtons().
  bool up    = ((buttons & (1UL << 12)) != 0);
  bool down  = ((buttons & (1UL << 13)) != 0);
  bool left  = ((buttons & (1UL << 14)) != 0);
  bool right = ((buttons & (1UL << 15)) != 0);

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

  if (USE_MCP23017)
  {
    // Logical button order in MCP mode:
    //  0=A 1=B 2=X 3=Y 4=L 5=R 6=START 7=SELECT
    //  8=HOTKEY 9=COIN 10=L2 11=R2 12=DPAD_UP 13=DPAD_DOWN 14=DPAD_LEFT 15=DPAD_RIGHT
    uint16_t mcp = readMcp23017Inputs();
    for (uint8_t b = 0; b < 16; b++)
    {
      if (((mcp >> b) & 1U) == 0U) // LOW = pressed
      {
        bits |= (1UL << b);
      }
    }

    static uint32_t lastMcpLogMs = 0;
    uint32_t now = millis();
    if ((now - lastMcpLogMs) >= 500)
    {
      lastMcpLogMs = now;
      char pressed[80] = {0};
      mcpPressedSummary(gLastMcpRaw, pressed, sizeof(pressed));
      esp_rom_printf("[mcp] raw=0x%04X bits=0x%04lX err=%lu ptrErr=%d req=%d pressed=%s\n",
                     (unsigned)gLastMcpRaw,
                     (unsigned long)(bits & 0xFFFF),
             (unsigned long)gLastMcpErr,
             gLastMcpPtrErr,
             gLastMcpReqErr,
             pressed);
    }

    return bits;
  }

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

  return bits;
}

static void mcpPressedSummary(uint16_t raw, char *out, size_t outLen)
{
  if (outLen == 0)
  {
    return;
  }

  out[0] = '\0';
  bool any = false;
  for (uint8_t bit = 0; bit < 16; bit++)
  {
    if (((raw >> bit) & 1U) != 0U)
    {
      continue;
    }

    char pinLabel[8];
    if (bit < 8)
    {
      snprintf(pinLabel, sizeof(pinLabel), "A%u", bit);
    }
    else
    {
      snprintf(pinLabel, sizeof(pinLabel), "B%u", (uint8_t)(bit - 8));
    }

    size_t used = strlen(out);
    if (used >= (outLen - 1))
    {
      break;
    }

    snprintf(out + used, outLen - used, "%s%s", any ? "," : "", pinLabel);
    any = true;
  }

  if (!any)
  {
    snprintf(out, outLen, "none");
  }
}

void initMcp23017()
{
  if (!USE_MCP23017)
  {
    esp_rom_printf("[mcp] USE_MCP23017=false, skipping\n");
    return;
  }

  bool present = false;
  for (uint8_t attempt = 0; attempt < 3 && !present; attempt++)
  {
    present = i2cDevicePresent(MCP23017_I2C_ADDR);
    if (!present)
    {
      delay(3);
    }
  }
  esp_rom_printf("[mcp] probe 0x%02X -> %s\n", MCP23017_I2C_ADDR, present ? "FOUND" : "NOT FOUND");
  gMcpReady = false;

  if (!present)
  {
    // Scan full bus so we can see what actually responded.
    esp_rom_printf("[mcp] I2C bus scan (SDA=%u SCL=%u):\n",
                   STATUS_OLED_SDA_PIN, STATUS_OLED_SCL_PIN);
    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
      if (i2cDevicePresent(addr))
      {
        esp_rom_printf("[mcp]   0x%02X responded\n", addr);
        if (addr == MCP23017_I2C_ADDR)
        {
          present = true;
        }
      }
    }
    if (!present)
    {
      return;
    }

    esp_rom_printf("[mcp] using scan fallback for 0x%02X\n", MCP23017_I2C_ADDR);
  }

  // Force sequential mode + unbanked register map.
  bool okIoconA = mcpWriteReg(MCP_REG_IOCONA, 0x00);
  bool okIoconB = mcpWriteReg(MCP_REG_IOCONB, 0x00);
  bool okDirA = mcpWriteReg(MCP_REG_IODIRA, 0xFF);
  bool okDirB = mcpWriteReg(MCP_REG_IODIRB, 0xFF);
  bool okPuA = mcpWriteReg(MCP_REG_GPPUA, 0xFF);
  bool okPuB = mcpWriteReg(MCP_REG_GPPUB, 0xFF);
  int readA = mcpReadReg(MCP_REG_GPIOA);
  int readB = mcpReadReg(MCP_REG_GPIOB);
  bool okRead = (readA >= 0 && readB >= 0);
  gMcpReady = okIoconA && okIoconB && okDirA && okDirB && okPuA && okPuB && okRead;

  esp_rom_printf("[mcp] init ioconA=%u ioconB=%u dirA=%u dirB=%u puA=%u puB=%u read=%u ready=%u\n",
                 okIoconA ? 1U : 0U,
                 okIoconB ? 1U : 0U,
                 okDirA ? 1U : 0U,
                 okDirB ? 1U : 0U,
                 okPuA ? 1U : 0U,
                 okPuB ? 1U : 0U,
                 okRead ? 1U : 0U,
                 gMcpReady ? 1U : 0U);
}

uint16_t readMcp23017Inputs()
{
  if (!USE_MCP23017)
  {
    return 0xFFFF;
  }

  uint32_t now = millis();
  if (!gMcpReady)
  {
    if ((now - gMcpLastInitAttemptMs) >= 1000)
    {
      gMcpLastInitAttemptMs = now;
      initMcp23017();
    }
    return 0xFFFF;
  }

  // Read GPIOA + GPIOB in one transaction to reduce bus overhead.
  Wire.setClock(MCP_I2C_CLOCK_HZ);
  Wire.beginTransmission(MCP23017_I2C_ADDR);
  Wire.write(MCP_REG_GPIOA);
  int ptrErr = Wire.endTransmission(false);
  if (ptrErr != 0)
  {
    gLastMcpErr++;
    gLastMcpPtrErr = ptrErr;
    gMcpReady = false;
    return 0xFFFF;
  }

  int got = Wire.requestFrom((int)MCP23017_I2C_ADDR, 2);
  if (got != 2)
  {
    gLastMcpErr++;
    gLastMcpReqErr = got;
    gMcpReady = false;
    return 0xFFFF;
  }

  uint8_t gpioA = (uint8_t)Wire.read();
  uint8_t gpioB = (uint8_t)Wire.read();
  gLastMcpRaw = (uint16_t)gpioA | ((uint16_t)gpioB << 8);
  return gLastMcpRaw;
}

static void calibrateJoy2Center()
{
  // Learn right-stick neutral at boot to avoid stuck-corner display/input.
  // Keep the stick untouched for ~80ms during boot.
  uint32_t sx = 0;
  uint32_t sy = 0;
  const uint8_t samples = 16;
  for (uint8_t i = 0; i < samples; i++)
  {
    sx += (uint32_t)analogRead(JOY2_X_PIN);
    sy += (uint32_t)analogRead(JOY2_Y_PIN);
    delay(5);
  }
  gJoy2CenterX = (int)(sx / samples);
  gJoy2CenterY = (int)(sy / samples);
  gJoy2FiltX = gJoy2CenterX;
  gJoy2FiltY = gJoy2CenterY;
  esp_rom_printf("[joy2] center x=%d y=%d\n", gJoy2CenterX, gJoy2CenterY);
}

static bool mcpWriteReg(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(MCP23017_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission(true) == 0);
}

static int mcpReadReg(uint8_t reg)
{
  // Keep the bus conservative for mixed-device wiring quality.
  Wire.setClock(MCP_I2C_CLOCK_HZ);

  Wire.beginTransmission(MCP23017_I2C_ADDR);
  Wire.write(reg);
  int ptrErr = Wire.endTransmission(true);
  if (ptrErr != 0)
  {
    gLastMcpPtrErr = ptrErr;
    return -1;
  }

  delayMicroseconds(120);
  int got = Wire.requestFrom((int)MCP23017_I2C_ADDR, 1);
  if (got != 1)
  {
    // One retry after a short settle delay.
    delayMicroseconds(150);
    got = Wire.requestFrom((int)MCP23017_I2C_ADDR, 1);
  }
  if (got != 1)
  {
    gLastMcpReqErr = got;
    return -1;
  }

  return (int)((uint8_t)Wire.read());
}

static int readAnalogMedian5(uint8_t pin)
{
  // Median-of-5 strongly rejects single-sample spikes from shared wiring noise.
  int v[5];
  for (uint8_t i = 0; i < 5; i++)
  {
    v[i] = analogRead(pin);
  }

  for (uint8_t i = 0; i < 4; i++)
  {
    for (uint8_t j = (uint8_t)(i + 1); j < 5; j++)
    {
      if (v[j] < v[i])
      {
        int t = v[i];
        v[i] = v[j];
        v[j] = t;
      }
    }
  }

  return v[2];
}

void setup()
{
  // Give USB-Serial/JTAG time to re-enumerate after reset so host can attach.
  Serial.begin(SERIAL_BAUD_RATE);
  delay(2000);
  esp_rom_printf("\n\n[boot] === BLE Everywhere boot ===\n");
  Serial.println("\n[boot] === BLE Everywhere boot ===");

  pinMode(ESP32_C3_LED_PIN, OUTPUT);
  digitalWrite(ESP32_C3_LED_PIN, HIGH);
#if defined(ESP32_C3_STATUS_OLED)
  blinkLed(1, 80, 80);
#endif

#if !defined(ESP32_C3_DIAGNOSTIC_INPUTS)
  analogReadResolution(12);

  // Use the OLED I2C pins explicitly. Wire.begin() with no args defaults to
  // GPIO8/9 on ESP32-C3, which conflicts with the onboard LED on GPIO8.
  esp_rom_printf("[boot] Wire.begin(SDA=%u, SCL=%u)\n", STATUS_OLED_SDA_PIN, STATUS_OLED_SCL_PIN);
  Wire.begin(STATUS_OLED_SDA_PIN, STATUS_OLED_SCL_PIN);
  Wire.setClock(MCP_I2C_CLOCK_HZ);
  initMcp23017();

  // Reclaim GPIO8 for LED after Wire.begin() in case it was touched.
  pinMode(ESP32_C3_LED_PIN, OUTPUT);
  digitalWrite(ESP32_C3_LED_PIN, HIGH);
#if defined(ESP32_C3_STATUS_OLED)
  blinkLed(2, 60, 60); // checkpoint 2: Wire.begin + MCP done
#endif
  esp_rom_printf("[boot] checkpoint 2: Wire + MCP done\n");

  if (!USE_MCP23017)
  {
    for (size_t i = 0; i < BUTTON_COUNT; i++)
    {
      if (isReservedDisplayPin(BUTTON_PINS[i]))
      {
        esp_rom_printf("[warn] skip button pin %u (reserved for OLED I2C)\n", BUTTON_PINS[i]);
        continue;
      }

      pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }
  }

  pinMode(JOY_X_PIN, INPUT);
  pinMode(JOY_Y_PIN, INPUT);
  pinMode(JOY2_X_PIN, INPUT);
  pinMode(JOY2_Y_PIN, INPUT);
  calibrateJoy2Center();
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
  bleGamepadConfig.setWhichAxes(true, true, true, true, false, false, false, false);
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
  delay(100);
  Serial.println("ESP32_C3_SERIAL_GAMEPAD_V1");
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

    statusUpdate(false, 0, 0, 0, 0, HAT_CENTERED, 0);
    delay(10);
    return;
  }

  int16_t x = 0;
  int16_t y = 0;
  int rawRx = readAnalogMedian5(JOY2_X_PIN);
  int rawRy = readAnalogMedian5(JOY2_Y_PIN);
  // Filter right-stick readings to reject momentary rail noise from button activity.
  gJoy2FiltX = (int)(((int32_t)gJoy2FiltX * 3 + rawRx) / 4);
  gJoy2FiltY = (int)(((int32_t)gJoy2FiltY * 3 + rawRy) / 4);

  // While near neutral, slowly track center drift caused by supply/ground shifts.
  int nearDx = gJoy2FiltX - gJoy2CenterX;
  int nearDy = gJoy2FiltY - gJoy2CenterY;
  if (abs(nearDx) < 260 && abs(nearDy) < 260)
  {
    gJoy2CenterX = (int)(((int32_t)gJoy2CenterX * 63 + gJoy2FiltX) / 64);
    gJoy2CenterY = (int)(((int32_t)gJoy2CenterY * 63 + gJoy2FiltY) / 64);
  }

  const int joy2DeadzoneRuntime = JOY2_DEADZONE + 140;
  int16_t rx = mapAxis(gJoy2FiltX, JOY_MIN, gJoy2CenterX, JOY_MAX, joy2DeadzoneRuntime);
  int16_t ry = mapAxis(gJoy2FiltY, JOY_MIN, gJoy2CenterY, JOY_MAX, joy2DeadzoneRuntime);

  // Keep output at true neutral unless motion is clearly intentional.
  if (abs(rx) < 5200 && abs(ry) < 5200)
  {
    rx = 0;
    ry = 0;
  }
  uint32_t buttons = 0;
  readControllerState(x, y, buttons);
  uint8_t hat = USE_MCP23017 ? hatFromButtons(buttons)
                             : axesToHat(x, JOY_INVERT_Y ? -y : y);

  // On dense handheld wiring, D-pad presses can inject brief noise into JOY2 ADC.
  // Keep right stick at neutral whenever digital d-pad is active.
  if (USE_MCP23017 && MCP_DPAD_LOCKS_RIGHT_STICK && hat != HAT_CENTERED)
  {
    rx = 0;
    ry = 0;
  }

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

  bleGamepad.setAxes(x, y, 0, 0, rx, ry, 0, 0);
  bleGamepad.setHat1(hat);
  statusUpdate(true, x, y, rx, ry, hat, buttons);
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
  if ((now - lastSerialFrameMs) < USB_SERIAL_FRAME_INTERVAL_MS)
  {
    delay(1);
    return;
  }
  lastSerialFrameMs = now;

  int16_t x = 0;
  int16_t y = 0;
  uint32_t buttons = 0;
  readControllerState(x, y, buttons);

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
