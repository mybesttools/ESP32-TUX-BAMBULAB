#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize mDNS responder with hostname
 * @param hostname The hostname to advertise (will be .local)
 * @return ESP_OK on success
 */
esp_err_t mdns_responder_init(const char *hostname);

/**
 * @brief Deinitialize mDNS responder
 * @return ESP_OK on success
 */
esp_err_t mdns_responder_deinit(void);

#ifdef __cplusplus
}
#endif
