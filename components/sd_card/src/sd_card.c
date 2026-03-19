/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sd_card.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "sd_protocol_types.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <fcntl.h>

// 1MB total file size, written in 16KB chunks
#define BENCH_FILE_SIZE (1 * 1024 * 1024) 
#define BENCH_CHUNK_SIZE (16 * 1024)

static const char *TAG = "sd_card";

static sdmmc_card_t *s_card = NULL;
static bool s_mounted = false;
static SemaphoreHandle_t s_sd_mutex = NULL;

#define SD_LOCK() do { if (s_sd_mutex) xSemaphoreTake(s_sd_mutex, portMAX_DELAY); } while(0)
#define SD_UNLOCK() do { if (s_sd_mutex) xSemaphoreGive(s_sd_mutex); } while(0)

static char* build_full_path(const char *path, char *full_path, size_t max_len)
{
    if (path[0] == '/') {
        snprintf(full_path, max_len, "%s", path);
    } else {
        snprintf(full_path, max_len, "%s/%s", SD_CARD_MOUNT_POINT, path);
    }
    return full_path;
}

char* sd_card_build_path(const char *path, char *full_path, size_t max_len)
{
    return build_full_path(path, full_path, max_len);
}

esp_err_t sd_card_init(void)
{
    esp_err_t ret = ESP_OK;

    if (s_sd_mutex == NULL) {
        s_sd_mutex = xSemaphoreCreateMutex();
        if (s_sd_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    SD_LOCK();

    if (s_mounted) {
        ESP_LOGW(TAG, "SD card already mounted");
        SD_UNLOCK();
        return ESP_OK;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 32 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // Enable the internal LDO for the SD card IO voltage domain (VDDPST_5)
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4, // LDO 4 is wired to SD_VREF on the ESP32-P4
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create on-chip LDO power control driver: %s", esp_err_to_name(ret));
        SD_UNLOCK();
        return ret;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.clk = 43;
    slot_config.cmd = 44;
    slot_config.d0 = 39;
    slot_config.d1 = 40;
    slot_config.d2 = 41;
    slot_config.d3 = 42;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting SD card on Slot %d...", host.slot);
    ESP_LOGI(TAG, "Pin config - CLK:%d CMD:%d D0:%d D1:%d D2:%d D3:%d", 
             slot_config.clk, slot_config.cmd, slot_config.d0, 
             slot_config.d1, slot_config.d2, slot_config.d3);
    
    ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        s_card = NULL;
        SD_UNLOCK();
        return ret;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD card mounted successfully");

    sdmmc_card_print_info(stdout, s_card);

    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_deinit(void)
{
    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGW(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        SD_UNLOCK();
        return ret;
    }

    s_card = NULL;
    s_mounted = false;
    ESP_LOGI(TAG, "SD card unmounted");

    SD_UNLOCK();
    return ESP_OK;
}

bool sd_card_is_mounted(void)
{
    bool mounted;
    SD_LOCK();
    mounted = s_mounted;
    SD_UNLOCK();
    return mounted;
}

float sd_card_get_total_capacity_gb(void)
{
    SD_LOCK();
    
    if (!s_mounted || s_card == NULL) {
        SD_UNLOCK();
        return 0.0f;
    }

    uint64_t total_bytes = ((uint64_t)s_card->csd.capacity) * s_card->csd.sector_size;
    float total_gb = (float)total_bytes / (1024.0f * 1024.0f * 1024.0f);

    SD_UNLOCK();
    return total_gb;
}

float sd_card_get_free_space_gb(void)
{
    SD_LOCK();

    if (!s_mounted) {
        SD_UNLOCK();
        return 0.0f;
    }

    uint64_t total_bytes, free_bytes;
    esp_err_t ret = esp_vfs_fat_info(SD_CARD_MOUNT_POINT, &total_bytes, &free_bytes);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get filesystem info: %s", esp_err_to_name(ret));
        SD_UNLOCK();
        return 0.0f;
    }

    float free_gb = (float)free_bytes / (1024.0f * 1024.0f * 1024.0f);

    SD_UNLOCK();
    return free_gb;
}

float sd_card_get_used_space_gb(void)
{
    float total = sd_card_get_total_capacity_gb();
    float free = sd_card_get_free_space_gb();
    return total - free;
}

esp_err_t sd_card_read_file(const char *path, uint8_t *buffer, size_t buffer_size, size_t *bytes_read)
{
    if (path == NULL || buffer == NULL || bytes_read == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    int fd = open(full_path, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    ssize_t result = read(fd, buffer, buffer_size);
    close(fd);

    if (result < 0) {
        ESP_LOGE(TAG, "Failed to read file: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    *bytes_read = (size_t)result;
    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_read_large_file(const char *path, uint8_t *buffer, size_t buffer_size, size_t *bytes_read)
{
    if (path == NULL || buffer == NULL || bytes_read == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    // 1. Open the file ONCE
    int fd = open(full_path, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    // 2. Adaptively allocate DMA buffer
    const size_t chunk_sizes[] = {32 * 1024, 16 * 1024, 8 * 1024, 4 * 1024};
    uint8_t *dma_buf = NULL;
    size_t chunk_size = 0;
    
    for (int i = 0; i < sizeof(chunk_sizes) / sizeof(chunk_sizes[0]); i++) {
        dma_buf = heap_caps_malloc(chunk_sizes[i], MALLOC_CAP_DMA | MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (dma_buf != NULL) {
            chunk_size = chunk_sizes[i];
            ESP_LOGI(TAG, "Allocated %zu KB DMA buffer for read", chunk_size / 1024);
            break;
        }
    }
    
    if (dma_buf == NULL) {
        ESP_LOGW(TAG, "Failed to allocate DMA buffer, falling back to direct read (will be slow)");
        ssize_t result = read(fd, buffer, buffer_size);
        close(fd);
        SD_UNLOCK();
        if (result < 0) return ESP_FAIL;
        *bytes_read = (size_t)result;
        return ESP_OK;
    }

    // 3. Chunk and Pump: Read from SD to fast DMA buffer, then memcpy to target (PSRAM)
    size_t total_read = 0;
    while (total_read < buffer_size) {
        size_t bytes_to_read = buffer_size - total_read;
        if (bytes_to_read > chunk_size) {
            bytes_to_read = chunk_size;
        }

        // Pull data from SD card directly into internal DMA RAM
        ssize_t r = read(fd, dma_buf, bytes_to_read);
        
        if (r < 0) {
            ESP_LOGE(TAG, "Read error at offset %zu", total_read);
            break; // Stop on error
        }
        
        if (r == 0) {
            break; // End of File reached
        }

        // Move the chunk from internal RAM to the user's buffer (likely PSRAM)
        memcpy(buffer + total_read, dma_buf, r);
        total_read += r;
        
        // If we read less than we asked for, we hit the EOF early
        if (r < bytes_to_read) {
            break; 
        }
    }

    // 4. Clean up
    free(dma_buf);
    close(fd);

    *bytes_read = total_read;
    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_write_file(const char *path, const uint8_t *data, size_t data_size)
{
    if (path == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    ssize_t written = write(fd, data, data_size);
    fsync(fd);
    close(fd);

    if (written != (ssize_t)data_size) {
        ESP_LOGE(TAG, "Write size mismatch: expected %zu, wrote %zd", data_size, written);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_write_large_file(const char *path, const uint8_t *data, size_t data_size)
{
    if (path == NULL || data == NULL || data_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    // 1. Open the file ONCE
    int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    // 2. Adaptively allocate DMA buffer - try larger sizes first for best performance
    const size_t chunk_sizes[] = {32 * 1024, 16 * 1024, 8 * 1024, 4 * 1024};
    uint8_t *dma_buf = NULL;
    size_t chunk_size = 0;
    
    for (int i = 0; i < sizeof(chunk_sizes) / sizeof(chunk_sizes[0]); i++) {
        dma_buf = heap_caps_malloc(chunk_sizes[i], MALLOC_CAP_DMA | MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (dma_buf != NULL) {
            chunk_size = chunk_sizes[i];
            ESP_LOGI(TAG, "Allocated %zu KB DMA buffer for write", chunk_size / 1024);
            break;
        }
    }
    
    if (dma_buf == NULL) {
        ESP_LOGW(TAG, "Failed to allocate any DMA buffer, falling back to direct write (will be slow)");
        ssize_t written = write(fd, data, data_size);
        fsync(fd);
        close(fd);
        SD_UNLOCK();
        return (written == (ssize_t)data_size) ? ESP_OK : ESP_FAIL;
    }

    // 3. Chunk and Pump: Copy from source (e.g., PSRAM) to DMA buffer, then write
    size_t bytes_written_total = 0;
    while (bytes_written_total < data_size) {
        size_t bytes_to_write = data_size - bytes_written_total;
        if (bytes_to_write > chunk_size) {
            bytes_to_write = chunk_size;
        }

        // Move a chunk into the fast DMA lane
        memcpy(dma_buf, data + bytes_written_total, bytes_to_write);

        // Blast the chunk to the SD card
        ssize_t written = write(fd, dma_buf, bytes_to_write);
        
        if (written != (ssize_t)bytes_to_write) {
            ESP_LOGE(TAG, "Write error at offset %zu. Card full or removed?", bytes_written_total);
            free(dma_buf);
            close(fd);
            SD_UNLOCK();
            return ESP_FAIL;
        }
        
        bytes_written_total += written;
    }

    // 4. Clean up, force the FAT table update ONCE, and close
    free(dma_buf);
    fsync(fd);
    close(fd);

    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_append_file(const char *path, const uint8_t *data, size_t data_size)
{
    if (path == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    int fd = open(full_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open file for appending: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    ssize_t written = write(fd, data, data_size);
    fsync(fd);
    close(fd);

    if (written != (ssize_t)data_size) {
        ESP_LOGE(TAG, "Append size mismatch: expected %zu, wrote %zd", data_size, written);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_delete_file(const char *path)
{
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    if (unlink(full_path) != 0) {
        ESP_LOGE(TAG, "Failed to delete file: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    SD_UNLOCK();
    return ESP_OK;
}

bool sd_card_file_exists(const char *path)
{
    if (path == NULL) {
        return false;
    }

    SD_LOCK();

    if (!s_mounted) {
        SD_UNLOCK();
        return false;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    struct stat st;
    bool exists = (stat(full_path, &st) == 0);

    SD_UNLOCK();
    return exists;
}

esp_err_t sd_card_get_file_size(const char *path, size_t *size)
{
    if (path == NULL || size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    struct stat st;
    if (stat(full_path, &st) != 0) {
        ESP_LOGE(TAG, "Failed to stat file: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    *size = st.st_size;

    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_rename_file(const char *old_path, const char *new_path)
{
    if (old_path == NULL || new_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_old_path[512];
    char full_new_path[512];
    build_full_path(old_path, full_old_path, sizeof(full_old_path));
    build_full_path(new_path, full_new_path, sizeof(full_new_path));

    if (rename(full_old_path, full_new_path) != 0) {
        ESP_LOGE(TAG, "Failed to rename file from %s to %s", full_old_path, full_new_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_list_dir(const char *path, sd_card_dir_callback_t callback, void *user_data)
{
    if (path == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    DIR *dir = opendir(full_path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        sd_card_file_info_t file_info = {0};
        snprintf(file_info.name, sizeof(file_info.name), "%s", entry->d_name);
        file_info.is_directory = (entry->d_type == DT_DIR);

        if (!file_info.is_directory) {
            char file_path[768];
            snprintf(file_path, sizeof(file_path), "%s/%s", full_path, entry->d_name);
            struct stat st;
            if (stat(file_path, &st) == 0) {
                file_info.size = st.st_size;
            }
        }

        callback(&file_info, user_data);
    }

    closedir(dir);
    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_create_dir(const char *path)
{
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    if (mkdir(full_path, 0775) != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    SD_UNLOCK();
    return ESP_OK;
}

esp_err_t sd_card_remove_dir(const char *path)
{
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    SD_LOCK();

    if (!s_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        SD_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    char full_path[512];
    build_full_path(path, full_path, sizeof(full_path));

    if (rmdir(full_path) != 0) {
        ESP_LOGE(TAG, "Failed to remove directory: %s", full_path);
        SD_UNLOCK();
        return ESP_FAIL;
    }

    SD_UNLOCK();
    return ESP_OK;
}

void sd_card_benchmark(void)
{
    const char *test_path = "bench.tmp";
    
    uint8_t *buf = malloc(BENCH_FILE_SIZE);
    if (!buf) {
        ESP_LOGE("BENCH", "Failed to allocate buffer (%d bytes)", BENCH_FILE_SIZE);
        return;
    }
    memset(buf, 0xAA, BENCH_FILE_SIZE);

    // --- WRITE BENCHMARK ---
    ESP_LOGI("BENCH", "Starting WRITE benchmark (1MB) using sd_card_write_large_file...");
    int64_t start_time = esp_timer_get_time();
    
    esp_err_t ret = sd_card_write_large_file(test_path, buf, BENCH_FILE_SIZE);
    
    int64_t end_time = esp_timer_get_time();
    float time_sec = (end_time - start_time) / 1000000.0f;
    float speed_mb = (BENCH_FILE_SIZE / (1024.0f * 1024.0f)) / time_sec;
    
    if (ret == ESP_OK) {
        ESP_LOGI("BENCH", "Write took %.2f sec -> ** %.2f MB/s **", time_sec, speed_mb);
    } else {
        ESP_LOGE("BENCH", "Write failed!");
    }

    // --- READ BENCHMARK (1MB test) ---
    ESP_LOGI("BENCH", "Starting READ benchmark (1MB) using sd_card_read_large_file...");
    start_time = esp_timer_get_time();
    
    size_t bytes_read = 0;
    // We are reading the whole 1MB file back into the 1MB PSRAM buffer
    ret = sd_card_read_large_file(test_path, buf, BENCH_FILE_SIZE, &bytes_read); 
    
    end_time = esp_timer_get_time();
    time_sec = (end_time - start_time) / 1000000.0f;
    speed_mb = (bytes_read / (1024.0f * 1024.0f)) / time_sec;
    
    if (ret == ESP_OK) {
        ESP_LOGI("BENCH", "Read %zu bytes took %.4f sec -> ** %.2f MB/s **", bytes_read, time_sec, speed_mb);
    } else {
        ESP_LOGE("BENCH", "Read failed!");
    }

    // --- CLEANUP ---
    sd_card_delete_file(test_path);
    free(buf);
    ESP_LOGI("BENCH", "Benchmark complete. Temp file deleted.");
}