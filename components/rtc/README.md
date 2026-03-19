# RTC Component

Real-Time Clock component for ESP32-P4 with hardware battery backup support.

## Overview

This component provides real-time clock functionality using the ESP32-P4's built-in hardware RTC with battery backup (CR1220 rechargeable battery). Time persists across power cycles when the battery is installed and charged.

## Features

- **Hardware RTC with Battery Backup**: Time persists across power cycles
- **Automatic Time Initialization**: Sets time from compile time on first boot
- **Battery Backup Detection**: Restores time from hardware RTC if valid
- **Timezone Support**: Configurable timezone with DST support (default: CET-1CEST)
- **Thread-Safe**: All operations protected by mutex
- **Non-Blocking Updates**: Background FreeRTOS task updates UI variables every second
- **UI Integration**: Callback mechanism for updating UI variables

## Hardware Requirements

- ESP32-P4 microcontroller
- CR1220 rechargeable battery (3V-3.3V) installed in RTC battery holder
- The board automatically charges the RTC battery when main power is connected

## API Functions

### Initialization

```c
esp_err_t rtc_init(void);
```

Initializes the RTC component. Call this early in `app_main()` before UI initialization.

### Time Retrieval

```c
time_t rtc_get_timestamp(void);
esp_err_t rtc_get_time_string(char *buffer, size_t buffer_size);
esp_err_t rtc_get_date_string(char *buffer, size_t buffer_size);
```

### Time Setting

```c
esp_err_t rtc_set_time(time_t timestamp);
esp_err_t rtc_set_time_from_tm(const struct tm *timeinfo);
```

### Timezone Configuration

```c
esp_err_t rtc_set_timezone(const char *tz_string);
```

Example timezone strings:
- `"CET-1CEST,M3.5.0,M10.5.0/3"` - Central European Time with DST
- `"UTC0"` - UTC
- `"EST5EDT,M3.2.0,M11.1.0"` - Eastern Time (US)

### UI Callback

```c
esp_err_t rtc_register_ui_callback(rtc_ui_update_callback_t callback);
```

Register a callback function that will be called every second with updated time and date strings.

## Usage Example

```c
#include "rtc_component.h"

// UI variable buffers
static char rtc_time_buffer[16] = "--:--:--";
static char rtc_date_buffer[32] = "Loading...";

// Callback for UI updates
void rtc_ui_update(const char *time_str, const char *date_str) {
    if (time_str && date_str) {
        strncpy(rtc_time_buffer, time_str, sizeof(rtc_time_buffer) - 1);
        strncpy(rtc_date_buffer, date_str, sizeof(rtc_date_buffer) - 1);
    }
}

// UI variable getters (for EEZ Studio)
const char* get_var_rtc_time_string(void) {
    return rtc_time_buffer;
}

const char* get_var_rtc_date_string(void) {
    return rtc_date_buffer;
}

void app_main(void) {
    // Initialize RTC
    esp_err_t ret = rtc_init();
    if (ret == ESP_OK) {
        // Register callback for UI updates
        rtc_register_ui_callback(rtc_ui_update);
    }
    
    // ... rest of application initialization
}
```

## Time Formats

- **Time String**: `HH:MM:SS` (24-hour format)
- **Date String**: `Weekday, YYYY-MM-DD` (e.g., "Monday, 2026-03-04")

## Battery Backup Behavior

1. **First Boot (no battery or discharged)**:
   - Time is set from compile time (`__DATE__` and `__TIME__`)
   - Logs: "RTC time set from compile time: ..."

2. **Subsequent Boots (battery backup working)**:
   - Time is restored from hardware RTC
   - Logs: "RTC time restored from battery backup: ..."

3. **Power Cycle Test**:
   - Power off device completely
   - Wait a few minutes
   - Power on device
   - Time should continue from where it left off (not reset to compile time)

## Configuration

### Default Timezone

The component defaults to Central European Time (CET) with automatic DST:
```c
setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
```

### Task Configuration

- **Stack Size**: 3072 bytes
- **Priority**: 1 (low priority, doesn't interfere with UI)
- **Update Interval**: 1 second

## Integration with EEZ Studio UI

To display time and date in the UI:

1. Add variables in EEZ Studio UI editor:
   - `rtc_time_string` (type: string)
   - `rtc_date_string` (type: string)

2. EEZ Studio will auto-generate declarations in `components/ui/vars.h`

3. Implement getters in `main.c` (see usage example above)

4. The RTC component will automatically update the buffers every second via callback

## Troubleshooting

### Time resets to compile time on every boot

- Check if CR1220 battery is installed
- Check if battery is charged (board charges it when powered)
- Battery may be dead and needs replacement

### Time is incorrect

- Use `rtc_set_time()` to manually set the correct time
- Consider adding SNTP (network time) synchronization for automatic updates

### Timezone not applied correctly

- Verify POSIX timezone string format
- Check ESP-IDF documentation for timezone string examples

## Future Enhancements

- SNTP (network time) synchronization
- User interface for manual time/date adjustment
- Battery voltage monitoring and low battery warnings
- Persistent timezone storage in NVS

## Dependencies

- ESP-IDF FreeRTOS
- Standard C library (time.h, sys/time.h)

## License

SPDX-License-Identifier: Apache-2.0
