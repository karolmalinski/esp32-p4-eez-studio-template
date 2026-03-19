/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sys/cdefs.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the screenshot component
 * 
 * Creates the screenshots directory on SD card and initializes the counter.
 * 
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_STATE: SD card not mounted
 *    - ESP_FAIL: Failed to create directory or initialize counter
 */
esp_err_t screenshot_init(void);

/**
 * @brief Capture the current screen and save to SD card as BMP
 * 
 * Takes a snapshot of the current LVGL screen and saves it as a BMP file
 * with sequential numbering (e.g., screenshot_001.bmp).
 * 
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_STATE: SD card not mounted or screenshot not initialized
 *    - ESP_ERR_NO_MEM: Failed to allocate memory for snapshot
 *    - ESP_FAIL: Failed to write file
 */
esp_err_t screenshot_capture_and_save(void);

/**
 * @brief Get the next screenshot number
 * 
 * @return The next sequential screenshot number
 */
uint32_t screenshot_get_next_number(void);

#ifdef __cplusplus
}
#endif
