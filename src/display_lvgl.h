/**
 * @file display_lvgl.h
 * @brief LVGL Display Driver for ESP32 Touch Display Panels
 * 
 * Supported boards:
 * - ESP32-4848S040 (JCZN 4.0" 480x480 ST7701 RGB Display with GT911 Touch)
 * - ESP32-S3-Touch-AMOLED-1.8 (Waveshare 1.8" 368x448 SH8601 QSPI with FT3168 Touch)
 * 
 * Uses Arduino_GFX library for display, Wire for touch
 * Board-specific configuration is in board_config.h
 */

#ifndef DISPLAY_LVGL_H
#define DISPLAY_LVGL_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <Preferences.h>
#include "board_config.h"
#include "settings_ui.h"
#include "wifi_ui.h"

// Forward declarations for screensaver
int getScreensaverTimeout();
void setScreensaverTimeout(int timeout);
void saveScreensaverTimeout();
void loadScreensaverSettings();

#include "menu_ui.h"
#include "screensaver_ui.h"

// Forward declarations from main
extern bool feedingModeActive;
extern String redsea_AQUARIUM_NAME;
extern String TUNZE_DEVICE_NAME;
extern bool ENABLE_redsea;
extern bool ENABLE_TUNZE;
extern void startFeedingMode();
extern void stopFeedingMode();

// ============================================================
// Display Configuration (Board-specific)
// ============================================================
#ifdef BOARD_ESP32_4848S040
// ESP32-4848S040: 480x480 ST7701 RGB Display with GT911 Touch

// 3-Wire SPI Bus for ST7701 initialization commands
Arduino_DataBus *bus = new Arduino_ESP32SPI(
    GFX_NOT_DEFINED /* DC */, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED /* MISO */);

// RGB Panel Bus - exact pin configuration for ESP32-4848S040
// Timing optimiert gegen horizontales Bildspringen (mehr back_porch für Stabilität)
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    TFT_DE, TFT_VSYNC, TFT_HSYNC, TFT_PCLK,
    11 /* R0 */, 12 /* R1 */, 13 /* R2 */, 14 /* R3 */, 0 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    4 /* B0 */, 5 /* B1 */, 6 /* B2 */, 7 /* B3 */, 15 /* B4 */,
    1 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    1 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
    1 /* pclk_active_neg - WICHTIG: reduziert Glitches */,
    14000000 /* prefer_speed - 14MHz für mehr Stabilität */);

// RGB Display with ST7701 type 9 init (specifically for GUITION ESP32-4848S040)
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    DISPLAY_WIDTH, DISPLAY_HEIGHT, rgbpanel, DISPLAY_ROTATION, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type9_init_operations, sizeof(st7701_type9_init_operations));

#endif // BOARD_ESP32_4848S040

// ============================================================
// Waveshare ESP32-S3-Touch-AMOLED-1.8 Display Configuration
// ============================================================
#ifdef BOARD_WAVESHARE_AMOLED_1_8

// QSPI Bus for SH8601 AMOLED
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_SDIO0, TFT_SDIO1, TFT_SDIO2, TFT_SDIO3);

// SH8601 AMOLED Display - use specific type for brightness control
Arduino_SH8601 *gfx = new Arduino_SH8601(bus, -1 /* RST */, 
    DISPLAY_ROTATION, DISPLAY_WIDTH, DISPLAY_HEIGHT);

#endif // BOARD_WAVESHARE_AMOLED_1_8

// ============================================================
// Touch Configuration (Board-specific)
// ============================================================
#include <Wire.h>

static bool touch_has_signal = false;
static int16_t touch_x = 0;
static int16_t touch_y = 0;

// ============================================================
// GT911 Touch (ESP32-4848S040)
// ============================================================
#ifdef BOARD_ESP32_4848S040

void touch_init() {
  Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
  Wire.setClock(400000);
}

