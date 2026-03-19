# ESP32-P4 EEZ Studio Template

A comprehensive firmware template for ESP32-P4 based projects using EEZ Studio for UI design and LVGL v9 for graphics rendering. This template provides a complete starting point with display management, WiFi connectivity, SD card support, audio playback, RTC functionality, and automatic screenshot capture.

## Hardware

This project is designed for the **ESP32-P4-WIFI6-Touch-LCD-7B** development board featuring:

- **MCU**: ESP32-P4 (Dual-core RISC-V, up to 400MHz)
- **Display**: 7-inch capacitive touch LCD (1024x600 resolution)
- **WiFi**: ESP32-C6 WiFi 6 module (connected via SDIO)
- **Memory**: 32MB Flash, PSRAM support
- **Storage**: SD card slot (SDMMC interface, 40MHz)
- **Audio**: ES8311 audio codec with I2S interface
- **RTC**: Hardware RTC with CR1220 battery backup
- **Connectivity**: USB, UART, GPIO expansion

## Features

- **Modern UI Framework**: EEZ Studio visual designer with LVGL v9
- **WiFi Connectivity**: Station mode with automatic reconnection and mDNS service discovery
- **SD Card Support**: High-speed SDMMC interface with comprehensive file operations
- **Audio Playback**: MP3 playback with ES8311 codec, volume control, and file iteration
- **Screenshot Capture**: Automatic BMP screenshots on screen/tab transitions
- **Real-Time Clock**: Battery-backed RTC with timezone support and DST
- **Display Management**: 1024x600 BGR display with rotation, backlight control, and PSRAM buffering
- **Optimized Performance**: PPA acceleration, dual draw units, 4MB image cache

## Prerequisites

### Required Software

- **ESP-IDF v5.4.0** or later
  - Installation guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
- **EEZ Studio v0.18.0** or later
  - Download: https://github.com/eez-open/studio/releases
- **Git** for cloning the repository
- **Python 3.8+** (included with ESP-IDF)

### Hardware Requirements

- ESP32-P4-WIFI6-Touch-LCD-7B development board
- USB-C cable for programming and power
- SD card (optional, for screenshots and audio files)
- CR1220 battery (optional, for RTC backup)

## Quick Start

### 1. Clone the Repository

```bash
git clone <repository-url>
cd esp32-p4-eez-studio-template
```

### 2. Set up ESP-IDF Environment

```bash
# Source the ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Or on Windows
%userprofile%\esp\esp-idf\export.bat
```

### 3. Build the Project

```bash
# Set target to ESP32-P4
idf.py set-target esp32p4

# Build the firmware
idf.py build
```

### 4. Flash to Device

```bash
# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# On Windows, use COM port
idf.py -p COM3 flash monitor
```

### 5. First Boot

On first boot, the device will:
- Initialize the display with LVGL UI
- Attempt to mount the SD card (warning if not present)
- Initialize WiFi module (not connected until configured)
- Set RTC time from compile time (or restore from battery backup)

## Project Structure

```
esp32-p4-eez-studio-template/
├── main/
│   ├── main.c                      # Application entry point
│   └── CMakeLists.txt
├── components/
│   ├── ui/                         # EEZ Studio generated UI code
│   │   ├── screens.c/h             # Screen definitions
│   │   ├── vars.h                  # UI variables
│   │   ├── actions.h               # UI actions
│   │   ├── eez-flow.cpp/h          # Flow engine
│   │   └── ui.c/h                  # UI initialization
│   ├── wifi_module/                # WiFi and mDNS functionality
│   ├── sd_card/                    # SD card driver (SDMMC)
│   ├── screenshot/                 # Screenshot capture component
│   ├── rtc/                        # Real-time clock component
│   └── bsp_extra/                  # Audio codec and player
├── esp32-p4-template.eez-project   # EEZ Studio project file
├── sdkconfig.defaults              # Default configuration
├── partitions.csv                  # Partition table
└── README.md                       # This file
```

## Using EEZ Studio

