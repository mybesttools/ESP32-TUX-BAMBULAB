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

static const char *TAG = "ESP32-TUX";
#include "main.hpp"
#include "WebServer.hpp"
#include "events/gui_events.hpp"
#include "esp_netif.h"
#include "mdns_responder.h"
#include "cJSON.h"
#include <sys/stat.h>

// Storage paths for printer cache - prefer SD card over SPIFFS
#define SDCARD_PRINTER_PATH "/sdcard/printer"
#define SPIFFS_PRINTER_PATH "/spiffs/printer"

// Async callback for config changes - runs in LVGL context safely
static void config_changed_async_cb(void *data) {
    ESP_LOGI(TAG, "Config changed async - updating language, brightness, theme and sending MSG_CONFIG_CHANGED");
    // Update language from config
    if (cfg && !cfg->Language.empty()) {
        set_language_from_code(cfg->Language.c_str());
    }
    
    // Apply brightness from config
    if (cfg) {
        lcd.setBrightness(cfg->Brightness);
        ESP_LOGI(TAG, "Applied brightness from config: %d", cfg->Brightness);
    }
    
    lv_msg_send(MSG_CONFIG_CHANGED, NULL);
    
    // Trigger immediate weather refresh to get descriptions in new language
    if (owm && timer_weather) {
        ESP_LOGI(TAG, "Language changed - triggering weather refresh");
        lv_timer_ready(timer_weather);
    }
}

// Get the printer storage path (SD card if available, else SPIFFS)
static const char* get_printer_storage_path() {
    struct stat st;
    if (stat("/sdcard", &st) == 0 && S_ISDIR(st.st_mode)) {
        // Ensure printer directory exists on SD card
        if (stat(SDCARD_PRINTER_PATH, &st) != 0) {
            mkdir(SDCARD_PRINTER_PATH, 0755);
        }
        return SDCARD_PRINTER_PATH;
    }
    return SPIFFS_PRINTER_PATH;
}

// Get full path to a printer cache file
static std::string get_printer_file_path(const std::string& serial) {
    return std::string(get_printer_storage_path()) + "/" + serial + ".json";
}

