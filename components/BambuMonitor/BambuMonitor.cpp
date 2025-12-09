/**
 * @file BambuMonitor.cpp
 * @brief Multi-printer Bambu Lab MQTT Monitor
 * 
 * Supports up to 6 simultaneous MQTT connections to Bambu Lab printers.
 * Each printer has its own MQTT client, state, and cache file.
 */

#include "BambuMonitor.hpp"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <cstring>
#include <string>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// Storage health monitoring functions (implemented in main/helpers/helper_storage_health.c)
extern "C" {
    void storage_health_record_sd_error(void);
    void storage_health_record_spiffs_error(void);
}

static const char* TAG = "BambuMonitor";

// Storage paths - prefer SD card over SPIFFS
#define SDCARD_PRINTER_PATH "/sdcard/printer"
#define SPIFFS_PRINTER_PATH "/spiffs/printer"

// SD card availability cache
static int sdcard_available = -1;  // -1 = not checked, 0 = not available, 1 = available

// Per-printer state structure
typedef struct {
    bool active;                        // Slot is in use
    bool connected;                     // MQTT is connected
    bambu_printer_state_t state;        // Current printer state
    bambu_printer_config_t config;      // Printer configuration
    esp_mqtt_client_handle_t mqtt_client;  // MQTT client handle
    cJSON* last_status;                 // Last received status JSON
    time_t last_update;                 // Last update timestamp
    time_t last_activity;               // Last activity (data received) timestamp
    char* data_buffer;                  // Buffer for fragmented MQTT data
    int buffer_len;                     // Current buffer length
    int buffer_size;                    // Allocated buffer size
    char topic_buffer[128];             // Store topic for fragmented messages
    int sd_write_failures;              // Consecutive SD write failures
    bool use_spiffs_only;               // Skip SD card after repeated failures
    char last_snapshot_path[256];       // Path to last captured snapshot
} printer_slot_t;

// Global state
static printer_slot_t printers[BAMBU_MAX_PRINTERS] = {0};
static esp_event_handler_t registered_handler = NULL;
static bool monitor_initialized = false;

// Connection pool management (max 2 concurrent MQTT connections)
#define MAX_CONCURRENT_CONNECTIONS 2
static int active_connection_count = 0;

// Round-robin rotation for fair printer updates
static int rotation_index = 0;  // Next printer to rotate in
#define STALE_THRESHOLD_SECONDS 30  // Rotate in printers not updated for this long

// Embedded Bambu Lab root certificates
extern const uint8_t bambu_cert_start[] asm("_binary_bambu_combined_cert_start");
extern const uint8_t bambu_cert_end[] asm("_binary_bambu_combined_cert_end");

// Define event base
ESP_EVENT_DEFINE_BASE(BAMBU_EVENT_BASE);

// Forward declarations
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, 
                              int32_t event_id, void* event_data);
static void process_printer_data(int index, const char* topic, const char* data, int data_len);

/**
 * @brief Check if SD card is available and create printer directory
 * Result is cached after first check.
 */
static bool is_sdcard_available() {
    // Return cached result if already checked
    if (sdcard_available >= 0) {
        return sdcard_available == 1;
    }
    
    struct stat st;
    
    // Check if /sdcard is a valid mounted directory
    if (stat("/sdcard", &st) != 0) {
        ESP_LOGW(TAG, "SD card not mounted: /sdcard stat failed (errno=%d)", errno);
        sdcard_available = 0;
        return false;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        ESP_LOGW(TAG, "SD card: /sdcard is not a directory");
        sdcard_available = 0;
        return false;
    }
    
    // Try to actually write to the SD card
    FILE* test = fopen("/sdcard/.bambu_test", "w");
    if (!test) {
        int err = errno;
        ESP_LOGW(TAG, "SD card not writable: fopen failed (errno=%d)", err);
        
        // Track SD write error for storage health monitoring
        if (err == 5 || err == 257) {  // I/O error or block read error
            storage_health_record_sd_error();
        }
        
        sdcard_available = 0;
        return false;
    }
    fprintf(test, "test");
    fclose(test);
    remove("/sdcard/.bambu_test");
    ESP_LOGI(TAG, "SD card is writable");
    
    // Create printer directory if needed
    if (stat(SDCARD_PRINTER_PATH, &st) != 0) {
        if (mkdir(SDCARD_PRINTER_PATH, 0755) == 0) {
            ESP_LOGI(TAG, "Created printer directory: %s", SDCARD_PRINTER_PATH);
        } else {
            ESP_LOGW(TAG, "Failed to create %s (errno=%d), will use SPIFFS", SDCARD_PRINTER_PATH, errno);
            sdcard_available = 0;
            return false;
        }
    } else {
        ESP_LOGI(TAG, "Printer directory exists: %s", SDCARD_PRINTER_PATH);
    }
    
    sdcard_available = 1;
    ESP_LOGI(TAG, "SD card available for printer cache");
    return true;
}

