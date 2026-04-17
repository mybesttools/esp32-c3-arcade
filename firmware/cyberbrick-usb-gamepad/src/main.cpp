#include <Arduino.h>
#include "pinmap.h"

#if defined(CONTROLLER_MODE_BLE)
#include <BleGamepad.h>
BleGamepad bleGamepad("CyberBrick Arcade", "CyberBrick", 100);
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

  for (size_t i = 0; i < BUTTON_COUNT; i++)
  {
    bool pressed = (digitalRead(BUTTON_PINS[i]) == LOW);
    if (pressed)
    {
      bits |= (1UL << i);
    }
  }

  return bits;
}

void setup()
{
  analogReadResolution(12);

  for (size_t i = 0; i < BUTTON_COUNT; i++)
  {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

  pinMode(JOY_X_PIN, INPUT);
  pinMode(JOY_Y_PIN, INPUT);

#if defined(CONTROLLER_MODE_BLE)
  bleGamepad.begin();
#else
  Serial.begin(SERIAL_BAUD_RATE);
  unsigned long start = millis();
  while (!Serial && (millis() - start < 2500))
  {
    delay(10);
  }
  Serial.println("CYBERBRICK_SERIAL_GAMEPAD_V1");
#endif
}

void loop()
{
  int rawX = analogRead(JOY_X_PIN);
  int rawY = analogRead(JOY_Y_PIN);

#if defined(CONTROLLER_MODE_BLE)
  if (!bleGamepad.isConnected())
  {
    delay(10);
    return;
  }

  uint32_t buttons = readButtons();
  for (size_t i = 0; i < BUTTON_COUNT; i++)
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

  bleGamepad.setAxes(
    mapAxis(rawX, JOY_MIN, JOY_CENTER_X, JOY_MAX, JOY_DEADZONE),
    mapAxis(rawY, JOY_MIN, JOY_CENTER_Y, JOY_MAX, JOY_DEADZONE),
    0,
    0,
    0,
    0,
    0,
    0
  );
#else
  int16_t x = mapAxis(rawX, JOY_MIN, JOY_CENTER_X, JOY_MAX, JOY_DEADZONE);
  int16_t y = mapAxis(rawY, JOY_MIN, JOY_CENTER_Y, JOY_MAX, JOY_DEADZONE);
  uint32_t buttons = readButtons();

  // CSV frame for Pi bridge: R,<x>,<y>,<buttons_hex>
  Serial.print("R,");
  Serial.print(x);
  Serial.print(',');
  Serial.print(y);
  Serial.print(',');
  Serial.println(buttons, HEX);
#endif

  delay(2);
}
