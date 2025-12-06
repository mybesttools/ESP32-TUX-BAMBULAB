#include "BambuMonitor.hpp"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "esp_tls.h"
#include "mbedtls/base64.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/pem.h"
#include <cstring>
#include <stdlib.h>

static const char* TAG = "BambuMonitor";

// Define event base
ESP_EVENT_DEFINE_BASE(BAMBU_EVENT_BASE);

// Global state
static bambu_printer_state_t current_state = BAMBU_STATE_OFFLINE;
static cJSON* last_status = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static esp_event_handler_t registered_handler = NULL;
static bool mqtt_initialized = false;
static char* cached_tls_certificate = NULL;  // Dynamically fetched and cached
static bambu_printer_config_t saved_config = {0};  // Save config for later use

// Embedded Bambu Lab root certificates (combined)
// These are sourced from Home Assistant integration: https://github.com/greghesp/ha-bambulab
// Contains all three Bambu Lab CA certificates: bambu.cert, bambu_p2s_250626.cert, bambu_h2c_251122.cert
extern const uint8_t bambu_cert_start[] asm("_binary_bambu_combined_cert_start");
extern const uint8_t bambu_cert_end[] asm("_binary_bambu_combined_cert_end");

/**
 * @brief MQTT event handler for Bambu printer communication
 */
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, 
                              int32_t event_id, void* event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT: {
            ESP_LOGD(TAG, "MQTT Before Connect");
            break;
        }
            
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "MQTT Connected to Bambu printer");
            // Subscribe to printer status topic for all devices
            int msg_id = esp_mqtt_client_subscribe(mqtt_client, "device/+/report", 1);
            ESP_LOGI(TAG, "Subscribed to device/+/report (msg_id: %d)", msg_id);
            current_state = BAMBU_STATE_IDLE;
            break;
        }
            
        case MQTT_EVENT_DISCONNECTED: {
            ESP_LOGW(TAG, "MQTT Disconnected from printer");
            current_state = BAMBU_STATE_OFFLINE;
            break;
        }
            
        case MQTT_EVENT_SUBSCRIBED: {
            ESP_LOGD(TAG, "MQTT Subscribed (msg_id: %d)", event->msg_id);
            break;
        }
            
        case MQTT_EVENT_UNSUBSCRIBED: {
            ESP_LOGD(TAG, "MQTT Unsubscribed (msg_id: %d)", event->msg_id);
            break;
        }
            
        case MQTT_EVENT_PUBLISHED: {
            ESP_LOGD(TAG, "MQTT Message published (msg_id: %d)", event->msg_id);
            break;
        }
            
        case MQTT_EVENT_DATA: {
            // Extract topic and payload safely
            char topic[128] = {0};
            if (event->topic_len > 0 && event->topic_len < sizeof(topic)) {
                strncpy(topic, event->topic, event->topic_len);
            }
            
            // Extract serial number from topic (format: device/SERIAL/report)
            // This is how Home Assistant does it instead of HTTP queries
            if (strncmp(topic, "device/", 7) == 0) {
                const char* serial_start = topic + 7;
                const char* serial_end = strchr(serial_start, '/');
                if (serial_end) {
                    size_t serial_len = serial_end - serial_start;
                    if (serial_len > 0 && serial_len < 64) {
                        char serial[64] = {0};
                        strncpy(serial, serial_start, serial_len);
                        ESP_LOGI(TAG, "Printer serial from MQTT topic: %s", serial);
                        // TODO: Save serial to config if not already set
                    }
                }
            }
            
            ESP_LOGD(TAG, "MQTT Data received on topic: %s (len: %d, data_len: %d)", 
                    topic, event->topic_len, event->data_len);
            
            // Parse JSON payload (limit to reasonable size)
            if (event->data_len > 0 && event->data_len <= 8192) {
                // Create null-terminated string for JSON parsing
                char* json_str = (char*)malloc(event->data_len + 1);
                if (json_str) {
                    strncpy(json_str, event->data, event->data_len);
                    json_str[event->data_len] = '\0';
                    
                    cJSON* json_data = cJSON_Parse(json_str);
                    if (json_data) {
                        if (last_status) {
                            cJSON_Delete(last_status);
                        }
                        last_status = json_data;
                        
                        // Extract printer state
                        cJSON* print_state = cJSON_GetObjectItem(json_data, "print_state");
                        if (print_state && print_state->valuestring) {
                            ESP_LOGD(TAG, "Printer state: %s", print_state->valuestring);
                            
                            if (strcmp(print_state->valuestring, "PRINTING") == 0) {
                                current_state = BAMBU_STATE_PRINTING;
                            } else if (strcmp(print_state->valuestring, "PAUSED") == 0) {
                                current_state = BAMBU_STATE_PAUSED;
                            } else if (strcmp(print_state->valuestring, "ERROR") == 0) {
                                current_state = BAMBU_STATE_ERROR;
                            } else {
                                current_state = BAMBU_STATE_IDLE;
                            }
                        }
                        
                        // Extract temperatures if available
                        cJSON* nozzle_temp = cJSON_GetObjectItem(json_data, "nozzle_temper");
                        cJSON* bed_temp = cJSON_GetObjectItem(json_data, "bed_temper");
                        if (nozzle_temp || bed_temp) {
                            ESP_LOGD(TAG, "Temperatures - Nozzle: %d°C, Bed: %d°C",
                                    nozzle_temp ? (int)nozzle_temp->valuedouble : 0,
                                    bed_temp ? (int)bed_temp->valuedouble : 0);
                        }
                        
                        // Notify registered handler if set
                        if (registered_handler) {
                            registered_handler(NULL, BAMBU_EVENT_BASE, 
                                             BAMBU_STATUS_UPDATED, (void*)current_state);
                        }
                    } else {
                        ESP_LOGW(TAG, "Failed to parse JSON from printer");
                    }
                    
                    free(json_str);
                } else {
                    ESP_LOGE(TAG, "Failed to allocate memory for JSON parsing");
                }
            } else {
                ESP_LOGW(TAG, "Payload too large or invalid (len: %d)", event->data_len);
            }
            break;
        }
            
        case MQTT_EVENT_ERROR: {
            ESP_LOGE(TAG, "MQTT Error");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", 
                        event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Last tls stack error number: 0x%x", 
                        event->error_handle->esp_tls_stack_err);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused");
            }
            current_state = BAMBU_STATE_OFFLINE;
            break;
        }
            
        default: {
            ESP_LOGD(TAG, "Other MQTT event: %d", event->event_id);
            break;
        }
    }
}