/**
 * @brief Reset SD card availability check (call after SD remount)
 */
void bambu_reset_sdcard_check(void) {
    sdcard_available = -1;
}

/**
 * @brief Get cache file path for a printer serial
 */
static std::string get_printer_cache_path(const char* serial) {
    if (is_sdcard_available()) {
        return std::string(SDCARD_PRINTER_PATH) + "/" + serial + ".json";
    }
    return std::string(SPIFFS_PRINTER_PATH) + "/" + serial + ".json";
}

/**
 * @brief Find printer index by MQTT client handle
 */
static int find_printer_by_client(esp_mqtt_client_handle_t client) {
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active && printers[i].mqtt_client == client) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Find printer index by device ID (serial)
 */
static int find_printer_by_device_id(const char* device_id) {
    if (!device_id) return -1;
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active && printers[i].config.device_id &&
            strcmp(printers[i].config.device_id, device_id) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Test TCP connectivity to a printer (quick check before MQTT)
 * @return true if printer is reachable, false otherwise
 */
static bool test_tcp_connectivity(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return false;
    }
    
    bool tcp_reachable = false;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(printers[index].config.port);
        inet_pton(AF_INET, printers[index].config.ip_address, &dest_addr.sin_addr);
        
        // Set short timeout for connectivity test
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err == 0) {
            tcp_reachable = true;
        }
        close(sock);
    }
    return tcp_reachable;
}

/**
 * @brief Find least recently active connected printer (for eviction)
 */
static int find_lru_connected_printer() {
    int lru_index = -1;
    time_t oldest_activity = 0;
    
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active && printers[i].connected) {
            if (lru_index == -1 || printers[i].last_activity < oldest_activity) {
                oldest_activity = printers[i].last_activity;
                lru_index = i;
            }
        }
    }
    return lru_index;
}

/**
 * @brief Ensure connection pool doesn't exceed MAX_CONCURRENT_CONNECTIONS
 * Disconnects least recently used connection if needed
 */
static void manage_connection_pool() {
    // Count active connections
    int count = 0;
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active && printers[i].connected) {
            count++;
        }
    }
    
    active_connection_count = count;
    
    // If we're at or over limit, disconnect LRU
    if (count >= MAX_CONCURRENT_CONNECTIONS) {
        int lru = find_lru_connected_printer();
        if (lru >= 0) {
            ESP_LOGI(TAG, "[%d] Connection pool full (%d/%d), disconnecting LRU printer %s", 
                     lru, count, MAX_CONCURRENT_CONNECTIONS, printers[lru].config.device_id);
            esp_mqtt_client_stop(printers[lru].mqtt_client);
            printers[lru].connected = false;
            active_connection_count--;
        }
    }
}