static void set_timezone()
{
    // Prefer timezone from web settings; fall back to a safe default if not set
    const char *source = "settings default";
    std::string tz_value = "UTC0";

    if (cfg && !cfg->TimeZone.empty()) {
        tz_value = cfg->TimeZone;
        source = "web config";
    }

    setenv("TZ", tz_value.c_str(), 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to %s (%s)", tz_value.c_str(), source);
}

// GEt time from internal RTC and update date/time of the clock
static void update_datetime_ui()
{
    // If we are on another screen where lbl_time is not valid
    //if (!lv_obj_is_valid(lbl_time)) return;

    // date/time format reference => https://cplusplus.com/reference/ctime/strftime/
    time_t now;
    struct tm datetimeinfo;
    time(&now);
    localtime_r(&now, &datetimeinfo);

    // tm_year will be (1970 - 1900) = 70, if not set
    if (datetimeinfo.tm_year < 100) // Time travel not supported :P
    {
        // ESP_LOGD(TAG, "Date/time not set yet [%d]",datetimeinfo.tm_year);    
        return;
    }

    // Send update time to UI via IPC queue (LVGL task will consume)
    ui_ipc_post_time(datetimeinfo);
}

static const char* get_id_string(esp_event_base_t base, int32_t id) {
    //if (base == TUX_EVENTS) {
        switch(id) {
            case TUX_EVENT_DATETIME_SET:
                return "TUX_EVENT_DATETIME_SET";
            case TUX_EVENT_OTA_STARTED:
                return "TUX_EVENT_OTA_STARTED";
            case TUX_EVENT_OTA_IN_PROGRESS:
                return "TUX_EVENT_OTA_IN_PROGRESS";
            case TUX_EVENT_OTA_ROLLBACK:
                return "TUX_EVENT_OTA_ROLLBACK";
            case TUX_EVENT_OTA_COMPLETED:
                return "TUX_EVENT_OTA_COMPLETED";
            case TUX_EVENT_OTA_FAILED:
                return "TUX_EVENT_OTA_FAILED";
            case TUX_EVENT_OTA_ABORTED:
                return "TUX_EVENT_OTA_ABORTED";
            case TUX_EVENT_WEATHER_UPDATED:
                return "TUX_EVENT_WEATHER_UPDATED";
            case TUX_EVENT_THEME_CHANGED:
                return "TUX_EVENT_THEME_CHANGED";
            case TUX_EVENT_CONFIG_CHANGED:
                return "TUX_EVENT_CONFIG_CHANGED";
            default:
                return "TUX_EVENT_UNKNOWN";        
        }
    //} 
}

// tux event handler
static void tux_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    ESP_LOGD(TAG, "tux_event_handler => %s:%s", event_base, get_id_string(event_base, event_id));
    if (event_base != TUX_EVENTS) return;   // bye bye - me not invited :(

    // Handle TUX_EVENTS
    if (event_id == TUX_EVENT_DATETIME_SET) {

        set_timezone();

        // update clock
        update_datetime_ui();

        // Enable timer after the date/time is set.
        lv_timer_ready(timer_weather); 
        
        // Start MQTT connection to Bambu printer now that time is synced
        // This ensures TLS certificate verification has accurate system time
        if (bambu_monitor_start() == ESP_OK) {
            ESP_LOGI(TAG, "Bambu Monitor MQTT connection started (time synced)");
        } else {
            ESP_LOGW(TAG, "Failed to start Bambu Monitor MQTT connection");
        }

    } else if (event_id == TUX_EVENT_OTA_STARTED) {
        // OTA Started
        char buffer[150] = {0};
        snprintf(buffer,sizeof(buffer),"OTA: %s",(char*)event_data);
        lv_msg_send(MSG_OTA_STATUS,buffer);

    } else if (event_id == TUX_EVENT_OTA_IN_PROGRESS) {
        // OTA In Progress - progressbar?
        char buffer[150] = {0};
        int bytes_read = (*(int *)event_data)/1024;
        snprintf(buffer,sizeof(buffer),"OTA: Data read : %dkb", bytes_read);
        lv_msg_send(MSG_OTA_STATUS,buffer);

    } else if (event_id == TUX_EVENT_OTA_ROLLBACK) {
        // OTA Rollback - god knows why!
        char buffer[150] = {0};
        snprintf(buffer,sizeof(buffer),"OTA: %s", (char*)event_data);
        lv_msg_send(MSG_OTA_STATUS,buffer);

    } else if (event_id == TUX_EVENT_OTA_COMPLETED) {
        // OTA Completed - YAY! Success
        char buffer[150] = {0};
        snprintf(buffer,sizeof(buffer),"OTA: %s", (char*)event_data);
        lv_msg_send(MSG_OTA_STATUS,buffer);

        // wait before reboot
        vTaskDelay(3000 / portTICK_PERIOD_MS);

    } else if (event_id == TUX_EVENT_OTA_ABORTED) {
        // OTA Aborted - Not a good day for updates
        char buffer[150] = {0};
        snprintf(buffer,sizeof(buffer),"OTA: %s", (char*)event_data);
        lv_msg_send(MSG_OTA_STATUS,buffer);

    } else if (event_id == TUX_EVENT_OTA_FAILED) {
        // OTA Failed - huh! - maybe in red color?
        char buffer[150] = {0};
        snprintf(buffer,sizeof(buffer),"OTA: %s", (char*)event_data);
        lv_msg_send(MSG_OTA_STATUS,buffer);

    } else if (event_id == TUX_EVENT_WEATHER_UPDATED) {
        // Weather updates - summer?

    } else if (event_id == TUX_EVENT_THEME_CHANGED) {
        // Theme changes - time to play with dark & light

    } else if (event_id == TUX_EVENT_CONFIG_CHANGED) {
        // Config changed via web UI - rebuild carousel
        // Use lv_async_call to safely call LVGL from event handler context
        ESP_LOGI(TAG, "Config changed, scheduling carousel rebuild");
        lv_async_call(config_changed_async_cb, NULL);
    }
}                          

