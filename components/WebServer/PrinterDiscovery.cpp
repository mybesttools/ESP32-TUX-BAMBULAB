/*
 * Printer Discovery Implementation
 * Uses mDNS for local network discovery, IP scanning for additional networks
 */

#include "PrinterDiscovery.hpp"
#include "SettingsConfig.hpp"
#include "../BambuMonitor/include/BambuMonitor.hpp"
#include <cstring>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <arpa/inet.h>
#include <vector>
#include <mutex>
#include <esp_netif.h>
#include <fcntl.h>
#include <errno.h>
#include <mqtt_client.h>
#include <cJSON.h>
#include <mdns.h>

extern SettingsConfig *cfg;

const char* PrinterDiscovery::TAG = "PrinterDiscovery";
PrinterFoundCallback PrinterDiscovery::s_printer_found_callback = nullptr;

PrinterDiscovery::PrinterDiscovery() {
}

PrinterDiscovery::~PrinterDiscovery() {
}

void PrinterDiscovery::set_printer_found_callback(PrinterFoundCallback cb) {
    s_printer_found_callback = cb;
}

std::string PrinterDiscovery::extract_model_from_hostname(const std::string &hostname) {
    // Bambu hostnames like: "Bambu Lab X1-XXXX" or "Bambu-P1P-XXXX"
    // Extract the model part
    if (hostname.find("X1") != std::string::npos) return "X1";
    if (hostname.find("P1P") != std::string::npos) return "P1P";
    if (hostname.find("P1S") != std::string::npos) return "P1S";
    if (hostname.find("P1") != std::string::npos) return "P1";
    if (hostname.find("A1") != std::string::npos) return "A1";
    
    return "Unknown";
}

std::vector<std::string> PrinterDiscovery::parse_subnet_ips(const std::string &subnet, int max_ips) {
    std::vector<std::string> ips;
    
    // Parse CIDR notation (e.g., "192.168.1.0/24")
    size_t slash_pos = subnet.find('/');
    if (slash_pos == std::string::npos) {
        ESP_LOGW(TAG, "Invalid CIDR format: %s", subnet.c_str());
        return ips;
    }
    
    std::string network = subnet.substr(0, slash_pos);
    std::string cidr_str = subnet.substr(slash_pos + 1);
    int cidr = std::stoi(cidr_str);
    
    // Find last dot to get base subnet
    size_t last_dot = network.rfind('.');
    if (last_dot == std::string::npos) {
        ESP_LOGW(TAG, "Invalid IP format: %s", network.c_str());
        return ips;
    }
    
    std::string subnet_base = network.substr(0, last_dot + 1);
    
    // For /24 networks, scan all hosts
    if (cidr == 24) {
        // Scan common hosts first (1-5, 10, 50, 80, 85, 100, 150, 200, 250)
        std::vector<int> common_hosts = {1, 2, 3, 4, 5, 10, 50, 80, 85, 100, 150, 200, 250};
        for (int host : common_hosts) {
            ips.push_back(subnet_base + std::to_string(host));
        }
        // Then add remaining hosts
        for (int host = 1; host <= 254; host++) {
            if (std::find(common_hosts.begin(), common_hosts.end(), host) == common_hosts.end()) {
                ips.push_back(subnet_base + std::to_string(host));
                if ((int)ips.size() >= max_ips) break;
            }
        }
    } else if (cidr == 25) {
        // /25 = 128 addresses
        for (int host = 0; host < 128; host++) {
            ips.push_back(subnet_base + std::to_string(host));
        }
    } else if (cidr == 23) {
        // /23 = 512 addresses (limit to max_ips)
        for (int i = 0; i < 2 && (int)ips.size() < max_ips; i++) {
            for (int host = 0; host < 254; host++) {
                ips.push_back(subnet_base + std::to_string(i * 256 + host));
                if ((int)ips.size() >= max_ips) break;
            }
        }
    }
    
    ESP_LOGI(TAG, "Generated %d IPs from subnet %s", (int)ips.size(), subnet.c_str());
    return ips;
}