EEZ Studio is a visual development tool for designing user interfaces with LVGL. This project uses EEZ Studio to generate the UI code in the `components/ui/` directory.

### Installation

1. Download EEZ Studio from https://github.com/eez-open/studio/releases
2. Install for your platform (Windows, macOS, or Linux)
3. Launch EEZ Studio

### Opening the Project

1. In EEZ Studio, go to **File → Open Project**
2. Navigate to the project directory
3. Select `esp32-p4-template.eez-project`
4. The project will open showing screens, widgets, variables, and flow

### Editing the UI

**Screens**: Design your UI screens with LVGL widgets
- Add widgets from the widget palette
- Configure properties in the properties panel
- Arrange widgets visually on the canvas

**Variables**: Define data bindings for dynamic content
- Native variables (C/C++ getters/setters)
- Flow variables (managed by EEZ Flow engine)
- Use variables in widget properties for dynamic updates

**Actions**: Define event handlers and user interactions
- Button clicks, value changes, screen transitions
- Can call native C functions or Flow actions

**Flow**: Visual programming for application logic
- State machines, timers, data processing
- Integrates with native code seamlessly

### Building and Exporting

1. In EEZ Studio, click **Build** (or press F7)
2. Generated code is automatically exported to `components/ui/`
3. Files updated:
   - `screens.c/h` - Screen layouts and widgets
   - `vars.h` - Variable declarations
   - `actions.h` - Action declarations
   - `eez-flow.cpp/h` - Flow engine code
4. Build the ESP-IDF project normally with `idf.py build`

### Integration with ESP-IDF

The generated UI code integrates with your firmware through:

```c
#include "ui.h"

// Initialize UI (call after display initialization)
ui_init();

// Update UI in main loop
ui_tick();
```

**Implementing Native Variables** (in `main.c` or other files):

```c
// Getter function for a string variable
const char* get_var_my_variable(void) {
    return "Hello World";
}

// Setter function for an integer variable
void set_var_my_number(int32_t value) {
    // Handle the value change
}
```

**Implementing Native Actions** (in `main.c` or other files):

```c
// Action handler
void action_my_button_clicked(lv_event_t * e) {
    // Handle button click
    ESP_LOGI("UI", "Button clicked!");
}
```

## Components

### WiFi Module (`wifi_module`)

Provides WiFi connectivity and mDNS service discovery for network access.

**Features**:
- WiFi station mode with automatic reconnection (up to 5 retries)
- WPA2-PSK authentication with PMF support
- mDNS hostname generation using device MAC address
- HTTP service advertisement via mDNS

**API Usage**:

```c
#include "wifi_module.h"

// Initialize WiFi driver (call once at startup)
esp_err_t ret = wifi_module_init();

// Connect to a WiFi network
ret = wifi_module_connect("YourSSID", "YourPassword");
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Connected to WiFi");
}

// Start mDNS service for network discovery
ret = wifi_module_start_mdns("my-device");
// Device will be accessible at: http://my-device-a1b2c3.local
```

**Key Functions**:
- `wifi_module_init()` - Initialize WiFi driver and TCP/IP stack
- `wifi_module_connect(ssid, password)` - Connect to WiFi network (blocking)
- `wifi_module_start_mdns(device_name)` - Start mDNS with unique hostname

### BSP Extra (`bsp_extra`)

Audio codec control and MP3 playback functionality using the ES8311 codec.

**Features**:
- ES8311 audio codec initialization and control
- I2S audio interface (playback and recording)
- MP3 file playback with libhelix decoder
- Volume control and mute functionality
- File iterator for browsing audio files
- Audio player task management

**API Usage**:

```c
#include "bsp_board_extra.h"

// Initialize codec
esp_err_t ret = bsp_extra_codec_init();

// Initialize audio player
ret = bsp_extra_player_init();

// Set volume (0-100)
int volume_set;
bsp_extra_codec_volume_set(60, &volume_set);

// Play an MP3 file from SD card
ret = bsp_extra_player_play_file("/sdcard/music/song.mp3");

// Create file iterator for a directory
file_iterator_instance_t *iterator;
bsp_extra_file_instance_init("/sdcard/music", &iterator);

// Play file by index
bsp_extra_player_play_index(iterator, 0);
```

