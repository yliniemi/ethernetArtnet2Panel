/*
    artnetesp32v2 Demo
    Copyright (C) 2023 Antti Yliniemi
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
I use these libraries:
https://github.com/hpwit/I2SClocklessLedDriver                // I use a heavily modified version of this library. Will be putting a pull request on his github repo after I've done more testing.
https://github.com/yliniemi/I2SClocklessLedDriver/tree/dev    // This is my current heavily modified version. Perhaps the changes end up in the hpwit's version some day
https://github.com/hpwit/artnetesp32v2
*/

#define COLOR_GRB
// #define _LEDMAPPING    // not actually needed
#define __SOFTWARE_MAP
// #define __HARDWARE_MAP

#include "I2SClocklessLedDriver.h"

#include "WiFi.h"
#include <ETH.h>
#include "artnetESP32V2.h"

// extern "C" {
#include "bootloader_random.h"
// }

struct CRGB {
  union {
    struct {
            union {
                uint8_t r;
                uint8_t red;
            };
            union {
                uint8_t g;
                uint8_t green;
            };
            union {
                uint8_t b;
                uint8_t blue;
            };
        };
    uint8_t raw[3];
  };
};

struct FloatOffset
{
  float x;
  float y;
};

#define PANEL_WIDTH 16
#define PANEL_HEIGHT 16
#define NUM_PANELS_PER_ROW 5
#define NUM_PANELS_PER_COLUMN 1
#define SCREEN_WIDTH PANEL_WIDTH * NUM_PANELS_PER_ROW
#define SCREEN_HEIGHT PANEL_HEIGHT * NUM_PANELS_PER_COLUMN
#define NB_CHANNEL_PER_LED 3
#define UNIVERSE_SIZE_IN_CHANNEL (170 * 3)
#define START_UNIVERSE 0

static bool eth_connected = false;

int maxCurrent = 15000;

const char * ssid = "wrt54gsv4";
const char * password = "batmanjarobinonmaailmanpaskinelokuva";

// CRGB* leds;

I2SClocklessLedDriver driver;
artnetESP32V2 artnet=artnetESP32V2();

__attribute__((always_inline)) IRAM_ATTR static int mapLed(int hardwareLed)
{
  int x, y;
  // = MOD(FLOOR(A11 / 4), 12)
  x = (hardwareLed / PANEL_HEIGHT) % (PANEL_WIDTH * NUM_PANELS_PER_ROW);
  // = MOD(MOD(FLOOR(A11 / 4) + 1, 2) * MOD(A11, 4) + MOD(FLOOR(A11 / 4), 2) * MOD(95 - A11, 4) + FLOOR(A11 / 48) * 4, 96)
  y = (hardwareLed / PANEL_HEIGHT) % 2;
  if (y == 0)
  {
    y = (hardwareLed % PANEL_HEIGHT) + (hardwareLed / (PANEL_WIDTH * PANEL_HEIGHT * NUM_PANELS_PER_ROW)) * PANEL_HEIGHT;
    // y = ((hardwareLed % PANEL_HEIGHT) + (hardwareLed / (PANEL_WIDTH * NUM_PANELS_PER_ROW)) * PANEL_HEIGHT) % (PANEL_WIDTH * NUM_PANELS_PER_ROW * NUM_PANELS_PER_COLUMN);
  }
  else
  {
    y = ((PANEL_HEIGHT * 12345 - 1 - hardwareLed) % PANEL_HEIGHT) + (hardwareLed / (PANEL_WIDTH * PANEL_HEIGHT * NUM_PANELS_PER_ROW)) * PANEL_HEIGHT;
    // y = (((PANEL_HEIGHT * 12345 - 1 - hardwareLed) % PANEL_HEIGHT) + (hardwareLed / (PANEL_WIDTH * NUM_PANELS_PER_ROW)) * PANEL_HEIGHT) % (PANEL_WIDTH * NUM_PANELS_PER_ROW * NUM_PANELS_PER_COLUMN);
  }
  // y = (((hardwareLed / 16 + 1) % 2) * (hardwareLed % 16) + ((hardwareLed / 16) % 2) * ((16 * 123 - 1 - hardwareLed) % 16) + (hardwareLed / (16 * 7)) * 16 ) % (16 * 7 * 2);
  x = (x + driver._offsetDisplay.offsetx) % driver._offsetDisplay.panel_width;
  y = (y + driver._offsetDisplay.offsety) % driver._offsetDisplay.panel_height;
  return x + y * driver._offsetDisplay.panel_width;
}