/**
 * @brief MQTT event handler for all printers
 */
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, 
                              int32_t event_id, void* event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    int index = (int)(intptr_t)handler_args;  // Printer index passed as user data
    
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        ESP_LOGW(TAG, "Event for invalid printer index: %d", index);
        return;
    }
    
    printer_slot_t* printer = &printers[index];
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "[%d] MQTT connected to %s", index, printer->config.ip_address);
            printer->connected = true;
            printer->state = BAMBU_STATE_IDLE;
            time(&printer->last_activity);  // Track connection time
            active_connection_count++;
            
            ESP_LOGI(TAG, "Active connections: %d/%d", active_connection_count, MAX_CONCURRENT_CONNECTIONS);
            
            // Subscribe to printer status topic
            char topic[128];
            snprintf(topic, sizeof(topic), "device/%s/report", printer->config.device_id);
            int msg_id = esp_mqtt_client_subscribe(printer->mqtt_client, topic, 1);
            ESP_LOGI(TAG, "[%d] Subscribed to %s (msg_id: %d)", index, topic, msg_id);
            
            // Notify handler
            if (registered_handler) {
                registered_handler(NULL, BAMBU_EVENT_BASE, BAMBU_PRINTER_CONNECTED, (void*)(intptr_t)index);
            }
            break;
        }
        
        case MQTT_EVENT_DISCONNECTED: {
            ESP_LOGW(TAG, "[%d] MQTT disconnected from %s", index, printer->config.ip_address);
            if (printer->connected) {
                active_connection_count--;
            }
            printer->connected = false;
            printer->state = BAMBU_STATE_OFFLINE;
            
            ESP_LOGI(TAG, "Active connections: %d/%d", active_connection_count, MAX_CONCURRENT_CONNECTIONS);
            
            if (registered_handler) {
                registered_handler(NULL, BAMBU_EVENT_BASE, BAMBU_PRINTER_DISCONNECTED, (void*)(intptr_t)index);
            }
            break;
        }
        
        case MQTT_EVENT_DATA: {
            // Update activity timestamp on data reception
            time(&printer->last_activity);
            
            // Handle fragmented MQTT messages
            if (event->data_len > 0) {
                // Store topic on first fragment
                if (event->current_data_offset == 0 && event->topic_len > 0) {
                    if (event->topic_len < sizeof(printer->topic_buffer)) {
                        strncpy(printer->topic_buffer, event->topic, event->topic_len);
                        printer->topic_buffer[event->topic_len] = '\0';
                    }
                }
                
                // Allocate or expand buffer for this fragment
                int new_len = printer->buffer_len + event->data_len;
                if (new_len > printer->buffer_size) {
                    int new_size = (new_len + 1023) & ~1023; // Round up to 1KB
                    if (new_size > 65536) {
                        ESP_LOGW(TAG, "[%d] Message too large (%d bytes), discarding", index, new_size);
                        free(printer->data_buffer);
                        printer->data_buffer = NULL;
                        printer->buffer_len = 0;
                        printer->buffer_size = 0;
                        break;
                    }
                    char* new_buf = (char*)realloc(printer->data_buffer, new_size);
                    if (!new_buf) {
                        ESP_LOGE(TAG, "[%d] Failed to allocate %d bytes for MQTT data", index, new_size);
                        free(printer->data_buffer);
                        printer->data_buffer = NULL;
                        printer->buffer_len = 0;
                        printer->buffer_size = 0;
                        break;
                    }
                    printer->data_buffer = new_buf;
                    printer->buffer_size = new_size;
                }
                
                // Append this fragment
                memcpy(printer->data_buffer + printer->buffer_len, event->data, event->data_len);
                printer->buffer_len += event->data_len;
                
                // Check if this is the last fragment
                if (printer->buffer_len >= event->total_data_len) {
                    // Process complete message
                    process_printer_data(index, printer->topic_buffer, printer->data_buffer, printer->buffer_len);
                    
                    // Reset buffer for next message
                    printer->buffer_len = 0;
                }
            }
            break;
        }
        
        case MQTT_EVENT_ERROR: {
            ESP_LOGE(TAG, "[%d] MQTT error for %s", index, printer->config.ip_address);
            if (event->error_handle) {
                if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                    ESP_LOGE(TAG, "[%d] TCP transport error - esp_err: 0x%x, tls_stack_err: 0x%x", 
                            index,
                            event->error_handle->esp_tls_last_esp_err,
                            event->error_handle->esp_tls_stack_err);
                    ESP_LOGE(TAG, "[%d] Possible network routing issue - check if ESP32 can reach %s from current network", 
                            index, printer->config.ip_address);
                } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                    ESP_LOGE(TAG, "[%d] Connection refused by %s - check credentials", index, printer->config.ip_address);
                } else {
                    ESP_LOGE(TAG, "[%d] Error type: %d", index, event->error_handle->error_type);
                }
            }
            printer->state = BAMBU_STATE_OFFLINE;
            break;
        }
        
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "[%d] Subscribed successfully", index);
            break;
            
        default:
            ESP_LOGD(TAG, "[%d] MQTT event: %d", index, event->event_id);
            break;
    }
}

/**
 * @brief Process incoming printer data and write to cache
 */