// Wifi & IP related event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    //ESP_LOGD(TAG, "%s:%s: wifi_event_handler", event_base, get_id_string(event_base, event_id));
    if (event_base == WIFI_EVENT  && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        is_wifi_connected = true;
        lv_timer_ready(timer_datetime);   // start timer

        // After OTA device restart, RTC will have time but not timezone
        set_timezone();

        // Not a warning but just for highlight
        ESP_LOGW(TAG,"WIFI_EVENT_STA_CONNECTED");
        lv_msg_send(MSG_WIFI_CONNECTED,NULL);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        is_wifi_connected = false;        
        lv_timer_pause(timer_datetime);   // stop/pause timer

        ESP_LOGW(TAG,"WIFI_EVENT_STA_DISCONNECTED");
        lv_msg_send(MSG_WIFI_DISCONNECTED,NULL);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        is_wifi_connected = true;
        lv_timer_ready(timer_datetime);   // start timer

        ESP_LOGW(TAG,"IP_EVENT_STA_GOT_IP");
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        snprintf(ip_payload,sizeof(ip_payload),"%d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        
        // Initialize mDNS responder for esp32-tux.local hostname
        esp_err_t mdns_err = mdns_responder_init("esp32-tux");
        if (mdns_err == ESP_OK) {
            ESP_LOGI(TAG, "mDNS responder initialized - hostname: esp32-tux.local");
        } else {
            ESP_LOGW(TAG, "mDNS responder initialization failed: %s", esp_err_to_name(mdns_err));
        }
        
        // Start web server now that WiFi is connected (after provisioning is done)
        // This avoids port 80 conflict with provisioning HTTP server
        if (web_server && !web_server->is_running()) {
            if (web_server->start() == ESP_OK) {
                ESP_LOGI(TAG, "Web server started successfully");
            } else {
                ESP_LOGE(TAG, "Failed to start web server");
            }
        }
        
        // Update Web UI URL label with actual IP
        if (lbl_webui_url) {
            lvgl_acquire();
            lv_label_set_text_fmt(lbl_webui_url, "Web UI: http://%s", ip_payload);
            lvgl_release();
        }
        
        ESP_LOGI(TAG, "Web UI available at: http://%s", ip_payload);
        ESP_LOGI(TAG, "Connected with IP Address:%s", ip_payload);
        
        // Auto-configure discovery network from current IP (if not already configured)
        if (cfg && cfg->get_network_count() == 0) {
            // Extract subnet from IP (e.g., 10.13.13.188 -> 10.13.13.0/24)
            uint32_t ip_addr = event->ip_info.ip.addr;
            uint32_t netmask = event->ip_info.netmask.addr;
            
            // Calculate network address
            uint32_t network_addr = ip_addr & netmask;
            
            // Format as CIDR notation
            char subnet_str[32] = {0};
            snprintf(subnet_str, sizeof(subnet_str), 
                     "%lu.%lu.%lu.%lu/24",
                     (network_addr >> 0) & 0xFF,
                     (network_addr >> 8) & 0xFF,
                     (network_addr >> 16) & 0xFF,
                     (network_addr >> 24) & 0xFF);
            
            cfg->add_network("Local Network", subnet_str);
            cfg->save_config();
            ESP_LOGI(TAG, "Auto-configured discovery network: %s", subnet_str);
        }
        
        // NOTE: Bambu MQTT connection will start after time is synced (TUX_EVENT_DATETIME_SET)
        // This ensures TLS certificate verification has accurate system time
        
        // We got IP, lets update time from SNTP. RTC keeps time unless powered off
        xTaskCreate(configure_time, "config_time", 1024*4, NULL, 3, NULL);

        // Kick weather timer but let it run from the LVGL timer context
        // (avoid heavy work on the sys_evt task stack)
        if (timer_weather) {
            lv_timer_ready(timer_weather);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP)
    {
        is_wifi_connected = false;
        ESP_LOGW(TAG,"IP_EVENT_STA_LOST_IP");
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_START) {
        ESP_LOGW(TAG,"WIFI_PROV_START");
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_RECV) {
        ESP_LOGW(TAG,"WIFI_PROV_CRED_RECV");
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_FAIL) {
        ESP_LOGW(TAG,"WIFI_PROV_CRED_FAIL");
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_SUCCESS) {
        ESP_LOGW(TAG,"WIFI_PROV_CRED_SUCCESS");
        // FIXME Refresh IP details once provision is successfull
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END) {
        ESP_LOGW(TAG,"WIFI_PROV_END");
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_SHOWQR) {
        ESP_LOGW(TAG,"WIFI_PROV_SHOWQR");
        strcpy(qr_payload,(char*)event_data);   // Add qr payload to the variable
    }
}

// Bambu Monitor task - polls printer status via MQTT
static void bambu_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Bambu Monitor task started");
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        // Poll every 10 seconds (avoid flooding MQTT broker)
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10000));
        
        // Send pushall query to get current printer status
        // Response will be handled by MQTT event handler and written to /spiffs/printer/<serial>.json
        // GUI timer will read the file and update carousel
        esp_err_t ret = bambu_send_query();
        if (ret == ESP_OK) {
            ESP_LOGD(TAG, "MQTT query sent successfully");
        } else {
            ESP_LOGD(TAG, "MQTT query failed (printer may be offline)");
        }
    }
    
    vTaskDelete(NULL);
}

