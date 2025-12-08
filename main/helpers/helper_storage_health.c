/**
 * @file helper_storage_health.c
 * @brief SD Card and SPIFFS health monitoring implementation
 */

#include "helper_storage_health.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* STORAGE_TAG = "StorageHealth";

// Global storage health state
static storage_health_t g_storage_health = {0};

void storage_health_record_sd_error(void) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Reset counter if outside error window
    if (now - g_storage_health.last_sd_error_time > ERROR_WINDOW_MS) {
        g_storage_health.sd_errors = 0;
    }
    
    g_storage_health.sd_errors++;
    g_storage_health.last_sd_error_time = now;
    
    if (g_storage_health.sd_errors >= SD_ERROR_THRESHOLD) {
        ESP_LOGW(STORAGE_TAG, "SD card error threshold reached (%lu errors in 60s)", 
                 (unsigned long)g_storage_health.sd_errors);
    }
}

void storage_health_record_spiffs_error(void) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Reset counter if outside error window
    if (now - g_storage_health.last_spiffs_error_time > ERROR_WINDOW_MS) {
        g_storage_health.spiffs_errors = 0;
    }
    
    g_storage_health.spiffs_errors++;
    g_storage_health.last_spiffs_error_time = now;
    
    if (g_storage_health.spiffs_errors >= SPIFFS_ERROR_THRESHOLD) {
        ESP_LOGW(STORAGE_TAG, "SPIFFS error threshold reached (%lu errors in 60s)", 
                 (unsigned long)g_storage_health.spiffs_errors);
    }
}

void storage_health_check(void) {
    if (g_storage_health.sd_errors > 0 || g_storage_health.spiffs_errors > 0) {
        ESP_LOGI(STORAGE_TAG, "Status - SD errors: %lu, SPIFFS errors: %lu",
                 (unsigned long)g_storage_health.sd_errors,
                 (unsigned long)g_storage_health.spiffs_errors);
    }
}

void storage_health_get_status(storage_health_t* status) {
    if (status) {
        *status = g_storage_health;
    }
}
