#ifndef BAMBU_MQTT_CLIENT_HPP
#define BAMBU_MQTT_CLIENT_HPP

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom lightweight MQTT-over-TLS client for Bambu Lab printers
 * 
 * This client provides full control over TLS settings, specifically supporting
 * TLS without certificate verification (matching Python's ssl.CERT_NONE).
 * 
 * The ESP-IDF mqtt_client doesn't properly support insecure TLS mode despite
 * CONFIG_ESP_TLS_INSECURE flags, so we implement our own using raw sockets + mbedtls.
 */

typedef enum {
    BAMBU_MQTT_STATE_DISCONNECTED = 0,
    BAMBU_MQTT_STATE_CONNECTING,
    BAMBU_MQTT_STATE_CONNECTED,
    BAMBU_MQTT_STATE_ERROR
} bambu_mqtt_state_t;

typedef enum {
    BAMBU_MQTT_EVENT_CONNECTED,
    BAMBU_MQTT_EVENT_SUBSCRIBED,
    BAMBU_MQTT_EVENT_DISCONNECTED,
    BAMBU_MQTT_EVENT_DATA,
    BAMBU_MQTT_EVENT_ERROR
} bambu_mqtt_event_type_t;

typedef struct {
    bambu_mqtt_event_type_t event_type;
    const char* topic;
    const char* data;
    int data_len;
    int error_code;
} bambu_mqtt_event_t;

typedef void (*bambu_mqtt_event_callback_t)(bambu_mqtt_event_t* event, void* user_data);

typedef struct {
    const char* host;
    uint16_t port;
    const char* username;
    const char* password;
    const char* client_id;
    bool use_tls;
    bool verify_cert;  // false = insecure mode (like Python ssl.CERT_NONE)
    bambu_mqtt_event_callback_t event_callback;
    void* user_data;
    int keepalive_seconds;
    int task_stack_size;
    int task_priority;
} bambu_mqtt_config_t;

typedef struct bambu_mqtt_client* bambu_mqtt_client_handle_t;

/**
 * @brief Initialize MQTT client
 */
bambu_mqtt_client_handle_t bambu_mqtt_init(const bambu_mqtt_config_t* config);

/**
 * @brief Start MQTT connection
 */
int bambu_mqtt_start(bambu_mqtt_client_handle_t client);

/**
 * @brief Stop MQTT connection
 */
int bambu_mqtt_stop(bambu_mqtt_client_handle_t client);

/**
 * @brief Subscribe to topic
 */
int bambu_mqtt_subscribe(bambu_mqtt_client_handle_t client, const char* topic, int qos);

/**
 * @brief Publish message
 */
int bambu_mqtt_publish(bambu_mqtt_client_handle_t client, const char* topic, 
                       const char* data, int len, int qos, int retain);

/**
 * @brief Destroy MQTT client
 */
int bambu_mqtt_destroy(bambu_mqtt_client_handle_t client);

/**
 * @brief Get current connection state
 */
bambu_mqtt_state_t bambu_mqtt_get_state(bambu_mqtt_client_handle_t client);

#ifdef __cplusplus
}
#endif

#endif // BAMBU_MQTT_CLIENT_HPP
