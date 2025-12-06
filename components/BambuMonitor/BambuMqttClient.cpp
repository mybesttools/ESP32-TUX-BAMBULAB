/**
 * @file BambuMqttClient.cpp
 * @brief Custom MQTT-over-TLS client for Bambu Lab printers
 * 
 * This implementation provides full control over TLS settings, specifically
 * supporting insecure TLS mode (no certificate verification) which matches
 * Python's ssl.CERT_NONE behavior that successfully connects to Bambu printers.
 */

#include "BambuMqttClient.hpp"
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

static const char* TAG = "BambuMQTT";

// MQTT Control Packet Types
#define MQTT_CONNECT     0x10
#define MQTT_CONNACK     0x20
#define MQTT_PUBLISH     0x30
#define MQTT_PUBACK      0x40
#define MQTT_SUBSCRIBE   0x82
#define MQTT_SUBACK      0x90
#define MQTT_PINGREQ     0xC0
#define MQTT_PINGRESP    0xD0
#define MQTT_DISCONNECT  0xE0

struct bambu_mqtt_client {
    bambu_mqtt_config_t config;
    bambu_mqtt_state_t state;
    
    // Network
    int socket_fd;
    mbedtls_net_context net_ctx;
    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config ssl_conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    
    // MQTT
    uint16_t packet_id;
    uint32_t last_ping_time;
    
    // Threading
    TaskHandle_t task_handle;
    SemaphoreHandle_t mutex;
    bool running;
};

// Helper: Write remaining length field
static int write_remaining_length(uint8_t* buf, int length) {
    int bytes = 0;
    do {
        uint8_t byte = length % 128;
        length /= 128;
        if (length > 0) byte |= 0x80;
        buf[bytes++] = byte;
    } while (length > 0);
    return bytes;
}

// Helper: Read remaining length field
static int read_remaining_length(bambu_mqtt_client_handle_t client, int* length) {
    *length = 0;
    int multiplier = 1;
    int bytes = 0;
    
    do {
        uint8_t byte;
        int ret = mbedtls_ssl_read(&client->ssl_ctx, &byte, 1);
        if (ret <= 0) return ret;
        
        *length += (byte & 0x7F) * multiplier;
        multiplier *= 128;
        bytes++;
        
        if (!(byte & 0x80)) break;
        if (bytes > 4) return -1; // Invalid
    } while (true);
    
    return bytes;
}

// Helper: Write string with length prefix
static int write_string(uint8_t* buf, const char* str) {
    uint16_t len = strlen(str);
    buf[0] = (len >> 8) & 0xFF;
    buf[1] = len & 0xFF;
    memcpy(buf + 2, str, len);
    return len + 2;
}

// Send MQTT CONNECT packet
static int send_connect(bambu_mqtt_client_handle_t client) {
    uint8_t buf[256];
    int pos = 0;
    
    // Fixed header
    buf[pos++] = MQTT_CONNECT;
    
    // Variable header
    int payload_start = pos + 5; // Reserve space for remaining length
    pos = payload_start;
    
    // Protocol name "MQTT"
    pos += write_string(buf + pos, "MQTT");
    
    // Protocol level (4 = MQTT 3.1.1)
    buf[pos++] = 0x04;
    
    // Connect flags (username + password + clean session)
    buf[pos++] = 0xC2;
    
    // Keep alive (seconds)
    uint16_t keepalive = client->config.keepalive_seconds;
    buf[pos++] = (keepalive >> 8) & 0xFF;
    buf[pos++] = keepalive & 0xFF;
    
    // Payload: Client ID
    pos += write_string(buf + pos, client->config.client_id);
    
    // Username
    pos += write_string(buf + pos, client->config.username);
    
    // Password
    pos += write_string(buf + pos, client->config.password);
    
    // Write remaining length
    int remaining_len = pos - payload_start;
    int len_bytes = write_remaining_length(buf + 1, remaining_len);
    
    // Shift payload if needed
    if (len_bytes != 4) {
        memmove(buf + 1 + len_bytes, buf + payload_start, remaining_len);
        pos = 1 + len_bytes + remaining_len;
    }
    
    ESP_LOGI(TAG, "Sending CONNECT packet (%d bytes)", pos);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, pos, ESP_LOG_INFO);
    int ret = mbedtls_ssl_write(&client->ssl_ctx, buf, pos);
    return ret > 0 ? 0 : ret;
}

