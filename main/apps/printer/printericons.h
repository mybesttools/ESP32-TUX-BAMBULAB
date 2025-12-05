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
*/

LV_FONT_DECLARE(font_fa_14)

// Printer Status State Icons
#define FA_PRINTER_PLAY           "\xEF\x8C\xA0"      // f330 - Printing/Running
#define FA_PRINTER_PAUSE          "\xEF\x88\x9E"      // f21e - Paused
#define FA_PRINTER_STOP           "\xEF\x8C\xA1"      // f331 - Stopped/Idle
#define FA_PRINTER_CHECK          "\xEF\x8A\x93"      // f293 - Ready/Success
#define FA_PRINTER_ERROR          "\xEF\x8A\x9B"      // f29b - Error/Problem
#define FA_PRINTER_WARNING        "\xEF\x8A\x9A"      // f29a - Warning/Alert
#define FA_PRINTER_INFO           "\xEF\x8A\x9C"      // f29c - Information

// Printer Component Icons
#define FA_PRINTER_CUBE           "\xEF\x97\xB3"      // f5f3 - Model/Job/3D Object
#define FA_PRINTER_TEMP           "\xEF\x8A\xA1"      // f2a1 - Temperature/Heat
#define FA_PRINTER_SPEED          "\xEF\x8A\xA0"      // f2a0 - Speed/Fan/Cooling
#define FA_PRINTER_NOZZLE         "\xEF\x83\xB3"      // f0f3 - Nozzle/Hotend
#define FA_PRINTER_BED            "\xEF\x84\x80"      // f100 - Bed/Platform

// Material & Progress Icons
#define FA_PRINTER_FILAMENT       "\xEF\x86\x84"      // f184 - Filament/Material
#define FA_PRINTER_TIME           "\xEF\x84\xB3"      // f133 - Time Remaining/ETA
#define FA_PRINTER_HOURGLASS      "\xEF\x8B\x9E"      // f2de - Duration/Elapsed Time
#define FA_PRINTER_FILE           "\xEF\x84\x90"      // f110 - File/Document/Project

// Connection & Control Icons
#define FA_PRINTER_PLUG           "\xEF\x86\xAF"      // f1af - Connected/Power
#define FA_PRINTER_COGS           "\xEF\x87\xA3"      // f1e3 - Settings/Config
#define FA_PRINTER_POWER          "\xEF\x83\x9F"      // f0df - Power On/Off

#endif // PRINTERICONS_H
