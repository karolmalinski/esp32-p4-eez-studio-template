/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_err.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "sd_card.h"
#include "ui.h"
#include "wifi_module.h"

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        }
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);

    bsp_display_backlight_on();

    if (disp != NULL)
    {
        bsp_display_rotate(disp, LV_DISPLAY_ROTATION_180);
    }

    bsp_display_lock(0);
    
    ui_init();
        
    bsp_display_unlock();

    esp_err_t ret;
    
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGW("MAIN", "SD card initialization failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("MAIN", "SD card initialized successfully");
    }

    ret = wifi_module_init();
    if (ret != ESP_OK) {
        ESP_LOGW("MAIN", "Wi-Fi module initialization failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("MAIN", "Wi-Fi module initialized successfully");
    }
    
    while (1) {
        bsp_display_lock(0);
        ui_tick();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}