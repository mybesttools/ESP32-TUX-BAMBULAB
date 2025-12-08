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
#include "cJSON.h"
#include <cstring>
#include <string>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>

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
} printer_slot_t;

// Global state
static printer_slot_t printers[BAMBU_MAX_PRINTERS] = {0};
static esp_event_handler_t registered_handler = NULL;
static bool monitor_initialized = false;

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
        ESP_LOGW(TAG, "SD card not writable: fopen failed (errno=%d)", errno);
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
            printer->connected = false;
            printer->state = BAMBU_STATE_OFFLINE;
            
            if (registered_handler) {
                registered_handler(NULL, BAMBU_EVENT_BASE, BAMBU_PRINTER_DISCONNECTED, (void*)(intptr_t)index);
            }
            break;
        }
        
        case MQTT_EVENT_DATA: {
            if (event->topic_len > 0 && event->data_len > 0) {
                char topic[128] = {0};
                if (event->topic_len < sizeof(topic)) {
                    strncpy(topic, event->topic, event->topic_len);
                }
                process_printer_data(index, topic, event->data, event->data_len);
            }
            break;
        }
        
        case MQTT_EVENT_ERROR: {
            ESP_LOGE(TAG, "[%d] MQTT error", index);
            if (event->error_handle) {
                if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                    ESP_LOGE(TAG, "[%d] TLS error: 0x%x, stack: 0x%x", index,
                            event->error_handle->esp_tls_last_esp_err,
                            event->error_handle->esp_tls_stack_err);
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
    
    // Parse JSON
    if (data_len <= 0 || data_len > 65536) return;
    
    char* json_str = (char*)malloc(data_len + 1);
    if (!json_str) return;
    
    memcpy(json_str, data, data_len);
    json_str[data_len] = '\0';
    
    cJSON* json = cJSON_Parse(json_str);
    free(json_str);
    
    if (!json) {
        ESP_LOGW(TAG, "[%d] Failed to parse JSON", index);
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
            
            // Write to file - try SD card first, fallback to SPIFFS
            char* output = cJSON_PrintUnformatted(mini);
            if (output) {
                std::string path = get_printer_cache_path(serial);
                FILE* f = fopen(path.c_str(), "w");
                if (f) {
                    size_t len = strlen(output);
                    size_t written = fwrite(output, 1, len, f);
                    fclose(f);
                    if (written == len) {
                        ESP_LOGI(TAG, "[%d] Cache updated: %s (%zu bytes)", index, path.c_str(), written);
                    } else {
                        ESP_LOGW(TAG, "[%d] Partial write to %s: %zu/%zu bytes", index, path.c_str(), written, len);
                    }
                } else {
                    int save_errno = errno;
                    ESP_LOGW(TAG, "[%d] Failed to write cache: %s (errno=%d)", index, path.c_str(), save_errno);
                    
                    // If SD card write failed, try SPIFFS as fallback
                    if (path.find("/sdcard/") == 0) {
                        std::string spiffs_path = std::string(SPIFFS_PRINTER_PATH) + "/" + serial + ".json";
                        
                        // Make sure SPIFFS printer directory exists
                        struct stat st;
                        if (stat(SPIFFS_PRINTER_PATH, &st) != 0) {
                            mkdir(SPIFFS_PRINTER_PATH, 0755);
                        }
                        
                        f = fopen(spiffs_path.c_str(), "w");
                        if (f) {
                            size_t len = strlen(output);
                            size_t written = fwrite(output, 1, len, f);
                            fclose(f);
                            if (written == len) {
                                ESP_LOGI(TAG, "[%d] Cache written to SPIFFS fallback: %s (%zu bytes)", index, spiffs_path.c_str(), written);
                            }
                        } else {
                            ESP_LOGE(TAG, "[%d] SPIFFS fallback also failed: %s (errno=%d)", index, spiffs_path.c_str(), errno);
                        }
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
    
    // Optimized buffer sizes for multi-printer (smaller than single)
    mqtt_cfg.buffer.size = 16384;       // 16KB receive buffer
    mqtt_cfg.buffer.out_size = 1024;    // 1KB send buffer
    mqtt_cfg.task.priority = 3;         // Lower priority
    mqtt_cfg.task.stack_size = 8192;    // 8KB stack per printer
    mqtt_cfg.session.keepalive = 60;
    
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
                ESP_LOGI(TAG, "Waiting 3 seconds before starting next printer connection...");
                vTaskDelay(pdMS_TO_TICKS(3000));
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
    
    if (!printers[index].mqtt_client || !printers[index].connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
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
    for (int i = 0; i < BAMBU_MAX_PRINTERS; i++) {
        if (printers[i].active && printers[i].connected) {
            if (bambu_send_query_index(i) == ESP_OK) {
                sent++;
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
