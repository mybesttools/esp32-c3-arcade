#pragma once

// USB strings shown on the Raspberry Pi.
static const char *USB_MANUFACTURER = "CyberBrick Arcade";
static const char *USB_PRODUCT = "CyberBrick C3 Controller";

// Wired USB mode on ESP32-C3 uses USB CDC serial + Pi-side uinput bridge.
static const unsigned long SERIAL_BAUD_RATE = 115200;

// Analog joystick pins (update to your shield wiring).
static const uint8_t JOY_X_PIN = A0;
static const uint8_t JOY_Y_PIN = A1;

// Calibrate these after your first test run.
static const int JOY_MIN = 0;
static const int JOY_MAX = 4095;
static const int JOY_CENTER_X = 2048;
static const int JOY_CENTER_Y = 2048;
static const int JOY_DEADZONE = 35;

// Buttons are active LOW (wired to GND when pressed).
static const uint8_t BUTTON_PINS[] = {
  2,  // B1
  3,  // B2
  4,  // B3
  5,  // B4
  6,  // L1
  7,  // R1
  8,  // START
  9,  // SELECT
  10, // HOTKEY
  11  // COIN
};

static const size_t BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);