bool touch_touched() {
  Wire.beginTransmission(TOUCH_GT911_ADDR);
  Wire.write(0x81);
  Wire.write(0x4E);
  Wire.endTransmission();
  
  Wire.requestFrom(TOUCH_GT911_ADDR, 1);
  if (Wire.available()) {
    uint8_t status = Wire.read();
    if (status & 0x80) {
      // Clear status
      Wire.beginTransmission(TOUCH_GT911_ADDR);
      Wire.write(0x81);
      Wire.write(0x4E);
      Wire.write(0x00);
      Wire.endTransmission();
      
      if ((status & 0x0F) > 0) {
        // Read touch point
        Wire.beginTransmission(TOUCH_GT911_ADDR);
        Wire.write(0x81);
        Wire.write(0x50);
        Wire.endTransmission();
        
        Wire.requestFrom(TOUCH_GT911_ADDR, 4);
        if (Wire.available() >= 4) {
          touch_x = Wire.read();
          touch_x |= Wire.read() << 8;
          touch_y = Wire.read();
          touch_y |= Wire.read() << 8;
          return true;
        }
      }
    }
  }
  return false;
}

#endif // BOARD_ESP32_4848S040 touch

// ============================================================
// FT3168 Touch (Waveshare AMOLED 1.8)
// ============================================================
#ifdef BOARD_WAVESHARE_AMOLED_1_8

// TCA9554 I/O Expander registers
#define TCA9554_INPUT_REG    0x00
#define TCA9554_OUTPUT_REG   0x01
#define TCA9554_POLARITY_REG 0x02
#define TCA9554_CONFIG_REG   0x03

// Simple TCA9554 helper functions
static uint8_t tca9554_output_state = 0xFF;

void tca9554_write_reg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(IO_EXPANDER_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void tca9554_set_pin_mode(uint8_t pin, uint8_t mode) {
  Wire.beginTransmission(IO_EXPANDER_ADDR);
  Wire.write(TCA9554_CONFIG_REG);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)IO_EXPANDER_ADDR, (uint8_t)1);
  uint8_t config = Wire.read();
  
  if (mode == OUTPUT) {
    config &= ~(1 << pin);  // Clear bit = output
  } else {
    config |= (1 << pin);   // Set bit = input
  }
  tca9554_write_reg(TCA9554_CONFIG_REG, config);
}

void tca9554_digital_write(uint8_t pin, uint8_t value) {
  if (value) {
    tca9554_output_state |= (1 << pin);
  } else {
    tca9554_output_state &= ~(1 << pin);
  }
  tca9554_write_reg(TCA9554_OUTPUT_REG, tca9554_output_state);
}

// Touch availability flag - must be before touch_init()
static bool touch_available = false;
static volatile bool touch_interrupt_flag = false;
static int touch_error_count = 0;  // Track consecutive errors

// Touch interrupt handler
void IRAM_ATTR touch_isr() {
  touch_interrupt_flag = true;
}

