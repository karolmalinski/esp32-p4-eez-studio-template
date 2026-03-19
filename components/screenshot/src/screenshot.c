/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "screenshot.h"
#include "sd_card.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "screenshot";

#define SCREENSHOT_DIR "screenshots"
#define COUNTER_FILE "screenshots/.counter"
#define DISPLAY_WIDTH 1024
#define DISPLAY_HEIGHT 600

static uint32_t s_screenshot_counter = 0;
static bool s_initialized = false;

// BMP file header structures
#pragma pack(push, 1)
typedef struct {
    uint16_t type;           // Magic identifier: 0x4d42 ('BM')
    uint32_t size;           // File size in bytes
    uint16_t reserved1;      // Not used
    uint16_t reserved2;      // Not used
    uint32_t offset;         // Offset to image data in bytes
} bmp_file_header_t;

typedef struct {
    uint32_t size;           // Header size in bytes
    int32_t width;           // Width of image
    int32_t height;          // Height of image
    uint16_t planes;         // Number of color planes
    uint16_t bits;           // Bits per pixel
    uint32_t compression;    // Compression type
    uint32_t imagesize;      // Image size in bytes
    int32_t xresolution;     // Pixels per meter
    int32_t yresolution;     // Pixels per meter
    uint32_t ncolors;        // Number of colors
    uint32_t importantcolors;// Important colors
} bmp_info_header_t;
#pragma pack(pop)

static esp_err_t load_counter(void)
{
    if (!sd_card_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!sd_card_file_exists(COUNTER_FILE)) {
        s_screenshot_counter = 0;
        return ESP_OK;
    }

    uint8_t buffer[16];
    size_t bytes_read;
    esp_err_t ret = sd_card_read_file(COUNTER_FILE, buffer, sizeof(buffer), &bytes_read);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read counter file, starting from 0");
        s_screenshot_counter = 0;
        return ESP_OK;
    }

    buffer[bytes_read] = '\0';
    s_screenshot_counter = atoi((char *)buffer);
    ESP_LOGI(TAG, "Loaded screenshot counter: %u", (unsigned int)s_screenshot_counter);
    return ESP_OK;
}

static esp_err_t save_counter(void)
{
    if (!sd_card_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%u", (unsigned int)s_screenshot_counter);
    
    esp_err_t ret = sd_card_write_file(COUNTER_FILE, (uint8_t *)buffer, strlen(buffer));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save counter file");
        return ret;
    }

    return ESP_OK;
}

esp_err_t screenshot_init(void)
{
    if (!sd_card_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    // Create screenshots directory if it doesn't exist
    if (!sd_card_file_exists(SCREENSHOT_DIR)) {
        esp_err_t ret = sd_card_create_dir(SCREENSHOT_DIR);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create screenshots directory");
            return ret;
        }
        ESP_LOGI(TAG, "Created screenshots directory");
    }

    // Load counter
    esp_err_t ret = load_counter();
    if (ret != ESP_OK) {
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Screenshot component initialized");
    return ESP_OK;
}

uint32_t screenshot_get_next_number(void)
{
    return s_screenshot_counter + 1;
}

static esp_err_t write_bmp_file(const char *filename, lv_draw_buf_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Get snapshot data
    uint32_t width = snapshot->header.w;
    uint32_t height = snapshot->header.h;
    uint8_t *data = (uint8_t *)snapshot->data;

    // BMP rows must be padded to 4-byte boundary
    uint32_t row_size = ((width * 3 + 3) / 4) * 4;
    uint32_t image_size = row_size * height;

    // Prepare BMP headers
    bmp_file_header_t file_header = {
        .type = 0x4D42,  // 'BM'
        .size = sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t) + image_size,
        .reserved1 = 0,
        .reserved2 = 0,
        .offset = sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t)
    };

    bmp_info_header_t info_header = {
        .size = sizeof(bmp_info_header_t),
        .width = width,
        .height = height,
        .planes = 1,
        .bits = 24,
        .compression = 0,
        .imagesize = image_size,
        .xresolution = 2835,  // 72 DPI
        .yresolution = 2835,
        .ncolors = 0,
        .importantcolors = 0
    };

    // Allocate buffer for one row with padding
    uint8_t *row_buffer = (uint8_t *)lv_malloc(row_size);
    if (row_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate row buffer");
        return ESP_ERR_NO_MEM;
    }

    // Open file for writing
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", filename);
        lv_free(row_buffer);
        return ESP_FAIL;
    }

    // Write headers
    fwrite(&file_header, sizeof(file_header), 1, f);
    fwrite(&info_header, sizeof(info_header), 1, f);

    // Write pixel data (bottom-up, BGR format)
    for (int32_t y = height - 1; y >= 0; y--) {
        memset(row_buffer, 0, row_size);
        
        for (uint32_t x = 0; x < width; x++) {
            // ARGB8888 format on little-endian: stored in memory as BGRA
            uint32_t pixel_offset = (y * width + x) * 4;
            uint8_t b = data[pixel_offset + 0];
            uint8_t g = data[pixel_offset + 1];
            uint8_t r = data[pixel_offset + 2];

            // BMP format: BGR (no alpha)
            uint32_t bmp_offset = x * 3;
            row_buffer[bmp_offset + 0] = b;
            row_buffer[bmp_offset + 1] = g;
            row_buffer[bmp_offset + 2] = r;
        }

        fwrite(row_buffer, row_size, 1, f);
    }

    fclose(f);
    lv_free(row_buffer);

    ESP_LOGI(TAG, "BMP file written: %s (%lux%lu)", filename, width, height);
    return ESP_OK;
}

esp_err_t screenshot_capture_and_save(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Screenshot component not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!sd_card_is_mounted()) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    // Get current screen
    lv_obj_t *screen = lv_scr_act();
    if (screen == NULL) {
        ESP_LOGE(TAG, "No active screen");
        return ESP_FAIL;
    }

    // Take snapshot
    ESP_LOGI(TAG, "Taking screenshot...");
    lv_draw_buf_t *snapshot = lv_snapshot_take(screen, LV_COLOR_FORMAT_ARGB8888);
    if (snapshot == NULL) {
        ESP_LOGE(TAG, "Failed to take snapshot (out of memory)");
        return ESP_ERR_NO_MEM;
    }

    // Generate filename
    s_screenshot_counter++;
    char relative_path[64];
    snprintf(relative_path, sizeof(relative_path), "%s/screenshot_%03u.bmp", SCREENSHOT_DIR, (unsigned int)s_screenshot_counter);

    // Build absolute path
    char filename[256];
    sd_card_build_path(relative_path, filename, sizeof(filename));

    ESP_LOGI(TAG, "Opening file: %s", filename);

    // Write BMP file
    esp_err_t ret = write_bmp_file(filename, snapshot);
    
    // Free snapshot buffer
    lv_draw_buf_destroy(snapshot);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write BMP file");
        s_screenshot_counter--;  // Revert counter on failure
        return ret;
    }

    // Save counter
    save_counter();

    ESP_LOGI(TAG, "Screenshot saved: %s", filename);
    return ESP_OK;
}
