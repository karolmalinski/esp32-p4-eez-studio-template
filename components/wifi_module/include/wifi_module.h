#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes NVS, the TCP/IP stack, and the Wi-Fi driver.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_module_init(void);

/**
 * @brief Connects to a specific Wi-Fi network and blocks until connected or failed.
 * @param ssid The Wi-Fi network name.
 * @param password The Wi-Fi password.
 * @return esp_err_t ESP_OK if connected and got IP, ESP_FAIL if connection failed.
 */
esp_err_t wifi_module_connect(const char *ssid, const char *password);

/**
 * @brief Initializes mDNS with a unique hostname based on the device's MAC address.
 * 
 * This function reads the ESP32's globally unique MAC address and appends the last
 * few characters to create a unique hostname like "device-name-a1b2.local".
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t wifi_module_start_mdns(const char *device_name);

#ifdef __cplusplus
}
#endif