void touch_init() {
  // Waveshare AMOLED 1.8: Initialize I2C for touch
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);  // 100kHz I2C - slower for reliability
  delay(200);  // Wait for I2C to stabilize
  
  Serial.println("\n===== I2C SCAN START =====");
  int devicesFound = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  ✓ I2C device found at 0x%02X\n", addr);
      devicesFound++;
    }
  }
  Serial.printf("I2C scan complete: %d device(s) found\n", devicesFound);
  Serial.println("===== I2C SCAN END =======\n");
  
  // Try multiple times to find TCA9554 I/O Expander
  bool tca_found = false;
  for (int attempt = 0; attempt < 5; attempt++) {
    Wire.beginTransmission(IO_EXPANDER_ADDR);
    if (Wire.endTransmission() == 0) {
      tca_found = true;
      break;
    }
    Serial.printf("TCA9554 not found, attempt %d/5\n", attempt + 1);
    delay(100);
  }
  
  if (tca_found) {
    Serial.println("TCA9554 I/O Expander found");
    
    // Initialize TCA9554 - all outputs high initially
    tca9554_output_state = 0xFF;
    tca9554_write_reg(TCA9554_OUTPUT_REG, tca9554_output_state);
    delay(10);
    
    // Set EXIO_TOUCH_RST as output
    tca9554_set_pin_mode(EXIO_TOUCH_RST, OUTPUT);
    delay(10);
    
    // Reset touch controller via EXIO1 - proper reset sequence
    tca9554_digital_write(EXIO_TOUCH_RST, LOW);
    delay(100);  // Hold reset longer for reliable init
    tca9554_digital_write(EXIO_TOUCH_RST, HIGH);
    delay(500);  // Wait LONGER for touch controller to fully initialize
    
    // Try multiple times to find touch controller
    bool touch_found = false;
    for (int attempt = 0; attempt < 5; attempt++) {
      Wire.beginTransmission(TOUCH_I2C_ADDR);
      if (Wire.endTransmission() == 0) {
        touch_found = true;
        break;
      }
      Serial.printf("FT3168 not found, attempt %d/5\n", attempt + 1);
      delay(100);
    }
    
    if (touch_found) {
      Serial.println("FT3168 Touch controller found");
      
      // Read chip ID for debugging
      Wire.beginTransmission(TOUCH_I2C_ADDR);
      Wire.write(0xA3);  // Chip ID register (correct address)
      Wire.endTransmission(false);
      uint8_t bytesRead = Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)1);
      if (bytesRead > 0) {
        uint8_t chipId = Wire.read();
        Serial.printf("FT3168 Chip ID: 0x%02X\n", chipId);
      }
      
      // Configure FT3168 properly
      delay(50);
      
      // 1. Set to normal operating mode
      Wire.beginTransmission(TOUCH_I2C_ADDR);
      Wire.write(0x00);  // Device mode register
      Wire.write(0x00);  // Normal operating mode
      if (Wire.endTransmission() != 0) {
        Serial.println("Failed to set device mode");
      }
      delay(10);
      
      // 2. Set touch threshold (lower = more sensitive)
      // 0x20=very sensitive, 0x40=normal, 0x60=less sensitive
      Wire.beginTransmission(TOUCH_I2C_ADDR);
      Wire.write(0x80);  // Threshold register
      Wire.write(0x20);  // Threshold value (32) - very sensitive
      Wire.endTransmission();
      delay(10);
      
      // 3. Set interrupt mode to polling (disable interrupt)
      Wire.beginTransmission(TOUCH_I2C_ADDR);
      Wire.write(0xA4);  // Interrupt mode register  
      Wire.write(0x00);  // Polling mode (we poll, not use interrupts)
      Wire.endTransmission();
      delay(10);
      
      // Verify touch is responding
      Wire.beginTransmission(TOUCH_I2C_ADDR);
      Wire.write(0x00);  // Read mode register
      Wire.endTransmission(false);
      if (Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)1) > 0) {
        uint8_t mode = Wire.read();
        Serial.printf("FT3168 Mode: 0x%02X\n", mode);
        touch_available = true;
      } else {
        Serial.println("FT3168 not responding after init");
        touch_available = false;
      }
      
    } else {
      Serial.println("FT3168 Touch controller NOT found after 5 attempts");
      touch_available = false;
    }
  } else {
    Serial.println("TCA9554 I/O Expander NOT found - touch disabled");
    touch_available = false;
  }
  
  Serial.printf("Touch initialization complete - available: %s\n", touch_available ? "YES" : "NO");
}

