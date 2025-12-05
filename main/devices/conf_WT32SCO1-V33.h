/*
MIT License

Copyright (c) 2022 Sukesh Ashok Kumar

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <driver/i2c.h>

// WT32-SC01 v3.3 Device Configuration
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7796  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Touch_FT5x06  _touch_instance;
  lgfx::Light_PWM     _light_instance;

public:
  LGFX(void)
  {
    // Display SPI Bus Configuration
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read  = 40000000;
      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = -1;
      cfg.pin_dc   = 21;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // Display Panel Configuration (ST7796)
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs      = 15;
      cfg.pin_rst     = 22;
      cfg.pin_busy    = -1;
      cfg.panel_width   = 320;  // ST7796 native width
      cfg.panel_height  = 480;  // ST7796 native height
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable        = true;
      cfg.invert          = false;
      cfg.rgb_order       = false;
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = false;
      _panel_instance.config(cfg);
    }

    // Backlight PWM Configuration
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 23;
      cfg.invert = false;
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    // Touch Panel Configuration (FT5x06 I2C)
    {
      auto cfg = _touch_instance.config();
      cfg.i2c_port = I2C_NUM_1;
      cfg.i2c_addr = 0x38;
      cfg.pin_sda  = 18;
      cfg.pin_scl  = 19;
      cfg.pin_int  = 39;
      cfg.pin_rst  = -1;
      cfg.freq     = 400000;
      cfg.x_min    = 0;
      cfg.x_max    = 319;  // FT5x06 reports in native 320 width
      cfg.y_min    = 0;
      cfg.y_max    = 479;  // FT5x06 reports in native 480 height
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

// SD Card not available on WT32-SC01-v3.3
#define SUPPORT_SDCARD 0