std::vector<PrinterDiscovery::PrinterInfo> PrinterDiscovery::scan_subnet(const std::string &subnet, ProgressCallback progress_cb) {
    std::vector<PrinterInfo> discovered;
    
    ESP_LOGI(TAG, "=== Starting sequential IP scan for subnet: %s ===", subnet.c_str());
    
    std::vector<std::string> ips_to_scan = parse_subnet_ips(subnet, 254);
    int total_ips = ips_to_scan.size();
    
    if (total_ips == 0) {
        ESP_LOGW(TAG, "No IPs to scan for subnet %s", subnet.c_str());
        if (progress_cb) progress_cb(100, 100);
        return discovered;
    }
    
    ESP_LOGI(TAG, "Scanning %d IPs sequentially from subnet %s (no parallel tasks)", total_ips, subnet.c_str());
    if (progress_cb) progress_cb(0, 100);
    
    int found_count = 0;
    int scanned_count = 0;
    
    // Sequential scan - test each IP one at a time (like bambu_project)
    // This avoids task creation failures and memory exhaustion
    for (int i = 0; i < total_ips; i++) {
        const std::string &ip = ips_to_scan[i];
        
        // Test connection directly without creating a task
        bool found = test_connection(ip, 8883, 500);
        
        if (found) {
            PrinterInfo info;
            info.ip_address = ip;
            info.hostname = "Bambu Lab Printer";
            info.model = "Unknown";
            discovered.push_back(info);
            found_count++;
            ESP_LOGI(TAG, "✓ Found printer at: %s", ip.c_str());
            
            // Notify static callback immediately
            if (s_printer_found_callback) {
                s_printer_found_callback(ip);
            }
        } else {
            ESP_LOGD(TAG, "✗ No printer at: %s", ip.c_str());
        }
        scanned_count++;
        
        // Update progress
        if (progress_cb) {
            int progress = (scanned_count * 100) / total_ips;
            if (progress % 10 == 0) {  // Log every 10%
                ESP_LOGI(TAG, "Progress: %d%% (%d/%d IPs scanned)", progress, scanned_count, total_ips);
            }
            progress_cb(progress, 100);
        }
        
        // Yield occasionally to prevent watchdog timeout
        if (i % 20 == 0) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    
    // Report 100% complete
    if (progress_cb) {
        progress_cb(100, 100);
    }
    
    ESP_LOGI(TAG, "=== Subnet scan complete: Found %d printers out of %d IPs scanned ===", found_count, scanned_count);
    return discovered;
}

// mDNS-based discovery for local network
// Bambu printers advertise as _bblp._tcp (Bambu Lab Lan Protocol)
std::vector<PrinterDiscovery::PrinterInfo> PrinterDiscovery::discover_mdns(int timeout_ms) {
    std::vector<PrinterInfo> discovered;
    
    ESP_LOGI(TAG, "Starting mDNS discovery for Bambu printers (_bblp._tcp)...");
    
    // Query for Bambu Lab Lan Protocol service
    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr("_bblp", "_tcp", timeout_ms, 10, &results);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS query failed: %s", esp_err_to_name(err));
        return discovered;
    }
    
    if (!results) {
        ESP_LOGI(TAG, "mDNS: No Bambu printers found via _bblp._tcp");
        return discovered;
    }
    
    // Process results
    mdns_result_t *r = results;
    while (r) {
        PrinterInfo info;
        
        // Get hostname
        if (r->hostname) {
            info.hostname = r->hostname;
            info.model = extract_model_from_hostname(info.hostname);
            ESP_LOGI(TAG, "mDNS found: %s (model: %s)", info.hostname.c_str(), info.model.c_str());
        }
        
        // Get IP address
        if (r->addr) {
            mdns_ip_addr_t *addr = r->addr;
            while (addr) {
                if (addr->addr.type == ESP_IPADDR_TYPE_V4) {
                    char ip_str[16];
                    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&addr->addr.u_addr.ip4));
                    info.ip_address = ip_str;
                    ESP_LOGI(TAG, "  IP: %s", info.ip_address.c_str());
                    break;  // Use first IPv4 address
                }
                addr = addr->next;
            }
        }
        
        // Only add if we have both hostname and IP
        if (!info.hostname.empty() && !info.ip_address.empty()) {
            discovered.push_back(info);
            
            // Notify callback if set
            if (s_printer_found_callback) {
                s_printer_found_callback(info.ip_address);
            }
        }
        
        r = r->next;
    }
    
    mdns_query_results_free(results);
    
    ESP_LOGI(TAG, "mDNS discovery complete: Found %d printer(s)", (int)discovered.size());
    return discovered;
}