static void process_printer_data(int index, const char* topic, const char* data, int data_len) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) return;
    
    printer_slot_t* printer = &printers[index];
    
    // Extract serial from topic (device/SERIAL/report)
    char serial[64] = {0};
    if (strncmp(topic, "device/", 7) == 0) {
        const char* start = topic + 7;
        const char* end = strchr(start, '/');
        if (end && (end - start) < sizeof(serial)) {
            strncpy(serial, start, end - start);
        }
    }
    
    // Use config device_id if serial not extracted
    if (serial[0] == '\0' && printer->config.device_id) {
        strncpy(serial, printer->config.device_id, sizeof(serial) - 1);
    }
    
    ESP_LOGD(TAG, "[%d] Data from %s (%d bytes)", index, serial, data_len);
    
    // Parse JSON - use ParseWithLength to handle large messages better
    if (data_len <= 0 || data_len > 65536) return;
    
    cJSON* json = cJSON_ParseWithLength(data, data_len);
    
    if (!json) {
        // Rate-limit error logging (max once per 30 seconds per printer)
        static time_t last_error_time[BAMBU_MAX_PRINTERS] = {0};
        time_t now = time(NULL);
        
        if (now - last_error_time[index] >= 30) {
            ESP_LOGW(TAG, "[%d] Failed to parse JSON (data_len=%d, first 50 chars: %.50s)", 
                     index, data_len, data);
            last_error_time[index] = now;
        }
        return;
    }
    
    // Update cached status
    if (printer->last_status) {
        cJSON_Delete(printer->last_status);
    }
    printer->last_status = json;
    
    // Extract printer state
    cJSON* print_obj = cJSON_GetObjectItem(json, "print");
    if (print_obj) {
        cJSON* gcode_state = cJSON_GetObjectItem(print_obj, "gcode_state");
        if (gcode_state && gcode_state->valuestring) {
            if (strcmp(gcode_state->valuestring, "PRINTING") == 0 ||
                strcmp(gcode_state->valuestring, "RUNNING") == 0) {
                printer->state = BAMBU_STATE_PRINTING;
            } else if (strcmp(gcode_state->valuestring, "PAUSE") == 0) {
                printer->state = BAMBU_STATE_PAUSED;
            } else if (strcmp(gcode_state->valuestring, "FAILED") == 0) {
                printer->state = BAMBU_STATE_ERROR;
            } else {
                printer->state = BAMBU_STATE_IDLE;
            }
        }
    }
    
    // Write minimal cache file (throttled to every 5 seconds)
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    
    if (serial[0] != '\0' && (tv_now.tv_sec - printer->last_update >= 5)) {
        printer->last_update = tv_now.tv_sec;
        
        // Create minimal JSON for cache
        cJSON* mini = cJSON_CreateObject();
        if (mini) {
            cJSON_AddNumberToObject(mini, "last_update", tv_now.tv_sec);
            
            if (print_obj) {
                // State
                cJSON* state = cJSON_GetObjectItem(print_obj, "gcode_state");
                cJSON_AddStringToObject(mini, "state", 
                    state && state->valuestring ? state->valuestring : "IDLE");
                
                // Progress
                cJSON* percent = cJSON_GetObjectItem(print_obj, "mc_percent");
                cJSON_AddNumberToObject(mini, "progress", percent ? percent->valueint : 0);
                
                // Remaining time
                cJSON* remaining = cJSON_GetObjectItem(print_obj, "mc_remaining_time");
                cJSON_AddNumberToObject(mini, "remaining_min", remaining ? remaining->valueint : 0);
                
                // Layers
                cJSON* layer = cJSON_GetObjectItem(print_obj, "layer_num");
                cJSON* total_layer = cJSON_GetObjectItem(print_obj, "total_layer_num");
                cJSON_AddNumberToObject(mini, "current_layer", layer ? layer->valueint : 0);
                cJSON_AddNumberToObject(mini, "total_layers", total_layer ? total_layer->valueint : 0);
                
                // Temperatures
                cJSON* nozzle = cJSON_GetObjectItem(print_obj, "nozzle_temper");
                cJSON* nozzle_target = cJSON_GetObjectItem(print_obj, "nozzle_target_temper");
                cJSON* bed = cJSON_GetObjectItem(print_obj, "bed_temper");
                cJSON* bed_target = cJSON_GetObjectItem(print_obj, "bed_target_temper");
                cJSON_AddNumberToObject(mini, "nozzle_temp", nozzle ? nozzle->valuedouble : 0);
                cJSON_AddNumberToObject(mini, "nozzle_target", nozzle_target ? nozzle_target->valuedouble : 0);
                cJSON_AddNumberToObject(mini, "bed_temp", bed ? bed->valuedouble : 0);
                cJSON_AddNumberToObject(mini, "bed_target", bed_target ? bed_target->valuedouble : 0);
                
                // File name
                cJSON* gcode_file = cJSON_GetObjectItem(print_obj, "gcode_file");
                if (gcode_file && gcode_file->valuestring) {
                    const char* fname = strrchr(gcode_file->valuestring, '/');
                    cJSON_AddStringToObject(mini, "file_name", fname ? fname + 1 : gcode_file->valuestring);
                }
                
                // WiFi
                cJSON* wifi = cJSON_GetObjectItem(print_obj, "wifi_signal");
                if (wifi && wifi->valuestring) {
                    cJSON_AddStringToObject(mini, "wifi_signal", wifi->valuestring);
                }
            }
            
            // Write to file - try SD card first (with retry), fallback to SPIFFS
            char* output = cJSON_PrintUnformatted(mini);
            if (output) {
                size_t len = strlen(output);
                std::string path = get_printer_cache_path(serial);
                bool is_sdcard = (path.find("/sdcard/") == 0);
                bool write_success = false;
                
                // Try SD card with retry logic (unless flagged to skip)
                if (is_sdcard && !printers[index].use_spiffs_only) {
                    const int MAX_RETRIES = 3;
                    const int RETRY_DELAYS_MS[] = {10, 50, 200};
                    
                    for (int retry = 0; retry < MAX_RETRIES && !write_success; retry++) {
                        if (retry > 0) {
                            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAYS_MS[retry - 1]));
                        }
                        
                        FILE* f = fopen(path.c_str(), "w");
                        if (f) {
                            size_t written = fwrite(output, 1, len, f);
                            fclose(f);
                            
                            // Verify write
                            struct stat st;
                            if (written == len && stat(path.c_str(), &st) == 0 && st.st_size == (off_t)len) {
                                ESP_LOGI(TAG, "[%d] Cache updated: %s (%zu bytes)%s", 
                                         index, path.c_str(), written, retry > 0 ? " (retry succeeded)" : "");
                                printers[index].sd_write_failures = 0; // Reset on success
                                write_success = true;
                            } else {
                                ESP_LOGW(TAG, "[%d] Write verification failed: %s (wrote %zu, stat=%lld)", 
                                         index, path.c_str(), written, (long long)(st.st_size));
                                storage_health_record_sd_error();
                            }
                        } else {
                            ESP_LOGW(TAG, "[%d] Failed to open %s (errno=%d)", index, path.c_str(), errno);
                            storage_health_record_sd_error();
                        }
                    }
                    
                    // Track failures and switch to SPIFFS-only after threshold
                    if (!write_success) {
                        printers[index].sd_write_failures++;
                        if (printers[index].sd_write_failures >= 3) {
                            ESP_LOGE(TAG, "[%d] SD card unreliable (%d consecutive failures), switching to SPIFFS-only mode", 
                                     index, printers[index].sd_write_failures);
                            printers[index].use_spiffs_only = true;
                        }
                    }
                } else if (!is_sdcard) {
                    // Direct SPIFFS write (no retry needed - more reliable)
                    FILE* f = fopen(path.c_str(), "w");
                    if (f) {
                        size_t written = fwrite(output, 1, len, f);
                        fclose(f);
                        if (written == len) {
                            ESP_LOGI(TAG, "[%d] Cache updated: %s (%zu bytes)", index, path.c_str(), written);
                            write_success = true;
                        } else {
                            ESP_LOGW(TAG, "[%d] Partial write to %s: %zu/%zu bytes", index, path.c_str(), written, len);
                            storage_health_record_spiffs_error();
                        }
                    } else {
                        int save_errno = errno;
                        ESP_LOGW(TAG, "[%d] Failed to write cache: %s (errno=%d)", index, path.c_str(), save_errno);
                        storage_health_record_spiffs_error();
                    }
                }
                
                // Fallback to SPIFFS if SD write failed or skipped
                if (!write_success && is_sdcard) {
                    std::string spiffs_path = std::string(SPIFFS_PRINTER_PATH) + "/" + serial + ".json";
                    
                    // Make sure SPIFFS printer directory exists
                    struct stat st;
                    if (stat(SPIFFS_PRINTER_PATH, &st) != 0) {
                        mkdir(SPIFFS_PRINTER_PATH, 0755);
                    }
                    
                    FILE* f = fopen(spiffs_path.c_str(), "w");
                    if (f) {
                        size_t written = fwrite(output, 1, len, f);
                        fclose(f);
                        if (written == len) {
                            ESP_LOGI(TAG, "[%d] Cache written to SPIFFS fallback: %s (%zu bytes)", index, spiffs_path.c_str(), written);
                        } else {
                            storage_health_record_spiffs_error();
                        }
                    } else {
                        ESP_LOGE(TAG, "[%d] SPIFFS fallback also failed: %s (errno=%d)", index, spiffs_path.c_str(), errno);
                        storage_health_record_spiffs_error();
                    }
                }
                cJSON_free(output);
            }
            cJSON_Delete(mini);
        }
    }
    
    // Notify handler
    if (registered_handler) {
        registered_handler(NULL, BAMBU_EVENT_BASE, BAMBU_STATUS_UPDATED, (void*)(intptr_t)index);
    }
}

