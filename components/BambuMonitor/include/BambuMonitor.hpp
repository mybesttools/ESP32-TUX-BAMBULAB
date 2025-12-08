#pragma once

#include "esp_event.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bambu Lab Printer Monitor Component
 * 
 * Handles MQTT communication with up to 6 Bambu Lab 3D printers simultaneously,
 * parses printer status, and manages printer state/animations.
 */

// Maximum number of simultaneous printer connections
#define BAMBU_MAX_PRINTERS 6

ESP_EVENT_DECLARE_BASE(BAMBU_EVENT_BASE);

typedef enum {
    BAMBU_STATUS_UPDATED,
    BAMBU_PRINTER_CONNECTED,
    BAMBU_PRINTER_DISCONNECTED,
} bambu_event_id_t;

typedef enum {
    BAMBU_STATE_IDLE,
    BAMBU_STATE_PRINTING,
    BAMBU_STATE_PAUSED,
    BAMBU_STATE_ERROR,
    BAMBU_STATE_OFFLINE,
} bambu_printer_state_t;

typedef struct {
    char* device_id;        // Serial number / device ID
    char* ip_address;       // Printer IP address
    uint16_t port;          // MQTT port (typically 8883)
    char* access_code;      // LAN access code
    char* tls_certificate;  // PEM-format TLS certificate (optional)
    bool disable_ssl_verify;  // Skip SSL certificate verification
} bambu_printer_config_t;

/**
 * @brief Initialize Bambu Monitor component (multi-printer support)
 * 
 * Initializes the monitor system. Call this once at startup.
 * 
 * @return ESP_OK on success
 */
esp_err_t bambu_monitor_init(void);

/**
 * @brief Initialize with a single printer config (legacy compatibility)
 * 
 * @param config Printer configuration
 * @return ESP_OK on success
 */
esp_err_t bambu_monitor_init_single(const bambu_printer_config_t* config);

/**
 * @brief Add a printer to monitor
 * 
 * @param config Printer configuration
 * @return Printer index (0-5) on success, -1 on failure
 */
int bambu_add_printer(const bambu_printer_config_t* config);

/**
 * @brief Remove a printer from monitoring
 * 
 * @param index Printer index to remove
 * @return ESP_OK on success
 */
esp_err_t bambu_remove_printer(int index);

/**
 * @brief Get number of active printer connections
 * 
 * @return Number of connected printers
 */
int bambu_get_printer_count(void);

/**
 * @brief Deinitialize Bambu Monitor component
 * 
 * Disconnects all printers and frees resources.
 * 
 * @return ESP_OK on success
 */
esp_err_t bambu_monitor_deinit(void);

/**
 * @brief Get printer state by index
 * 
 * @param index Printer index (0-5)
 * @return Current printer state
 */
bambu_printer_state_t bambu_get_printer_state(int index);

/**
 * @brief Get printer state (legacy - returns first printer state)
 */
bambu_printer_state_t bambu_get_printer_state_default(void);

/**
 * @brief Get printer status as JSON by index
 * 
 * @param index Printer index (0-5)
 * @return cJSON object with printer status (caller must free)
 */
cJSON* bambu_get_status_json(int index);

/**
 * @brief Register event handler for printer events
 * 
 * @param handler Event handler function
 * @return ESP_OK on success
 */
esp_err_t bambu_register_event_handler(esp_event_handler_t handler);

/**
 * @brief Start all MQTT connections (call after WiFi is connected)
 * 
 * @return ESP_OK on success
 */
esp_err_t bambu_monitor_start(void);

/**
 * @brief Start MQTT connection for a specific printer
 * 
 * @param index Printer index (0-5)
 * @return ESP_OK on success
 */
esp_err_t bambu_start_printer(int index);

/**
 * @brief Stop MQTT connection for a specific printer
 * 
 * @param index Printer index (0-5)
 * @return ESP_OK on success
 */
esp_err_t bambu_stop_printer(int index);

/**
 * @brief Send MQTT query request to a specific printer
 * 
 * @param index Printer index (0-5)
 * @return ESP_OK on success
 */
esp_err_t bambu_send_query_index(int index);

/**
 * @brief Send MQTT query to all connected printers
 * 
 * @return ESP_OK if at least one query sent successfully
 */
esp_err_t bambu_send_query(void);

/**
 * @brief Send custom MQTT command to a specific printer
 * 
 * @param index Printer index (0-5)
 * @param command JSON command string to send
 * @return ESP_OK on success
 */
esp_err_t bambu_send_command(int index, const char* command);

/**
 * @brief Get printer serial/device ID by index
 * 
 * @param index Printer index (0-5)
 * @return Device ID string or NULL if not configured
 */
const char* bambu_get_device_id(int index);

/**
 * @brief Check if a printer slot is in use
 * 
 * @param index Printer index (0-5)
 * @return true if printer is configured at this index
 */
bool bambu_is_printer_active(int index);

/**
 * @brief Reset SD card availability check
 * 
 * Call this after remounting the SD card to force a re-check.
 */
void bambu_reset_sdcard_check(void);

#ifdef __cplusplus
}
#endif