// Send MQTT SUBSCRIBE packet
static int send_subscribe(bambu_mqtt_client_handle_t client, const char* topic, int qos) {
    uint8_t buf[128];  // Reduced from 256 to save stack
    int pos = 0;
    
    // Fixed header
    buf[pos++] = MQTT_SUBSCRIBE;
    
    //Reserve 1 byte for remaining length (we'll fill it in later)
    int remaining_len_pos = pos++;
    
    // Packet ID
    uint16_t packet_id = ++client->packet_id;
    buf[pos++] = (packet_id >> 8) & 0xFF;
    buf[pos++] = packet_id & 0xFF;
    
    // Topic filter
    pos += write_string(buf + pos, topic);
    
    // QoS
    buf[pos++] = qos & 0x03;
    
    // Calculate and write remaining length
    int remaining_len = pos - remaining_len_pos - 1;
    int len_bytes = write_remaining_length(buf + remaining_len_pos, remaining_len);
    
    // If remaining length needs more than 1 byte, shift payload
    if (len_bytes > 1) {
        memmove(buf + remaining_len_pos + len_bytes, buf + remaining_len_pos + 1, remaining_len);
        pos += (len_bytes - 1);
    }
    
    ESP_LOGI(TAG, "Sending SUBSCRIBE to '%s' (qos=%d)", topic, qos);
    
    // Log packet hex for debugging
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, pos, ESP_LOG_INFO);
    
    int ret = mbedtls_ssl_write(&client->ssl_ctx, buf, pos);
    return ret > 0 ? 0 : ret;
}

static int send_publish(bambu_mqtt_client_handle_t client, const char* topic, const char* payload, int qos) {
    uint8_t buf[256];  // Reduced from 512 to save stack
    int pos = 0;
    
    // Fixed header (PUBLISH with QoS)
    buf[pos++] = MQTT_PUBLISH | (qos > 0 ? 0x02 : 0x00);  // QoS in bits 1-2
    
    int payload_start = pos + 2;
    pos = payload_start;
    
    // Topic
    pos += write_string(buf + pos, topic);
    
    // Packet ID (only for QoS > 0)
    if (qos > 0) {
        uint16_t packet_id = ++client->packet_id;
        buf[pos++] = (packet_id >> 8) & 0xFF;
        buf[pos++] = packet_id & 0xFF;
    }
    
    // Payload
    int payload_len = strlen(payload);
    memcpy(buf + pos, payload, payload_len);
    pos += payload_len;
    
    // Write remaining length
    int remaining_len = pos - payload_start;
    int len_bytes = write_remaining_length(buf + 1, remaining_len);
    
    if (len_bytes != 1) {
        memmove(buf + 1 + len_bytes, buf + payload_start, remaining_len);
        pos = 1 + len_bytes + remaining_len;
    }
    
    ESP_LOGI(TAG, "Sending PUBLISH to '%s' (qos=%d, payload_len=%d)", topic, qos, payload_len);
    int ret = mbedtls_ssl_write(&client->ssl_ctx, buf, pos);
    return ret > 0 ? 0 : ret;
}

