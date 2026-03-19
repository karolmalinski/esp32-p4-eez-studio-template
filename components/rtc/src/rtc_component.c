#include <string.h>
#include <sys/time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "rtc_component.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

// Hardware registers required for ESP32-P4 PMU and VBAT configuration
#include "soc/pmu_reg.h"

static const char *TAG = "rtc";

#define RTC_TASK_STACK_SIZE 3072
#define RTC_TASK_PRIORITY 1
#define RTC_UPDATE_INTERVAL_MS 1000

#define TIME_STRING_SIZE 9
#define DATE_STRING_SIZE 32

#define VBAT_MODE_VDDA 0
#define VBAT_MODE_VBAT 1
#define VBAT_MODE_AUTO 2

static SemaphoreHandle_t s_rtc_mutex = NULL;
static rtc_ui_update_callback_t s_ui_callback = NULL;
static bool s_initialized = false;

#define RTC_LOCK() do { if (s_rtc_mutex) xSemaphoreTake(s_rtc_mutex, portMAX_DELAY); } while(0)
#define RTC_UNLOCK() do { if (s_rtc_mutex) xSemaphoreGive(s_rtc_mutex); } while(0)

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP time synchronization event received.");
    
    // The OS time is already updated by SNTP.
    if (tv != NULL) {
        rtc_set_time(tv->tv_sec);
    }
}

esp_err_t rtc_init_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP client...");
    
    // Set up standard SNTP configuration pointing to the global pool
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    
    // Attach our custom callback
    config.sync_cb = time_sync_notification_cb;
    
    // Initialize and start the SNTP service
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "SNTP client initialized. Waiting for Wi-Fi connection...");
    return ESP_OK;
}

static int parse_month(const char *month_str)
{
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++) {
        if (strncmp(month_str, months[i], 3) == 0) {
            return i;
        }
    }
    return 0;
}

static esp_err_t parse_compile_time(struct tm *timeinfo)
{
    const char *date = __DATE__;
    const char *time = __TIME__;
    char month_str[4];
    int day, year, hour, min, sec;
    
    if (sscanf(date, "%3s %d %d", month_str, &day, &year) != 3) return ESP_FAIL;
    if (sscanf(time, "%d:%d:%d", &hour, &min, &sec) != 3) return ESP_FAIL;
    
    timeinfo->tm_year = year - 1900;
    timeinfo->tm_mon = parse_month(month_str);
    timeinfo->tm_mday = day;
    timeinfo->tm_hour = hour;
    timeinfo->tm_min = min;
    timeinfo->tm_sec = sec;
    timeinfo->tm_isdst = -1;
    
    return ESP_OK;
}

static void rtc_update_task(void *arg)
{
    char time_str[TIME_STRING_SIZE];
    char date_str[DATE_STRING_SIZE];
    
    while (1) {
        RTC_LOCK();
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm timeinfo;
        localtime_r(&tv.tv_sec, &timeinfo);
        
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
        strftime(date_str, sizeof(date_str), "%A, %Y-%m-%d", &timeinfo);
        
        if (s_ui_callback) {
            s_ui_callback(time_str, date_str);
        }
        RTC_UNLOCK();
        vTaskDelay(pdMS_TO_TICKS(RTC_UPDATE_INTERVAL_MS));
    }
}

static void configure_vbat_auto_switch(void)
{
    // Enable automatic switching between main power (VDDA) and battery (VBAT)
    REG_SET_FIELD(PMU_HP_SLEEP_LP_DIG_POWER_REG, PMU_HP_SLEEP_VDDBAT_MODE, VBAT_MODE_AUTO);
    REG_SET_BIT(PMU_VDDBAT_CFG_REG, PMU_VDDBAT_SW_UPDATE);
    
    // Wait for the hardware configuration to be applied
    while(VBAT_MODE_AUTO != REG_GET_FIELD(PMU_VDDBAT_CFG_REG, PMU_ANA_VDDBAT_MODE)) {
        // Wait loop
    }
    ESP_LOGI(TAG, "PMU configured for automatic VBAT power switching.");
}

