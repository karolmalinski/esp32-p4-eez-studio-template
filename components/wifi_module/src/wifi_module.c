#include "wifi_module.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "esp_mac.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi_module";

// FreeRTOS event group to signal when we are connected & have an IP
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
#define MAXIMUM_RETRY 5

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retrying connection to the AP...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGE(TAG, "Connect to the AP failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_module_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // esp_wifi_remote intercepts this and initializes the ESP32-C6 over SDIO!
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi module initialized.");
    return ESP_OK;
}

esp_err_t wifi_module_connect(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    // Safely copy the SSID and Password into the config struct
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "Connecting to SSID: %s...", wifi_config.sta.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Reset the retry counter and clear previous event bits before connecting
    s_retry_num = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    // If Wi-Fi is already started, this will trigger a disconnect/reconnect cycle
    esp_wifi_connect();

    // Block here until we either get an IP address or fail after MAXIMUM_RETRY attempts
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Successfully connected to AP and got IP.");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to AP.");
        return ESP_FAIL;
    }

    ESP_LOGE(TAG, "Unexpected Wi-Fi event occurred.");
    return ESP_FAIL;
}

esp_err_t wifi_module_start_mdns(const char *device_name)
{
    uint8_t mac[6];
    char hostname[32];
    char sanitized_name[32];

    //sanitize the device name to only contain alphanumeric characters and hyphens
    for (int i = 0; i < strlen(device_name); i++) {
        if (!isalnum((unsigned char)device_name[i]) && device_name[i] != '-') {
            sanitized_name[i] = '-';
        } else {
            sanitized_name[i] = device_name[i];
        }
    }
    
    esp_err_t ret = esp_efuse_mac_get_default(mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address: %s", esp_err_to_name(ret));
        return ret;
    }
    
    snprintf(hostname, sizeof(hostname), "%s-%02x%02x%02x", sanitized_name, mac[3], mac[4], mac[5]);
    
    ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mDNS initialization failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = mdns_hostname_set(hostname);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mDNS hostname: %s", esp_err_to_name(ret));
        mdns_free();
        return ret;
    }
    
    ret = mdns_instance_name_set(device_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mDNS instance name: %s", esp_err_to_name(ret));
        mdns_free();
        return ret;
    }
    
    ret = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add HTTP service: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "mDNS started with hostname: %s.local", hostname);
    ESP_LOGI(TAG, "Device accessible at: http://%s.local", hostname);
    
    return ESP_OK;
}