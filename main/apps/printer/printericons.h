/*
MIT License

Copyright (c) 2024 Sukesh Ashok Kumar

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

/*
    FontAwesome Icons for 3D Printer Status Display
    Suitable for Bambu Lab Monitor and similar 3D printer integrations
    
    Generated font: font_fa_printer_42.c (42px, 4bpp)
    Icons included: play, pause, stop, check, warning, clock, thermometer, 
                    fire, sun, cloud, wifi, hourglass (start/half/end)
*/

#ifndef PRINTERICONS_H
#define PRINTERICONS_H

#include "lvgl.h"

// Declare the printer icon font (42px)
LV_FONT_DECLARE(font_fa_printer_42)

// Printer Status State Icons (Font Awesome 5)
#define FA_PRINTER_PLAY         "\xEF\x81\x8B"      // f04b - Play (Printing)
#define FA_PRINTER_PAUSE        "\xEF\x81\x8C"      // f04c - Pause (Paused)
#define FA_PRINTER_STOP         "\xEF\x81\x8D"      // f04d - Stop (Idle/Stopped)
#define FA_PRINTER_CHECK        "\xEF\x80\x8C"      // f00c - Check (Complete/Ready)
#define FA_PRINTER_WARNING      "\xEF\x81\xB1"      // f071 - Warning Triangle (Error)

// Temperature & Heating Icons
#define FA_PRINTER_THERMOMETER  "\xEF\x8B\x89"      // f2c9 - Thermometer Half
#define FA_PRINTER_FIRE         "\xEF\x81\xAD"      // f06d - Fire (Heating)
#define FA_PRINTER_SUN          "\xEF\x86\x85"      // f185 - Sun (Hot/Warm)

// Time & Progress Icons
#define FA_PRINTER_CLOCK        "\xEF\x80\x97"      // f017 - Clock (Time remaining)
#define FA_PRINTER_HOURGLASS_S  "\xEF\x89\x91"      // f251 - Hourglass Start
#define FA_PRINTER_HOURGLASS    "\xEF\x89\x92"      // f252 - Hourglass Half
#define FA_PRINTER_HOURGLASS_E  "\xEF\x89\x93"      // f253 - Hourglass End

// Connection & Environment Icons
#define FA_PRINTER_WIFI         "\xEF\x87\xAB"      // f1eb - WiFi Signal
#define FA_PRINTER_CLOUD        "\xEF\x83\x82"      // f0c2 - Cloud (Online)

// Color definitions for printer states (LVGL hex colors)
#define COLOR_PRINTER_IDLE      0x888888    // Gray
#define COLOR_PRINTER_PRINTING  0x00CC00    // Green
#define COLOR_PRINTER_PAUSED    0xFFAA00    // Orange/Amber
#define COLOR_PRINTER_ERROR     0xFF3333    // Red
#define COLOR_PRINTER_COMPLETE  0x00AAFF    // Blue
#define COLOR_PRINTER_HEATING   0xFF6600    // Orange
#define COLOR_PRINTER_COOLING   0x66CCFF    // Light Blue
#define COLOR_PRINTER_OFFLINE   0x444444    // Dark Gray

// Background colors for printer carousel slides
#define COLOR_BG_PRINTER        0x3a1e2f    // Purple (default printer theme)
#define COLOR_BG_IDLE           0x2a2a2a    // Dark gray
#define COLOR_BG_PRINTING       0x1a3a1a    // Dark green
#define COLOR_BG_PAUSED         0x3a3a1a    // Dark yellow/amber
#define COLOR_BG_ERROR          0x3a1a1a    // Dark red
#define COLOR_BG_COMPLETE       0x1a2a3a    // Dark blue

#endif // PRINTERICONS_H
