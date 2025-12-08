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

#include "SettingsConfig.hpp"
#include <vector>
#include <sys/stat.h>
static const char* TAG = "SettingsConfig";

// Storage health monitoring (extern C for linkage with main.cpp)
extern "C" {
    void storage_health_record_sd_error(void);
}

// Storage paths - prefer SD card over SPIFFS
#define SDCARD_SETTINGS_PATH "/sdcard/settings.json"
#define SPIFFS_SETTINGS_PATH "/spiffs/settings.json"

/**
 * @brief Check if SD card is available and writable
 */
static bool is_sdcard_available() {
    struct stat st;
    
    // Check if /sdcard is mounted
    if (stat("/sdcard", &st) != 0 || !S_ISDIR(st.st_mode)) {
        return false;
    }
    
    // Try to create/open a test file to verify write access
    FILE* test = fopen("/sdcard/.settings_test", "w");
    if (!test) {
        return false;
    }
    fprintf(test, "test");
    fclose(test);
    remove("/sdcard/.settings_test");
    
    return true;
}

/**
 * @brief Get the best available settings file path
 * Prefers SD card if available, falls back to SPIFFS
 */
static string get_settings_path() {
    struct stat st;
    
    // Priority 1: Check if SD card is available and has settings
    if (is_sdcard_available()) {
        if (stat(SDCARD_SETTINGS_PATH, &st) == 0 && S_ISREG(st.st_mode)) {
            ESP_LOGI(TAG, "Using settings from SD card: %s", SDCARD_SETTINGS_PATH);
            return SDCARD_SETTINGS_PATH;
        }
        
        // SD card available but no settings - check if we should migrate from SPIFFS
        if (stat(SPIFFS_SETTINGS_PATH, &st) == 0 && S_ISREG(st.st_mode)) {
            ESP_LOGI(TAG, "Migrating settings from SPIFFS to SD card");
            // Read from SPIFFS
            ifstream src(SPIFFS_SETTINGS_PATH);
            if (src.is_open()) {
                string content((std::istreambuf_iterator<char>(src)),
                               std::istreambuf_iterator<char>());
                src.close();
                
                // Write to SD card
                ofstream dst(SDCARD_SETTINGS_PATH);
                if (dst.is_open()) {
                    dst << content;
                    dst.close();
                    ESP_LOGI(TAG, "Settings migrated to SD card: %s", SDCARD_SETTINGS_PATH);
                    return SDCARD_SETTINGS_PATH;
                }
            }
        }
        
        // SD card available, use it for new settings
        ESP_LOGI(TAG, "New settings will be stored on SD card: %s", SDCARD_SETTINGS_PATH);
        return SDCARD_SETTINGS_PATH;
    }
    
    // Priority 2: Fall back to SPIFFS if SD card not available
    if (stat(SPIFFS_SETTINGS_PATH, &st) == 0 && S_ISREG(st.st_mode)) {
        ESP_LOGI(TAG, "Using settings from SPIFFS: %s", SPIFFS_SETTINGS_PATH);
        return SPIFFS_SETTINGS_PATH;
    }
    
    ESP_LOGI(TAG, "New settings will be stored on SPIFFS: %s", SPIFFS_SETTINGS_PATH);
    return SPIFFS_SETTINGS_PATH;
}

SettingsConfig::SettingsConfig(string filename)
{
    // If load is not called, these are the default values
    DeviceName = "MYDEVICE";
    Brightness = 128;                       // 0-255
    TimeZone = "UTC0";                      // Default to UTC (must match dropdown option)
    CurrentTheme = "dark";                  // light / theme / ???
    #ifdef CONFIG_TUX_HAVE_BATTERY
    HasBattery = true;
    #else
    HasBattery = false;
    #endif
    
    #ifdef CONFIG_TUX_HAVE_BLUETOOTH
    HasBluetooth = true;
    #else
    HasBluetooth = false;
    #endif

    WeatherProvider = "OpenWeatherMaps";
    WeatherLocation = "Bangalore, India";
    WeatherAPIkey = "";
    Language = "pl";                   // Polish language
    WeatherUpdateInterval = 5 * 60;    // Every 5mins
    TemperatureUnits = WEATHER_UNITS_CELSIUS;

    // Use the best available path - SD card preferred over SPIFFS
    // The filename parameter is kept for backward compatibility but we determine the actual path
    file_name = get_settings_path();
}

