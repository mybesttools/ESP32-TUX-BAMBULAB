#pragma once

#include "BambuMonitor.hpp"
#include "esp_log.h"
#include "SettingsConfig.hpp"

// Forward declaration
extern SettingsConfig *cfg;

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
 * the Bambu Monitor component. Reads from SettingsConfig if available.
 * 
 * @return ESP_OK on success
 */
static esp_err_t bambu_helper_init()
{
    ESP_LOGI(TAG_BAMBU, "Initializing Bambu Monitor helper");
    
    // Check if we have a printer configured in settings
    if (!cfg || cfg->get_printer_count() == 0) {
        ESP_LOGW(TAG_BAMBU, "No printer configured in settings, skipping Bambu Monitor init");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get first enabled printer from config
    printer_config_t printer = cfg->get_printer(0);
    if (!printer.enabled) {
        ESP_LOGW(TAG_BAMBU, "First printer is disabled, skipping Bambu Monitor init");
        return ESP_ERR_NOT_FOUND;
    }
    
    if (printer.ip_address.empty() || printer.token.empty()) {
        ESP_LOGW(TAG_BAMBU, "Printer IP or access code not configured");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG_BAMBU, "Using printer: %s at %s", printer.name.c_str(), printer.ip_address.c_str());
    
    // Create Bambu printer configuration
    bambu_printer_config_t bambu_config = {
        .device_id = (char*)printer.serial.c_str(),
        .ip_address = (char*)printer.ip_address.c_str(),
        .port = 8883,                          // MQTT secure port
        .access_code = (char*)printer.token.c_str(),
        .tls_certificate = NULL,               // Not used with skip_verify
        .disable_ssl_verify = printer.disable_ssl_verify,
    };
    
    // Initialize Bambu Monitor
    esp_err_t ret = bambu_monitor_init(&bambu_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_BAMBU, "Failed to initialize Bambu Monitor: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG_BAMBU, "Bambu Monitor initialized successfully");
    return ESP_OK;
}
