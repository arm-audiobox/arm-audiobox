#pragma once
#include <Arduino.h>
#include <SPI.h>

// Select the controller through the PlatformIO environment.
#if defined(PLAYER_DISPLAY_ST7789) == defined(PLAYER_DISPLAY_ILI9341)
#error "Select exactly one display controller in platformio.ini"
#endif

namespace display_config {
#if defined(PLAYER_DISPLAY_WAVESHARE_C6_147)
constexpr int SCLK=7, MOSI=6, MISO=5, CS=14, DC=15, RESET=21, BACKLIGHT=22;
constexpr uint16_t ST7789_WIDTH=172, ST7789_HEIGHT=320;
constexpr uint32_t SPI_HZ=12000000;
// Waveshare recommends no more than 50% physical backlight duty.
constexpr uint8_t BACKLIGHT_MAX_DUTY=127;
constexpr const char *MODEL="Waveshare ESP32-C6 1.47";
#else
constexpr int SCLK=18, MOSI=23, MISO=19, CS=15, DC=2, RESET=4, BACKLIGHT=32;
constexpr uint16_t ST7789_WIDTH=170, ST7789_HEIGHT=320;
constexpr uint32_t SPI_HZ=20000000;
constexpr uint8_t BACKLIGHT_MAX_DUTY=255;
constexpr const char *MODEL="ESP32-WROOM-32 1.96\xe2\x80\xb3";
#endif
constexpr int BACKLIGHT_ON=HIGH;
constexpr uint8_t ROTATION=1;
constexpr uint8_t SPI_MODE=SPI_MODE0;
constexpr bool INVERT=true;
constexpr uint16_t FOREGROUND = 0xFFFF;
constexpr uint16_t BACKGROUND = 0x0000;
constexpr int UI_SCALE = 2; // 128x64 UI becomes 256x128, centered on TFT.
}
