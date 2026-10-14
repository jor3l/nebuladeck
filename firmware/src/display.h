#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#if defined(BOARD_SC01_PLUS)
// Portrait with the USB port at the bottom.
static constexpr int DISPLAY_ROTATION = 2;
// Panlee / Wireless-Tag SC01 Plus: ST7796 on 8-bit parallel, FT6336 touch on I2C.
class Display : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 panel_;
  lgfx::Bus_Parallel8 bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_FT5x06 touch_;

 public:
  Display() {
    {
      auto c = bus_.config();
      c.freq_write = 40000000;  // was 20 MHz; drop back if the image shows noise
      c.pin_wr = 47;
      c.pin_rd = -1;
      c.pin_rs = 0;
      c.pin_d0 = 9;  c.pin_d1 = 46; c.pin_d2 = 3;  c.pin_d3 = 8;
      c.pin_d4 = 18; c.pin_d5 = 17; c.pin_d6 = 16; c.pin_d7 = 15;
      bus_.config(c);
      panel_.setBus(&bus_);
    }
    {
      auto c = panel_.config();
      c.pin_cs = -1;
      c.pin_rst = 4;
      c.pin_busy = -1;
      c.panel_width = 320;
      c.panel_height = 480;
      c.invert = true;
      c.rgb_order = false;
      c.bus_shared = false;
      panel_.config(c);
    }
    {
      auto c = light_.config();
      c.pin_bl = 45;
      c.invert = false;
      c.freq = 44100;
      c.pwm_channel = 7;
      light_.config(c);
      panel_.setLight(&light_);
    }
    {
      auto c = touch_.config();
      c.i2c_port = 1;
      c.i2c_addr = 0x38;
      c.pin_sda = 6;
      c.pin_scl = 5;
      c.pin_int = 7;
      c.freq = 400000;
      c.x_min = 0; c.x_max = 319;
      c.y_min = 0; c.y_max = 479;
      touch_.config(c);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }
};

#elif defined(BOARD_SC01)
// Portrait. Not verified on hardware yet: if the screen is upside down, use 0.
static constexpr int DISPLAY_ROTATION = 2;
// Panlee / Wireless-Tag SC01 (WT32-SC01): ST7796 on SPI (HSPI), FT6336 touch on I2C.
// Pins as in LovyanGFX's WT32-SC01 autodetect.
class Display : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_FT5x06 touch_;

 public:
  Display() {
    {
      auto c = bus_.config();
      c.spi_host = HSPI_HOST;
      c.spi_mode = 0;
      c.freq_write = 40000000;
      c.freq_read = 16000000;
      c.spi_3wire = true;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 14;
      c.pin_mosi = 13;
      c.pin_miso = -1;
      c.pin_dc = 21;
      bus_.config(c);
      panel_.setBus(&bus_);
    }
    {
      auto c = panel_.config();
      c.pin_cs = 15;
      c.pin_rst = 22;
      c.pin_busy = -1;
      c.panel_width = 320;
      c.panel_height = 480;
      c.readable = false;
      c.invert = false;
      c.rgb_order = false;
      c.bus_shared = false;
      panel_.config(c);
    }
    {
      auto c = light_.config();
      c.pin_bl = 23;
      c.invert = false;
      c.freq = 44100;
      c.pwm_channel = 7;
      light_.config(c);
      panel_.setLight(&light_);
    }
    {
      auto c = touch_.config();
      c.i2c_port = 1;
      c.i2c_addr = 0x38;
      c.pin_sda = 18;
      c.pin_scl = 19;
      c.pin_int = 39;
      c.freq = 400000;
      c.x_min = 0; c.x_max = 319;
      c.y_min = 0; c.y_max = 479;
      touch_.config(c);
      panel_.setTouch(&touch_);
    }
    setPanel(&panel_);
  }
};

#else
#error "Pick a board: build with -DBOARD_SC01_PLUS or -DBOARD_SC01 (see platformio.ini)"
#endif