/**
 * @brief Free printer config strings
 */
static void free_printer_config(bambu_printer_config_t* config) {
    if (config->device_id) { free(config->device_id); config->device_id = NULL; }
    if (config->ip_address) { free(config->ip_address); config->ip_address = NULL; }
    if (config->access_code) { free(config->access_code); config->access_code = NULL; }
    if (config->tls_certificate) { free(config->tls_certificate); config->tls_certificate = NULL; }
}

// ============== Public API ==============

esp_err_t bambu_monitor_init(void) {
    if (monitor_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    // Initialize all printer slots
    memset(printers, 0, sizeof(printers));
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        printers[i].state = BAMBU_STATE_OFFLINE;
    }
    
    monitor_initialized = true;
    ESP_LOGI(TAG, "Multi-printer monitor initialized (max %d printers)", BAMBU_MAX_PRINTERS);
    return ESP_OK;
}

esp_err_t bambu_monitor_init_single(const bambu_printer_config_t* config) {
    esp_err_t ret = bambu_monitor_init();
    if (ret != ESP_OK) return ret;
    
    int idx = bambu_add_printer(config);
    return (idx >= 0) ? ESP_OK : ESP_FAIL;
}

int bambu_add_printer(const bambu_printer_config_t* config) {
    if (!monitor_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return -1;
    }
    
    if (!config || !config->device_id || !config->ip_address || !config->access_code) {
        ESP_LOGE(TAG, "Invalid config");
        return -1;
    }
    
    // Check if already exists
    int existing = find_printer_by_device_id(config->device_id);
    if (existing >= 0) {
        ESP_LOGW(TAG, "Printer %s already at index %d", config->device_id, existing);
        return existing;
    }
    
    // Find empty slot
    int index = -1;
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (!printers[i].active) {
            index = i;
            break;
        }
    }
    
    if (index < 0) {
        ESP_LOGE(TAG, "No free slots (max %d printers)", BAMBU_MAX_PRINTERS);
        return -1;
    }
    
    printer_slot_t* printer = &printers[index];
    
    // Copy config
    printer->config.device_id = strdup(config->device_id);
    printer->config.ip_address = strdup(config->ip_address);
    printer->config.port = config->port > 0 ? config->port : 8883;
    printer->config.access_code = strdup(config->access_code);
    printer->config.disable_ssl_verify = config->disable_ssl_verify;
    if (config->tls_certificate) {
        printer->config.tls_certificate = strdup(config->tls_certificate);
    }
    
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.hostname = printer->config.ip_address;
    mqtt_cfg.broker.address.port = printer->config.port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
    mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
    
    mqtt_cfg.credentials.username = "bblp";
    mqtt_cfg.credentials.authentication.password = printer->config.access_code;
    
    // Network timeouts (important for cross-subnet connections)
    mqtt_cfg.network.timeout_ms = 10000;         // 10 second TCP timeout
    mqtt_cfg.network.refresh_connection_after_ms = 0;  // Disable auto-refresh
    mqtt_cfg.network.disable_auto_reconnect = false;
    
    // Aggressively optimized buffer sizes for 3 printers (prevent memory exhaustion)
    mqtt_cfg.buffer.size = 6144;        // 6KB receive buffer (fragmentation handles larger messages)
    mqtt_cfg.buffer.out_size = 384;     // 384B send buffer (queries ~100-150 bytes)
    mqtt_cfg.task.priority = 3;         // Lower priority
    mqtt_cfg.task.stack_size = 3072;    // 3KB stack per printer (reduced to save memory)
    mqtt_cfg.session.keepalive = 60;
    
    ESP_LOGI(TAG, "Configuring MQTT for %s at %s:%d (stack: %d, heap free: %ld)", 
             config->device_id, printer->config.ip_address, printer->config.port,
             mqtt_cfg.task.stack_size, esp_get_free_heap_size());
    
    printer->mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!printer->mqtt_client) {
        ESP_LOGE(TAG, "Failed to create MQTT client for %s", config->device_id);
        free_printer_config(&printer->config);
        return -1;
    }
    
    // Register event handler with printer index as user data
    esp_mqtt_client_register_event(printer->mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, 
                                   mqtt_event_handler, (void*)(intptr_t)index);
    
    printer->active = true;
    printer->state = BAMBU_STATE_OFFLINE;
    
    ESP_LOGI(TAG, "[%d] Added printer: %s at %s:%d", 
             index, config->device_id, config->ip_address, printer->config.port);
    
    return index;
}