/*
__attribute__((always_inline)) IRAM_ATTR static int mapLedRotate(int hardwareLed)
{
  int x, y;
  // = MOD(FLOOR(A11 / 4), 12)
  x = (hardwareLed / PANEL_HEIGHT) % (PANEL_WIDTH * NUM_PANELS_PER_ROW);
  // = MOD(MOD(FLOOR(A11 / 4) + 1, 2) * MOD(A11, 4) + MOD(FLOOR(A11 / 4), 2) * MOD(95 - A11, 4) + FLOOR(A11 / 48) * 4, 96)
  y = (hardwareLed / PANEL_HEIGHT) % 2;
  if (y == 0)
  {
    y = (hardwareLed % PANEL_HEIGHT) + (hardwareLed / (PANEL_WIDTH * PANEL_HEIGHT * NUM_PANELS_PER_ROW)) * PANEL_HEIGHT;
    // y = ((hardwareLed % PANEL_HEIGHT) + (hardwareLed / (PANEL_WIDTH * NUM_PANELS_PER_ROW)) * PANEL_HEIGHT) % (PANEL_WIDTH * NUM_PANELS_PER_ROW * NUM_PANELS_PER_COLUMN);
  }
  else
  {
    y = ((PANEL_HEIGHT * 12345 - 1 - hardwareLed) % PANEL_HEIGHT) + (hardwareLed / (PANEL_WIDTH * PANEL_HEIGHT * NUM_PANELS_PER_ROW)) * PANEL_HEIGHT;
    // y = (((PANEL_HEIGHT * 12345 - 1 - hardwareLed) % PANEL_HEIGHT) + (hardwareLed / (PANEL_WIDTH * NUM_PANELS_PER_ROW)) * PANEL_HEIGHT) % (PANEL_WIDTH * NUM_PANELS_PER_ROW * NUM_PANELS_PER_COLUMN);
  }
  // y = (((hardwareLed / 16 + 1) % 2) * (hardwareLed % 16) + ((hardwareLed / 16) % 2) * ((16 * 123 - 1 - hardwareLed) % 16) + (hardwareLed / (16 * 7)) * 16 ) % (16 * 7 * 2);
  // x = (x + driver._offsetDisplay.offsetx) % driver._offsetDisplay.panel_width;
  // y = (y + driver._offsetDisplay.offsety) % driver._offsetDisplay.panel_height;
  // return x + y * driver._offsetDisplay.panel_width;
  // X = cx + (x-cx)*cosA - (y-cy)*sinA
  // Y = cy + (x-cx)*sinA + (y-cy)*cosA  int newX = (driver._offsetDisplay.offsetx + (x - driver._offsetDisplay.offsetx) * cosA - (y - driver._offsetDisplay.offsety) * sinA) % driver._offsetDisplay.panel_width;
  int newY = (driver._offsetDisplay.offsety + (x - driver._offsetDisplay.offsetx) * sinA + (y - driver._offsetDisplay.offsety) * cosA) % driver._offsetDisplay.panel_height;
}
*/

float sqrtApprox(float number)
{
  union { float f; uint32_t u; } y = {number};
  y.u = 0x5F1FFFF9ul - (y.u >> 1);
  return number * 0.703952253f * y.f * (2.38924456f - number * y.f * y.f);
}

void displayfunction(subArtnet *subartnet)
{
  driver.showPixels(NO_WAIT, subartnet->data);
}

void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}

void setupArtnet()
{
  /*
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int consecutiveTries = 0;
  while(1)
  {
    if (consecutiveTries > 30) ESP.restart();
    delay(1000);
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
      Serial.println("WiFi Succeeded");
      delay(100);
      break;
    }
    else
    {
      Serial.printf("WiFi didn't succeed yet. %d/%d tries.\n", consecutiveTries, 30);
    }
    consecutiveTries++;
  }
  */
  
  WiFi.onEvent(WiFiEvent);
  ETH.begin();
  // Serial.println(ETH_DMA_BUFFER_SIZE);
  //esp_eth_ioctl(eth_handle, ETH_CMD_S_DUPLEX_MODE, &new_duplex_mode);
  
  delay(1000);
  artnet.addSubArtnet(START_UNIVERSE, 85 * 168 * NB_CHANNEL_PER_LED, UNIVERSE_SIZE_IN_CHANNEL, &displayfunction);
  // artnet.addSubArtnet(START_UNIVERSE, SCREEN_WIDTH * SCREEN_HEIGHT * NB_CHANNEL_PER_LED, UNIVERSE_SIZE_IN_CHANNEL, &displayfunction);

  if(artnet.listen(6454))
  {
    Serial.print("artnet Listening on IP: ");
    Serial.println(WiFi.localIP());
  }

}

