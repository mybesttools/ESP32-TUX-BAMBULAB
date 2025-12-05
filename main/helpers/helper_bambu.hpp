#pragma once

#include "BambuMonitor.hpp"
#include "esp_log.h"

static const char* TAG_BAMBU = "BambuHelper";

/**
 * @brief Get GIF path for a printer state
 * 
 * @param state Printer state
 * @return Path to GIF file in SPIFFS
 */
static const char* bambu_get_gif_path(bambu_printer_state_t state)
{
    switch (state) {
        case BAMBU_STATE_PRINTING:
            return "S:/printing.gif";
        case BAMBU_STATE_PAUSED:
            return "S:/standby.gif";
        case BAMBU_STATE_ERROR:
            return "S:/error.gif";
        case BAMBU_STATE_IDLE:
            return "S:/standby.gif";
        case BAMBU_STATE_OFFLINE:
        default:
            return "S:/logo.gif";
    }
}

/**
 * @brief Get state description string
 * 
 * @param state Printer state
 * @return State description
 */
static const char* bambu_get_state_str(bambu_printer_state_t state)
{
    switch (state) {
        case BAMBU_STATE_PRINTING:
            return "Printing";
        case BAMBU_STATE_PAUSED:
            return "Paused";
        case BAMBU_STATE_ERROR:
            return "Error";
        case BAMBU_STATE_IDLE:
            return "Idle";
        case BAMBU_STATE_OFFLINE:
            return "Offline";
        default:
            return "Unknown";
    }
}

/**
 * @brief Initialize Bambu Monitor with default settings
 * 
 * This function creates a basic configuration and initializes
 * the Bambu Monitor component. Currently uses placeholder values.
 * 
 * @return ESP_OK on success
 */
static esp_err_t bambu_helper_init()
{
    ESP_LOGI(TAG_BAMBU, "Initializing Bambu Monitor helper");
    
    // Default printer configuration
    bambu_printer_config_t printer_config = {
        .device_id = (char*)"00M09D530200738",
        .ip_address = (char*)"10.13.13.85",
        .port = 8883,                          // MQTT secure port
        .access_code = (char*)"5d35821c",
        .tls_certificate = NULL,               // Will be fetched dynamically
    };
    
    // Fetch TLS certificate from printer (done once at init, before WiFi)
    // Note: This should ideally be done after WiFi is connected
    // For now, certificate will be fetched when MQTT starts
    
    // Initialize Bambu Monitor
    esp_err_t ret = bambu_monitor_init(&printer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_BAMBU, "Failed to initialize Bambu Monitor: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG_BAMBU, "Bambu Monitor initialized successfully");
    return ESP_OK;
}