/**
 * @brief Storage health monitoring task
 * Periodically checks SD card and SPIFFS health, attempts auto-repair if issues detected
 */
/**
 * @brief Storage health monitoring task
 * Periodically checks SD card and SPIFFS health, logs error counts
 */
static void storage_health_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Storage health monitor task started");
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        // Check health every 60 seconds
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(60000));
        
        // Run health check and log status
        storage_health_check();
        
        // Get status for logging
        storage_health_t status;
        storage_health_get_status(&status);
        if (status.sd_errors > 0 || status.spiffs_errors > 0) {
            ESP_LOGW(TAG, "Storage errors detected - SD: %lu, SPIFFS: %lu", 
                     (unsigned long)status.sd_errors, (unsigned long)status.spiffs_errors);
        }
    }
    
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_DEBUG);      // enable DEBUG logs for this App
    //esp_log_level_set("SettingsConfig", ESP_LOG_DEBUG);    
    esp_log_level_set("wifi", ESP_LOG_WARN);    // enable WARN logs from WiFi stack

    // Print device info
    ESP_LOGE(TAG,"\n%s",device_info().c_str());

    //Initialize NVS
    esp_err_t err = nvs_flash_init();

    // NVS partition contains new data format or unable to access
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err); 

    // Initialize TCP/IP stack early (before WiFi provisioning and WebServer)
    // This must happen before any networking code runs
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Init SPIFF - needed for lvgl images
    init_spiff();

#ifdef SD_SUPPORTED
    // Initializing SDSPI 
    if (init_sdspi() == ESP_OK) // SD SPI
    {
        is_sdcard_enabled = true;
    }
#endif   
//********************** CONFIG HELPER TESTING STARTS

    // Use SD card for config if available, otherwise fall back to SPIFFS
    if (is_sdcard_enabled) {
        cfg = new SettingsConfig("/sdcard/settings.json");
        ESP_LOGI(TAG, "Using SD card for config storage");
    } else {
        cfg = new SettingsConfig("/spiffs/settings.json");
        ESP_LOGI(TAG, "Using SPIFFS for config storage (SD card not available)");
    }
    
    // Load existing config first to check if it's new or existing
    cfg->load_config();
    
    // Set UI language from config
    if (!cfg->Language.empty()) {
        set_language_from_code(cfg->Language.c_str());
        ESP_LOGI(TAG, "UI language set to: %s", cfg->Language.c_str());
    }
    
    // Only apply defaults and re-save if this is a fresh config (no weather locations yet)
    if (cfg->get_weather_location_count() == 0) {
        // This is a fresh config, set defaults
        cfg->DeviceName = "ESP32-TUX";
        cfg->WeatherAPIkey = CONFIG_WEATHER_API_KEY;
        cfg->WeatherLocation = CONFIG_WEATHER_LOCATION;
        cfg->WeatherProvider = CONFIG_WEATHER_OWM_URL;
        cfg->Brightness = 250;
        
        // Initialize default weather locations
        cfg->add_weather_location("Home", "Bedburg-Hau", "Germany", 51.761f, 6.1763f);
        cfg->add_weather_location("Reference", "Warsaw", "Poland", 52.2298f, 21.0118f);
        cfg->add_weather_location("Travel", "Amsterdam", "Netherlands", 52.374f, 4.8897f);
        cfg->save_config();
        ESP_LOGI(TAG, "Initialized default configuration");
    } else {
        // Config already exists, just log that we loaded it
        ESP_LOGI(TAG, "Loaded existing configuration");
    }