void SettingsConfig::load_config()
{
    ESP_LOGD(TAG,"******************* Loading JSON *******************");

    if (!file_name.empty()) read_json_file();   // read into jsonString

    // Read json variable and parse to cjson object
    root = cJSON_Parse(jsonString.c_str());
    
    if (!root) {
        ESP_LOGW(TAG, "Failed to parse JSON, using defaults");
        cJSON_Delete(root);
        return;
    }

    // Get root element item with null checks
	settings = cJSON_GetObjectItem(root,"settings");
    
    if (settings) {
        cJSON *brightness_item = cJSON_GetObjectItem(settings,"brightness");
        if (brightness_item && brightness_item->type == cJSON_Number) {
            this->Brightness = brightness_item->valueint;
        }
        
        cJSON *theme_item = cJSON_GetObjectItem(settings,"theme");
        if (theme_item && theme_item->valuestring) {
            this->CurrentTheme = theme_item->valuestring;
        }
        
        cJSON *tz_item = cJSON_GetObjectItem(settings,"timezone");
        if (tz_item && tz_item->valuestring) {
            this->TimeZone = tz_item->valuestring;
            ESP_LOGI(TAG, "Loaded timezone: %s", this->TimeZone.c_str());
        }
        
        cJSON *battery_item = cJSON_GetObjectItem(settings,"has_battery");
        if (battery_item && battery_item->type != cJSON_NULL) {
            this->HasBattery = battery_item->type == cJSON_True;
        }
        
        cJSON *ble_item = cJSON_GetObjectItem(settings,"has_bluetooth");
        if (ble_item && ble_item->type != cJSON_NULL) {
            this->HasBluetooth = ble_item->type == cJSON_True;
        }
        
        cJSON *weather_provider_item = cJSON_GetObjectItem(settings,"weather_provider");
        if (weather_provider_item && weather_provider_item->valuestring) {
            this->WeatherProvider = weather_provider_item->valuestring;
        }
        
        cJSON *weather_location_item = cJSON_GetObjectItem(settings,"weather_location");
        if (weather_location_item && weather_location_item->valuestring) {
            this->WeatherLocation = weather_location_item->valuestring;
        }
        
        cJSON *weather_apikey_item = cJSON_GetObjectItem(settings,"weather_apikey");
        if (weather_apikey_item && weather_apikey_item->valuestring) {
            this->WeatherAPIkey = weather_apikey_item->valuestring;
        }
        
        cJSON *weather_lang_item = cJSON_GetObjectItem(settings,"language");
        if (weather_lang_item && weather_lang_item->valuestring) {
            this->Language = weather_lang_item->valuestring;
        }
        
        cJSON *weather_interval_item = cJSON_GetObjectItem(settings,"weather_update_interval");
        if (weather_interval_item && weather_interval_item->type == cJSON_Number) {
            this->WeatherUpdateInterval = weather_interval_item->valueint;
        }
        
        cJSON *temp_units_item = cJSON_GetObjectItem(settings,"temperature_units");
        if (temp_units_item && temp_units_item->type == cJSON_Number) {
            this->TemperatureUnits = (weather_units_t)temp_units_item->valueint;
        }
    }

    // Load networks from JSON - clear first to avoid duplicates
    NetworkList.clear();
    cJSON *networks_array = cJSON_GetObjectItem(root, "networks");
    if (networks_array && cJSON_IsArray(networks_array)) {
        cJSON *network_item = NULL;
        cJSON_ArrayForEach(network_item, networks_array) {
            const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(network_item, "name"));
            const char *subnet = cJSON_GetStringValue(cJSON_GetObjectItem(network_item, "subnet"));
            cJSON *enabled_item = cJSON_GetObjectItem(network_item, "enabled");
            
            if (name && subnet) {
                network_config_t network;
                network.name = name;
                network.subnet = subnet;
                network.enabled = enabled_item ? enabled_item->type == cJSON_True : true;
                NetworkList.push_back(network);
                ESP_LOGI(TAG, "Loaded network: %s (%s)", name, subnet);
            }
        }
    }

    // Load weather locations from JSON - clear first to avoid duplicates
    WeatherLocations.clear();
    cJSON *locations_array = cJSON_GetObjectItem(root, "weather_locations");
    if (locations_array && cJSON_IsArray(locations_array)) {
        cJSON *location_item = NULL;
        cJSON_ArrayForEach(location_item, locations_array) {
            const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(location_item, "name"));
            const char *city = cJSON_GetStringValue(cJSON_GetObjectItem(location_item, "city"));
            const char *country = cJSON_GetStringValue(cJSON_GetObjectItem(location_item, "country"));
            cJSON *lat_item = cJSON_GetObjectItem(location_item, "latitude");
            cJSON *lon_item = cJSON_GetObjectItem(location_item, "longitude");
            cJSON *enabled_item = cJSON_GetObjectItem(location_item, "enabled");
            
            if (name && city && country && lat_item && lon_item) {
                weather_location_t location;
                location.name = name;
                location.city = city;
                location.country = country;
                location.latitude = lat_item->valuedouble;
                location.longitude = lon_item->valuedouble;
                location.enabled = enabled_item ? enabled_item->type == cJSON_True : true;
                WeatherLocations.push_back(location);
                ESP_LOGI(TAG, "Loaded weather location: %s (%s, %s)", name, city, country);
            }
        }
    }

    // Load printers from JSON - clear first to avoid duplicates
    PrinterList.clear();
    cJSON *printers_array = cJSON_GetObjectItem(root, "printers");
    if (printers_array && cJSON_IsArray(printers_array)) {
        cJSON *printer_item = NULL;
        cJSON_ArrayForEach(printer_item, printers_array) {
            const char *name = cJSON_GetStringValue(cJSON_GetObjectItem(printer_item, "name"));
            const char *ip = cJSON_GetStringValue(cJSON_GetObjectItem(printer_item, "ip_address"));
            const char *token = cJSON_GetStringValue(cJSON_GetObjectItem(printer_item, "token"));
            const char *serial = cJSON_GetStringValue(cJSON_GetObjectItem(printer_item, "serial"));
            cJSON *enabled_item = cJSON_GetObjectItem(printer_item, "enabled");
            cJSON *ssl_verify_item = cJSON_GetObjectItem(printer_item, "disable_ssl_verify");
            
            if (name && ip) {
                printer_config_t printer;
                printer.name = name;
                printer.ip_address = ip;
                printer.token = token ? token : "";
                printer.serial = serial ? serial : "";
                printer.enabled = enabled_item ? enabled_item->type == cJSON_True : true;
                printer.disable_ssl_verify = ssl_verify_item ? ssl_verify_item->type == cJSON_True : true;  // Default to true for easy setup
                PrinterList.push_back(printer);
                ESP_LOGI(TAG, "Loaded printer: %s at %s (SSL verify: %s)", name, ip, printer.disable_ssl_verify ? "disabled" : "enabled");
            }
        }
    }

    ESP_LOGD(TAG,"Loaded:\n%s",jsonString.c_str());

    // Cleanup
    cJSON_Delete(root);
}

