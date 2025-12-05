/*
 * Printer Discovery Implementation
 * Based on bambu_project's IP scanning logic
 */

#include "PrinterDiscovery.hpp"
#include "SettingsConfig.hpp"
#include <cstring>
#include <esp_err.h>
#include <esp_log.h>
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

extern SettingsConfig *cfg;

const char *PrinterDiscovery::TAG = "PrinterDiscovery";

PrinterDiscovery::PrinterDiscovery() {
}

PrinterDiscovery::~PrinterDiscovery() {
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

std::vector<PrinterDiscovery::PrinterInfo> PrinterDiscovery::discover(int timeout_ms, ProgressCallback progress_cb) {
    std::vector<PrinterInfo> discovered;
    
    ESP_LOGI(TAG, "=== Starting Bambu Lab printer discovery ===");
    
    if (progress_cb) {
        ESP_LOGI(TAG, "Progress callback registered");
        progress_cb(0, 100);
    } else {
        ESP_LOGW(TAG, "No progress callback provided");
    }
    
    // Get configured networks from SettingsConfig
    if (!cfg) {
        ESP_LOGE(TAG, "ERROR: cfg is NULL!");
        if (progress_cb) progress_cb(100, 100);
        return discovered;
    }
    
    int network_count = cfg->get_network_count();
    ESP_LOGI(TAG, "SettingsConfig available. Network count: %d", network_count);
    
    if (network_count <= 0) {
        ESP_LOGW(TAG, "No networks configured! Add networks in web UI to enable IP scanning.");
        if (progress_cb) progress_cb(100, 100);
        return discovered;
    }
    
    ESP_LOGI(TAG, "Scanning %d configured network(s)...", network_count);
    
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
            network_progress_cb = [progress_cb, i, total_networks](int current, int total) {
                // Scale progress: each network gets (100/total_networks)% of the bar
                int network_start = (i * 100) / total_networks;
                int network_range = 100 / total_networks;
                int overall_progress = network_start + (current * network_range) / total;
                progress_cb(overall_progress, 100);
            };
        }
        
        std::vector<PrinterInfo> subnet_results = scan_subnet(network.subnet, network_progress_cb);
        ESP_LOGI(TAG, "  → Scan complete: Found %d printers", (int)subnet_results.size());
        discovered.insert(discovered.end(), subnet_results.begin(), subnet_results.end());
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
    
    // Cleanup: explicitly clear vectors to release memory
    ESP_LOGI(TAG, "Cleanup: Clearing discovery resources...");
    // (vectors will auto-destruct when scope ends, but be explicit about it)
    
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

// Structure to hold MQTT message data
struct mqtt_message_data {
    std::string topic;
    std::string payload;
    bool received = false;
    SemaphoreHandle_t semaphore;
};

static const char *MQTT_TAG = "MQTT_Query";

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    mqtt_message_data *msg_data = (mqtt_message_data *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGD(MQTT_TAG, "MQTT connected");
            break;
            
        case MQTT_EVENT_DATA:
            if (event->topic_len > 0 && event->data_len > 0) {
                msg_data->topic.assign(event->topic, event->topic_len);
                msg_data->payload.assign(event->data, event->data_len);
                msg_data->received = true;
                if (msg_data->semaphore) {
                    xSemaphoreGive(msg_data->semaphore);
                }
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGD(MQTT_TAG, "MQTT disconnected");
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGW(MQTT_TAG, "MQTT error");
            break;
            
        default:
            break;
    }
}

PrinterDiscovery::PrinterStatus PrinterDiscovery::query_printer_status(const std::string &ip, const std::string &access_code, int timeout_ms) {
    PrinterStatus status;
    status.ip_address = ip;
    
    ESP_LOGI(TAG, "Starting MQTT query for printer at %s with timeout %d ms", ip.c_str(), timeout_ms);
    
    // Verify connection first
    if (!test_connection(ip, 8883, 500)) {
        ESP_LOGE(TAG, "Printer not reachable at %s:8883", ip.c_str());
        return status;
    }
    
    // For now, return a placeholder indicating the feature is in development
    // Full MQTT client implementation coming soon  
    status.serial = "MQTT_QUERY_PENDING";
    status.state = "INITIALIZING";
    
    ESP_LOGW(TAG, "MQTT query returns placeholder (development in progress)");
    return status;
}