//******************************************** 
    owm = new OpenWeatherMap();
    owm->set_config(cfg);  // Share the global config with OpenWeatherMap
    
    // Initialize Bambu Lab printer monitoring (optional)
    if (bambu_helper_init() != ESP_OK) {
        ESP_LOGW(TAG, "Bambu Monitor initialization optional - continuing without it");
    }
//********************** CONFIG HELPER TESTING ENDS

    lcd.init();         // Initialize LovyanGFX
    lcd.initDMA();      // Init DMA
    lv_init();          // Initialize lvgl

    if (lv_display_init() != ESP_OK) // Configure LVGL
    {
        ESP_LOGE(TAG, "LVGL setup failed!!!");
    }

    // Event loop already initialized at start of app_main()

    /* Register for event handler for Wi-Fi, IP related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    /* Events related to provisioning */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    // TUX EVENTS
    ESP_ERROR_CHECK(esp_event_handler_instance_register(TUX_EVENTS, ESP_EVENT_ANY_ID, tux_event_handler, NULL, NULL));

    // LV_FS integration & print readme.txt from the root for testing
    lv_print_readme_txt("F:/readme.txt");   // SPIFF / FAT
    lv_print_readme_txt("S:/readme.txt");   // SDCARD

/* Push LVGL/UI to its own UI task later*/
    // Splash screen
    lvgl_acquire();
    create_splash_screen();
    lvgl_release();

    // Main UI
    lvgl_acquire();
    lv_setup_styles();    
    show_ui();
    lvgl_release();
    ui_ipc_init();
/* Push these to its own UI task later*/

#ifdef SD_SUPPORTED
    // Icon status color update
    lv_msg_send(MSG_SDCARD_STATUS,&is_sdcard_enabled);
#endif

    // Wifi Provision and connection.
    // Use idf.py menuconfig to configure 
    // Use SoftAP only / BLE has some issues
    // Tuning PSRAM options visible only in IDF5, so will wait till then for BLE.
    xTaskCreate(provision_wifi, "wifi_prov", 1024*8, NULL, 3, NULL);

    // Create web server instance (but don't start yet - wait for WiFi connection
    // to avoid port 80 conflict with provisioning HTTP server)
    web_server = new WebServer();

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());

    // Date/Time update timer - once per sec
    timer_datetime = lv_timer_create(timer_datetime_callback, 1000,  NULL);
    //lv_timer_pause(timer_datetime); // enable only when wifi is connected

    // Weather update timer - Once per min (60*1000) or maybe once in 10 mins (10*60*1000)
    timer_weather = lv_timer_create(timer_weather_callback, WEATHER_UPDATE_INTERVAL,  NULL);

    // Printer data update timer - Poll SPIFFS files every 5 seconds (like weather file polling)
    timer_printer = lv_timer_create(timer_printer_callback, 5000, NULL);
    ESP_LOGI(TAG, "Printer file polling timer started (5s interval)");

    //lv_timer_set_repeat_count(timer_weather,1);
    //lv_timer_pause(timer_weather);  // enable after wifi is connected

    // Subscribe to page change events in the UI
    /* SPELLING MISTAKE IN API BUG => https://github.com/lvgl/lvgl/issues/3822 */
    lv_msg_subscribe(MSG_PAGE_HOME, tux_ui_change_cb, NULL);
    lv_msg_subscribe(MSG_PAGE_REMOTE, tux_ui_change_cb, NULL);
    lv_msg_subscribe(MSG_PAGE_SETTINGS, tux_ui_change_cb, NULL);
    lv_msg_subscribe(MSG_PAGE_OTA, tux_ui_change_cb, NULL);
    lv_msg_subscribe(MSG_PAGE_BAMBU, tux_ui_change_cb, NULL);
    lv_msg_subscribe(MSG_OTA_INITIATE, tux_ui_change_cb, NULL);    // Initiate OTA

    // Start Bambu Monitor task for continuous printer polling
    xTaskCreatePinnedToCore(
        bambu_monitor_task,
        "bambu_monitor",
        1024 * 4,
        NULL,
        2,
        NULL,
        0  // Core 0
    );
    
    // Start Storage Health Monitor task
    xTaskCreatePinnedToCore(
        storage_health_task,
        "storage_health",
        1024 * 3,
        NULL,
        1,  // Lower priority than bambu_monitor
        NULL,
        0  // Core 0
    );
    ESP_LOGI(TAG, "Storage health monitor started (60s interval)");
}