void setup()
{
  Serial.begin(115200);
  
  Serial.println("Just booted up");
  Serial.print("ESP.getFreeHeap() = ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) = ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.print("heap_caps_get_largest_free_block(MALLOC_CAP_32BIT) = ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
  delay(100);
  
  Serial.printf("SCREEN_WIDTH = %d\n", SCREEN_WIDTH);
  Serial.printf("SCREEN_HEIGHT = %d\n", SCREEN_HEIGHT);
    
  int pins[] = {2, 4, 12, 14, 15};              // esp32 dev kit v1
  #ifndef WAIT_UNTIL_DRAWING_DONE
  driver.__displayMode = NO_WAIT;
  #endif
  // leds = (CRGB*)calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(CRGB));
  driver.setMapLed(mapLed);
  driver._offsetDisplay.panel_width = 85;
  driver._offsetDisplay.panel_height = 85;
  // driver._offsetDisplay.panel_width = SCREEN_WIDTH;
  // driver._offsetDisplay.panel_height = SCREEN_HEIGHT;
  driver._defaultOffsetDisplay.panel_height = driver._offsetDisplay.panel_height;
    
  Serial.println();
  Serial.println("const uint16_t ledMappingCoordinates[] PROGMEM =");
  Serial.print("{");
  for (int ledNumber = 0; ledNumber < SCREEN_WIDTH * SCREEN_HEIGHT; ledNumber++)
  {
    if (ledNumber % (PANEL_WIDTH * PANEL_HEIGHT) == 0) Serial.println();
    Serial.print(driver.mapLed(ledNumber));
    if (ledNumber != SCREEN_WIDTH * SCREEN_HEIGHT - 1) Serial.print(", ");
  }
  Serial.println();
  Serial.println("};");
  Serial.println();

  uint8_t *leds = (uint8_t*)calloc((NUM_PANELS_PER_ROW * PANEL_WIDTH + 5) * NUM_PANELS_PER_COLUMN * PANEL_HEIGHT * 3, sizeof(uint8_t));
  driver.initled(leds, pins, NUM_PANELS_PER_ROW * NUM_PANELS_PER_COLUMN, PANEL_WIDTH * PANEL_HEIGHT, ORDER_GRB);
  
  // This is not cool. These settings are nuked by initled()
  driver._offsetDisplay.panel_width = 85;
  driver._offsetDisplay.panel_height = 85;
  // driver._offsetDisplay.panel_width = SCREEN_WIDTH;
  // driver._offsetDisplay.panel_height = SCREEN_HEIGHT;
  driver._defaultOffsetDisplay.panel_height = driver._offsetDisplay.panel_height;
    
  Serial.printf("driver._offsetDisplay.panel_width = %d\n", driver._offsetDisplay.panel_width);
  Serial.printf("driver._offsetDisplay.panel_height = %d\n", driver._offsetDisplay.panel_height);

  uint32_t index = 2;
  //
  // bootloader_random_enable();
  // delay(100);
  index = esp_random() % 3;
  // delay(100);
  // bootloader_random_disable();
  
  for (uint32_t i = 0; i < (NUM_PANELS_PER_ROW * PANEL_WIDTH + 5) * NUM_PANELS_PER_COLUMN * PANEL_HEIGHT; i++)
  {
    leds[i * 3 + index] = 1;
  }
  delay(10);
  driver.showPixels(NO_WAIT);
  delay(200);
  free(leds);
  delay(10);
  
  setupArtnet();
  
  delay(1000);
  Serial.print("ESP.getFreeHeap() = ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) = ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.print("heap_caps_get_largest_free_block(MALLOC_CAP_32BIT) = ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));
  
  // pinMode(13, OUTPUT);
  // digitalWrite(13, LOW);
  // gpio_set_direction((gpio_num_t)13, GPIO_MODE_OUTPUT);
  // gpio_set_level((gpio_num_t)13, 0);

}

void loop()
{
  // vTaskDelete(NULL);
  
  delay(60000);
  Serial.print("ESP.getFreeHeap() = ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) = ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.print("heap_caps_get_largest_free_block(MALLOC_CAP_32BIT) = ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_32BIT));

}