/**
 * @brief Fetch TLS certificate from Bambu printer
 * 
 * NOTE: Certificate fetching during query is complex and causing memory issues.
 * For now, we'll skip this step - certificates should be pre-configured or
 * we can add a separate "fetch certificate" button in the UI.
 * 
 * @param ip_address Printer IP address  
 * @param port TLS port (typically 8883)
 * @return NULL (certificate fetch skipped for stability)
 */
char* bambu_fetch_tls_certificate(const char* ip_address, uint16_t port)
{
    if (!ip_address) {
        ESP_LOGE(TAG, "Invalid IP address for certificate fetch");
        return NULL;
    }

    ESP_LOGW(TAG, "Certificate fetch skipped - configure manually or use pre-extracted cert");
    ESP_LOGW(TAG, "To extract cert: openssl s_client -connect %s:%d -showcerts < /dev/null 2>/dev/null | openssl x509 -outform PEM", 
             ip_address, port);
    
    return NULL;
}

esp_err_t bambu_monitor_init(const bambu_printer_config_t* config)
{
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing Bambu Monitor for device: %s at %s:%d", 
             config->device_id, config->ip_address, config->port);
    
    // Save config for later use (for queries)
    saved_config.device_id = config->device_id ? strdup(config->device_id) : NULL;
    saved_config.ip_address = config->ip_address ? strdup(config->ip_address) : NULL;
    saved_config.port = config->port;
    saved_config.access_code = config->access_code ? strdup(config->access_code) : NULL;
    saved_config.tls_certificate = config->tls_certificate ? strdup(config->tls_certificate) : NULL;
    saved_config.disable_ssl_verify = config->disable_ssl_verify;
    
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.hostname = config->ip_address;
    mqtt_cfg.broker.address.port = config->port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
    
    // SSL/TLS Configuration - INSECURE MODE (bypass certificate validation)
    // Bambu Lab printers use self-signed certificates that fail mbedTLS strict validation
    // Home Assistant integration uses the same approach: ssl.CERT_NONE when disable_ssl_verify=True
    // This is safe for LAN-only connections to known local devices
    ESP_LOGI(TAG, "Configuring SSL in insecure mode (no certificate validation)");
    
    // With CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY=y in sdkconfig, esp-tls will skip verification
    // We still need to set skip_cert_common_name_check to prevent name validation
    mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
    
    ESP_LOGI(TAG, "SSL certificate validation disabled via CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY");
    
    // Authentication
    mqtt_cfg.credentials.username = "bblp";
    mqtt_cfg.credentials.authentication.password = config->access_code;
    
    // Buffer settings - Bambu printers send large JSON payloads (10KB+)
    mqtt_cfg.buffer.size = 16384;       // 16KB receive buffer
    mqtt_cfg.buffer.out_size = 4096;    // 4KB send buffer
    
    // Task settings - increase stack size for larger buffers
    mqtt_cfg.task.priority = 4;         // Priority 4 (lower than LVGL task which is 5)
    mqtt_cfg.task.stack_size = 8192;    // 8KB stack
    
    // Session settings
    mqtt_cfg.session.keepalive = 60;
    mqtt_cfg.session.disable_clean_session = false;
    
    // Create MQTT client
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return ESP_FAIL;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // DO NOT start MQTT client yet - wait for WiFi to connect first
    // The MQTT client will be started when WiFi connection is established
    
    ESP_LOGI(TAG, "Bambu Monitor configured (MQTT will connect after WiFi is ready)");
    current_state = BAMBU_STATE_IDLE;
    return ESP_OK;
}