esp_err_t rtc_set_time(time_t timestamp)
{
    RTC_LOCK();
    struct timeval tv = { .tv_sec = timestamp, .tv_usec = 0 };
    // This inherently sets the ESP-IDF hardware RTC counter
    settimeofday(&tv, NULL); 
    RTC_UNLOCK();
    
    struct tm timeinfo;
    localtime_r(&timestamp, &timeinfo);
    char time_str[DATE_STRING_SIZE];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "RTC time manually set to: %s", time_str);
    
    return ESP_OK;
}

esp_err_t rtc_init(void)
{
    if (s_initialized) return ESP_OK;
    
    if (s_rtc_mutex == NULL) {
        s_rtc_mutex = xSemaphoreCreateMutex();
        if (s_rtc_mutex == NULL) return ESP_ERR_NO_MEM;
    }
    
    // 1. First and most important: Enable hardware VBAT switching!
    configure_vbat_auto_switch();
    
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to CET-1CEST");
    
    // 2. Check current hardware time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm timeinfo;
    localtime_r(&tv.tv_sec, &timeinfo);
    
    if (timeinfo.tm_year < (2026 - 1900)) {
        ESP_LOGW(TAG, "RTC lost power (Year %d). Setting to compile time.", timeinfo.tm_year + 1900);
        
        struct tm compile_timeinfo = {0};
        if (parse_compile_time(&compile_timeinfo) == ESP_OK) {
            time_t compile_time = mktime(&compile_timeinfo);
            rtc_set_time(compile_time);
        }
    } else {
        char time_str[DATE_STRING_SIZE];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "RTC successfully preserved time from VBAT: %s", time_str);
    }
    
    xTaskCreate(rtc_update_task, "rtc_update", RTC_TASK_STACK_SIZE, NULL, RTC_TASK_PRIORITY, NULL);
    
    s_initialized = true;
    ESP_LOGI(TAG, "RTC component initialized correctly with PMU support");
    return ESP_OK;
}

esp_err_t rtc_set_time_from_tm(const struct tm *timeinfo)
{
    if (!timeinfo) return ESP_ERR_INVALID_ARG;
    struct tm tm_copy = *timeinfo;
    return rtc_set_time(mktime(&tm_copy));
}

time_t rtc_get_timestamp(void)
{
    RTC_LOCK();
    struct timeval tv;
    gettimeofday(&tv, NULL);
    RTC_UNLOCK();
    return tv.tv_sec;
}

esp_err_t rtc_get_time_string(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < TIME_STRING_SIZE) return ESP_ERR_INVALID_ARG;
    RTC_LOCK();
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm timeinfo;
    localtime_r(&tv.tv_sec, &timeinfo);
    strftime(buffer, buffer_size, "%H:%M:%S", &timeinfo);
    RTC_UNLOCK();
    return ESP_OK;
}

esp_err_t rtc_get_date_string(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < DATE_STRING_SIZE) return ESP_ERR_INVALID_ARG;
    RTC_LOCK();
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm timeinfo;
    localtime_r(&tv.tv_sec, &timeinfo);
    strftime(buffer, buffer_size, "%A, %Y-%m-%d", &timeinfo);
    RTC_UNLOCK();
    return ESP_OK;
}

esp_err_t rtc_set_timezone(const char *tz_string)
{
    if (!tz_string) return ESP_ERR_INVALID_ARG;
    RTC_LOCK();
    setenv("TZ", tz_string, 1);
    tzset();
    RTC_UNLOCK();
    ESP_LOGI(TAG, "Timezone changed to: %s", tz_string);
    return ESP_OK;
}

esp_err_t rtc_register_ui_callback(rtc_ui_update_callback_t callback)
{
    if (!callback) return ESP_ERR_INVALID_ARG;
    RTC_LOCK();
    s_ui_callback = callback;
    RTC_UNLOCK();
    return ESP_OK;
}