**Key Functions**:
- `bsp_extra_codec_init()` - Initialize ES8311 codec
- `bsp_extra_player_init()` - Initialize audio player task
- `bsp_extra_codec_volume_set(volume, *volume_set)` - Set playback volume
- `bsp_extra_codec_mute_set(enable)` - Mute/unmute audio
- `bsp_extra_player_play_file(path)` - Play audio file
- `bsp_extra_file_instance_init(path, **instance)` - Create file iterator

### SD Card (`sd_card`)

High-speed SD card driver with comprehensive file operations.

**Features**:
- 40MHz SDMMC interface for fast data transfer
- Thread-safe operations with mutex protection
- FAT filesystem support
- File read/write/append/delete operations
- Directory management
- Capacity and free space reporting

**Hardware Pins** (ESP32-P4-WIFI6-Touch-LCD-7B):
- CLK: GPIO 43
- CMD: GPIO 44
- D0: GPIO 39
- D1: GPIO 40
- D2: GPIO 41
- D3: GPIO 42

**API Usage**:

```c
#include "sd_card.h"

// Initialize SD card
esp_err_t ret = sd_card_init();
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "SD card not available");
}

// Get capacity information
float total_gb = sd_card_get_total_capacity_gb();
float free_gb = sd_card_get_free_space_gb();

// Write to file
const char *data = "Hello, SD Card!";
sd_card_write_file("/sdcard/test.txt", (uint8_t *)data, strlen(data));

// Read from file
uint8_t buffer[256];
size_t bytes_read;
sd_card_read_file("/sdcard/test.txt", buffer, sizeof(buffer), &bytes_read);

// Create directory
sd_card_create_dir("/sdcard/screenshots");

// Check if file exists
if (sd_card_file_exists("/sdcard/test.txt")) {
    ESP_LOGI(TAG, "File exists");
}
```

**Key Functions**:
- `sd_card_init()` - Initialize SD card at 40MHz
- `sd_card_write_file(path, data, size)` - Write file
- `sd_card_read_file(path, buffer, size, *bytes_read)` - Read file
- `sd_card_create_dir(path)` - Create directory
- `sd_card_get_total_capacity_gb()` - Get total capacity
- `sd_card_get_free_space_gb()` - Get free space

For detailed API documentation, see `components/sd_card/README.md`.

### Screenshot (`screenshot`)

Automatic screenshot capture component for debugging and documentation.

**Features**:
- Automatic capture on screen transitions and tab switches
- BMP format (24-bit RGB, uncompressed)
- Sequential numbering (screenshot_001.bmp, screenshot_002.bmp, etc.)
- Saves to `/sdcard/screenshots/` directory
- Persistent counter across reboots
- Manual capture support

**API Usage**:

```c
#include "screenshot.h"

// Initialize after SD card is mounted
esp_err_t ret = screenshot_init();

// Manual screenshot capture
ret = screenshot_capture_and_save();

// Get next screenshot number
uint32_t next_num = screenshot_get_next_number();
```

**Automatic Capture**:
Screenshots are automatically captured when:
1. User switches between tabs in the main screen
2. Navigating between different screens (Splash, Main, Menu, etc.)

**File Details**:
- Format: BMP (24-bit RGB)
- Size: ~1.8MB per screenshot (1024x600)
- Location: `/sdcard/screenshots/`
- Naming: `screenshot_001.bmp`, `screenshot_002.bmp`, etc.

For detailed documentation, see `components/screenshot/README.md`.

### RTC (`rtc`)

Real-time clock with battery backup for persistent timekeeping.

**Features**:
- Hardware RTC with CR1220 battery backup
- Time persists across power cycles
- Automatic initialization from compile time on first boot
- Timezone support with DST (default: CET)
- Background task updates UI variables every second
- Thread-safe operations

**API Usage**:

