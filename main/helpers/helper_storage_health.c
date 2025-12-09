/**
 * @file helper_storage_health.c
 * @brief SD Card and SPIFFS health monitoring implementation
 */

#include "helper_storage_health.h"
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

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

bool storage_backup_config_to_spiffs(void) {
    ESP_LOGI(STORAGE_TAG, "Attempting to backup config to SPIFFS...");
    
    // Copy settings.json from SD card to SPIFFS as backup
    FILE* src = fopen("/sdcard/settings.json", "r");
    if (!src) {
        ESP_LOGW(STORAGE_TAG, "Cannot backup: SD card settings.json not found");
        return false;
    }
    
    FILE* dst = fopen("/spiffs/settings_backup.json", "w");
    if (!dst) {
        fclose(src);
        ESP_LOGE(STORAGE_TAG, "Cannot create SPIFFS backup file");
        return false;
    }
    
    char buffer[512];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            ESP_LOGE(STORAGE_TAG, "Backup write failed");
            fclose(src);
            fclose(dst);
            return false;
        }
    }
    
    fclose(src);
    fclose(dst);
    ESP_LOGI(STORAGE_TAG, "Config backed up to SPIFFS successfully");
    return true;
}

bool storage_restore_config_from_spiffs(void) {
    // Restore settings.json from SPIFFS backup to SD card
    FILE* src = fopen("/spiffs/settings_backup.json", "r");
    if (!src) {
        ESP_LOGW(STORAGE_TAG, "No SPIFFS backup found to restore");
        return false;
    }
    
    FILE* dst = fopen("/sdcard/settings.json", "w");
    if (!dst) {
        fclose(src);
        ESP_LOGE(STORAGE_TAG, "Cannot write to SD card settings.json");
        return false;
    }
    
    char buffer[512];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            ESP_LOGE(STORAGE_TAG, "Restore write failed");
            fclose(src);
            fclose(dst);
            return false;
        }
    }
    
    fclose(src);
    fclose(dst);
    ESP_LOGI(STORAGE_TAG, "Config restored from SPIFFS backup successfully");
    return true;
}

bool storage_format_and_restore_sd(void) {
    ESP_LOGW(STORAGE_TAG, "SD card persistent errors detected - attempting recovery");
    
    // Use the wrapper function from main.cpp to reinitialize SD card
    extern bool reinit_sdspi_wrapper(void);
    if (!reinit_sdspi_wrapper()) {
        ESP_LOGE(STORAGE_TAG, "Failed to reinitialize SD card");
        // Reset error counter anyway to prevent infinite loop
        g_storage_health.sd_errors = 0;
        g_storage_health.last_sd_error_time = 0;
        return false;
    }
    
    // Restore settings from SPIFFS backup if available
    if (!storage_restore_config_from_spiffs()) {
        ESP_LOGW(STORAGE_TAG, "No backup available to restore, will use defaults");
    }
    
    // Reset error counters
    g_storage_health.sd_errors = 0;
    g_storage_health.last_sd_error_time = 0;
    
    ESP_LOGI(STORAGE_TAG, "SD card recovery complete - reset error counters");
    return true;
}