esp_err_t bambu_remove_printer(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return ESP_ERR_INVALID_ARG;
    }
    
    printer_slot_t* printer = &printers[index];
    
    // Stop and destroy MQTT client
    if (printer->mqtt_client) {
        esp_mqtt_client_stop(printer->mqtt_client);
        // Wait for TLS buffers to be freed
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_mqtt_client_destroy(printer->mqtt_client);
        printer->mqtt_client = NULL;
    }
    
    // Free status JSON
    if (printer->last_status) {
        cJSON_Delete(printer->last_status);
        printer->last_status = NULL;
    }
    
    // Free data buffer
    if (printer->data_buffer) {
        free(printer->data_buffer);
        printer->data_buffer = NULL;
        printer->buffer_len = 0;
        printer->buffer_size = 0;
    }
    
    // Free config
    free_printer_config(&printer->config);
    
    printer->active = false;
    printer->connected = false;
    printer->state = BAMBU_STATE_OFFLINE;
    
    ESP_LOGI(TAG, "[%d] Printer removed", index);
    return ESP_OK;
}

int bambu_get_printer_count(void) {
    int count = 0;
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active) count++;
    }
    return count;
}

esp_err_t bambu_monitor_deinit(void) {
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active) {
            bambu_remove_printer(i);
        }
    }
    
    registered_handler = NULL;
    monitor_initialized = false;
    sdcard_available = -1;  // Reset SD card check for next init
    
    ESP_LOGI(TAG, "Monitor deinitialized");
    return ESP_OK;
}

bambu_printer_state_t bambu_get_printer_state(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return BAMBU_STATE_OFFLINE;
    }
    return printers[index].state;
}

bambu_printer_state_t bambu_get_printer_state_default(void) {
    // Return state of first active printer
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active) {
            return printers[i].state;
        }
    }
    return BAMBU_STATE_OFFLINE;
}