std::vector<PrinterDiscovery::PrinterInfo> PrinterDiscovery::discover(int timeout_ms, ProgressCallback progress_cb) {
    std::vector<PrinterInfo> discovered;
    
    ESP_LOGI(TAG, "=== Starting Bambu Lab printer discovery ===");
    
    if (progress_cb) {
        ESP_LOGI(TAG, "Progress callback registered");
        progress_cb(0, 100);
    } else {
        ESP_LOGW(TAG, "No progress callback provided");
    }
    
    // Step 1: Try mDNS discovery first (fast, works on local network)
    ESP_LOGI(TAG, "Step 1: mDNS discovery on local network...");
    if (progress_cb) progress_cb(5, 100);
    
    std::vector<PrinterInfo> mdns_results = discover_mdns(2000);  // 2 second timeout
    if (!mdns_results.empty()) {
        ESP_LOGI(TAG, "mDNS found %d printer(s) on local network", (int)mdns_results.size());
        discovered.insert(discovered.end(), mdns_results.begin(), mdns_results.end());
    }
    
    if (progress_cb) progress_cb(15, 100);
    
    // Step 2: Get configured networks from SettingsConfig for IP scanning
    if (!cfg) {
        ESP_LOGE(TAG, "ERROR: cfg is NULL!");
        if (progress_cb) progress_cb(100, 100);
        return discovered;
    }
    
    int network_count = cfg->get_network_count();
    ESP_LOGI(TAG, "Step 2: IP scanning on %d configured network(s)...", network_count);
    
    if (network_count <= 0) {
        ESP_LOGI(TAG, "No additional networks configured for IP scanning.");
        if (progress_cb) progress_cb(100, 100);
        return discovered;
    }
    
    // Calculate progress range for IP scanning (15-100%)
    int scan_progress_start = 15;
    int scan_progress_range = 85;  // 100 - 15
    
    int total_networks = network_count;
    for (int i = 0; i < total_networks; i++) {
        network_config_t network = cfg->get_network(i);
        ESP_LOGI(TAG, "[Network %d/%d] %s (%s) enabled=%d", i+1, total_networks,
                 network.name.c_str(), network.subnet.c_str(), network.enabled);
        
        if (!network.enabled) {
            ESP_LOGI(TAG, "  → Skipped (disabled)");
            continue;
        }
        
        ESP_LOGI(TAG, "  → Starting IP scan on subnet: %s", network.subnet.c_str());
        
        // Create a lambda that scales progress based on network index
        ProgressCallback network_progress_cb = nullptr;
        if (progress_cb) {
            network_progress_cb = [progress_cb, i, total_networks, scan_progress_start, scan_progress_range](int current, int total) {
                // Scale progress within scan range
                int network_start = scan_progress_start + (i * scan_progress_range) / total_networks;
                int network_range = scan_progress_range / total_networks;
                int overall_progress = network_start + (current * network_range) / total;
                progress_cb(overall_progress, 100);
            };
        }
        
        std::vector<PrinterInfo> subnet_results = scan_subnet(network.subnet, network_progress_cb);
        ESP_LOGI(TAG, "  → Scan complete: Found %d printers", (int)subnet_results.size());
        
        // Add results, avoiding duplicates (by IP)
        for (const auto& printer : subnet_results) {
            bool duplicate = false;
            for (const auto& existing : discovered) {
                if (existing.ip_address == printer.ip_address) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                discovered.push_back(printer);
            }
        }
    }
    
    // Ensure progress shows 100% at the end
    if (progress_cb) {
        progress_cb(100, 100);
    }
    
    if (discovered.empty()) {
        ESP_LOGW(TAG, "=== Discovery complete: No Bambu Lab printers found ===");
    } else {
        ESP_LOGI(TAG, "=== Discovery complete: Found %d printer(s) ===", (int)discovered.size());
    }
    
    return discovered;
}

