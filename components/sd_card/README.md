# SD Card Component

SD card driver for ESP32-P4 with SDMMC interface running at 40MHz.

## Features

- 40MHz high-speed SDMMC bus operation
- Thread-safe operations with mutex protection
- FAT filesystem support
- Comprehensive file and directory operations
- Capacity and free space reporting

## Hardware Configuration

The component is configured for the ESP32-P4-WIFI6-Touch-LCD-7B board with the following pin mapping:

- CLK: GPIO 43
- CMD: GPIO 44
- D0: GPIO 39
- D1: GPIO 40
- D2: GPIO 41
- D3: GPIO 42

## API Reference

### Initialization

```c
esp_err_t sd_card_init(void);
```
Initialize the SD card with 40MHz bus speed. Returns ESP_OK on success.

```c
esp_err_t sd_card_deinit(void);
```
Unmount and cleanup the SD card.

```c
bool sd_card_is_mounted(void);
```
Check if SD card is currently mounted.

### Capacity Information

```c
float sd_card_get_total_capacity_gb(void);
```
Get total SD card capacity in GB. Returns 0 if not mounted.

```c
float sd_card_get_free_space_gb(void);
```
Get free space in GB. Returns 0 if not mounted.

```c
float sd_card_get_used_space_gb(void);
```
Get used space in GB. Returns 0 if not mounted.

### File Operations

```c
esp_err_t sd_card_read_file(const char *path, uint8_t *buffer, size_t buffer_size, size_t *bytes_read);
```
Read file from SD card into buffer.

```c
esp_err_t sd_card_write_file(const char *path, const uint8_t *data, size_t data_size);
```
Write data to file (overwrites existing file).

```c
esp_err_t sd_card_append_file(const char *path, const uint8_t *data, size_t data_size);
```
Append data to existing file.

```c
esp_err_t sd_card_delete_file(const char *path);
```
Delete a file.

```c
bool sd_card_file_exists(const char *path);
```
Check if file exists.

```c
esp_err_t sd_card_get_file_size(const char *path, size_t *size);
```
Get file size in bytes.

```c
esp_err_t sd_card_rename_file(const char *old_path, const char *new_path);
```
Rename or move a file.

### Directory Operations

```c
esp_err_t sd_card_list_dir(const char *path, sd_card_dir_callback_t callback, void *user_data);
```
List files in directory. Callback is called for each file/directory.

```c
esp_err_t sd_card_create_dir(const char *path);
```
Create a directory.

```c
esp_err_t sd_card_remove_dir(const char *path);
```
Remove an empty directory.

## Usage Example

```c
#include "sd_card.h"

void app_main(void)
{
    // Initialize SD card
    esp_err_t ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card init failed");
        return;
    }

    // Get capacity info
    float total_gb = sd_card_get_total_capacity_gb();
    float free_gb = sd_card_get_free_space_gb();
    ESP_LOGI(TAG, "SD Card: %.2f GB total, %.2f GB free", total_gb, free_gb);

    // Write a file
    const char *data = "Hello, SD Card!";
    sd_card_write_file("test.txt", (uint8_t *)data, strlen(data));

    // Read the file
    uint8_t buffer[256];
    size_t bytes_read;
    sd_card_read_file("test.txt", buffer, sizeof(buffer), &bytes_read);
    buffer[bytes_read] = '\0';
    ESP_LOGI(TAG, "Read: %s", buffer);
}
```

## Error Handling

If SD card initialization fails (card not inserted, corrupted, etc.), the application continues running with:
- All capacity functions return 0
- File operations return ESP_ERR_INVALID_STATE
- The UI can notify the user about the missing SD card

## Thread Safety

All SD card operations are protected by a mutex, making them safe to call from multiple tasks.

## Buffer Size

File read/write operations use a 16KB buffer size for optimal performance.
