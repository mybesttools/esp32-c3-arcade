#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "pinmap.h"

// HID report descriptor: 12 buttons + X/Y joystick axes.
static const uint8_t hidReportDescriptor[] = {
  TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(1))
};

Adafruit_USBD_HID usbHid(
  hidReportDescriptor,
  sizeof(hidReportDescriptor),
  HID_ITF_PROTOCOL_NONE,
  2,
  false
);

static hid_gamepad_report_t report = {0};

// Convert analog joystick reading to 0-255 with center deadzone.
uint8_t mapAxis(int raw, int minV, int centerV, int maxV, int deadzone)
{
  if (raw < centerV - deadzone)
  {
    long v = map(raw, minV, centerV - deadzone, 0, 127);
    return (uint8_t)constrain(v, 0, 127);
  }

  if (raw > centerV + deadzone)
  {
    long v = map(raw, centerV + deadzone, maxV, 128, 255);
    return (uint8_t)constrain(v, 128, 255);
  }

  return 128;
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
  for (size_t i = 0; i < BUTTON_COUNT; i++)
  {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

  pinMode(JOY_X_PIN, INPUT);
  pinMode(JOY_Y_PIN, INPUT);

  usbHid.setPollInterval(2);
  usbHid.begin();

  TinyUSBDevice.setManufacturerDescriptor(USB_MANUFACTURER);
  TinyUSBDevice.setProductDescriptor(USB_PRODUCT);

  while (!TinyUSBDevice.mounted())
  {
    delay(10);
  }
}

void loop()
{
  int rawX = analogRead(JOY_X_PIN);
  int rawY = analogRead(JOY_Y_PIN);

  report.x = (int8_t)((int)mapAxis(rawX, JOY_MIN, JOY_CENTER_X, JOY_MAX, JOY_DEADZONE) - 128);
  report.y = (int8_t)((int)mapAxis(rawY, JOY_MIN, JOY_CENTER_Y, JOY_MAX, JOY_DEADZONE) - 128);

  // Keep unused axes centered.
  report.z = 0;
  report.rz = 0;
  report.rx = 0;
  report.ry = 0;

  // Optional D-pad hat (0-7) or neutral (0x0F). Keep neutral for now.
  report.hat = 0x0F;

  report.buttons = readButtons();

  if (usbHid.ready())
  {
    usbHid.sendReport(1, &report, sizeof(report));
  }

  delay(2);
}