```c
#include "rtc_component.h"

// Initialize RTC (call early in app_main)
esp_err_t ret = rtc_init();

// Register callback for UI updates
void rtc_ui_update(const char *time_str, const char *date_str) {
    // Update your UI variables here
    ESP_LOGI(TAG, "Time: %s, Date: %s", time_str, date_str);
}
rtc_register_ui_callback(rtc_ui_update);

// Get current time
time_t timestamp = rtc_get_timestamp();

// Get formatted strings
char time_str[16];
char date_str[32];
rtc_get_time_string(time_str, sizeof(time_str));  // "HH:MM:SS"
rtc_get_date_string(date_str, sizeof(date_str));  // "Monday, 2026-03-19"

// Set time manually
time_t new_time = /* your timestamp */;
rtc_set_time(new_time);

// Change timezone
rtc_set_timezone("EST5EDT,M3.2.0,M11.1.0");  // Eastern Time
```

**Battery Backup**:
- Install CR1220 rechargeable battery in RTC holder
- Board automatically charges battery when powered
- Time persists across power cycles
- First boot sets time from compile time
- Subsequent boots restore time from battery backup

For detailed documentation, see `components/rtc/README.md`.

### UI Component (`ui`)

EEZ Studio generated LVGL v9 user interface code.

**Generated Files**:
- `screens.c/h` - Screen layouts and widget definitions
- `vars.h` - Variable declarations (native and flow)
- `actions.h` - Action/event handler declarations
- `eez-flow.cpp/h` - Flow engine for visual programming
- `ui.c/h` - UI initialization and tick functions

**Integration**:

```c
#include "ui.h"

void app_main(void) {
    // Initialize display first
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    
    // Initialize UI
    bsp_display_lock(0);
    ui_init();
    bsp_display_unlock();
    
    // Main loop
    while (1) {
        bsp_display_lock(0);
        ui_tick();  // Update UI and flow engine
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**Display Configuration**:
- Resolution: 1024x600
- Color Format: BGR
- Rotation: 180 degrees
- Buffer: PSRAM-based, double buffering
- Refresh Rate: 60Hz (16ms period)
- LVGL Version: 9.2.2

## Build Configuration

### Default Configuration

The project includes `sdkconfig.defaults` with optimized settings:

**Performance**:
- `CONFIG_COMPILER_OPTIMIZATION_PERF=y` - Optimize for performance
- `CONFIG_SPIRAM_SPEED_200M=y` - 200MHz PSRAM speed
- `CONFIG_FREERTOS_HZ=1000` - 1ms tick rate for responsive UI

**Display & LVGL**:
- `CONFIG_LV_DEF_REFR_PERIOD=16` - 50Hz refresh rate
- `CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=1` - Single draw unit for better performance
- `CONFIG_LV_CACHE_DEF_SIZE=4194304` - 4MB image cache
- `CONFIG_LVGL_PORT_ENABLE_PPA=y` - Hardware acceleration

**Storage**:
- `CONFIG_FATFS_LFN_HEAP=y` - Long filename support
- `CONFIG_SPIFFS_MAX_PARTITIONS=3` - Multiple SPIFFS partitions

**RTC**:
- `CONFIG_RTC_CLK_SRC_EXT_CRYS=y` - External crystal for RTC
- `CONFIG_ESP_VBAT_USE_RECHARGEABLE_BATTERY=y` - Battery backup support

### Custom Configuration

Modify configuration using menuconfig:

```bash
idf.py menuconfig
```

**Common Options**:
- **Component config → LVGL configuration** - LVGL settings
- **Component config → BSP Board Support Package** - Display settings
- **Component config → ESP32P4-Specific** - Chip-specific options

### Partition Table

Custom partition table in `partitions.csv`:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 3M,
storage,  data, spiffs,  ,        1M,
```

## Advanced Usage

### WiFi Configuration

Configure WiFi credentials in your code:

```c
wifi_module_connect("YourSSID", "YourPassword");
```

For production, consider storing credentials in NVS (Non-Volatile Storage) for persistence.