bool touch_touched() {
  // Skip if touch not available
  if (!touch_available) {
    return false;
  }
  
  static unsigned long last_successful_read = millis();
  static int consecutive_errors = 0;
  static unsigned long last_debug = 0;
  
  // Try to read touch data - up to 2 attempts
  for (int attempt = 0; attempt < 2; attempt++) {
    // Write register address
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0x02);  // TD_STATUS register
    int write_err = Wire.endTransmission();
    
    if (write_err != 0) {
      // Debug: Log write errors periodically
      if (millis() - last_debug > 2000) {
        Serial.printf("Touch: Write err=%d, attempt=%d\\n", write_err, attempt);
        last_debug = millis();
      }
      // First attempt failed - FT3168 may be sleeping
      // Wait a bit and retry (the beginTransmission itself helps wake it)
      delay(2);
      continue;  // Try again
    }
    
    // Small delay for FT3168 to prepare data
    delayMicroseconds(100);
    
    // Read data
    uint8_t bytesRead = Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)5);
    if (bytesRead < 5) {
      if (millis() - last_debug > 2000) {
        Serial.printf("Touch: Read only %d bytes\\n", bytesRead);
        last_debug = millis();
      }
      delay(2);
      continue;  // Try again
    }
    
    // Mark successful read
    last_successful_read = millis();
    consecutive_errors = 0;
    
    uint8_t td_status = Wire.read();
    uint8_t touches = td_status & 0x0F;
    
    if (touches == 0 || touches > 2) {
      // No touch - clear remaining bytes
      while (Wire.available()) Wire.read();
      return false;
    }
    
    // Read coordinates
    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();
    
    touch_x = ((xh & 0x0F) << 8) | xl;
    touch_y = ((yh & 0x0F) << 8) | yl;
    
    return true;
  }
  
  // Both attempts failed
  consecutive_errors++;
  
  // Only disable after many consecutive errors (not intermittent ones)
  if (consecutive_errors > 200) {
    Serial.println("⚠ Touch disabled - too many consecutive errors");
    touch_available = false;
  }
  
  return false;
}

#endif // BOARD_WAVESHARE_AMOLED_1_8 touch

// ============================================================
// LVGL Variables
// ============================================================
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

// UI Elements
static lv_obj_t *screen_main;
static lv_obj_t *status_card;
static lv_obj_t *status_icon;
static lv_obj_t *status_label;
static lv_obj_t *main_button;
static lv_obj_t *btn_label;
static lv_obj_t *redsea_card;
static lv_obj_t *tunze_card;
static lv_obj_t *header_label;
static lv_obj_t *wifi_settings_btn;

// State tracking
static bool last_feeding_state = false;

// Screensaver timeout
static unsigned long last_touch_time = 0;
static int screensaver_timeout = 60;  // Default: 60 seconds (0 = disabled)

// ============================================================
// Screensaver Settings
// ============================================================
void setScreensaverTimeout(int seconds) {
  screensaver_timeout = seconds;
}

void saveScreensaverTimeout() {
  Preferences prefs;
  prefs.begin("feeding-break", false);
  prefs.putInt("scr_timeout", screensaver_timeout);
  prefs.end();
}

int getScreensaverTimeout() {
  return screensaver_timeout;
}

void loadScreensaverSettings() {
  Preferences prefs;
  prefs.begin("feeding-break", true);
  screensaver_timeout = prefs.getInt("scr_timeout", 60);
  prefs.end();
}

// ============================================================
// Color Theme (Dark Mode)
// ============================================================
#define UI_COLOR_BG          lv_color_hex(0x1a1a2e)  // Dark blue background
#define UI_COLOR_CARD        lv_color_hex(0x16213e)  // Card background
#define UI_COLOR_HEADER      lv_color_hex(0x0f3460)  // Header
#define UI_COLOR_ACTIVE      lv_color_hex(0x00ff87)  // Green for active
#define UI_COLOR_INACTIVE    lv_color_hex(0xff6b6b)  // Red for inactive
#define UI_COLOR_TEXT        lv_color_hex(0xffffff)  // White text
#define UI_COLOR_TEXT_DIM    lv_color_hex(0xb8c4d8)  // Dimmed text (brighter)
#define UI_COLOR_REDSEA      lv_color_hex(0xe94560)  // Red Sea accent
#define UI_COLOR_TUNZE       lv_color_hex(0x00d9ff)  // Tunze cyan
#define UI_COLOR_BUTTON      lv_color_hex(0x533483)  // Button purple

// ============================================================
// LVGL Display Flush Callback (from official demo)
// ============================================================
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