static void timer_datetime_callback(lv_timer_t * timer)
{
    // Battery icon animation
    if (battery_value>100) battery_value=0;
    battery_value+=10;
    
    lv_msg_send(MSG_BATTERY_STATUS,&battery_value);
    update_datetime_ui();
}

// Daily API call tracking to stay under OpenWeatherMap free tier limit
static int weather_api_calls_today = 0;
static int weather_api_last_reset_day = -1;

static void timer_weather_callback(lv_timer_t * timer)
{
    ESP_LOGD(TAG, "timer_weather_callback fired");
    if (cfg->WeatherAPIkey.empty()) {
        ESP_LOGW(TAG,"Weather API Key not set");
        return;
    }

    // Reset daily counter at midnight
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_yday != weather_api_last_reset_day) {
        weather_api_calls_today = 0;
        weather_api_last_reset_day = timeinfo.tm_yday;
        ESP_LOGI(TAG, "Weather API daily counter reset");
    }

    // Count enabled locations
    int location_count = cfg->get_weather_location_count();
    int enabled_count = 0;
    for (int i = 0; i < location_count; i++) {
        if (cfg->get_weather_location(i).enabled) enabled_count++;
    }

    // Check if we'd exceed daily limit
    if (weather_api_calls_today + enabled_count > WEATHER_DAILY_API_LIMIT) {
        ESP_LOGW(TAG, "Weather API daily limit reached (%d/%d), skipping update",
                 weather_api_calls_today, WEATHER_DAILY_API_LIMIT);
        return;
    }

    // Update weather for all enabled locations
    for (int i = 0; i < location_count; i++) {
        weather_location_t loc = cfg->get_weather_location(i);
        if (loc.enabled) {
            std::string location_query = loc.city;
            if (!loc.country.empty()) {
                location_query += "," + loc.country;
            }
            ESP_LOGD(TAG, "Updating weather for: %s (API calls today: %d)",
                     location_query.c_str(), weather_api_calls_today + 1);
            owm->request_weather_update(location_query);
            weather_api_calls_today++;
            vTaskDelay(pdMS_TO_TICKS(2000));  // Delay between API calls
        }
    }
}

// Timer callback to poll printer data files from SD card or SPIFFS
static void timer_printer_callback(lv_timer_t * timer)
{
    ESP_LOGI(TAG, "timer_printer_callback fired - checking printer files");
    
    // Read all printer JSON files from SD card or SPIFFS
    // Only process files with recent timestamps (online printers)
    const char* printer_path = get_printer_storage_path();
    DIR* dir = opendir(printer_path);
    if (!dir) {
        ESP_LOGD(TAG, "Printer directory not found: %s", printer_path);
        return;
    }
    
    struct dirent* entry;
    time_t now = time(NULL);
    const time_t ONLINE_THRESHOLD = 60;  // Printer is "online" if updated within 60 seconds
    
    int online_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".json")) {
            char filepath[384];  // Larger buffer for full path
            snprintf(filepath, sizeof(filepath), "%s/%s", printer_path, entry->d_name);
            
            // Read file contents
            std::ifstream printer_file(filepath);
            if (!printer_file.is_open()) continue;
            
            std::string json_str((std::istreambuf_iterator<char>(printer_file)),
                                 std::istreambuf_iterator<char>());
            printer_file.close();
            
            // Parse JSON
            cJSON* json = cJSON_Parse(json_str.c_str());
            if (!json) continue;
            
            // Check timestamp
            cJSON* last_update = cJSON_GetObjectItem(json, "last_update");
            if (!last_update || !cJSON_IsNumber(last_update)) {
                cJSON_Delete(json);
                continue;
            }
            
            time_t update_time = (time_t)last_update->valuedouble;
            bool is_online = (now - update_time) < ONLINE_THRESHOLD;
            
            if (is_online) {
                online_count++;
                // Extract key data for logging only - GUI polls files separately
                cJSON* nozzle_temp = cJSON_GetObjectItem(json, "nozzle_temper");
                cJSON* bed_temp = cJSON_GetObjectItem(json, "bed_temper");
                cJSON* progress = cJSON_GetObjectItem(json, "mc_percent");
                cJSON* gcode_state = cJSON_GetObjectItem(json, "gcode_state");
                
                int nozzle = nozzle_temp ? (int)nozzle_temp->valuedouble : 0;
                int bed = bed_temp ? (int)bed_temp->valuedouble : 0;
                int prog = progress ? (int)progress->valuedouble : 0;
                const char* state = (gcode_state && gcode_state->valuestring) ? 
                                   gcode_state->valuestring : "UNKNOWN";
                
                ESP_LOGD(TAG, "Printer %s: nozzle=%d°C, bed=%d°C, progress=%d%%, state=%s",
                         entry->d_name, nozzle, bed, prog, state);
            } else {
                ESP_LOGD(TAG, "Printer %s offline (last update %ld seconds ago)",
                         entry->d_name, (long)(now - update_time));
            }
            
            cJSON_Delete(json);
        }
    }
    closedir(dir);
    
    if (online_count > 0) {
        ESP_LOGI(TAG, "Found %d online printer(s)", online_count);
    }
}