std::vector<PrinterDiscovery::PrinterInfo> PrinterDiscovery::discover_by_subnet(const std::string &subnet) {
    return scan_subnet(subnet);
}

bool PrinterDiscovery::test_connection(const std::string &ip, int port, int timeout_ms) {
    ESP_LOGD(TAG, "Testing connection to %s:%d (timeout: %dms)", ip.c_str(), port, timeout_ms);
    
    // Use BSD socket for simple port connection test
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        ESP_LOGD(TAG, "Failed to create socket for %s:%d", ip.c_str(), port);
        return false;
    }
    
    // Set non-blocking mode so connect() doesn't block
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Prepare address structure
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    
    // Try to connect (will return EINPROGRESS for non-blocking)
    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    
    bool success = false;
    
    if (err == 0) {
        // Connected immediately (rare but possible on local network)
        success = true;
    } else if (errno == EINPROGRESS) {
        // Connection in progress, wait for it with select()
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);
        
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        int select_result = select(sock + 1, NULL, &writefds, NULL, &tv);
        
        if (select_result > 0) {
            // Socket is writable, check if connection succeeded
            int conn_err = 0;
            socklen_t conn_err_len = sizeof(conn_err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &conn_err, &conn_err_len);
            success = (conn_err == 0);
        }
        // If select_result <= 0, timeout or error occurred, success stays false
    }
    // Otherwise connection failed immediately, success stays false
    
    close(sock);
    
    if (success) {
        ESP_LOGI(TAG, "✓ Found printer at %s:%d", ip.c_str(), port);
    } else {
        ESP_LOGD(TAG, "✗ No connection to %s:%d", ip.c_str(), port);
    }
    
    return success;
}

// Extract serial number from Bambu printer MQTT topic path
// Topic format: device/{SERIAL_NUMBER}/report
// Returns empty string if unable to extract
std::string PrinterDiscovery::extract_serial_from_topic(const std::string &topic) {
    // Expected format: "device/SERIAL_NUMBER/report"
    size_t first_slash = topic.find('/');
    if (first_slash == std::string::npos || topic.substr(0, first_slash) != "device") {
        return "";
    }
    
    size_t second_slash = topic.find('/', first_slash + 1);
    if (second_slash == std::string::npos) {
        return "";
    }
    
    std::string serial = topic.substr(first_slash + 1, second_slash - first_slash - 1);
    
    // Validate serial format (should be alphanumeric, typically starts with 00M)
    if (serial.length() >= 8 && serial.length() <= 20) {
        ESP_LOGI(TAG, "Extracted serial from topic: %s", serial.c_str());
        return serial;
    }
    
    return "";
}

// Structure to hold MQTT query task parameters and results
struct mqtt_query_params {
    std::string ip;
    std::string access_code;
    int timeout_ms;
    PrinterDiscovery::PrinterStatus result;
    SemaphoreHandle_t done_semaphore;
    bool task_complete;
};

// Structure to hold MQTT message data
struct mqtt_message_data {
    std::string topic;
    std::string payload;
    bool received;
    bool connected;
    SemaphoreHandle_t msg_semaphore;      // Signaled when message received
    SemaphoreHandle_t connect_semaphore;  // Signaled when connected
    