// ============================================================
// LVGL Touch Read Callback
// ============================================================
static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  // Rate limit touch polling - 30ms for smooth scrolling (33Hz)
  static unsigned long lastTouchPoll = 0;
  static bool lastTouchState = false;
  static int16_t lastX = 0, lastY = 0;
  
  unsigned long now = millis();
  if (now - lastTouchPoll < 30) {
    // Return last known state for smooth scrolling
    data->state = lastTouchState ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    data->point.x = lastX;
    data->point.y = lastY;
    return;
  }
  lastTouchPoll = now;
  
  if (touch_touched()) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touch_x;
    data->point.y = touch_y;
    lastTouchState = true;
    lastX = touch_x;
    lastY = touch_y;
    
    // Reset screensaver timer on touch
    last_touch_time = millis();
    
    // If screensaver is active, hide it
    if (isScreensaverActive()) {
      hideScreensaver();
      data->state = LV_INDEV_STATE_REL;  // Don't process the touch that woke up
      lastTouchState = false;
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
    lastTouchState = false;
  }
}

// ============================================================
// Button Event Handler
// ============================================================
static void main_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    Serial.println("Main button clicked!");
    if (feedingModeActive) {
      stopFeedingMode();
    } else {
      startFeedingMode();
    }
  }
}

// ============================================================
// Settings Button Event Handler (replaces WiFi button)
// ============================================================
static void settings_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    Serial.println("Settings button clicked!");
    showSettingsScreen();
  }
}

// ============================================================
// Styles
// ============================================================
static lv_style_t style_card;
static lv_style_t style_btn_active;
static lv_style_t style_btn_inactive;
static lv_style_t style_status_active;
static lv_style_t style_status_inactive;

static void create_styles() {
  // Card style
  lv_style_init(&style_card);
  lv_style_set_bg_color(&style_card, UI_COLOR_CARD);
  lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
  lv_style_set_radius(&style_card, 20);
  lv_style_set_shadow_width(&style_card, 20);
  lv_style_set_shadow_color(&style_card, lv_color_hex(0x000000));
  lv_style_set_shadow_opa(&style_card, LV_OPA_30);
  lv_style_set_pad_all(&style_card, 20);
  
  // Active button style (red - to stop)
  lv_style_init(&style_btn_active);
  lv_style_set_bg_color(&style_btn_active, UI_COLOR_INACTIVE);
  lv_style_set_bg_grad_color(&style_btn_active, lv_color_hex(0xd63031));
  lv_style_set_bg_grad_dir(&style_btn_active, LV_GRAD_DIR_VER);
  lv_style_set_radius(&style_btn_active, 25);
  lv_style_set_shadow_width(&style_btn_active, 15);
  lv_style_set_shadow_color(&style_btn_active, UI_COLOR_INACTIVE);
  lv_style_set_shadow_opa(&style_btn_active, LV_OPA_50);
  lv_style_set_text_color(&style_btn_active, UI_COLOR_TEXT);
  
  // Inactive button style (green - to start)
  lv_style_init(&style_btn_inactive);
  lv_style_set_bg_color(&style_btn_inactive, UI_COLOR_ACTIVE);
  lv_style_set_bg_grad_color(&style_btn_inactive, lv_color_hex(0x00b894));
  lv_style_set_bg_grad_dir(&style_btn_inactive, LV_GRAD_DIR_VER);
  lv_style_set_radius(&style_btn_inactive, 25);
  lv_style_set_shadow_width(&style_btn_inactive, 15);
  lv_style_set_shadow_color(&style_btn_inactive, UI_COLOR_ACTIVE);
  lv_style_set_shadow_opa(&style_btn_inactive, LV_OPA_50);
  lv_style_set_text_color(&style_btn_inactive, lv_color_hex(0x1a1a2e));
  
  // Status active style
  lv_style_init(&style_status_active);
  lv_style_set_bg_color(&style_status_active, UI_COLOR_ACTIVE);
  lv_style_set_bg_opa(&style_status_active, LV_OPA_COVER);
  lv_style_set_radius(&style_status_active, LV_RADIUS_CIRCLE);
  
  // Status inactive style
  lv_style_init(&style_status_inactive);
  lv_style_set_bg_color(&style_status_inactive, UI_COLOR_INACTIVE);
  lv_style_set_bg_opa(&style_status_inactive, LV_OPA_COVER);
  lv_style_set_radius(&style_status_inactive, LV_RADIUS_CIRCLE);
}

