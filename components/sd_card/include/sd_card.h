/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sys/cdefs.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SD_CARD_MOUNT_POINT "/sdcard"
#define SD_CARD_BUFFER_SIZE (16 * 1024)

typedef struct {
    char name[256];
    size_t size;
    bool is_directory;
} sd_card_file_info_t;

typedef void (*sd_card_dir_callback_t)(const sd_card_file_info_t *file_info, void *user_data);

esp_err_t sd_card_init(void);

esp_err_t sd_card_deinit(void);

bool sd_card_is_mounted(void);

float sd_card_get_total_capacity_gb(void);

float sd_card_get_free_space_gb(void);

float sd_card_get_used_space_gb(void);

esp_err_t sd_card_read_file(const char *path, uint8_t *buffer, size_t buffer_size, size_t *bytes_read);

esp_err_t sd_card_read_large_file(const char *path, uint8_t *buffer, size_t buffer_size, size_t *bytes_read);

/**
 * @brief Writes a large file to the SD card using optimized DMA chunking.
 * Ideal for writing framebuffers from PSRAM (e.g., BMP screenshots).
 * @param path File path relative to the mount point (e.g., "screenshots/img1.bmp")
 * @param data Pointer to the data to write (can be in PSRAM)
 * @param data_size Total size of the data in bytes
 * @return esp_err_t ESP_OK on success, or an error code
 */
esp_err_t sd_card_write_large_file(const char *path, const uint8_t *data, size_t data_size);

esp_err_t sd_card_write_file(const char *path, const uint8_t *data, size_t data_size);

esp_err_t sd_card_append_file(const char *path, const uint8_t *data, size_t data_size);

esp_err_t sd_card_delete_file(const char *path);

bool sd_card_file_exists(const char *path);

esp_err_t sd_card_get_file_size(const char *path, size_t *size);

esp_err_t sd_card_rename_file(const char *old_path, const char *new_path);

esp_err_t sd_card_list_dir(const char *path, sd_card_dir_callback_t callback, void *user_data);

esp_err_t sd_card_create_dir(const char *path);

esp_err_t sd_card_remove_dir(const char *path);

char* sd_card_build_path(const char *path, char *full_path, size_t max_len);

void sd_card_benchmark(void);

#ifdef __cplusplus
}
#endif