cJSON* bambu_get_status_json(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return NULL;
    }
    if (!printers[index].last_status) {
        return NULL;
    }
    return cJSON_Duplicate(printers[index].last_status, 1);
}

esp_err_t bambu_register_event_handler(esp_event_handler_t handler) {
    registered_handler = handler;
    return ESP_OK;
}

esp_err_t bambu_monitor_start(void) {
    int started = 0;
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active && !printers[i].connected) {
            // Stagger connection starts to avoid concurrent TLS buffer allocations
            // Each TLS handshake requires ~17KB temporary buffer
            if (started > 0) {
                ESP_LOGI(TAG, "Waiting 8 seconds before starting next printer connection...");
                vTaskDelay(pdMS_TO_TICKS(8000));  // Increased from 3s to allow full TLS buffer deallocation
            }
            if (bambu_start_printer(i) == ESP_OK) {
                started++;
            }
        }
    }
    ESP_LOGI(TAG, "Started %d printer connection(s)", started);
    return (started > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t bambu_start_printer(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!printers[index].mqtt_client) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // IMPORTANT: Test TCP connectivity FIRST, BEFORE managing connection pool
    // This prevents disconnecting an LRU printer only to find target is unreachable
    ESP_LOGI(TAG, "[%d] Testing connectivity to %s:%d...", 
             index, printers[index].config.ip_address, printers[index].config.port);
    
    bool tcp_reachable = test_tcp_connectivity(index);
    
    if (!tcp_reachable) {
        ESP_LOGW(TAG, "[%d] TCP connect test failed to %s:%d", 
                 index, printers[index].config.ip_address, printers[index].config.port);
        ESP_LOGW(TAG, "[%d] Skipping MQTT - printer unreachable. Check: 1) Printer powered on, 2) Network routing, 3) Firewall rules", index);
        
        // Clean up any stale MQTT client state from previous connection attempts
        // This prevents "select() timeout" errors when printer comes back online
        if (printers[index].mqtt_client) {
            esp_mqtt_client_stop(printers[index].mqtt_client);
        }
        printers[index].connected = false;
        
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "[%d] TCP connect test successful to %s:%d", 
             index, printers[index].config.ip_address, printers[index].config.port);
    
    // Now that we know printer is reachable, manage connection pool
    // This may disconnect an LRU printer to make room
    manage_connection_pool();
    
    ESP_LOGI(TAG, "[%d] Starting MQTT connection to %s", index, printers[index].config.ip_address);
    return esp_mqtt_client_start(printers[index].mqtt_client);
}

esp_err_t bambu_stop_printer(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (printers[index].mqtt_client) {
        esp_mqtt_client_stop(printers[index].mqtt_client);
        printers[index].connected = false;
        printers[index].state = BAMBU_STATE_OFFLINE;
    }
    
    return ESP_OK;
}

esp_err_t bambu_send_query_index(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // If not connected, try to connect (will manage pool automatically)
    if (!printers[index].connected) {
        ESP_LOGI(TAG, "[%d] Not connected, attempting to start connection for query", index);
        esp_err_t ret = bambu_start_printer(index);
        if (ret != ESP_OK) {
            return ESP_ERR_INVALID_STATE;
        }
        // Wait a bit for connection to establish
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (!printers[index].connected) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    if (!printers[index].mqtt_client) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Update activity timestamp (keep this connection alive)
    time(&printers[index].last_activity);
    
    char topic[128];
    snprintf(topic, sizeof(topic), "device/%s/request", printers[index].config.device_id);
    
    const char* cmd = "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}";
    
    int msg_id = esp_mqtt_client_publish(printers[index].mqtt_client, topic, cmd, 0, 1, 0);
    if (msg_id < 0) {
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "[%d] Query sent (msg_id: %d)", index, msg_id);
    return ESP_OK;
}

esp_err_t bambu_send_query(void) {
    int sent = 0;
    time_t now;
    time(&now);
    
    // First, send queries to all connected printers
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active && printers[i].connected) {
            if (bambu_send_query_index(i) == ESP_OK) {
                sent++;
            }
        }
    }
    
    // Round-robin rotation: find a stale disconnected printer to rotate in
    // This ensures all printers get updated even with only 2 connection slots
    int checked = 0;
    while (checked < BAMBU_MAX_PRINTERS) {
        int idx = rotation_index;
        rotation_index = (rotation_index + 1) % BAMBU_MAX_PRINTERS;
        checked++;
        
        if (!printers[idx].active) continue;
        if (printers[idx].connected) continue;  // Already connected
        
        // Check if this printer is stale (hasn't been updated recently)
        time_t age = now - printers[idx].last_update;
        if (age >= STALE_THRESHOLD_SECONDS) {
            ESP_LOGI(TAG, "[%d] Printer %s is stale (age=%ld sec), rotating in...", 
                     idx, printers[idx].config.device_id, (long)age);
            
            // This will disconnect LRU printer and connect this one
            if (bambu_send_query_index(idx) == ESP_OK) {
                sent++;
                break;  // Only rotate one printer per cycle to avoid thrashing
            }
        }
    }
    
    return (sent > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t bambu_send_command(int index, const char* command) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!printers[index].mqtt_client || !printers[index].connected || !command) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char topic[128];
    snprintf(topic, sizeof(topic), "device/%s/request", printers[index].config.device_id);
    
    int msg_id = esp_mqtt_client_publish(printers[index].mqtt_client, topic, command, 0, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

const char* bambu_get_device_id(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return NULL;
    }
    return printers[index].config.device_id;
}