// Send PING request
static int send_ping(bambu_mqtt_client_handle_t client) {
    uint8_t buf[2] = {MQTT_PINGREQ, 0x00};
    int ret = mbedtls_ssl_write(&client->ssl_ctx, buf, 2);
    if (ret > 0) {
        client->last_ping_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    return ret > 0 ? 0 : ret;
}

// Process incoming MQTT packet
static int process_packet(bambu_mqtt_client_handle_t client) {
    uint8_t header;
    int ret = mbedtls_ssl_read(&client->ssl_ctx, &header, 1);
    if (ret <= 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return 0;  // No data available, not an error
        }
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            ESP_LOGI(TAG, "Server closed connection cleanly (close_notify)");
            return -1;  // Signal to exit gracefully
        }
        ESP_LOGE(TAG, "Failed to read header: 0x%X", -ret);
        return ret;
    }
    
    ESP_LOGD(TAG, "Got header byte: 0x%02X", header);
    
    int remaining_len;
    ret = read_remaining_length(client, &remaining_len);
    if (ret == 0) {
        // No complete length yet, wait for more data
        ESP_LOGD(TAG, "Waiting for remaining length bytes...");
        return 0;
    }
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to read remaining length: 0x%X", -ret);
        return ret;
    }
    
    ESP_LOGD(TAG, "Received packet: type=0x%02X, len=%d", header & 0xF0, remaining_len);
    
    switch (header & 0xF0) {
        case MQTT_CONNACK: {
            uint8_t connack[2];
            mbedtls_ssl_read(&client->ssl_ctx, connack, 2);
            if (connack[1] == 0) {
                ESP_LOGI(TAG, "MQTT Connected!");
                client->state = BAMBU_MQTT_STATE_CONNECTED;
                if (client->config.event_callback) {
                    bambu_mqtt_event_t event = {
                        .event_type = BAMBU_MQTT_EVENT_CONNECTED
                    };
                    client->config.event_callback(&event, client->config.user_data);
                }
            } else {
                ESP_LOGE(TAG, "CONNACK failed: %d", connack[1]);
                return -1;
            }
            break;
        }
        
        case MQTT_PUBLISH: {
            // Read topic length (2 bytes, big-endian)
            uint8_t len_bytes[2];
            if (mbedtls_ssl_read(&client->ssl_ctx, len_bytes, 2) != 2) {
                ESP_LOGE(TAG, "Failed to read topic length");
                return -1;
            }
            uint16_t topic_len = (len_bytes[0] << 8) | len_bytes[1];
            
            if (topic_len >= 128) {
                ESP_LOGW(TAG, "Topic too long: %d bytes", topic_len);
                topic_len = 127;
            }
            
            char topic[128];
            if (mbedtls_ssl_read(&client->ssl_ctx, (uint8_t*)topic, topic_len) != topic_len) {
                ESP_LOGE(TAG, "Failed to read topic");
                return -1;
            }
            topic[topic_len] = '\0';
            
            // Read payload
            int payload_len = remaining_len - topic_len - 2;
            char* payload = (char*)malloc(payload_len + 1);
            if (payload) {
                mbedtls_ssl_read(&client->ssl_ctx, (uint8_t*)payload, payload_len);
                payload[payload_len] = '\0';
                
                ESP_LOGI(TAG, "PUBLISH: %s (%d bytes)", topic, payload_len);
                
                if (client->config.event_callback) {
                    bambu_mqtt_event_t event = {
                        .event_type = BAMBU_MQTT_EVENT_DATA,
                        .topic = topic,
                        .data = payload,
                        .data_len = payload_len
                    };
                    client->config.event_callback(&event, client->config.user_data);
                }
                
                free(payload);
            }
            break;
        }
        
        case MQTT_PINGRESP:
            ESP_LOGD(TAG, "PINGRESP received");
            break;
            
        case MQTT_SUBACK: {
            ESP_LOGI(TAG, "SUBACK received (remaining: %d bytes)", remaining_len);
            // Skip packet ID and QoS
            if (remaining_len > 0) {
                uint8_t* suback_data = (uint8_t*)malloc(remaining_len);
                if (suback_data) {
                    int read_ret = mbedtls_ssl_read(&client->ssl_ctx, suback_data, remaining_len);
                    if (read_ret == remaining_len && remaining_len >= 3) {
                        uint8_t return_code = suback_data[2];
                        ESP_LOGI(TAG, "SUBACK packet ID: %d, return code: 0x%02X", 
                                 (suback_data[0] << 8) | suback_data[1], return_code);
                        
                        if (return_code == 0x00 || return_code == 0x01 || return_code == 0x02) {
                            ESP_LOGI(TAG, "Subscription successful (QoS %d granted)", return_code);
                            
                            // Emit SUBSCRIBED event
                            if (client->config.event_callback) {
                                bambu_mqtt_event_t event = {
                                    .event_type = BAMBU_MQTT_EVENT_SUBSCRIBED,
                                    .data = NULL,
                                    .data_len = 0
                                };
                                client->config.event_callback(&event, client->config.user_data);
                            }
                        } else if (return_code == 0x80) {
                            ESP_LOGE(TAG, "Subscription FAILED - server rejected subscription");
                        }
                    }
                    free(suback_data);
                } else {
                    ESP_LOGE(TAG, "Failed to allocate SUBACK buffer");
                    return -1;
                }
            }
            break;
        }
            
        default:
            ESP_LOGW(TAG, "Unknown packet type: 0x%02X", header & 0xF0);
            // Drain remaining bytes
            if (remaining_len > 0) {
                uint8_t* drain = (uint8_t*)malloc(remaining_len);
                if (drain) {
                    mbedtls_ssl_read(&client->ssl_ctx, drain, remaining_len);
                    free(drain);
                }
            }
            break;
    }
    
    return 0;
}