    mqtt_message_data() : received(false), connected(false), msg_semaphore(nullptr), connect_semaphore(nullptr) {}
};

static const char *MQTT_TAG = "MQTT_Query";

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    mqtt_message_data *msg_data = (mqtt_message_data *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MQTT_TAG, "✓ MQTT connected to printer!");
            msg_data->connected = true;
            if (msg_data->connect_semaphore) {
                xSemaphoreGive(msg_data->connect_semaphore);
            }
            break;
            
        case MQTT_EVENT_DATA:
            if (event->topic_len > 0 && !msg_data->received) {
                msg_data->topic.assign(event->topic, event->topic_len);
                if (event->data_len > 0) {
                    msg_data->payload.assign(event->data, event->data_len);
                }
                msg_data->received = true;
                ESP_LOGI(MQTT_TAG, "✓ Got message on topic: %.*s", event->topic_len, event->topic);
                if (msg_data->msg_semaphore) {
                    xSemaphoreGive(msg_data->msg_semaphore);
                }
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(MQTT_TAG, "MQTT disconnected");
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(MQTT_TAG, "MQTT error occurred");
            if (event->error_handle) {
                if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                    ESP_LOGE(MQTT_TAG, "Transport error: 0x%x", event->error_handle->esp_transport_sock_errno);
                    // Check for TLS errors
                    if (event->error_handle->esp_tls_last_esp_err) {
                        ESP_LOGE(MQTT_TAG, "TLS error: 0x%x", event->error_handle->esp_tls_last_esp_err);
                    }
                    if (event->error_handle->esp_tls_stack_err) {
                        ESP_LOGE(MQTT_TAG, "TLS stack error: 0x%x (may indicate wrong access code)", event->error_handle->esp_tls_stack_err);
                    }
                } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                    ESP_LOGE(MQTT_TAG, "Connection refused, code: 0x%x", event->error_handle->connect_return_code);
                    // MQTT connection refused codes:
                    // 0x04 = Bad username or password (wrong access code)
                    // 0x05 = Not authorized
                    if (event->error_handle->connect_return_code == 0x04 || 
                        event->error_handle->connect_return_code == 0x05) {
                        ESP_LOGE(MQTT_TAG, "*** WRONG ACCESS CODE - check printer LAN access code ***");
                    }
                }
            }
            break;
            
        default:
            ESP_LOGD(MQTT_TAG, "MQTT event: %d", (int)event->event_id);
            break;
    }
}

