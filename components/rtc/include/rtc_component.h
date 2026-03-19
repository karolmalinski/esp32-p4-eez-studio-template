/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sys/cdefs.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type for UI variable updates
 * 
 * @param time_str Formatted time string (HH:MM:SS)
 * @param date_str Formatted date string (e.g., "Monday, 2026-03-04")
 */
typedef void (*rtc_ui_update_callback_t)(const char *time_str, const char *date_str);


/**
 * @brief Initializes the SNTP client to sync time via Wi-Fi.
 * Should be called after Wi-Fi is connected, or right before.
 * * @return esp_err_t ESP_OK on success.
 */
esp_err_t rtc_init_sntp(void);

/**
 * @brief Initialize the RTC component
 * 
 * Sets up timezone, checks if hardware RTC has valid time from battery backup,
 * and initializes time from compile time if needed. Creates a background task
 * to update UI variables every second.
 * 
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Failed to initialize
 */
esp_err_t rtc_init(void);

/**
 * @brief Set the RTC time manually
 * 
 * @param timestamp Unix timestamp to set
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Failed to set time
 */
esp_err_t rtc_set_time(time_t timestamp);

/**
 * @brief Set the RTC time from struct tm
 * 
 * @param timeinfo Pointer to struct tm with time information
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Failed to set time
 */
esp_err_t rtc_set_time_from_tm(const struct tm *timeinfo);

/**
 * @brief Get current timestamp from hardware RTC
 * 
 * @return Current Unix timestamp
 */
time_t rtc_get_timestamp(void);

/**
 * @brief Get formatted time string
 * 
 * @param buffer Buffer to store time string (minimum 9 bytes for "HH:MM:SS\0")
 * @param buffer_size Size of the buffer
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t rtc_get_time_string(char *buffer, size_t buffer_size);

/**
 * @brief Get formatted date string
 * 
 * @param buffer Buffer to store date string (minimum 32 bytes recommended)
 * @param buffer_size Size of the buffer
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t rtc_get_date_string(char *buffer, size_t buffer_size);

/**
 * @brief Set timezone
 * 
 * @param tz_string POSIX timezone string (e.g., "CET-1CEST,M3.5.0,M10.5.0/3")
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t rtc_set_timezone(const char *tz_string);

/**
 * @brief Register callback for UI variable updates
 * 
 * The callback will be called every second with updated time and date strings.
 * 
 * @param callback Callback function pointer
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t rtc_register_ui_callback(rtc_ui_update_callback_t callback);

#ifdef __cplusplus
}
#endif