// ============================================================
// Create UI Elements
// ============================================================
static void create_ui() {
  // Use new menu UI as main screen
  createMenuScreen();
  screen_main = getMenuScreen();
}

// ============================================================
// Update UI State
// ============================================================
void updateLvglUI() {
  // Update the menu UI
  updateMenuUI();
}

// ============================================================
// Setup Display with LVGL
// ============================================================
void setupDisplay() {
  Serial.println("Initialisiere Display mit Arduino_GFX...");
  
  // Init Arduino_GFX
  if (!gfx->begin()) {
    Serial.println("FEHLER: gfx->begin() fehlgeschlagen!");
    return;
  }
  Serial.println("Arduino_GFX initialisiert");
  
  // Setup backlight / brightness
  #if defined(TFT_BL) && TFT_BL >= 0
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  #endif
  
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  // Waveshare AMOLED: Set brightness via display command (0-255)
  // Must be done AFTER gfx->begin()
  Serial.println("Setze AMOLED Helligkeit...");
  gfx->setBrightness(255);  // Full brightness to start
  delay(50);
  #endif
  
  // Test display with white screen first, then black
  Serial.println("Display Test: Weiss...");
  gfx->fillScreen(0xFFFF); // WHITE
  delay(200);
  Serial.println("Display Test: Schwarz...");
  gfx->fillScreen(0x0000); // BLACK
  delay(100);
  
  // Init Touch
  touch_init();
  Serial.println("Touch initialisiert");
  
  // Init LVGL
  lv_init();
  Serial.println("LVGL initialisiert");
  
  // Allocate display buffer in INTERNAL RAM (not PSRAM!) to avoid cache issues with WiFi
  // PSRAM shares the bus with Flash and can cause "Cache disabled" crashes during WiFi operations
  uint32_t bufSize = DISPLAY_WIDTH * 20;  // Reduced size to fit in internal RAM
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  
  if (!disp_draw_buf) {
    Serial.println("WARNUNG: Konnte Display-Buffer nicht in internem RAM allozieren! Versuche PSRAM...");
    bufSize = DISPLAY_WIDTH * 40;  // Larger buffer for PSRAM
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  
  if (!disp_draw_buf) {
    Serial.println("FEHLER: Konnte Display-Buffer nicht allozieren!");
    return;
  }
  
  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, bufSize);
  Serial.println("Display-Buffer alloziert");
  
  // Setup display driver
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = DISPLAY_WIDTH;
  disp_drv.ver_res = DISPLAY_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  Serial.println("Display-Treiber registriert");
  
  // Setup touch input
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
  Serial.println("Touch-Treiber registriert");
  
  // Load screensaver settings
  loadScreensaverSettings();
  
  // Create styles and UI
  create_styles();
  create_ui();
  
  // Initialize screensaver
  createScreensaver();
  
  Serial.println("LVGL UI erstellt - Display bereit!");
}

// ============================================================
// Show WiFi Setup if in config mode (call after WiFi setup)
// ============================================================
void showWiFiSetupIfNeeded() {
  extern bool wifiConfigMode;
  if (wifiConfigMode) {
    Serial.println("WiFi nicht konfiguriert - zeige Setup Screen");
    showWiFiScreen();
  }
}

// ============================================================
// Update Display (call in loop)
// ============================================================
void updateDisplay() {
  lv_timer_handler();
  updateLvglUI();
  updateWiFiUI();  // Update WiFi screen if active
  
  // Check screensaver timeout
  extern bool feedingModeActive;
  if (screensaver_timeout > 0 && !isScreensaverActive() && !feedingModeActive) {
    if (millis() - last_touch_time > screensaver_timeout * 1000) {
      showScreensaver();
    }
  }
}

// ============================================================
// Getter for main screen (used by wifi_ui.h)
// ============================================================
lv_obj_t* getMainScreen() {
  return screen_main;
}

#endif // DISPLAY_LVGL_H
