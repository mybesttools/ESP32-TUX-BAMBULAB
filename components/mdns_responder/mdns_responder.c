#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "mdns.h"

static const char *TAG = "mdns_responder";
static char g_hostname[64] = {0};
static bool g_initialized = false;

/**
 * @brief Initialize mDNS responder with hostname and HTTP service
 * 
 * This uses the ESP-IDF mDNS component to properly advertise the device
 * on the local network. After initialization, the device will respond to:
 *   - <hostname>.local  (A record - hostname resolution)
 *   - _http._tcp service queries (service discovery)
 * 
 * @param hostname The hostname to advertise (without .local suffix)
 * @return ESP_OK on success
 */
esp_err_t mdns_responder_init(const char *hostname)
{
    if (!hostname) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // Store hostname for reference
    strncpy(g_hostname, hostname, sizeof(g_hostname) - 1);
    g_hostname[sizeof(g_hostname) - 1] = '\0';

    // Initialize mDNS service
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Set hostname - this allows resolution of <hostname>.local
    err = mdns_hostname_set(hostname);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS hostname set failed: %s", esp_err_to_name(err));
        mdns_free();
        return err;
    }

    // Set instance name (friendly name shown in service browsers)
    err = mdns_instance_name_set("ESP32-TUX Display");
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS instance name set failed: %s", esp_err_to_name(err));
        // Non-fatal, continue
    }

    // Add HTTP service - allows discovery via service browsers
    err = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS HTTP service add failed: %s", esp_err_to_name(err));
        // Non-fatal, hostname resolution should still work
    }

    g_initialized = true;
    
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "mDNS responder initialized");
    ESP_LOGI(TAG, "Hostname: %s.local", hostname);
    ESP_LOGI(TAG, "HTTP service advertised on port 80");
    ESP_LOGI(TAG, "================================================");
    
    return ESP_OK;
}

esp_err_t mdns_responder_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }

    // Remove HTTP service
    mdns_service_remove("_http", "_tcp");
    
    // Free mDNS resources
    mdns_free();
    
    memset(g_hostname, 0, sizeof(g_hostname));
    g_initialized = false;
    ESP_LOGI(TAG, "mDNS responder deinitialized");
    return ESP_OK;
}
