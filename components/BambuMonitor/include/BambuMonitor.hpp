#pragma once

#include "esp_event.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bambu Lab Printer Monitor Component
 * 
 * Handles MQTT communication with Bambu Lab 3D printers,
 * parses printer status, and manages printer state/animations.
 */

ESP_EVENT_DECLARE_BASE(BAMBU_EVENT_BASE);

typedef enum {
    BAMBU_STATUS_UPDATED,
} bambu_event_id_t;

typedef enum {
    BAMBU_STATE_IDLE,
    BAMBU_STATE_PRINTING,
    BAMBU_STATE_PAUSED,
    BAMBU_STATE_ERROR,
    BAMBU_STATE_OFFLINE,
} bambu_printer_state_t;

typedef struct {
    char* device_id;
    char* ip_address;
    uint16_t port;
    char* access_code;
    char* tls_certificate;  // PEM-format TLS certificate (optional if disable_ssl_verify=true)
    bool disable_ssl_verify;  // Skip SSL certificate verification (less secure, easier setup)
} bambu_printer_config_t;

/**
 * @brief Initialize Bambu Monitor component
 * 
 * @param config Printer configuration
 * @return ESP_OK on success
 */
esp_err_t bambu_monitor_init(const bambu_printer_config_t* config);

/**
 * @brief Deinitialize Bambu Monitor component
 * 
 * @return ESP_OK on success
 */
esp_err_t bambu_monitor_deinit(void);

/**
 * @brief Get current printer state
 * 
 * @return Current printer state
 */
bambu_printer_state_t bambu_get_printer_state(void);

/**
 * @brief Get printer status as JSON
 * 
 * @return cJSON object with printer status (caller must free)
 */
cJSON* bambu_get_status_json(void);

/**
 * @brief Register event handler for printer events
 * 
 * @param handler Event handler function
 * @return ESP_OK on success
 */
esp_err_t bambu_register_event_handler(esp_event_handler_t handler);

/**
 * @brief Start MQTT connection (call after WiFi is connected)
 * 
 * @return ESP_OK on success
 */
esp_err_t bambu_monitor_start_mqtt(void);

/**
 * @brief Fetch TLS certificate from printer
 * 
 * @param ip_address Printer IP address
 * @param port TLS port (typically 8883)
 * @return Allocated PEM certificate string (caller must free) or NULL on failure
 */
char* bambu_fetch_tls_certificate(const char* ip_address, uint16_t port);

#ifdef __cplusplus
}
#endif
