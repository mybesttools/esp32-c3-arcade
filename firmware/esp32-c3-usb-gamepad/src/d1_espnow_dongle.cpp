/**
 * ESP8266 D1 – ESP-NOW gamepad dongle
 *
 * Receives 3-byte gamepad frames from the ESP32-C3 over ESP-NOW and
 * forwards them over USB serial to the Raspberry Pi bridge script.
 *
 * Frame format (sent by mpy/boot.py):
 *   Byte 0 : hat  (0-7 = N/NE/E/SE/S/SW/W/NW, 0xFF = center)
 *   Byte 1 : buttons low byte  (bits 0-7:  A B X Y L R START SELECT)
 *   Byte 2 : buttons high byte (bits 0-1:  HOTKEY COIN)
 *
 * Serial output line (parsed by tools/pi_espnow_to_uinput.py):
 *   GP,<hat>,<buttons_decimal>\n
 *
 * Build with:
 *   pio run -e d1_dongle -t upload
 *
 * First boot: open serial monitor to find the D1's MAC address, then
 * update DONGLE_MAC in mpy/boot.py on the ESP32-C3.
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

static volatile bool     s_frameReady = false;
static volatile uint8_t  s_hat        = 0xFF;
static volatile uint16_t s_buttons    = 0;

static void onReceive(uint8_t *mac, uint8_t *data, uint8_t len)
{
  if (len < 3) return;
  s_hat     = data[0];
  s_buttons = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
  s_frameReady = true;
}

void setup()
{
  Serial.begin(115200);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Print MAC so you can copy it into mpy/boot.py DONGLE_MAC
  Serial.print("D1 MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != 0)
  {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceive);

  Serial.println("ESP-NOW dongle ready");
}

void loop()
{
  if (s_frameReady)
  {
    s_frameReady = false;
    Serial.print("GP,");
    Serial.print(s_hat);
    Serial.print(",");
    Serial.println(s_buttons);
  }
}