void SettingsConfig::save_config()
{    
    ESP_LOGD(TAG,"******************* Saving JSON *******************");
    // Create json object
	root=cJSON_CreateObject();
    
    // set root string elements
	cJSON_AddItemToObject(root, "devicename", cJSON_CreateString(this->DeviceName.c_str()));
    
    // create settings object on root
    cJSON_AddItemToObject(root, "settings", settings=cJSON_CreateObject());

    // Create elements
	cJSON_AddNumberToObject(settings,"brightness",this->Brightness);
	cJSON_AddStringToObject(settings,"theme",this->CurrentTheme.c_str());
	cJSON_AddStringToObject(settings,"timezone",this->TimeZone.c_str());
    cJSON_AddBoolToObject(settings, "has_battery", this->HasBattery);
    cJSON_AddBoolToObject(settings, "has_bluetooth", this->HasBluetooth);
    cJSON_AddStringToObject(settings, "weather_provider", this->WeatherProvider.c_str());
    cJSON_AddStringToObject(settings, "weather_location", this->WeatherLocation.c_str());
    cJSON_AddStringToObject(settings, "weather_apikey", this->WeatherAPIkey.c_str());
    cJSON_AddStringToObject(settings, "language", this->Language.c_str());
    cJSON_AddNumberToObject(settings, "weather_update_interval", this->WeatherUpdateInterval);
    cJSON_AddNumberToObject(settings, "temperature_units", this->TemperatureUnits);

    // Create networks array and add to root
    cJSON *networks_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "networks", networks_array);
    
    for (const auto &network : NetworkList) {
        cJSON *network_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(network_obj, "name", network.name.c_str());
        cJSON_AddStringToObject(network_obj, "subnet", network.subnet.c_str());
        cJSON_AddBoolToObject(network_obj, "enabled", network.enabled);
        cJSON_AddItemToArray(networks_array, network_obj);
    }

    // Create weather locations array and add to root
    cJSON *locations_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "weather_locations", locations_array);
    
    for (const auto &location : WeatherLocations) {
        cJSON *location_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(location_obj, "name", location.name.c_str());
        cJSON_AddStringToObject(location_obj, "city", location.city.c_str());
        cJSON_AddStringToObject(location_obj, "country", location.country.c_str());
        cJSON_AddNumberToObject(location_obj, "latitude", location.latitude);
        cJSON_AddNumberToObject(location_obj, "longitude", location.longitude);
        cJSON_AddBoolToObject(location_obj, "enabled", location.enabled);
        cJSON_AddItemToArray(locations_array, location_obj);
    }

    // Create printers array and add to root
    cJSON *printers_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "printers", printers_array);
    
    for (const auto &printer : PrinterList) {
        cJSON *printer_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(printer_obj, "name", printer.name.c_str());
        cJSON_AddStringToObject(printer_obj, "ip_address", printer.ip_address.c_str());
        cJSON_AddStringToObject(printer_obj, "token", printer.token.c_str());
        cJSON_AddStringToObject(printer_obj, "serial", printer.serial.c_str());
        cJSON_AddBoolToObject(printer_obj, "enabled", printer.enabled);
        cJSON_AddBoolToObject(printer_obj, "disable_ssl_verify", printer.disable_ssl_verify);
        cJSON_AddItemToArray(printers_array, printer_obj);
    }

    // Save json string to variable
    jsonString = cJSON_Print(root);   // back to string
    
    // Cleanup
    cJSON_Delete(root);

    ESP_LOGD(TAG,"Saved:\n%s",jsonString.c_str());

    // Save jsonString to peristant storage
    if (!file_name.empty()) write_json_file();
}