bool bambu_is_printer_active(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS) {
        return false;
    }
    return printers[index].active;
}

/**
 * @brief HTTP event handler for snapshot download
 */
static esp_err_t snapshot_http_event_handler(esp_http_client_event_t *evt) {
    FILE** file_ptr = (FILE**)evt->user_data;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (*file_ptr && evt->data_len > 0) {
                fwrite(evt->data, 1, evt->data_len, *file_ptr);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief Capture a snapshot from printer camera
 */
esp_err_t bambu_capture_snapshot(int index, const char* custom_path) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        ESP_LOGE(TAG, "Invalid printer index for snapshot: %d", index);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build snapshot URL: http://IP/snapshot.cgi?user=bblp&pwd=ACCESS_CODE
    char url[256];
    snprintf(url, sizeof(url), "http://%s/snapshot.cgi?user=bblp&pwd=%s",
             printers[index].config.ip_address,
             printers[index].config.access_code);
    
    // Generate save path
    char save_path[256];
    if (custom_path) {
        strncpy(save_path, custom_path, sizeof(save_path) - 1);
    } else {
        // Auto-generate: /sdcard/snapshots/<serial>_<timestamp>.jpg
        time_t now;
        time(&now);
        
        // Try SD card first
        bool use_sd = is_sdcard_available() && !printers[index].use_spiffs_only;
        const char* base_dir = use_sd ? "/sdcard/snapshots" : "/spiffs/snapshots";
        
        // Create snapshots directory
        struct stat st;
        if (stat(base_dir, &st) != 0) {
            mkdir(base_dir, 0755);
        }
        
        // Use fixed filename (overwrite previous snapshot to save space)
        snprintf(save_path, sizeof(save_path), "%s/%s.jpg",
                 base_dir,
                 printers[index].config.device_id);
    }
    
    ESP_LOGI(TAG, "[%d] Capturing snapshot from %s", index, printers[index].config.ip_address);
    ESP_LOGI(TAG, "[%d] Snapshot URL: %s", index, url);
    ESP_LOGI(TAG, "[%d] Save path: %s", index, save_path);
    
    // Open file for writing
    FILE* snapshot_file = fopen(save_path, "wb");
    if (!snapshot_file) {
        ESP_LOGE(TAG, "[%d] Failed to open %s for writing (errno=%d)", index, save_path, errno);
        return ESP_FAIL;
    }
    
    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = snapshot_http_event_handler;
    config.user_data = &snapshot_file;
    config.timeout_ms = 15000;  // 15 second timeout
    config.buffer_size = 4096;  // 4KB buffer for JPEG streaming
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "[%d] Failed to initialize HTTP client", index);
        fclose(snapshot_file);
        unlink(save_path);
        return ESP_FAIL;
    }
    
    // Perform HTTP GET
    esp_err_t err = esp_http_client_perform(client);
    fclose(snapshot_file);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        if (status_code == 200 && content_length > 0) {
            // Verify file was written
            struct stat st;
            if (stat(save_path, &st) == 0 && st.st_size > 0) {
                ESP_LOGI(TAG, "[%d] Snapshot saved: %s (%lld bytes)", index, save_path, (long long)st.st_size);
                strncpy(printers[index].last_snapshot_path, save_path, sizeof(printers[index].last_snapshot_path) - 1);
                esp_http_client_cleanup(client);
                return ESP_OK;
            } else {
                ESP_LOGW(TAG, "[%d] Snapshot file empty or unreadable", index);
            }
        } else {
            ESP_LOGW(TAG, "[%d] HTTP request failed: status=%d, length=%d", index, status_code, content_length);
        }
    } else {
        ESP_LOGE(TAG, "[%d] HTTP GET failed: %s", index, esp_err_to_name(err));
    }
    
    // Cleanup on failure
    esp_http_client_cleanup(client);
    unlink(save_path);  // Delete failed/empty file
    return ESP_FAIL;
}

/**
 * @brief Get the last captured snapshot path for a printer
 */
const char* bambu_get_last_snapshot_path(int index) {
    if (index < 0 || index >= BAMBU_MAX_PRINTERS || !printers[index].active) {
        return NULL;
    }
    
    if (printers[index].last_snapshot_path[0] == '\0') {
        return NULL;
    }
    
    return printers[index].last_snapshot_path;
}
