#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
namespace Pins {
constexpr int TFT_MISO = 12;
constexpr int TFT_MOSI = 13;
constexpr int TFT_SCLK = 14;
constexpr int TFT_CS = 15;
constexpr int TFT_DC = 2;
constexpr int TFT_BL = 21;

constexpr int TOUCH_CLK = 25;
constexpr int TOUCH_CS = 33;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_IRQ = 36;

constexpr int SPEAKER = 26;
}  // namespace Pins

class CydDisplay : public lgfx::LGFX_Device {
 private:
  lgfx::Bus_SPI bus_;
#if defined(CYD_ILI9341)
  lgfx::Panel_ILI9341 panel_;
#else
  lgfx::Panel_ST7789 panel_;
#endif
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;

 public:
  CydDisplay() {
    {
      auto cfg = bus_.config();
      cfg.spi_host = HSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk = Pins::TFT_SCLK;
      cfg.pin_mosi = Pins::TFT_MOSI;
      cfg.pin_miso = Pins::TFT_MISO;
      cfg.pin_dc = Pins::TFT_DC;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = Pins::TFT_CS;
      cfg.pin_rst = -1;  // TFT-Reset liegt am ESP32-EN-Signal.
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    {
      auto cfg = light_.config();
      cfg.pin_bl = Pins::TFT_BL;
      cfg.invert = false;
      cfg.freq = 5000;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    {
      auto cfg = touch_.config();
      // Startwerte fuer XPT2046; Feinabgleich ueber Touch-Kalibrierung.
      cfg.x_min = 200;
      cfg.x_max = 3800;
      cfg.y_min = 240;
      cfg.y_max = 3800;
      cfg.pin_int = -1;  // Poll XPT2046 directly; do not gate reads on IRQ.
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.spi_host = VSPI_HOST;
      cfg.freq = 1000000;
      cfg.pin_sclk = Pins::TOUCH_CLK;
      cfg.pin_mosi = Pins::TOUCH_MOSI;
      cfg.pin_miso = Pins::TOUCH_MISO;
      cfg.pin_cs = Pins::TOUCH_CS;
      touch_.config(cfg);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }
};