void SettingsConfig::read_json_file()
{
    // read json file to string    
    ifstream jsonfile(file_name);
    if (!jsonfile.is_open())
    {
        ESP_LOGW(TAG,"File open for read failed %s - trying backup",file_name.c_str());
        
        // Try to restore from backup
        std::string backup_path = file_name + ".backup";
        ifstream backup_file(backup_path);
        if (backup_file.is_open()) {
            ESP_LOGI(TAG, "Restoring config from backup: %s", backup_path.c_str());
            jsonString.assign((std::istreambuf_iterator<char>(backup_file)),
                        (std::istreambuf_iterator<char>()));
            backup_file.close();
            
            // Write restored backup to main file
            ofstream restore_file(file_name);
            if (restore_file.is_open()) {
                restore_file << jsonString;
                restore_file.close();
                ESP_LOGI(TAG, "Config restored from backup successfully");
            }
            return;
        }
        
        ESP_LOGE(TAG,"No backup found, creating default config");
        save_config();  // create file with default values
        return;
    }

    jsonString.assign((std::istreambuf_iterator<char>(jsonfile)),
                (std::istreambuf_iterator<char>()));

    jsonfile.close();
    
    // Validate JSON by attempting to parse it
    cJSON* test_root = cJSON_Parse(jsonString.c_str());
    if (test_root == NULL) {
        ESP_LOGE(TAG, "Config file corrupted (invalid JSON) - restoring from backup");
        
        // Try to restore from backup
        std::string backup_path = file_name + ".backup";
        ifstream backup_file(backup_path);
        if (backup_file.is_open()) {
            ESP_LOGI(TAG, "Restoring config from backup: %s", backup_path.c_str());
            jsonString.assign((std::istreambuf_iterator<char>(backup_file)),
                        (std::istreambuf_iterator<char>()));
            backup_file.close();
            
            // Validate backup
            cJSON* backup_root = cJSON_Parse(jsonString.c_str());
            if (backup_root != NULL) {
                cJSON_Delete(backup_root);
                
                // Write restored backup to main file
                ofstream restore_file(file_name);
                if (restore_file.is_open()) {
                    restore_file << jsonString;
                    restore_file.close();
                    ESP_LOGI(TAG, "Config restored from backup successfully");
                }
            } else {
                ESP_LOGE(TAG, "Backup also corrupted - creating default config");
                save_config();
            }
        } else {
            ESP_LOGE(TAG, "No backup found - creating default config");
            save_config();
        }
    } else {
        cJSON_Delete(test_root);
    }
}