// Callback to notify App UI change
static void tux_ui_change_cb(void * s, lv_msg_t *m)
{
    LV_UNUSED(s);
    unsigned int page_id = lv_msg_get_id(m);
    const char * msg_payload = (const char *)lv_msg_get_payload(m);
    const char * msg_data = (const char *)lv_msg_get_user_data(m);

    //ESP_LOGW(TAG,"[%" PRIu32 "] page event triggered",page_id);

    switch (page_id)
    {
        case MSG_PAGE_HOME:
            // Update date/time and current weather
            // Date/time is updated every second anyway
            // Weather updates via file polling - no event needed
            break;
        case MSG_PAGE_REMOTE:
            // Trigger loading buttons data
            break;
        case MSG_PAGE_SETTINGS:
            if (!is_wifi_connected)  {// Provisioning mode
                lv_msg_send(MSG_WIFI_PROV_MODE, qr_payload);
                //lv_msg_send(MSG_QRCODE_CHANGED, qr_payload);
            } else {
                lv_msg_send(MSG_WIFI_CONNECTED, ip_payload);
            }
            break;
        case MSG_PAGE_OTA:
            // Update firmware current version info
            lv_msg_send(MSG_DEVICE_INFO,device_info().c_str());
            break;
        case MSG_OTA_INITIATE:
            // OTA update from button trigger
            xTaskCreate(run_ota_task, "run_ota_task", 1024 * 10, NULL, 5, NULL);
            break;
    }
}

static string device_info()
{
    std::string s_chip_info = "";

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);

    // CPU Speed - 80Mhz / 160 Mhz / 240Mhz
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);

    multi_heap_info_t info;    
	heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    float psramsize = (info.total_free_bytes + info.total_allocated_bytes) / (1024.0 * 1024.0);

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        s_chip_info += fmt::format("Firmware Ver : {}\n",running_app_info.version);
        s_chip_info += fmt::format("Project Name : {}\n",running_app_info.project_name);
        // running_app_info.time
        // running_app_info.date
    }
    s_chip_info += fmt::format("IDF Version  : {}\n\n",esp_get_idf_version());

    s_chip_info += fmt::format("Controller   : {} Rev.{}\n",CONFIG_IDF_TARGET,chip_info.revision);  
    //s_chip_info += fmt::format("\nModel         : {}",chip_info.model); // esp_chip_model_t type
    s_chip_info += fmt::format("CPU Cores    : {}\n", (chip_info.cores==2)? "Dual Core" : "Single Core");
    s_chip_info += fmt::format("CPU Speed    : {}Mhz\n",conf.freq_mhz);
    if(esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
    s_chip_info += fmt::format("Flash Size   : {}MB {}\n",flash_size / (1024 * 1024),
                                            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "[embedded]" : "[external]");
    }
    s_chip_info += fmt::format("PSRAM Size   : {}MB {}\n",static_cast<int>(round(psramsize)),
                                            (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "[embedded]" : "[external]");

    s_chip_info += fmt::format("Connectivity : {}{}{}\n",(chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "2.4GHz WIFI" : "NA",
                                                    (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                                                    (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    //s_chip_info += fmt::format("\nIEEE 802.15.4 : {}",string((chip_info.features & CHIP_FEATURE_IEEE802154) ? "YES" : "NA"));

    //ESP_LOGE(TAG,"\n%s",device_info().c_str());
    return s_chip_info;
}