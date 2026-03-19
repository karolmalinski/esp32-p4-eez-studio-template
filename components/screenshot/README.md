# Screenshot Component

Automatic screenshot capture component for ESP32-P4 with LVGL v9.

## Features

- Captures screenshots on tab switches
- Captures screenshots on screen transitions
- Saves as BMP format (uncompressed, 24-bit color)
- Sequential numbering (screenshot_001.bmp, screenshot_002.bmp, etc.)
- Stores screenshots in `/sdcard/screenshots/` directory
- Automatic directory creation
- Persistent counter across reboots

## Usage

### Initialization

```c
#include "screenshot.h"

// Initialize after SD card is mounted
esp_err_t ret = screenshot_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Screenshot init failed");
}
```

### Manual Capture

```c
// Capture current screen and save to SD card
esp_err_t ret = screenshot_capture_and_save();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Screenshot capture failed");
}
```

### Get Next Screenshot Number

```c
uint32_t next_num = screenshot_get_next_number();
ESP_LOGI(TAG, "Next screenshot will be: screenshot_%03lu.bmp", next_num);
```

## Automatic Capture

Screenshots are automatically captured in two scenarios:

1. **Tab Switches**: When user switches between tabs in the main screen
2. **Screen Transitions**: When navigating between different screens (Splash, Main, Menu, etc.)

## File Format

- **Format**: BMP (Bitmap)
- **Color Depth**: 24-bit RGB
- **Compression**: None
- **Size**: ~1.8MB per screenshot (1024x600 resolution)
- **Pixel Format**: BGR (converted from ARGB8888)

## Storage

- **Location**: `/sdcard/screenshots/`
- **Naming**: `screenshot_001.bmp`, `screenshot_002.bmp`, etc.
- **Counter File**: `/sdcard/screenshots/.counter` (tracks last screenshot number)

## Memory Usage

- **Snapshot Buffer**: ~2.4MB (1024x600x4 bytes ARGB8888)
- **Row Buffer**: ~3KB (1024x3 bytes with padding)
- Memory is allocated using LVGL's allocator and freed immediately after writing

## Error Handling

The component gracefully handles:
- SD card not mounted
- Insufficient memory for snapshot
- File write failures
- Directory creation failures

All errors are logged using ESP_LOG.

## Dependencies

- `sd_card` component
- `lvgl` (LVGL v9)

## Integration Points

- `main/chart_helper.c`: Tab switch detection
- `components/ui/eez-flow.cpp`: Screen transition detection
- `main/main.c`: Component initialization