### Audio File Preparation

1. Convert audio files to MP3 format (supported bitrates: 32-320 kbps)
2. Copy MP3 files to SD card: `/sdcard/music/`
3. Use file iterator to browse and play files

### Screenshot Usage

Screenshots are saved to `/sdcard/screenshots/` in BMP format. To view:
1. Remove SD card from device
2. Insert into computer
3. Open screenshots folder
4. View BMP files with any image viewer

### RTC Time Synchronization

For accurate time, consider adding SNTP (network time protocol):

```c
// After WiFi connection
esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
esp_sntp_setservername(0, "pool.ntp.org");
esp_sntp_init();
```

## Pin Mappings

### SD Card (SDMMC)
- CLK: GPIO 43
- CMD: GPIO 44
- D0: GPIO 39
- D1: GPIO 40
- D2: GPIO 41
- D3: GPIO 42

### I2S Audio (ES8311)
Configured in BSP component (refer to board schematic)

### Display
Configured in BSP component (RGB interface)

### Touch Controller
Configured in BSP component (I2C interface)

## Troubleshooting

### Build Issues

**Error: "esp32p4 target not found"**
- Ensure ESP-IDF v5.4.0 or later is installed
- Run `idf.py set-target esp32p4`

**Error: "LVGL configuration mismatch"**
- Clean build: `idf.py fullclean`
- Rebuild: `idf.py build`

### Runtime Issues

**Display not working**
- Check display cable connections
- Verify backlight is enabled in code
- Check power supply (5V, sufficient current)

**SD card not detected**
- Ensure SD card is formatted as FAT32
- Check SD card is properly inserted
- Verify SD card is not write-protected
- Try different SD card (some cards are incompatible)

**WiFi connection fails**
- Verify SSID and password are correct
- Check WiFi network is 2.4GHz (5GHz not supported by ESP32-C6 module)
- Ensure network allows new device connections
- Check signal strength

**Audio not playing**
- Verify MP3 file is valid and supported bitrate
- Check volume is not muted or set to 0
- Ensure SD card is mounted and file path is correct
- Verify audio codec is initialized

**RTC time resets on reboot**
- Install CR1220 battery in RTC holder
- Allow battery to charge (board charges when powered)
- Battery may be dead - replace if old

**Screenshots not saving**
- Ensure SD card is mounted successfully
- Check SD card has sufficient free space (~2MB per screenshot)
- Verify `/sdcard/screenshots/` directory exists (auto-created)

### Memory Issues

**Heap allocation failures**
- Reduce `CONFIG_LV_CACHE_DEF_SIZE` in menuconfig
- Disable screenshot component if not needed
- Reduce LVGL draw buffer size

**Stack overflow**
- Increase task stack sizes in menuconfig
- Check for recursive function calls
- Reduce local variable sizes

## Performance Optimization

### Display Performance
- Enable PPA acceleration (already enabled in defaults)
- Use PSRAM for frame buffers (already configured)
- Minimize full-screen redraws
- Use LVGL's built-in caching

### Memory Usage
- Store large assets (images, fonts) in SPIFFS partition
- Use compressed fonts when possible
- Free unused resources promptly

### Power Consumption
- Reduce display brightness when possible
- Implement sleep modes for idle periods
- Disable unused peripherals

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly on hardware
5. Submit a pull request

## License

This project is licensed under the Apache License 2.0. See individual component licenses for details.

**Third-party components**:
- LVGL: MIT License
- ESP-IDF: Apache License 2.0
- EEZ Studio: MIT License

## Resources

- **ESP-IDF Documentation**: https://docs.espressif.com/projects/esp-idf/
- **LVGL Documentation**: https://docs.lvgl.io/
- **EEZ Studio**: https://github.com/eez-open/studio
- **ESP32-P4 Datasheet**: https://www.espressif.com/en/products/socs/esp32-p4

## Support

For issues and questions:
- Check existing component READMEs in `components/` directories
- Review ESP-IDF documentation
- Open an issue on the project repository

---

**Happy Building!** 🚀