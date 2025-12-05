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

/*
Config Json format:
{
    "devicename": "yow",
    "settings": {
        "brightness":128,       // 0-255
        "theme":"dark",         // dark/light ???
        "timezone":"+5:30",     // IST
    }
}
*/

#ifndef TUX_SETTINGSCONFIG_H_
#define TUX_SETTINGSCONFIG_H_

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <inttypes.h>
#include <esp_log.h>
#include <fstream>
#include <cJSON.h>

using namespace std;

typedef enum
{
    WEATHER_UNITS_KELVIN,
    WEATHER_UNITS_CELSIUS,
    WEATHER_UNITS_FAHRENHEIT
} weather_units_t;

typedef enum{
    UNITS_METRICS,
    UNITS_IMPERIAL
} measurement_units_t;

// Maximum number of printers that can be configured
#define MAX_PRINTERS 5

// Maximum number of weather locations that can be configured
#define MAX_WEATHER_LOCATIONS 5

// Printer configuration structure
typedef struct {
    string name;
    string ip_address;
    string token;
    string serial;  // Printer serial number from MQTT
    bool enabled;
} printer_config_t;

// Weather location configuration structure
typedef struct {
    string name;           // Display name (e.g., "Home", "Office")
    string city;           // City name
    string country;        // Country name
    float latitude;        // Latitude for OpenWeather API
    float longitude;       // Longitude for OpenWeather API
    bool enabled;
} weather_location_t;

// Network configuration for printer discovery (subnet ranges to scan)
typedef struct {
    string name;           // Network name (e.g., "Guest Network", "Office")
    string subnet;         // CIDR notation (e.g., "192.168.1.0/24")
    bool enabled;
} network_config_t;

#define MAX_NETWORKS 3  // Maximum number of networks to scan

class SettingsConfig
{
    public:
        string DeviceName;
        uint8_t Brightness;        // 0-255
        string TimeZone;           // +5:30
        string CurrentTheme;       // dark / light
        bool HasBattery;           // Device has battery
        bool HasBluetooth;         // Device has Bluetooth

        string WeatherProvider;     // OpenWeatherMap
        string WeatherLocation;            // Bangalore, India
        string WeatherAPIkey;              // "ABCD..."
        uint WeatherUpdateInterval;        // in seconds
        weather_units_t TemperatureUnits;

        // Printer configuration
        vector<printer_config_t> PrinterList;  // List of configured printers

        // Weather locations configuration
        vector<weather_location_t> WeatherLocations;  // List of weather locations to monitor

        // Network configuration for scanner
        vector<network_config_t> NetworkList;  // List of networks to scan for printers

        //SettingsConfig();
        SettingsConfig(string filename);
        void load_config();
        void save_config();
        void add_printer(const string &name, const string &ip, const string &token, const string &serial = "");
        void remove_printer(int index);
        printer_config_t get_printer(int index);
        int get_printer_count();
        
        void add_weather_location(const string &name, const string &city, const string &country, float lat, float lon);
        void remove_weather_location(int index);
        weather_location_t get_weather_location(int index);
        int get_weather_location_count();
        
        void add_network(const string &name, const string &subnet);
        void remove_network(int index);
        network_config_t get_network(int index);
        int get_network_count();

    private:
        void read_json_file();
        void write_json_file();

        string file_name;
        string jsonString;
        cJSON *root;
        cJSON *settings;
    protected:
};

#endif
