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
 * This function initializes the multi-printer Bambu Monitor component
 * and adds all configured printers from SettingsConfig.
 * Supports up to 6 simultaneous printer connections.
 * 
 * @return ESP_OK on success
 */
static esp_err_t bambu_helper_init()
{
    ESP_LOGI(TAG_BAMBU, "Initializing Bambu Monitor helper (multi-printer)");
    
    // Initialize the monitor system first
    esp_err_t ret = bambu_monitor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_BAMBU, "Failed to initialize Bambu Monitor system");
        return ret;
    }
    
    // Check if we have any printers configured
    if (!cfg || cfg->get_printer_count() == 0) {
        ESP_LOGW(TAG_BAMBU, "No printers configured in settings");
        return ESP_ERR_NOT_FOUND;
    }
    
    int printer_count = cfg->get_printer_count();
    int added = 0;
    
    ESP_LOGI(TAG_BAMBU, "Found %d configured printer(s), adding up to %d", 
             printer_count, BAMBU_MAX_PRINTERS);
    
    // Add all enabled printers (up to BAMBU_MAX_PRINTERS)
    for (int i = 0; i < printer_count && added < BAMBU_MAX_PRINTERS; i++) {
        printer_config_t printer = cfg->get_printer(i);
        
        if (!printer.enabled) {
            ESP_LOGD(TAG_BAMBU, "Printer %d (%s) is disabled, skipping", i, printer.name.c_str());
            continue;
        }
        
        if (printer.ip_address.empty() || printer.token.empty()) {
            ESP_LOGW(TAG_BAMBU, "Printer %d (%s) missing IP or access code, skipping", 
                     i, printer.name.c_str());
            continue;
        }
        
        if (printer.serial.empty()) {
            ESP_LOGW(TAG_BAMBU, "Printer %d (%s) missing serial number, skipping", 
                     i, printer.name.c_str());
            continue;
        }
        
        ESP_LOGI(TAG_BAMBU, "Adding printer %d: %s at %s (serial: %s)", 
                 i, printer.name.c_str(), printer.ip_address.c_str(), printer.serial.c_str());
        
        // Create Bambu printer configuration
        bambu_printer_config_t bambu_config = {
            .device_id = (char*)printer.serial.c_str(),
            .ip_address = (char*)printer.ip_address.c_str(),
            .port = 8883,
            .access_code = (char*)printer.token.c_str(),
            .tls_certificate = NULL,
            .disable_ssl_verify = printer.disable_ssl_verify,
        };
        
        int idx = bambu_add_printer(&bambu_config);
        if (idx >= 0) {
            ESP_LOGI(TAG_BAMBU, "Printer %s added at slot %d", printer.name.c_str(), idx);
            added++;
        } else {
            ESP_LOGE(TAG_BAMBU, "Failed to add printer %s", printer.name.c_str());
        }
    }
    
    if (added == 0) {
        ESP_LOGW(TAG_BAMBU, "No printers were added");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG_BAMBU, "Bambu Monitor initialized with %d printer(s)", added);
    return ESP_OK;
}

/**
 * @brief Reinitialize Bambu Monitor (e.g., after adding a new printer)
 * 
 * Deinitializes and reinitializes BambuMonitor with updated settings.
 * Call this after modifying printer configuration.
 * 
 * @return ESP_OK on success
 */
esp_err_t reinit_bambu_monitor()
{
    ESP_LOGI(TAG_BAMBU, "Reinitializing Bambu Monitor with updated configuration");
    
    // Deinitialize existing instance
    bambu_monitor_deinit();
    
    // Wait for TLS memory cleanup (each connection needs ~17KB)
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Reinitialize with new config
    esp_err_t ret = bambu_helper_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_BAMBU, "Failed to reinitialize Bambu Monitor");
        return ret;
    }
    
    // Start MQTT connection if WiFi is connected
    ret = bambu_monitor_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG_BAMBU, "Failed to start MQTT after reinit (WiFi may not be ready)");
        return ret;
    }
    
    ESP_LOGI(TAG_BAMBU, "Bambu Monitor reinitialized and MQTT started");
    return ESP_OK;
}
