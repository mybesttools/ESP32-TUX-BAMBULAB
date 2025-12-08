/**
 * @file helper_storage_health.h
 * @brief SD Card and SPIFFS health monitoring header
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error thresholds
#define SD_ERROR_THRESHOLD 5        // Trigger warning after 5 errors
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
} storage_health_t;

/**
 * @brief Record an SD card error
 */
void storage_health_record_sd_error(void);

/**
 * @brief Record a SPIFFS error
 */
void storage_health_record_spiffs_error(void);

/**
 * @brief Log current storage health status
 */
void storage_health_check(void);

/**
 * @brief Get current storage health status
 * @param status Pointer to structure to fill with current status
 */
void storage_health_get_status(storage_health_t* status);

#ifdef __cplusplus
}
#endif