void SettingsConfig::write_json_file()
{
    // Create backup before writing (backup path = original + ".backup")
    std::string backup_path = file_name + ".backup";
    
    // If original exists, copy it to backup
    struct stat st;
    if (stat(file_name.c_str(), &st) == 0) {
        // Read existing file
        ifstream src(file_name, std::ios::binary);
        if (src.is_open()) {
            ofstream dst(backup_path, std::ios::binary);
            if (dst.is_open()) {
                dst << src.rdbuf();
                dst.close();
                ESP_LOGI(TAG, "Created backup: %s", backup_path.c_str());
            }
            src.close();
        }
    }
    
    // Write new config
    ofstream jsonfile(file_name);
    if (!jsonfile.is_open())
    {
        ESP_LOGE(TAG,"File open for write failed %s",file_name.c_str());
        
        // Record SD error if writing to SD card
        if (file_name.find("/sdcard/") == 0) {
            storage_health_record_sd_error();
        }
        
        return ;//ESP_FAIL;
    }
    jsonfile << jsonString;
    jsonfile.flush();
    
    // Check for write errors
    if (jsonfile.fail()) {
        ESP_LOGE(TAG, "Write failed for %s", file_name.c_str());
        
        // Record SD error if writing to SD card
        if (file_name.find("/sdcard/") == 0) {
            storage_health_record_sd_error();
        }
    }
    
    jsonfile.close();
    
    ESP_LOGI(TAG, "Wrote config to %s (size: %d bytes)", file_name.c_str(), jsonString.length());
}

void SettingsConfig::add_printer(const string &name, const string &ip, const string &token, const string &serial)
{
    if (PrinterList.size() >= MAX_PRINTERS) {
        ESP_LOGW(TAG, "Maximum printers reached (%d)", MAX_PRINTERS);
        return;
    }
    
    printer_config_t printer;
    printer.name = name;
    printer.ip_address = ip;
    printer.token = token;
    printer.serial = serial;
    printer.enabled = true;
    printer.disable_ssl_verify = true;  // Default to disabled (easier setup, matches Home Assistant)
    PrinterList.push_back(printer);
    
    ESP_LOGI(TAG, "Added printer: %s at %s (serial: %s)", name.c_str(), ip.c_str(), serial.c_str());
}

void SettingsConfig::remove_printer(int index)
{
    if (index >= 0 && index < (int)PrinterList.size()) {
        PrinterList.erase(PrinterList.begin() + index);
        ESP_LOGI(TAG, "Removed printer at index %d", index);
    }
}

printer_config_t SettingsConfig::get_printer(int index)
{
    printer_config_t empty_printer = {"", "", "", "", false, true};
    if (index >= 0 && index < (int)PrinterList.size()) {
        return PrinterList[index];
    }
    return empty_printer;
}

int SettingsConfig::get_printer_count()
{
    return PrinterList.size();
}

void SettingsConfig::add_weather_location(const string &name, const string &city, const string &country, float lat, float lon)
{
    if (WeatherLocations.size() >= MAX_WEATHER_LOCATIONS) {
        ESP_LOGW(TAG, "Maximum weather locations reached (%d)", MAX_WEATHER_LOCATIONS);
        return;
    }
    
    weather_location_t location;
    location.name = name;
    location.city = city;
    location.country = country;
    location.latitude = lat;
    location.longitude = lon;
    location.enabled = true;
    WeatherLocations.push_back(location);
    
    ESP_LOGI(TAG, "Added weather location: %s (%s, %s) at %.2f, %.2f", 
             name.c_str(), city.c_str(), country.c_str(), lat, lon);
}

void SettingsConfig::remove_weather_location(int index)
{
    if (index >= 0 && index < (int)WeatherLocations.size()) {
        WeatherLocations.erase(WeatherLocations.begin() + index);
        ESP_LOGI(TAG, "Removed weather location at index %d", index);
    }
}

weather_location_t SettingsConfig::get_weather_location(int index)
{
    weather_location_t empty_location = {"", "", "", 0.0f, 0.0f, false};
    if (index >= 0 && index < (int)WeatherLocations.size()) {
        return WeatherLocations[index];
    }
    return empty_location;
}

int SettingsConfig::get_weather_location_count()
{
    return WeatherLocations.size();
}

void SettingsConfig::add_network(const string &name, const string &subnet)
{
    if (NetworkList.size() >= MAX_NETWORKS) {
        ESP_LOGW(TAG, "Maximum networks reached (%d)", MAX_NETWORKS);
        return;
    }
    
    network_config_t network;
    network.name = name;
    network.subnet = subnet;
    network.enabled = true;
    NetworkList.push_back(network);
    
    ESP_LOGI(TAG, "Added network: %s (%s)", name.c_str(), subnet.c_str());
}

void SettingsConfig::remove_network(int index)
{
    if (index >= 0 && index < (int)NetworkList.size()) {
        NetworkList.erase(NetworkList.begin() + index);
        ESP_LOGI(TAG, "Removed network at index %d", index);
    }
}

network_config_t SettingsConfig::get_network(int index)
{
    network_config_t empty_network = {"", "", false};
    if (index >= 0 && index < (int)NetworkList.size()) {
        return NetworkList[index];
    }
    return empty_network;
}

int SettingsConfig::get_network_count()
{
    return NetworkList.size();
}