// Main MQTT task
static void mqtt_task(void* arg) {
    bambu_mqtt_client_handle_t client = (bambu_mqtt_client_handle_t)arg;
    
    ESP_LOGI(TAG, "MQTT task started");
    
    while (client->running) {
        // Check for incoming data (with timeout)
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(client->socket_fd, &readfds);
        
        struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
        int select_ret = select(client->socket_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (select_ret > 0 && FD_ISSET(client->socket_fd, &readfds)) {
            if (process_packet(client) < 0) {
                ESP_LOGE(TAG, "Packet processing failed");
                break;
            }
        }
        
        // Send periodic PING
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - client->last_ping_time > (client->config.keepalive_seconds * 1000 / 2)) {
            send_ping(client);
        }
    }
    
    ESP_LOGI(TAG, "MQTT task exiting");
    client->state = BAMBU_MQTT_STATE_DISCONNECTED;
    vTaskDelete(NULL);
}

// Public API Implementation

bambu_mqtt_client_handle_t bambu_mqtt_init(const bambu_mqtt_config_t* config) {
    bambu_mqtt_client_handle_t client = (bambu_mqtt_client_handle_t)calloc(1, sizeof(struct bambu_mqtt_client));
    if (!client) return NULL;
    
    memcpy(&client->config, config, sizeof(bambu_mqtt_config_t));
    client->state = BAMBU_MQTT_STATE_DISCONNECTED;
    client->mutex = xSemaphoreCreateMutex();
    client->packet_id = 0;
    
    // Initialize mbedtls
    mbedtls_net_init(&client->net_ctx);
    mbedtls_ssl_init(&client->ssl_ctx);
    mbedtls_ssl_config_init(&client->ssl_conf);
    mbedtls_entropy_init(&client->entropy);
    mbedtls_ctr_drbg_init(&client->ctr_drbg);
    
    ESP_LOGI(TAG, "MQTT client initialized (TLS=%s, verify_cert=%s)", 
             config->use_tls ? "yes" : "no",
             config->verify_cert ? "yes" : "NO - INSECURE MODE");
    
    return client;
}

