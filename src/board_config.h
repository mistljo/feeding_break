/**
 * @file board_config.h
 * @brief Hardware Configuration for different ESP32 boards
 * 
 * To support a different board, create a new section with the appropriate
 * pin definitions and display configuration.
 * 
 * Currently supported boards:
 * - ESP32-4848S040 (JCZN 4.0" 480x480 Touch Panel with RGB Display)
 * - ESP32-S3-Touch-AMOLED-1.8 (Waveshare 1.8" 368x448 AMOLED with QSPI)
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ============================================================
// Board Selection
// The board is selected via platformio.ini build_flags:
//   -DBOARD_ESP32_4848S040 or -DBOARD_WAVESHARE_AMOLED_1_8
// 
// For manual testing, you can uncomment ONE line below:
// ============================================================
// #define BOARD_ESP32_4848S040              // JCZN 4.0" 480x480 Touch Panel
// #define BOARD_WAVESHARE_AMOLED_1_8        // Waveshare 1.8" 368x448 AMOLED

// ============================================================
// ESP32-4848S040 Configuration
// JCZN 4.0" Touch Panel with ST7701 Display and GT911 Touch
// https://homeding.github.io/boards/esp32s3/panel-4848S040.htm
// ============================================================
#ifdef BOARD_ESP32_4848S040

// Board Info
#define BOARD_NAME              "ESP32-4848S040"
#define BOARD_DESCRIPTION       "JCZN 4.0\" 480x480 Touch Panel"

// Display Configuration
#define DISPLAY_WIDTH           480
#define DISPLAY_HEIGHT          480
#define DISPLAY_ROTATION        0
#define DISPLAY_COLOR_DEPTH     16

// ST7701 Display SPI (9-bit mode for commands)
#define TFT_CS                  39
#define TFT_SCK                 48
#define TFT_MOSI                47

// RGB Panel Pins (directly from ESP32-S3 to display)
#define TFT_DE                  18
#define TFT_VSYNC               17
#define TFT_HSYNC               16
#define TFT_PCLK                21

// RGB Data Pins - directly in bus initialization
// R: 4,5,6,7,15  G: 8,20,3,46,9,10  B: 11,12,13,14,0

// Display Backlight
#define TFT_BL                  38
#define TFT_BL_PWM_CHANNEL      0

// Touch Controller GT911 (I2C)
#define TOUCH_GT911_SCL         45
#define TOUCH_GT911_SDA         19
#define TOUCH_GT911_INT         -1    // Not connected
#define TOUCH_GT911_RST         -1    // Not connected
#define TOUCH_GT911_ADDR        0x5D

// Relay Pins (accent/active LOW - HIGH = OFF)
#define RELAY1_PIN              40
#define RELAY2_PIN              2
#define RELAY3_PIN              1
#define RELAY_ACTIVE_LOW        true  // true = active LOW (HIGH = off)

// User Button (hidden SW1 beside USB)
#define BUTTON_PIN              -1    // Not documented, disabled
#define FACTORY_RESET_PIN       -1    // Use touch UI instead

// Status LED
#define LED_PIN                 -1    // No separate LED on this board

// SD Card (SPI)
#define SD_CS                   42
#define SD_SCK                  48    // Shared with display
#define SD_MOSI                 47    // Shared with display
#define SD_MISO                 41

// I2C Bus (for touch and expansion)
#define I2C_SDA                 19
#define I2C_SCL                 45

// Speaker (NS4168 - optional)
#define SPEAKER_PIN             -1    // Not yet documented

// Battery (IP5306 - optional)
#define BATTERY_PIN             -1    // Not yet documented

// PSRAM Configuration
#define HAS_PSRAM               true
#define PSRAM_SIZE_MB           8

// Flash Configuration
#define FLASH_SIZE_MB           16

#endif // BOARD_ESP32_4848S040

// ============================================================
// Waveshare ESP32-S3-Touch-AMOLED-1.8 Configuration
// 1.8" AMOLED with SH8601 Display (QSPI) and FT3168 Touch
// https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8
// ============================================================
#ifdef BOARD_WAVESHARE_AMOLED_1_8

// Board Info
#define BOARD_NAME              "ESP32-S3-Touch-AMOLED-1.8"
#define BOARD_DESCRIPTION       "Waveshare 1.8\" 368x448 AMOLED"

// Display Configuration
#define DISPLAY_WIDTH           368
#define DISPLAY_HEIGHT          448
#define DISPLAY_ROTATION        0
#define DISPLAY_COLOR_DEPTH     16

// Display Type
#define DISPLAY_TYPE_QSPI       1   // Uses QSPI interface (not RGB)
#define DISPLAY_CONTROLLER_SH8601   1

// SH8601 QSPI Display Pins (from Waveshare official pin_config.h)
#define TFT_CS                  12    // LCD_CS
#define TFT_SCK                 11    // LCD_SCLK
#define TFT_SDIO0               4     // LCD_SDIO0
#define TFT_SDIO1               5     // LCD_SDIO1
#define TFT_SDIO2               6     // LCD_SDIO2
#define TFT_SDIO3               7     // LCD_SDIO3
#define TFT_RST                 -1    // No reset pin, uses software reset

// Display Backlight (controlled via display command 0x51)
#define TFT_BL                  -1    // No GPIO backlight, use display command
#define TFT_BL_PWM_CHANNEL      -1

// Touch Controller FT3168 (I2C)
#define TOUCH_FT3168            1     // Use FT3168 instead of GT911
#define TOUCH_SDA               15    // IIC_SDA (from Waveshare pin_config.h)
#define TOUCH_SCL               14    // IIC_SCL (from Waveshare pin_config.h)
#define TOUCH_INT               21    // TP_INT (from Waveshare pin_config.h)
#define TOUCH_RST               -1    // Reset via I/O expander
#define TOUCH_I2C_ADDR          0x38

// TCA9554 I/O Expander (for touch reset, SD card CS, etc.)
#define IO_EXPANDER_ADDR        0x20
#define EXIO_TOUCH_RST          1     // EXIO1 = Touch reset
#define EXIO_SD_CS              7     // EXIO7 = SD card CS

// No Relay Pins on this board
#define RELAY1_PIN              -1
#define RELAY2_PIN              -1
#define RELAY3_PIN              -1
#define RELAY_ACTIVE_LOW        true

// Buttons - disabled for touch-only control
#define BUTTON_PIN              -1    // Disabled - use touch only
#define BUTTON_BOOT_PIN         0     // BOOT button (GPIO0) - available but not used
#define BUTTON_PWR_PIN          -1    // PWR via EXIO4
#define FACTORY_RESET_PIN       -1    // Disabled - use web interface instead

// Status LED
#define LED_PIN                 -1    // No separate LED

// SD Card (SDMMC)
#define SD_CS                   -1    // Via TCA9554 EXIO7
#define SD_MOSI                 1     // SDMMC_CMD
#define SD_MISO                 3     // SDMMC_DATA
#define SD_SCK                  2     // SDMMC_CLK

// I2C Bus (shared for touch, sensors, PMIC, RTC)
#define I2C_SDA                 15    // IIC_SDA
#define I2C_SCL                 14    // IIC_SCL

// Audio (ES8311 Codec)
#define AUDIO_I2S_BCLK          46
#define AUDIO_I2S_LRCK          45
#define AUDIO_I2S_DIN           42
#define AUDIO_I2S_DOUT          41
#define AUDIO_PA_EN             -1    // Via TCA9554 EXIO0

// Additional I2C Devices
#define HAS_AXP2101             true  // Power management IC
#define HAS_QMI8658             true  // 6-axis IMU
#define HAS_PCF85063            true  // RTC

// PSRAM Configuration
#define HAS_PSRAM               true
#define PSRAM_SIZE_MB           8

// Flash Configuration
#define FLASH_SIZE_MB           16

#endif // BOARD_WAVESHARE_AMOLED_1_8

// ============================================================
// Template for new boards - copy and modify
// ============================================================
/*
#ifdef BOARD_YOUR_BOARD_NAME

#define BOARD_NAME              "Your Board Name"
#define BOARD_DESCRIPTION       "Board description"

// Display Configuration
#define DISPLAY_WIDTH           320
#define DISPLAY_HEIGHT          240
#define DISPLAY_ROTATION        0
#define DISPLAY_COLOR_DEPTH     16

// Display SPI Pins
#define TFT_CS                  5
#define TFT_DC                  4
#define TFT_RST                 -1
#define TFT_MOSI                23
#define TFT_SCK                 18
#define TFT_BL                  -1

// Touch Pins (if applicable)
#define TOUCH_CS                -1
// or for I2C touch:
#define TOUCH_SDA               21
#define TOUCH_SCL               22

// Relay Pins
#define RELAY1_PIN              -1
#define RELAY2_PIN              -1
#define RELAY3_PIN              -1
#define RELAY_ACTIVE_LOW        true

// Button/LED
#define BUTTON_PIN              0
#define FACTORY_RESET_PIN       -1
#define LED_PIN                 2

#endif // BOARD_YOUR_BOARD_NAME
*/

// ============================================================
// Validation - ensure a board is selected
// ============================================================
#if !defined(BOARD_ESP32_4848S040) && !defined(BOARD_WAVESHARE_AMOLED_1_8)
  #error "No board selected! Please define a board in board_config.h"
#endif

#endif // BOARD_CONFIG_H
