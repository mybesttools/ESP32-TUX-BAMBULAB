/*
 * Printer Discovery Helper
 * Detects Bambu Lab printers on the network using mDNS
 */

#ifndef PRINTER_DISCOVERY_HPP
#define PRINTER_DISCOVERY_HPP

#include <vector>
#include <string>
#include <esp_log.h>
#include <functional>

// Forward declare mdns types to avoid header inclusion issues
struct mdns_result_s;
typedef struct mdns_result_s mdns_result_t;

// Progress callback type: (current, total) -> void
typedef std::function<void(int, int)> ProgressCallback;

class PrinterDiscovery {
public:
    struct PrinterInfo {
        std::string hostname;
        std::string ip_address;
        std::string model;  // e.g., "X1", "P1P", etc.
    };
    
    struct PrinterStatus {
        std::string serial;
        std::string ip_address;
        std::string state;              // IDLE, RUNNING, PAUSE, etc.
        float bed_temperature;
        float bed_target_temperature;
        float nozzle_temperature;
        float nozzle_target_temperature;
        int ams_status;
        int ams_rfid_status;
        std::string wifi_signal;
        int print_error;
        std::string model_id;
    };
    
    PrinterDiscovery();
    ~PrinterDiscovery();
    
    /**
     * Discover Bambu Lab printers on the network
     * Uses mDNS first, then falls back to IP scanning on configured networks
     * 
     * @param timeout_ms: How long to search (default 4000ms)
     * @param progress_cb: Optional callback for progress updates (current, total)
     * @return Vector of discovered printers
     */
    std::vector<PrinterInfo> discover(int timeout_ms = 4000, ProgressCallback progress_cb = nullptr);
    
    /**
     * Scan specific subnet for printers via HTTP probing
     * @param subnet: CIDR notation (e.g., "192.168.1.0/24")
     * @param progress_cb: Optional callback for progress updates (current, total)
     * @return Vector of discovered printers
     */
    std::vector<PrinterInfo> scan_subnet(const std::string &subnet, ProgressCallback progress_cb = nullptr);
    
    /**
     * Discover printers on specific IP ranges via network scan
     * Scans all configured networks from SettingsConfig
     */
    std::vector<PrinterInfo> discover_by_subnet(const std::string &subnet);
    
    /**
     * Test connection to a printer IP
     * @return true if printer responds
     */
    static bool test_connection(const std::string &ip, int port = 80, int timeout_ms = 1000);
    
    /**
     * Extract serial number from Bambu MQTT topic
     * Topic format: device/{SERIAL_NUMBER}/report
     * @param topic: MQTT topic string
     * @return Serial number or empty string if unable to extract
     */
    static std::string extract_serial_from_topic(const std::string &topic);
    
    /**
     * Query printer status via MQTT
     * Connects to printer MQTT broker and retrieves current status
     * @param ip: Printer IP address
     * @param access_code: Printer access code (MQTT password)
     * @param timeout_ms: Connection timeout in milliseconds
     * @return PrinterStatus with retrieved data or empty on error
     */
    static PrinterStatus query_printer_status(const std::string &ip, const std::string &access_code, int timeout_ms = 5000);

private:
    static const char *TAG;
    
    std::string extract_model_from_hostname(const std::string &hostname);
    
    /**
     * Parse CIDR subnet and generate IP addresses to scan
     * @param subnet: CIDR notation (e.g., "192.168.1.0/24")
     * @return Vector of IP addresses to scan
     */
    std::vector<std::string> parse_subnet_ips(const std::string &subnet, int max_ips = 50);
};

#endif // PRINTER_DISCOVERY_HPP
