#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

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