// Task function that runs the MQTT query with adequate stack for mbedTLS
static void mqtt_query_task(void *pvParameters) {
    mqtt_query_params *params = (mqtt_query_params *)pvParameters;
    
    ESP_LOGI(MQTT_TAG, "MQTT query task started for %s", params->ip.c_str());
    
    params->result.ip_address = params->ip;
    
    // Setup MQTT message data with semaphores for waiting
    mqtt_message_data msg_data;
    msg_data.msg_semaphore = xSemaphoreCreateBinary();
    msg_data.connect_semaphore = xSemaphoreCreateBinary();
    
    if (!msg_data.msg_semaphore || !msg_data.connect_semaphore) {
        ESP_LOGE(MQTT_TAG, "Failed to create semaphores");
        if (msg_data.msg_semaphore) vSemaphoreDelete(msg_data.msg_semaphore);
        if (msg_data.connect_semaphore) vSemaphoreDelete(msg_data.connect_semaphore);
        params->result.state = "ERROR";
        params->task_complete = true;
        xSemaphoreGive(params->done_semaphore);
        vTaskDelete(NULL);
        return;
    }
    
    // Check available heap before creating MQTT client
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(MQTT_TAG, "Free heap before MQTT client: %zu bytes", free_heap);
    
    if (free_heap < 50000) {
        ESP_LOGW(MQTT_TAG, "Low memory (%zu bytes), skipping MQTT query", free_heap);
        vSemaphoreDelete(msg_data.msg_semaphore);
        if (msg_data.connect_semaphore) vSemaphoreDelete(msg_data.connect_semaphore);
        params->result.state = "LOW_MEMORY";
        params->task_complete = true;
        xSemaphoreGive(params->done_semaphore);
        vTaskDelete(NULL);
        return;
    }
    
    // Configure MQTT client with SSL skip verification
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.hostname = params->ip.c_str();
    mqtt_cfg.broker.address.port = 8883;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
    
    // Skip certificate verification for Bambu printers
    mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
    mqtt_cfg.broker.verification.use_global_ca_store = false;
    mqtt_cfg.broker.verification.crt_bundle_attach = NULL;
    
    // Client credentials - Bambu uses "bblp" username and access code as password
    mqtt_cfg.credentials.client_id = "esp32_discovery";
    mqtt_cfg.credentials.username = "bblp";
    mqtt_cfg.credentials.authentication.password = params->access_code.c_str();
    
    // Minimal buffers for discovery - we only need the serial number from topic
    mqtt_cfg.buffer.size = 4096;  // 4KB receive buffer (reduced from 8KB)
    mqtt_cfg.buffer.out_size = 256;  // 256B output buffer
    mqtt_cfg.network.timeout_ms = params->timeout_ms;
    mqtt_cfg.network.disable_auto_reconnect = true;  // Don't auto-reconnect for discovery
    mqtt_cfg.session.keepalive = 15;  // Shorter keepalive for discovery
    
    // Minimal stack for discovery task
    mqtt_cfg.task.stack_size = 4096;  // 4KB stack (reduced from 6KB)
    mqtt_cfg.task.priority = 4;  // Lower priority than main MQTT task
    
    ESP_LOGI(MQTT_TAG, "Creating MQTT client for %s...", params->ip.c_str());
    
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client) {
        ESP_LOGE(MQTT_TAG, "Failed to create MQTT client");
        vSemaphoreDelete(msg_data.msg_semaphore);
        vSemaphoreDelete(msg_data.connect_semaphore);
        params->result.state = "ERROR";
        params->task_complete = true;
        xSemaphoreGive(params->done_semaphore);
        vTaskDelete(NULL);
        return;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, &msg_data);
    
    // Start MQTT client
    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(MQTT_TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(client);
        vSemaphoreDelete(msg_data.msg_semaphore);
        vSemaphoreDelete(msg_data.connect_semaphore);
        params->result.state = "ERROR";
        params->task_complete = true;
        xSemaphoreGive(params->done_semaphore);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(MQTT_TAG, "MQTT client started, waiting for connection...");
    
    // Wait for MQTT_EVENT_CONNECTED (up to 8 seconds for SSL handshake)
    if (xSemaphoreTake(msg_data.connect_semaphore, pdMS_TO_TICKS(8000)) != pdTRUE) {
        ESP_LOGE(MQTT_TAG, "Timeout waiting for MQTT connection");
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        vSemaphoreDelete(msg_data.msg_semaphore);
        vSemaphoreDelete(msg_data.connect_semaphore);
        params->result.state = "CONNECT_TIMEOUT";
        params->task_complete = true;
        xSemaphoreGive(params->done_semaphore);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(MQTT_TAG, "Connected! Now subscribing...");
    
    // Subscribe to wildcard topic to discover serial number
    // Bambu printers publish reports on device/{SERIAL}/report
    int msg_id = esp_mqtt_client_subscribe(client, "device/+/report", 0);
    ESP_LOGI(MQTT_TAG, "Subscribed to device/+/report (msg_id: %d)", msg_id);
    
    // Give the subscription time to be processed
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Send push_all request to trigger printer to send status
    // This is required on newer firmware - printers don't auto-send without request
    // The request goes to device/{serial}/request but we use a generic approach
    const char* push_all_cmd = "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}";
    
    // Try to publish to a wildcard-ish topic (some printers accept this)
    // If we don't know the serial yet, try publishing to request topic
    int pub_id = esp_mqtt_client_publish(client, "device/local/request", push_all_cmd, 0, 0, 0);
    if (pub_id >= 0) {
        ESP_LOGI(MQTT_TAG, "Sent push_all request (msg_id: %d)", pub_id);
    }
    
    // Bambu printers should respond within a few seconds
    ESP_LOGI(MQTT_TAG, "Waiting for printer status report (timeout: %d ms)...", params->timeout_ms);
    
    if (xSemaphoreTake(msg_data.msg_semaphore, pdMS_TO_TICKS(params->timeout_ms)) == pdTRUE) {
        ESP_LOGI(MQTT_TAG, "Received MQTT message on topic: %s", msg_data.topic.c_str());
        
        // Extract serial from topic
        std::string serial = PrinterDiscovery::extract_serial_from_topic(msg_data.topic);
        if (!serial.empty()) {
            params->result.serial = serial;
            params->result.state = "READY";
            ESP_LOGI(MQTT_TAG, "✓ Discovered printer serial: %s", serial.c_str());
        } else {
            ESP_LOGW(MQTT_TAG, "Could not extract serial from topic: %s", msg_data.topic.c_str());
            params->result.state = "UNKNOWN";
        }
    } else {
        ESP_LOGW(MQTT_TAG, "Timeout waiting for printer response");
        params->result.state = "TIMEOUT";
    }
    
    // Cleanup
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    vSemaphoreDelete(msg_data.msg_semaphore);
    vSemaphoreDelete(msg_data.connect_semaphore);
    
    params->task_complete = true;
    xSemaphoreGive(params->done_semaphore);
    
    ESP_LOGI(MQTT_TAG, "MQTT query task complete");
    vTaskDelete(NULL);
}

PrinterDiscovery::PrinterStatus PrinterDiscovery::query_printer_status(const std::string &ip, const std::string &access_code, int timeout_ms) {
    PrinterStatus status;
    status.ip_address = ip;
    
    ESP_LOGI(TAG, "Starting MQTT query for printer at %s with timeout %d ms", ip.c_str(), timeout_ms);
    
    // Verify connection first (quick TCP check)
    if (!test_connection(ip, 8883, 500)) {
        ESP_LOGE(TAG, "Printer not reachable at %s:8883", ip.c_str());
        status.state = "OFFLINE";
        return status;
    }
    
    ESP_LOGI(TAG, "✓ Printer reachable at %s:8883", ip.c_str());
    
    // Create parameters structure for the task
    mqtt_query_params params;
    params.ip = ip;
    params.access_code = access_code;
    params.timeout_ms = timeout_ms;
    params.task_complete = false;
    params.done_semaphore = xSemaphoreCreateBinary();
    
    if (!params.done_semaphore) {
        ESP_LOGE(TAG, "Failed to create done semaphore");
        status.state = "ERROR";
        return status;
    }
    
    // Create task with large stack for mbedTLS SSL operations (12KB)
    // The httpd task only has 4KB which is not enough for SSL handshake
    BaseType_t task_created = xTaskCreate(
        mqtt_query_task,
        "mqtt_query",
        12288,  // 12KB stack - mbedTLS needs ~8KB for SSL handshake
        &params,
        5,      // Priority
        NULL
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT query task");
        vSemaphoreDelete(params.done_semaphore);
        status.state = "ERROR";
        return status;
    }
    
    // Wait for task to complete with overall timeout (connection + query + buffer)
    int total_timeout = timeout_ms + 5000;  // Add 5s for SSL handshake overhead
    if (xSemaphoreTake(params.done_semaphore, pdMS_TO_TICKS(total_timeout)) == pdTRUE) {
        status = params.result;
        ESP_LOGI(TAG, "MQTT query completed with state: %s", status.state.c_str());
    } else {
        ESP_LOGW(TAG, "MQTT query task timed out");
        status.state = "TIMEOUT";
    }
    
    vSemaphoreDelete(params.done_semaphore);
    
    return status;
}