bambu_printer_state_t bambu_get_printer_state(void)
{
    return current_state;
}

cJSON* bambu_get_status_json(void)
{
    if (!last_status) {
        return NULL;
    }
    return cJSON_Duplicate(last_status, 1);
}

esp_err_t bambu_register_event_handler(esp_event_handler_t handler)
{
    registered_handler = handler;
    return ESP_OK;
}

esp_err_t bambu_monitor_start(void)
{
    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Starting MQTT client connection to printer");
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t bambu_monitor_deinit(void)
{
    if (mqtt_client) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    
    if (last_status) {
        cJSON_Delete(last_status);
        last_status = NULL;
    }
    
    registered_handler = NULL;
    current_state = BAMBU_STATE_OFFLINE;
    
    ESP_LOGI(TAG, "Bambu Monitor deinitialized");
    return ESP_OK;
}

/**
 * @brief Connect to printer MQTT broker (can be called after init)
 */
esp_err_t bambu_connect(const char* ip_address, uint16_t port, const char* access_code)
{
    if (mqtt_client) {
        ESP_LOGW(TAG, "MQTT client already initialized");
        return ESP_OK;
    }
    
    if (!ip_address || !access_code) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configure MQTT client with TLS support for Bambu Lab printers
    esp_mqtt_client_config_t mqtt_cfg = {};
    
    // Broker configuration
    mqtt_cfg.broker.address.hostname = ip_address;
    mqtt_cfg.broker.address.port = port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
    
    // Broker verification - skip strict CN check for self-signed Bambu certs
    mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
    
    // Client credentials
    mqtt_cfg.credentials.client_id = "esp32_tux";
    mqtt_cfg.credentials.username = "bblp";
    mqtt_cfg.credentials.authentication.password = access_code;
    
    // Buffer configuration
    mqtt_cfg.buffer.size = 8192;
    
    // Network timeouts
    mqtt_cfg.network.reconnect_timeout_ms = 10000;
    mqtt_cfg.network.timeout_ms = 30000;
    
    // Keep-alive
    mqtt_cfg.session.keepalive = 60;
    
    ESP_LOGI(TAG, "Creating MQTT connection to %s:%d", ip_address, port);
    
    // Create MQTT client
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, 
                                  mqtt_event_handler, NULL);
    
    // Start MQTT client
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT connection initiated");
    return ESP_OK;
}

/**
 * @brief Send MQTT query request to printer
 * 
 * Sends a "pushall" request to get complete printer status.
 * The printer will respond on device/{serial}/report topic.
 */
esp_err_t bambu_send_query(void)
{
    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }
    
    // Build query topic: device/{device_id}/request
    char topic[128];
    snprintf(topic, sizeof(topic), "device/%s/request", 
             saved_config.device_id ? saved_config.device_id : "unknown");
    
    // Standard Bambu "pushall" command to request full status
    const char* pushall_cmd = "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}";
    
    ESP_LOGI(TAG, "Sending pushall query to %s", topic);
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, pushall_cmd, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish query (msg_id: %d)", msg_id);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Query sent successfully (msg_id: %d)", msg_id);
    return ESP_OK;
}

/**
 * @brief Send custom MQTT command to printer
 */
esp_err_t bambu_send_command(const char* command)
{
    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }
    
    if (!command) {
        ESP_LOGE(TAG, "Command is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build request topic
    char topic[128];
    snprintf(topic, sizeof(topic), "device/%s/request", 
             saved_config.device_id ? saved_config.device_id : "unknown");
    
    ESP_LOGI(TAG, "Sending command to %s: %s", topic, command);
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, command, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish command (msg_id: %d)", msg_id);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Command sent successfully (msg_id: %d)", msg_id);
    return ESP_OK;
}
