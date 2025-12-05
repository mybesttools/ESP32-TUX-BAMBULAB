#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"

static const char *TAG = "mdns_responder";
static char g_hostname[64] = {0};
static bool g_initialized = false;

/**
 * @brief Initialize hostname responder
 * 
 * This function sets the hostname which will be used by the device.
 * For mDNS .local resolution to work:
 * 1. Router must support DHCP hostname advertising, OR
 * 2. Client must have static hosts file entry, OR  
 * 3. Network must use a full mDNS responder (component unavailable in ESP-IDF v5.0)
 * 
 * Recommended: Add to /etc/hosts on client: 10.13.13.188 esp32-tux.local
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

    // Get the WiFi station netif
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        ESP_LOGE(TAG, "Failed to get WiFi STA netif");
        return ESP_FAIL;
    }

    // Set the hostname - some routers will advertise this via DHCP
    esp_err_t ret = esp_netif_set_hostname(netif, hostname);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set hostname: %s", esp_err_to_name(ret));
        return ret;
    }

    g_initialized = true;
    
    // Log helpful information
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "Hostname set to: %s", hostname);
    ESP_LOGI(TAG, "For .local resolution (esp32-tux.local):");
    ESP_LOGI(TAG, "  Option 1: Router DHCP hostname support");
    ESP_LOGI(TAG, "  Option 2: Add to /etc/hosts on client:");
    ESP_LOGI(TAG, "    echo '10.13.13.188 esp32-tux.local' >> /etc/hosts");
    ESP_LOGI(TAG, "  Option 3: Router DNS/DHCP configuration");
    ESP_LOGI(TAG, "================================================");
    
    return ESP_OK;
}

esp_err_t mdns_responder_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }

    memset(g_hostname, 0, sizeof(g_hostname));
    g_initialized = false;
    ESP_LOGI(TAG, "Deinitialized");
    return ESP_OK;
}
