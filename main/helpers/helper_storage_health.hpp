/**
 * @file helper_storage_health.hpp
 * @brief SD Card and SPIFFS health monitoring - simplified version
 * 
 * Tracks file system errors for monitoring and alerting
 */

#pragma once

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* STORAGE_TAG = "StorageHealth";

// Error thresholds
#define SD_ERROR_THRESHOLD 5        // Trigger warning after 5 errors
#define SD_CRITICAL_THRESHOLD 10    // Trigger auto-recovery after 10 errors
#define SPIFFS_ERROR_THRESHOLD 10   // Trigger warning after 10 errors
#define ERROR_WINDOW_MS 60000       // 60 second error counting window

// Storage health status
typedef struct {
    uint32_t sd_errors;
    uint32_t spiffs_errors;
    uint32_t last_sd_error_time;
    uint32_t last_spiffs_error_time;
    bool sd_mounted;
    bool spiffs_mounted;
    bool sd_recovery_in_progress;
} storage_health_t;

// Global storage health state
static storage_health_t g_storage_health = {0};

/**
 * @brief Record an SD card error
 */
static inline void storage_health_record_sd_error() {
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
    
    // Check if we need to trigger recovery
    storage_check_and_recover_sd();
}

/**
 * @brief Record a SPIFFS error
 */
static inline void storage_health_record_spiffs_error() {
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

/**
 * @brief Run periodic health check
 * Call this from a timer or periodic task
 */
static inline void storage_health_check() {
    // Log current error counts if any errors present
    if (g_storage_health.sd_errors > 0 || g_storage_health.spiffs_errors > 0) {
        ESP_LOGI(STORAGE_TAG, "Error counts - SD: %lu, SPIFFS: %lu",
                 (unsigned long)g_storage_health.sd_errors, (unsigned long)g_storage_health.spiffs_errors);
    }
}

/**
 * @brief Get current storage health status
 * @return Pointer to storage health structure
 */
static inline const storage_health_t* storage_health_get_status() {
    return &g_storage_health;
}

/**
 * @brief Initialize storage health monitoring
 */
static inline void storage_health_init(bool sd_mounted, bool spiffs_mounted) {
    g_storage_health.sd_mounted = sd_mounted;
    g_storage_health.spiffs_mounted = spiffs_mounted;
    g_storage_health.sd_recovery_in_progress = false;
    
    ESP_LOGI(STORAGE_TAG, "Storage health monitor initialized (SD:%s, SPIFFS:%s)",
             sd_mounted ? "YES" : "NO", spiffs_mounted ? "YES" : "NO");
}

/**
 * @brief Backup config from SD card to SPIFFS
 * @return ESP_OK on success
 */
static inline esp_err_t storage_backup_config_to_spiffs() {
    ESP_LOGI(STORAGE_TAG, "Backing up config from SD to SPIFFS...");
    
    // Read from SD card
    FILE* src = fopen("/sdcard/settings.json", "r");
    if (!src) {
        ESP_LOGW(STORAGE_TAG, "No config file on SD card to backup");
        return ESP_OK;  // Not an error - file may not exist
    }
    
    // Get file size
    fseek(src, 0, SEEK_END);
    long size = ftell(src);
    fseek(src, 0, SEEK_SET);
    
    if (size <= 0 || size > 32768) {  // Sanity check: max 32KB
        ESP_LOGE(STORAGE_TAG, "Invalid config file size: %ld", size);
        fclose(src);
        return ESP_FAIL;
    }
    
    // Allocate buffer
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        ESP_LOGE(STORAGE_TAG, "Failed to allocate %ld bytes for backup", size);
        fclose(src);
        return ESP_ERR_NO_MEM;
    }
    
    // Read data
    size_t read = fread(buffer, 1, size, src);
    fclose(src);
    
    if (read != (size_t)size) {
        ESP_LOGE(STORAGE_TAG, "Failed to read config (got %zu of %ld bytes)", read, size);
        free(buffer);
        return ESP_FAIL;
    }
    
    buffer[size] = '\0';
    
    // Write to SPIFFS
    FILE* dst = fopen("/spiffs/settings.json.backup", "w");
    if (!dst) {
        ESP_LOGE(STORAGE_TAG, "Failed to open SPIFFS backup file");
        free(buffer);
        return ESP_FAIL;
    }
    
    size_t written = fwrite(buffer, 1, size, dst);
    fclose(dst);
    free(buffer);
    
    if (written != (size_t)size) {
        ESP_LOGE(STORAGE_TAG, "Failed to write backup (wrote %zu of %ld bytes)", written, size);
        return ESP_FAIL;
    }
    
    ESP_LOGI(STORAGE_TAG, "Config backed up to SPIFFS (%ld bytes)", size);
    return ESP_OK;
}

/**
 * @brief Format SD card and restore config
 * This is a stub - actual implementation should be in main.cpp where sdcard handle is available
 * @return ESP_OK on success
 */
static inline esp_err_t storage_format_and_restore_sd() {
    ESP_LOGW(STORAGE_TAG, "=== SD card recovery requested ===");
    ESP_LOGE(STORAGE_TAG, "SD card format/restore not yet implemented - needs sdcard handle");
    ESP_LOGW(STORAGE_TAG, "Manual recovery: 1) Backup config, 2) Reformat SD card, 3) Reboot");
    return ESP_FAIL;
}

/**
 * @brief Trigger SD card recovery if critical threshold reached
 */
static inline void storage_check_and_recover_sd() {
    if (g_storage_health.sd_recovery_in_progress) {
        return;  // Already recovering
    }
    
    if (g_storage_health.sd_errors >= SD_CRITICAL_THRESHOLD) {
        ESP_LOGE(STORAGE_TAG, "SD card critical error threshold reached!");
        g_storage_health.sd_recovery_in_progress = true;
        
        // Backup config to SPIFFS
        esp_err_t ret = storage_backup_config_to_spiffs();
        if (ret == ESP_OK) {
            // Format and restore
            ret = storage_format_and_restore_sd();
            if (ret == ESP_OK) {
                // Reset error counter on successful recovery
                g_storage_health.sd_errors = 0;
                ESP_LOGI(STORAGE_TAG, "SD card recovery successful - errors reset");
            } else {
                ESP_LOGE(STORAGE_TAG, "SD card recovery failed!");
            }
        } else {
            ESP_LOGE(STORAGE_TAG, "Config backup failed - aborting recovery");
        }
        
        g_storage_health.sd_recovery_in_progress = false;
    }
}