int bambu_mqtt_start(bambu_mqtt_client_handle_t client) {
    if (!client || client->state != BAMBU_MQTT_STATE_DISCONNECTED) {
        return -1;
    }
    
    client->state = BAMBU_MQTT_STATE_CONNECTING;
    ESP_LOGI(TAG, "Connecting to %s:%d", client->config.host, client->config.port);
    
    // Seed RNG
    const char* pers = "bambu_mqtt";
    int ret = mbedtls_ctr_drbg_seed(&client->ctr_drbg, mbedtls_entropy_func, 
                                     &client->entropy, (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed failed: -0x%04x", -ret);
        return -1;
    }
    
    // TCP connect
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", client->config.port);
    ret = mbedtls_net_connect(&client->net_ctx, client->config.host, port_str, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        ESP_LOGE(TAG, "TCP connect failed: -0x%04x", -ret);
        return -1;
    }
    
    client->socket_fd = client->net_ctx.fd;
    
    // Set socket timeout to prevent infinite hangs
    struct timeval timeout;
    timeout.tv_sec = 10;  // 10 second timeout
    timeout.tv_usec = 0;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    ESP_LOGI(TAG, "TCP connected");
    
    if (client->config.use_tls) {
        // Setup SSL config - THIS IS THE KEY PART FOR INSECURE MODE
        ret = mbedtls_ssl_config_defaults(&client->ssl_conf, MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_config_defaults failed: -0x%04x", -ret);
            return -1;
        }
        
        // CRITICAL: Disable certificate verification (Python ssl.CERT_NONE equivalent)
        if (!client->config.verify_cert) {
            mbedtls_ssl_conf_authmode(&client->ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
            ESP_LOGI(TAG, "SSL verification disabled (MBEDTLS_SSL_VERIFY_NONE)");
        }
        
        mbedtls_ssl_conf_rng(&client->ssl_conf, mbedtls_ctr_drbg_random, &client->ctr_drbg);
        
        ret = mbedtls_ssl_setup(&client->ssl_ctx, &client->ssl_conf);
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_setup failed: -0x%04x", -ret);
            return -1;
        }
        
        mbedtls_ssl_set_bio(&client->ssl_ctx, &client->net_ctx, 
                             mbedtls_net_send, mbedtls_net_recv, NULL);
        
        // TLS handshake with timeout
        ESP_LOGI(TAG, "Starting TLS handshake...");
        uint32_t handshake_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
        const uint32_t HANDSHAKE_TIMEOUT_MS = 30000;  // 30 second timeout
        
        while ((ret = mbedtls_ssl_handshake(&client->ssl_ctx)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                char error_buf[100];
                mbedtls_strerror(ret, error_buf, sizeof(error_buf));
                ESP_LOGE(TAG, "TLS handshake failed: -0x%04x (%s)", -ret, error_buf);
                return -1;
            }
            
            // Check timeout
            uint32_t elapsed = (xTaskGetTickCount() * portTICK_PERIOD_MS) - handshake_start;
            if (elapsed > HANDSHAKE_TIMEOUT_MS) {
                ESP_LOGE(TAG, "TLS handshake timeout after %lu ms", elapsed);
                return -1;
            }
            
            // Small delay to prevent busy loop
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        ESP_LOGI(TAG, "TLS handshake complete! (insecure mode - no cert verification)");
    }
    
    // Start task FIRST (it needs to be running to receive CONNACK)
    client->running = true;
    client->last_ping_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Log free memory before task creation
    // CRITICAL: Task stacks MUST be allocated from INTERNAL DRAM, not PSRAM!
    ESP_LOGI(TAG, "Free heap before task: %u bytes total (largest block: %u)", 
             (unsigned int)esp_get_free_heap_size(), 
             (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "Internal DRAM free: %u bytes (largest block: %u) - THIS is what matters for task stacks!", 
             (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    
    // Use smaller stack size - internal DRAM is very limited (~232KB total)
    // LVGL uses 8KB but runs early. By the time we start, WiFi/WebServer/etc have consumed most internal RAM.
    // Our task is simple (select loop + packet processing), doesn't need much stack
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        mqtt_task, 
        "bambu_mqtt", 
        2048,  // 2KB - much smaller to fit in remaining internal DRAM
        client, 
        3,     // Priority 3
        &client->task_handle,
        1  // Core 1
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT task (ret=%d, stack=2048, internal_dram_free=%u)", 
                 task_ret, 
                 (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        client->running = false;
        return -1;
    }
    
    ESP_LOGI(TAG, "MQTT task created successfully");
    
    // Now send MQTT CONNECT (task is running and can receive CONNACK)
    ret = send_connect(client);
    if (ret < 0) {
        ESP_LOGE(TAG, "MQTT CONNECT failed");
        client->running = false;
        return -1;
    }
    
    return 0;
}

int bambu_mqtt_subscribe(bambu_mqtt_client_handle_t client, const char* topic, int qos) {
    if (!client || client->state != BAMBU_MQTT_STATE_CONNECTED) {
        return -1;
    }
    
    return send_subscribe(client, topic, qos);
}

int bambu_mqtt_publish(bambu_mqtt_client_handle_t client, const char* topic, 
                       const char* data, int len, int qos, int retain) {
    if (!client || client->state != BAMBU_MQTT_STATE_CONNECTED) {
        return -1;
    }
    
    return send_publish(client, topic, data, qos);
}

int bambu_mqtt_stop(bambu_mqtt_client_handle_t client) {
    if (!client) return -1;
    
    client->running = false;
    
    if (client->task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    mbedtls_ssl_close_notify(&client->ssl_ctx);
    mbedtls_net_free(&client->net_ctx);
    
    client->state = BAMBU_MQTT_STATE_DISCONNECTED;
    
    return 0;
}

int bambu_mqtt_destroy(bambu_mqtt_client_handle_t client) {
    if (!client) return -1;
    
    bambu_mqtt_stop(client);
    
    mbedtls_ssl_free(&client->ssl_ctx);
    mbedtls_ssl_config_free(&client->ssl_conf);
    mbedtls_ctr_drbg_free(&client->ctr_drbg);
    mbedtls_entropy_free(&client->entropy);
    
    if (client->mutex) {
        vSemaphoreDelete(client->mutex);
    }
    
    free(client);
    
    return 0;
}

bambu_mqtt_state_t bambu_mqtt_get_state(bambu_mqtt_client_handle_t client) {
    return client ? client->state : BAMBU_MQTT_STATE_ERROR;
}
