/**
 * @file menu_ui.h
 * @brief Main Menu UI with Sidebar Navigation (like Web Interface)
 */

#ifndef MENU_UI_H
#define MENU_UI_H

#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <Preferences.h>
#include "board_config.h"

// Forward declarations
extern bool feedingModeActive;
extern bool ENABLE_redsea;
extern bool ENABLE_TUNZE;
extern void startFeedingMode();
extern void stopFeedingMode();
lv_obj_t* getMainScreen();

// Tasmota getters (defined in tasmota_api.h)
bool tasmotaIsEnabled();
int tasmotaGetPulseTime();

// ============================================================
// Menu Colors (match web interface dark theme)
// ============================================================
#define MENU_BG             lv_color_hex(0x1a1a2e)
#define MENU_HEADER         lv_color_hex(0x0f3460)
#define MENU_SIDEBAR        lv_color_hex(0x16213e)
#define MENU_CARD           lv_color_hex(0x1e2a47)
#define MENU_ACCENT         lv_color_hex(0x2196F3)
#define MENU_TEXT           lv_color_hex(0xffffff)
#define MENU_TEXT_DIM       lv_color_hex(0x8892b0)
#define MENU_SUCCESS        lv_color_hex(0x00ff87)
#define MENU_ERROR          lv_color_hex(0xff6b6b)
#define MENU_REDSEA         lv_color_hex(0xe94560)
#define MENU_TUNZE          lv_color_hex(0x00d9ff)
#define MENU_TASMOTA        lv_color_hex(0xffa502)

// ============================================================
// Menu State
// ============================================================
static lv_obj_t *menu_screen = NULL;
static lv_obj_t *menu_sidebar = NULL;
static lv_obj_t *menu_content = NULL;
static lv_obj_t *menu_overlay = NULL;
static bool menu_sidebar_visible = false;

// Menu item buttons for highlighting
static lv_obj_t *menu_btn_control = NULL;
static lv_obj_t *menu_btn_redsea = NULL;
static lv_obj_t *menu_btn_tunze = NULL;
static lv_obj_t *menu_btn_tasmota = NULL;
static lv_obj_t *menu_btn_device = NULL;
static lv_obj_t *menu_btn_reset = NULL;

// Current active menu item
static int active_menu_item = 0;

// ============================================================
// Forward Declarations
// ============================================================
static void show_control_section();
static void show_redsea_section();
static void show_tunze_section();
static void show_tasmota_section();
static void show_device_section();
static void show_reset_section();
static void toggle_sidebar();
static void hide_sidebar();

// ============================================================
// Menu Item Style
// ============================================================
static void update_menu_item_style(lv_obj_t *btn, bool active) {
  if (active) {
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(btn, 4, 0);
    lv_obj_set_style_border_color(btn, MENU_ACCENT, 0);
  } else {
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(btn, 0, 0);
  }
}

static void set_active_menu(int index) {
  active_menu_item = index;
  update_menu_item_style(menu_btn_control, index == 0);
  update_menu_item_style(menu_btn_redsea, index == 1);
  update_menu_item_style(menu_btn_tunze, index == 2);
  update_menu_item_style(menu_btn_tasmota, index == 3);
  update_menu_item_style(menu_btn_device, index == 4);
  update_menu_item_style(menu_btn_reset, index == 5);
}

// ============================================================
// Overlay Click Handler
// ============================================================
static void overlay_click_cb(lv_event_t *e) {
  hide_sidebar();
}

// ============================================================
// Sidebar Toggle
// ============================================================
static void toggle_sidebar() {
  if (menu_sidebar_visible) {
    hide_sidebar();
  } else {
    // Show sidebar
    lv_obj_clear_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(menu_sidebar, LV_OBJ_FLAG_HIDDEN);
    
    // Animate in
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, menu_sidebar);
    lv_anim_set_values(&a, -220, 0);
    lv_anim_set_time(&a, 200);
    lv_anim_set_exec_cb(&a, [](void *var, int32_t v) {
      lv_obj_set_x((lv_obj_t*)var, v);
    });
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    
    menu_sidebar_visible = true;
  }
}

static void hide_sidebar() {
  if (!menu_sidebar_visible) return;
  
  // Animate out
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, menu_sidebar);
  lv_anim_set_values(&a, 0, -220);
  lv_anim_set_time(&a, 200);
  lv_anim_set_exec_cb(&a, [](void *var, int32_t v) {
    lv_obj_set_x((lv_obj_t*)var, v);
  });
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
  lv_anim_set_ready_cb(&a, [](lv_anim_t *a) {
    lv_obj_add_flag(menu_sidebar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
  });
  lv_anim_start(&a);
  
  menu_sidebar_visible = false;
}

// ============================================================
// Hamburger Button Handler
// ============================================================
static void hamburger_click_cb(lv_event_t *e) {
  toggle_sidebar();
}

// ============================================================
// Menu Button Handlers
// ============================================================
static void menu_control_cb(lv_event_t *e) {
  set_active_menu(0);
  show_control_section();
  hide_sidebar();
}

static void menu_redsea_cb(lv_event_t *e) {
  set_active_menu(1);
  show_redsea_section();
  hide_sidebar();
}

static void menu_tunze_cb(lv_event_t *e) {
  set_active_menu(2);
  show_tunze_section();
  hide_sidebar();
}

static void menu_tasmota_cb(lv_event_t *e) {
  set_active_menu(3);
  show_tasmota_section();
  hide_sidebar();
}

static void menu_device_cb(lv_event_t *e) {
  set_active_menu(4);
  show_device_section();
  hide_sidebar();
}

static void menu_reset_cb(lv_event_t *e) {
  set_active_menu(5);
  show_reset_section();
  hide_sidebar();
}

// ============================================================
// Create Menu Item Button
// ============================================================
static lv_obj_t* create_menu_item(lv_obj_t *parent, const char *icon, const char *text, 
                                   lv_event_cb_t cb, bool is_sub = false) {
  lv_obj_t *btn = lv_btn_create(parent);
  
  // Größere Buttons für kleine Displays (bessere Touch-Bedienung)
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(btn, 200, is_sub ? 55 : 60);
  const lv_font_t *icon_font = &lv_font_montserrat_18;
  const lv_font_t *text_font = is_sub ? &lv_font_montserrat_16 : &lv_font_montserrat_18;
  #else
  lv_obj_set_size(btn, 200, is_sub ? 45 : 50);
  const lv_font_t *icon_font = &lv_font_montserrat_16;
  const lv_font_t *text_font = is_sub ? &lv_font_montserrat_14 : &lv_font_montserrat_16;
  #endif
  
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x16213e), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_left(btn, is_sub ? 35 : 15, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
  
  // Container for icon + text
  lv_obj_t *cont = lv_obj_create(btn);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  
  // Icon
  lv_obj_t *icon_lbl = lv_label_create(cont);
  lv_label_set_text(icon_lbl, icon);
  lv_obj_set_style_text_font(icon_lbl, icon_font, 0);
  lv_obj_set_style_text_color(icon_lbl, MENU_TEXT_DIM, 0);
  
  // Spacer
  lv_obj_t *spacer = lv_obj_create(cont);
  lv_obj_set_size(spacer, 10, 1);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);
  
  // Text
  lv_obj_t *text_lbl = lv_label_create(cont);
  lv_label_set_text(text_lbl, text);
  lv_obj_set_style_text_font(text_lbl, text_font, 0);
  lv_obj_set_style_text_color(text_lbl, MENU_TEXT, 0);
  
  return btn;
}

// ============================================================
// Clear Content Area
// ============================================================
static void clear_content() {
  lv_obj_clean(menu_content);
}

// ============================================================
// SECTION: Control (Main)
// ============================================================
extern bool shouldIgnoreTouchAfterScreensaver();
extern void clearScreensaverExitTime();

static void start_btn_cb(lv_event_t *e) {
  static unsigned long last_click = 0;
  unsigned long now = millis();
  
  // Debouncing: Ignore clicks within 500ms of last click
  if (now - last_click < 500) {
    Serial.println("⊘ Button click ignored (debounce)");
    return;
  }
  last_click = now;
  
  // Ignore touch if we just exited screensaver (prevents accidental activation)
  if (shouldIgnoreTouchAfterScreensaver()) {
    clearScreensaverExitTime();
    return;
  }
  
  if (feedingModeActive) {
    stopFeedingMode();
  } else {
    startFeedingMode();
  }
}

static void show_control_section() {
  clear_content();
  
  // Title - größere Schrift für kleine Displays
  lv_obj_t *title = lv_label_create(menu_content);
  lv_label_set_text(title, "Steuerung");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  #else
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  #endif
  lv_obj_set_style_text_color(title, MENU_TEXT, 0);
  
  // Status Card - größer für kleine Displays
  lv_obj_t *status_card = lv_obj_create(menu_content);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(status_card, LV_PCT(100), 150);
  int dot_size = 60;
  const lv_font_t *title_font = &lv_font_montserrat_18;
  const lv_font_t *value_font = &lv_font_montserrat_32;
  int title_offset_x = 90;
  int title_offset_y = 20;
  int value_offset_x = 90;
  int value_offset_y = -20;
  #else
  lv_obj_set_size(status_card, LV_PCT(100), 120);
  int dot_size = 50;
  const lv_font_t *title_font = &lv_font_montserrat_16;
  const lv_font_t *value_font = &lv_font_montserrat_28;
  int title_offset_x = 80;
  int title_offset_y = 15;
  int value_offset_x = 80;
  int value_offset_y = -15;
  #endif
  
  lv_obj_set_style_bg_color(status_card, MENU_CARD, 0);
  lv_obj_set_style_radius(status_card, 15, 0);
  lv_obj_set_style_border_width(status_card, 0, 0);
  lv_obj_set_style_pad_all(status_card, 15, 0);
  lv_obj_clear_flag(status_card, LV_OBJ_FLAG_SCROLLABLE);
  
  // Status indicator
  lv_obj_t *status_dot = lv_obj_create(status_card);
  lv_obj_set_size(status_dot, dot_size, dot_size);
  lv_obj_align(status_dot, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(status_dot, 0, 0);
  lv_obj_set_style_bg_color(status_dot, feedingModeActive ? MENU_SUCCESS : MENU_ERROR, 0);
  lv_obj_clear_flag(status_dot, LV_OBJ_FLAG_SCROLLABLE);
  
  // Status text
  lv_obj_t *status_title = lv_label_create(status_card);
  lv_label_set_text(status_title, "Fuetterungsmodus");
  lv_obj_set_style_text_font(status_title, title_font, 0);
  lv_obj_set_style_text_color(status_title, MENU_TEXT_DIM, 0);
  lv_obj_align(status_title, LV_ALIGN_TOP_LEFT, title_offset_x, title_offset_y);
  
  lv_obj_t *status_value = lv_label_create(status_card);
  lv_label_set_text(status_value, feedingModeActive ? "AKTIV" : "INAKTIV");
  lv_obj_set_style_text_font(status_value, value_font, 0);
  lv_obj_set_style_text_color(status_value, feedingModeActive ? MENU_SUCCESS : MENU_ERROR, 0);
  lv_obj_align(status_value, LV_ALIGN_BOTTOM_LEFT, value_offset_x, value_offset_y);
  
  // Start/Stop Button - viel größer für Touch-Bedienung
  lv_obj_t *action_btn = lv_btn_create(menu_content);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(action_btn, LV_PCT(95), 80);
  const lv_font_t *btn_font = &lv_font_montserrat_24;
  #else
  lv_obj_set_size(action_btn, 200, 60);
  const lv_font_t *btn_font = &lv_font_montserrat_20;
  #endif
  
  lv_obj_set_style_bg_color(action_btn, feedingModeActive ? MENU_ERROR : MENU_SUCCESS, 0);
  lv_obj_set_style_radius(action_btn, 15, 0);
  lv_obj_add_event_cb(action_btn, start_btn_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *btn_lbl = lv_label_create(action_btn);
  lv_label_set_text(btn_lbl, feedingModeActive ? LV_SYMBOL_STOP " STOPPEN" : LV_SYMBOL_PLAY " STARTEN");
  lv_obj_set_style_text_font(btn_lbl, btn_font, 0);
  lv_obj_set_style_text_color(btn_lbl, feedingModeActive ? MENU_TEXT : lv_color_hex(0x1a1a2e), 0);
  lv_obj_center(btn_lbl);
}

// ============================================================
// SECTION: Red Sea
// ============================================================
static void show_redsea_section() {
  clear_content();
  
  lv_obj_t *title = lv_label_create(menu_content);
  lv_label_set_text(title, LV_SYMBOL_SETTINGS " Red Sea");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  #else
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  #endif
  lv_obj_set_style_text_color(title, MENU_REDSEA, 0);
  
  lv_obj_t *status_card = lv_obj_create(menu_content);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(status_card, LV_PCT(100), 100);
  #else
  lv_obj_set_size(status_card, LV_PCT(100), 80);
  #endif
  lv_obj_set_style_bg_color(status_card, MENU_CARD, 0);
  lv_obj_set_style_radius(status_card, 15, 0);
  lv_obj_set_style_border_width(status_card, 0, 0);
  lv_obj_set_style_pad_all(status_card, 15, 0);
  lv_obj_clear_flag(status_card, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *status_lbl = lv_label_create(status_card);
  lv_label_set_text(status_lbl, ENABLE_redsea ? "Aktiviert" : "Deaktiviert");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_24, 0);
  #else
  lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_20, 0);
  #endif
  lv_obj_set_style_text_color(status_lbl, ENABLE_redsea ? MENU_SUCCESS : MENU_TEXT_DIM, 0);
  lv_obj_center(status_lbl);
  
  lv_obj_t *info = lv_label_create(menu_content);
  lv_label_set_text(info, "Red Sea Einstellungen\nkoennen im Web Interface\ngeaendert werden.");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(info, &lv_font_montserrat_16, 0);
  #else
  lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
  #endif
  lv_obj_set_style_text_color(info, MENU_TEXT_DIM, 0);
  lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
}

// ============================================================
// SECTION: Tunze
// ============================================================
static void show_tunze_section() {
  clear_content();
  
  lv_obj_t *title = lv_label_create(menu_content);
  lv_label_set_text(title, LV_SYMBOL_SETTINGS " Tunze Hub");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  #else
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  #endif
  lv_obj_set_style_text_color(title, MENU_TUNZE, 0);
  
  lv_obj_t *status_card = lv_obj_create(menu_content);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(status_card, LV_PCT(100), 100);
  #else
  lv_obj_set_size(status_card, LV_PCT(100), 80);
  #endif
  lv_obj_set_style_bg_color(status_card, MENU_CARD, 0);
  lv_obj_set_style_radius(status_card, 15, 0);
  lv_obj_set_style_border_width(status_card, 0, 0);
  lv_obj_set_style_pad_all(status_card, 15, 0);
  lv_obj_clear_flag(status_card, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *status_lbl = lv_label_create(status_card);
  lv_label_set_text(status_lbl, ENABLE_TUNZE ? "Aktiviert" : "Deaktiviert");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_24, 0);
  #else
  lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_20, 0);
  #endif
  lv_obj_set_style_text_color(status_lbl, ENABLE_TUNZE ? MENU_SUCCESS : MENU_TEXT_DIM, 0);
  lv_obj_center(status_lbl);
  
  lv_obj_t *info = lv_label_create(menu_content);
  lv_label_set_text(info, "Tunze Hub Einstellungen\nkoennen im Web Interface\ngeaendert werden.");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(info, &lv_font_montserrat_16, 0);
  #else
  lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
  #endif
  lv_obj_set_style_text_color(info, MENU_TEXT_DIM, 0);
  lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
}

// ============================================================
// SECTION: Tasmota
// ============================================================
static void show_tasmota_section() {
  clear_content();
  
  lv_obj_t *title = lv_label_create(menu_content);
  lv_label_set_text(title, LV_SYMBOL_POWER " Tasmota");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  #else
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  #endif
  lv_obj_set_style_text_color(title, MENU_TASMOTA, 0);
  
  bool isEnabled = tasmotaIsEnabled();
  int pulseTime = tasmotaGetPulseTime();
  
  // Status Card
  lv_obj_t *status_card = lv_obj_create(menu_content);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(status_card, LV_PCT(100), 120);
  #else
  lv_obj_set_size(status_card, LV_PCT(100), 100);
  #endif
  lv_obj_set_style_bg_color(status_card, MENU_CARD, 0);
  lv_obj_set_style_radius(status_card, 15, 0);
  lv_obj_set_style_border_width(status_card, 0, 0);
  lv_obj_set_style_pad_all(status_card, 15, 0);
  lv_obj_clear_flag(status_card, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *status_lbl = lv_label_create(status_card);
  lv_label_set_text(status_lbl, isEnabled ? "Aktiviert" : "Deaktiviert");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_24, 0);
  #else
  lv_obj_set_style_text_font(status_lbl, &lv_font_montserrat_20, 0);
  #endif
  lv_obj_set_style_text_color(status_lbl, isEnabled ? MENU_SUCCESS : MENU_TEXT_DIM, 0);
  lv_obj_align(status_lbl, LV_ALIGN_TOP_LEFT, 10, 10);
  
  // Pulse time info
  char pulse_text[32];
  snprintf(pulse_text, sizeof(pulse_text), "Auto-On: %d Sek.", pulseTime);
  lv_obj_t *pulse_lbl = lv_label_create(status_card);
  lv_label_set_text(pulse_lbl, pulse_text);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(pulse_lbl, &lv_font_montserrat_18, 0);
  #else
  lv_obj_set_style_text_font(pulse_lbl, &lv_font_montserrat_16, 0);
  #endif
  lv_obj_set_style_text_color(pulse_lbl, MENU_TEXT_DIM, 0);
  lv_obj_align(pulse_lbl, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  
  lv_obj_t *info = lv_label_create(menu_content);
  lv_label_set_text(info, "Tasmota Geraete werden\nautomatisch gesteuert.\n\nKonfiguration im Web Interface.");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(info, &lv_font_montserrat_16, 0);
  #else
  lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
  #endif
  lv_obj_set_style_text_color(info, MENU_TEXT_DIM, 0);
  lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
}

// ============================================================
// SECTION: Device Info
// ============================================================
static void show_device_section() {
  clear_content();
  
  lv_obj_t *title = lv_label_create(menu_content);
  lv_label_set_text(title, LV_SYMBOL_HOME " Geraeteinfo");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  #else
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  #endif
  lv_obj_set_style_text_color(title, MENU_TEXT, 0);
  
  // Info Card
  lv_obj_t *info_card = lv_obj_create(menu_content);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(info_card, LV_PCT(100), 240);
  const lv_font_t *info_font = &lv_font_montserrat_18;
  const lv_font_t *detail_font = &lv_font_montserrat_16;
  int row_padding = 10;
  #else
  lv_obj_set_size(info_card, LV_PCT(100), 200);
  const lv_font_t *info_font = &lv_font_montserrat_16;
  const lv_font_t *detail_font = &lv_font_montserrat_14;
  int row_padding = 8;
  #endif
  
  lv_obj_set_style_bg_color(info_card, MENU_CARD, 0);
  lv_obj_set_style_radius(info_card, 15, 0);
  lv_obj_set_style_border_width(info_card, 0, 0);
  lv_obj_set_style_pad_all(info_card, 15, 0);
  lv_obj_set_flex_flow(info_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(info_card, row_padding, 0);
  // Scrolling enabled by default - no need to block it
  
  // WiFi Status
  char wifi_text[64];
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(wifi_text, sizeof(wifi_text), LV_SYMBOL_WIFI " %s", WiFi.SSID().c_str());
  } else {
    snprintf(wifi_text, sizeof(wifi_text), LV_SYMBOL_WIFI " Nicht verbunden");
  }
  lv_obj_t *wifi_lbl = lv_label_create(info_card);
  lv_label_set_text(wifi_lbl, wifi_text);
  lv_obj_set_style_text_font(wifi_lbl, info_font, 0);
  lv_obj_set_style_text_color(wifi_lbl, WiFi.status() == WL_CONNECTED ? MENU_SUCCESS : MENU_ERROR, 0);
  
  // IP Address
  char ip_text[32];
  snprintf(ip_text, sizeof(ip_text), "IP: %s", WiFi.localIP().toString().c_str());
  lv_obj_t *ip_lbl = lv_label_create(info_card);
  lv_label_set_text(ip_lbl, ip_text);
  lv_obj_set_style_text_font(ip_lbl, detail_font, 0);
  lv_obj_set_style_text_color(ip_lbl, MENU_TEXT_DIM, 0);
  
  // Signal strength
  char rssi_text[32];
  snprintf(rssi_text, sizeof(rssi_text), "Signal: %d dBm", WiFi.RSSI());
  lv_obj_t *rssi_lbl = lv_label_create(info_card);
  lv_label_set_text(rssi_lbl, rssi_text);
  lv_obj_set_style_text_font(rssi_lbl, detail_font, 0);
  lv_obj_set_style_text_color(rssi_lbl, MENU_TEXT_DIM, 0);
  
  // Free heap
  char heap_text[32];
  snprintf(heap_text, sizeof(heap_text), "RAM: %d KB frei", ESP.getFreeHeap() / 1024);
  lv_obj_t *heap_lbl = lv_label_create(info_card);
  lv_label_set_text(heap_lbl, heap_text);
  lv_obj_set_style_text_font(heap_lbl, detail_font, 0);
  lv_obj_set_style_text_color(heap_lbl, MENU_TEXT_DIM, 0);
  
  // Current Time
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  char time_text[32];
  strftime(time_text, sizeof(time_text), "Zeit: %H:%M:%S", &timeinfo);
  lv_obj_t *time_lbl = lv_label_create(info_card);
  lv_label_set_text(time_lbl, time_text);
  lv_obj_set_style_text_font(time_lbl, detail_font, 0);
  lv_obj_set_style_text_color(time_lbl, MENU_ACCENT, 0);
  
  // Version
  lv_obj_t *ver_lbl = lv_label_create(info_card);
  lv_label_set_text(ver_lbl, "Version: 2.0");
  lv_obj_set_style_text_font(ver_lbl, detail_font, 0);
  lv_obj_set_style_text_color(ver_lbl, MENU_TEXT_DIM, 0);
  
  // Time Settings Title
  lv_obj_t *time_title = lv_label_create(menu_content);
  lv_label_set_text(time_title, LV_SYMBOL_REFRESH " Zeiteinstellungen");
  lv_obj_set_style_text_font(time_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(time_title, MENU_TEXT, 0);
  lv_obj_set_style_pad_top(time_title, 20, 0);
  
  // Timezone Card
  lv_obj_t *tz_card = lv_obj_create(menu_content);
  lv_obj_set_size(tz_card, LV_PCT(100), 100);
  lv_obj_set_style_bg_color(tz_card, MENU_CARD, 0);
  lv_obj_set_style_radius(tz_card, 15, 0);
  lv_obj_set_style_border_width(tz_card, 0, 0);
  lv_obj_set_style_pad_all(tz_card, 15, 0);
  // Scrolling enabled by default
  
  // Timezone dropdown
  lv_obj_t *tz_label = lv_label_create(tz_card);
  lv_label_set_text(tz_label, "Zeitzone (mit autom. Sommer-/Winterzeit):");
  lv_obj_set_style_text_font(tz_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(tz_label, MENU_TEXT, 0);
  
  extern int getCurrentTimezoneIndex();
  extern String getTimezoneString(int, bool);
  extern String tzString;
  extern void saveTimeConfig();
  extern void setupNTP();
  
  lv_obj_t *tz_dropdown = lv_dropdown_create(tz_card);
  lv_dropdown_set_options(tz_dropdown, "UTC\n"
                                       "Westeuropa (UK)\n"
                                       "Mitteleuropa (DE)\n"
                                       "Osteuropa\n"
                                       "Moskau\n"
                                       "US Eastern\n"
                                       "US Central\n"
                                       "US Pacific");
  lv_dropdown_set_selected(tz_dropdown, getCurrentTimezoneIndex());
  lv_obj_set_width(tz_dropdown, LV_PCT(100));
  lv_obj_align(tz_dropdown, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_style_bg_color(tz_dropdown, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_text_color(tz_dropdown, MENU_TEXT, LV_PART_MAIN);
  
  // Event handler for timezone change (DST always enabled for automatic switching)
  lv_obj_add_event_cb(tz_dropdown, [](lv_event_t *e) {
    lv_obj_t *dropdown = lv_event_get_target(e);
    int sel = lv_dropdown_get_selected(dropdown);
    tzString = getTimezoneString(sel, true);  // Always use DST rules
    saveTimeConfig();
    setupNTP();
  }, LV_EVENT_VALUE_CHANGED, NULL);
  
  // Screensaver Settings Title
  lv_obj_t *screensaver_title = lv_label_create(menu_content);
  lv_label_set_text(screensaver_title, LV_SYMBOL_EYE_CLOSE " Bildschirmschoner");
  lv_obj_set_style_text_font(screensaver_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(screensaver_title, MENU_TEXT, 0);
  lv_obj_set_style_pad_top(screensaver_title, 20, 0);
  
  // Screensaver Card
  lv_obj_t *screensaver_card = lv_obj_create(menu_content);
  lv_obj_set_size(screensaver_card, LV_PCT(100), 140);
  lv_obj_set_style_bg_color(screensaver_card, MENU_CARD, 0);
  lv_obj_set_style_radius(screensaver_card, 15, 0);
  lv_obj_set_style_border_width(screensaver_card, 0, 0);
  lv_obj_set_style_pad_all(screensaver_card, 15, 0);
  lv_obj_clear_flag(screensaver_card, LV_OBJ_FLAG_SCROLLABLE);
  
  // Label
  lv_obj_t *timeout_label = lv_label_create(screensaver_card);
  lv_label_set_text(timeout_label, "Timeout (Sekunden):");
  lv_obj_set_style_text_font(timeout_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(timeout_label, MENU_TEXT, 0);
  
  // Slider for timeout (0-300 seconds)
  lv_obj_t *slider = lv_slider_create(screensaver_card);
  lv_obj_set_width(slider, LV_PCT(100));
  lv_obj_align(slider, LV_ALIGN_CENTER, 0, 10);
  lv_slider_set_range(slider, 0, 300);
  lv_slider_set_value(slider, getScreensaverTimeout(), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, MENU_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, MENU_ACCENT, LV_PART_KNOB);
  
  // Value Label
  lv_obj_t *value_label = lv_label_create(screensaver_card);
  char timeout_text[32];
  snprintf(timeout_text, sizeof(timeout_text), "%d s (0 = aus)", getScreensaverTimeout());
  lv_label_set_text(value_label, timeout_text);
  lv_obj_set_style_text_font(value_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(value_label, MENU_ACCENT, 0);
  lv_obj_align(value_label, LV_ALIGN_BOTTOM_MID, 0, 0);
  
  // Slider event to update value label (while dragging)
  lv_obj_add_event_cb(slider, [](lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t*)lv_event_get_user_data(e);
    int value = lv_slider_get_value(slider);
    char text[32];
    snprintf(text, sizeof(text), "%d s (0 = aus)", value);
    lv_label_set_text(label, text);
    setScreensaverTimeout(value);
  }, LV_EVENT_VALUE_CHANGED, value_label);
  
  // Save to Preferences when slider is released
  lv_obj_add_event_cb(slider, [](lv_event_t *e) {
    saveScreensaverTimeout();
  }, LV_EVENT_RELEASED, NULL);
}

// ============================================================
// SECTION: Factory Reset
// ============================================================
static lv_obj_t *reset_msgbox = NULL;

static void reset_msgbox_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_current_target(e);
  const char *btn_text = lv_msgbox_get_active_btn_text(obj);
  
  if (btn_text != NULL && strcmp(btn_text, "Reset") == 0) {
    Serial.println("Factory reset...");
    Preferences prefs;
    prefs.begin("feeding-break", false);
    prefs.clear();
    prefs.end();
    WiFi.disconnect(false);
    delay(100);
    ESP.restart();
  }
  
  lv_msgbox_close(reset_msgbox);
  reset_msgbox = NULL;
}

static void reset_btn_cb(lv_event_t *e) {
  static const char *btns[] = {"Abbrechen", "Reset", ""};
  
  reset_msgbox = lv_msgbox_create(NULL, 
    LV_SYMBOL_WARNING " Werksreset", 
    "Alle Einstellungen werden\ngeloescht!\n\nFortfahren?",
    btns, true);
  
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(reset_msgbox, DISPLAY_WIDTH - 40, LV_SIZE_CONTENT);
  lv_obj_set_style_text_font(reset_msgbox, &lv_font_montserrat_18, 0);
  #else
  lv_obj_set_size(reset_msgbox, 400, LV_SIZE_CONTENT);
  #endif
  
  lv_obj_set_style_bg_color(reset_msgbox, MENU_CARD, 0);
  lv_obj_set_style_text_color(reset_msgbox, MENU_TEXT, 0);
  lv_obj_set_style_border_color(reset_msgbox, MENU_ERROR, 0);
  lv_obj_set_style_border_width(reset_msgbox, 2, 0);
  lv_obj_center(reset_msgbox);
  
  lv_obj_add_event_cb(reset_msgbox, reset_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void show_reset_section() {
  clear_content();
  
  lv_obj_t *title = lv_label_create(menu_content);
  lv_label_set_text(title, LV_SYMBOL_WARNING " Werksreset");
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  #else
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  #endif
  lv_obj_set_style_text_color(title, MENU_ERROR, 0);
  
  // Warning Card
  lv_obj_t *warn_card = lv_obj_create(menu_content);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(warn_card, LV_PCT(100), 140);
  const lv_font_t *warn_font = &lv_font_montserrat_16;
  #else
  lv_obj_set_size(warn_card, LV_PCT(100), 100);
  const lv_font_t *warn_font = &lv_font_montserrat_14;
  #endif
  
  lv_obj_set_style_bg_color(warn_card, MENU_CARD, 0);
  lv_obj_set_style_radius(warn_card, 15, 0);
  lv_obj_set_style_border_width(warn_card, 1, 0);
  lv_obj_set_style_border_color(warn_card, MENU_ERROR, 0);
  lv_obj_set_style_pad_all(warn_card, 15, 0);
  lv_obj_clear_flag(warn_card, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *warn_text = lv_label_create(warn_card);
  lv_label_set_text(warn_text, "Loescht alle Einstellungen:\n- WiFi Zugangsdaten\n- Alle Konfigurationen");
  lv_obj_set_style_text_font(warn_text, warn_font, 0);
  lv_obj_set_style_text_color(warn_text, MENU_TEXT_DIM, 0);
  lv_obj_center(warn_text);
  
  // Reset Button - größer für Touch
  lv_obj_t *reset_btn = lv_btn_create(menu_content);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(reset_btn, LV_PCT(90), 70);
  const lv_font_t *btn_font = &lv_font_montserrat_20;
  #else
  lv_obj_set_size(reset_btn, 200, 55);
  const lv_font_t *btn_font = &lv_font_montserrat_16;
  #endif
  
  lv_obj_set_style_bg_color(reset_btn, MENU_ERROR, 0);
  lv_obj_set_style_radius(reset_btn, 12, 0);
  lv_obj_add_event_cb(reset_btn, reset_btn_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *btn_lbl = lv_label_create(reset_btn);
  lv_label_set_text(btn_lbl, LV_SYMBOL_TRASH " Zuruecksetzen");
  lv_obj_set_style_text_font(btn_lbl, btn_font, 0);
  lv_obj_set_style_text_color(btn_lbl, MENU_TEXT, 0);
  lv_obj_center(btn_lbl);
}

// ============================================================
// Create Menu Screen
// ============================================================
void createMenuScreen() {
  if (menu_screen != NULL) {
    lv_obj_del(menu_screen);
    menu_screen = NULL;
  }
  
  // Main screen
  menu_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(menu_screen, MENU_BG, 0);
  
  // ========== Header ==========
  // Größerer Header für kleine Displays
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  int header_height = 70;
  int hamburger_size = 60;
  int hamburger_line_width = 28;
  int hamburger_line_height = 4;
  const lv_font_t *title_font = &lv_font_montserrat_20;
  #else
  int header_height = 60;
  int hamburger_size = 50;
  int hamburger_line_width = 22;
  int hamburger_line_height = 3;
  const lv_font_t *title_font = &lv_font_montserrat_22;
  #endif
  
  lv_obj_t *header = lv_obj_create(menu_screen);
  lv_obj_set_size(header, DISPLAY_WIDTH, header_height);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, MENU_HEADER, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  // Hamburger button
  lv_obj_t *hamburger = lv_btn_create(header);
  lv_obj_set_size(hamburger, hamburger_size, hamburger_size - 5);
  lv_obj_align(hamburger, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_set_style_bg_color(hamburger, lv_color_hex(0x0a2540), 0);
  lv_obj_set_style_radius(hamburger, 8, 0);
  lv_obj_add_event_cb(hamburger, hamburger_click_cb, LV_EVENT_CLICKED, NULL);
  
  // Hamburger lines (3 lines)
  for (int i = 0; i < 3; i++) {
    lv_obj_t *line = lv_obj_create(hamburger);
    lv_obj_set_size(line, hamburger_line_width, hamburger_line_height);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, (i - 1) * 10);
    lv_obj_set_style_bg_color(line, MENU_TEXT, 0);
    lv_obj_set_style_radius(line, 2, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
  }
  
  // Title
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Feeding Break");
  lv_obj_set_style_text_font(title, title_font, 0);
  lv_obj_set_style_text_color(title, MENU_TEXT, 0);
  lv_obj_center(title);
  
  // ========== Content Area ==========
  menu_content = lv_obj_create(menu_screen);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(menu_content, DISPLAY_WIDTH - 20, DISPLAY_HEIGHT - 90);
  lv_obj_align(menu_content, LV_ALIGN_TOP_MID, 0, 80);
  #else
  lv_obj_set_size(menu_content, DISPLAY_WIDTH - 20, DISPLAY_HEIGHT - 80);
  lv_obj_align(menu_content, LV_ALIGN_TOP_MID, 0, 70);
  #endif
  lv_obj_set_style_bg_opa(menu_content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(menu_content, 0, 0);
  lv_obj_set_style_pad_all(menu_content, 10, 0);
  lv_obj_set_flex_flow(menu_content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(menu_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(menu_content, 15, 0);
  
  // ========== Overlay (for closing sidebar) ==========
  menu_overlay = lv_obj_create(menu_screen);
  lv_obj_set_size(menu_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_obj_set_pos(menu_overlay, 0, 0);
  lv_obj_set_style_bg_color(menu_overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(menu_overlay, LV_OPA_50, 0);
  lv_obj_set_style_border_width(menu_overlay, 0, 0);
  lv_obj_add_event_cb(menu_overlay, overlay_click_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(menu_overlay, LV_OBJ_FLAG_HIDDEN);
  
  // ========== Sidebar ==========
  menu_sidebar = lv_obj_create(menu_screen);
  lv_obj_set_size(menu_sidebar, 220, DISPLAY_HEIGHT);
  lv_obj_set_pos(menu_sidebar, -220, 0);  // Start hidden
  lv_obj_set_style_bg_color(menu_sidebar, MENU_SIDEBAR, 0);
  lv_obj_set_style_radius(menu_sidebar, 0, 0);
  lv_obj_set_style_border_width(menu_sidebar, 0, 0);
  lv_obj_set_style_pad_all(menu_sidebar, 0, 0);
  lv_obj_set_style_shadow_width(menu_sidebar, 20, 0);
  lv_obj_set_style_shadow_opa(menu_sidebar, LV_OPA_30, 0);
  lv_obj_clear_flag(menu_sidebar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(menu_sidebar, LV_OBJ_FLAG_HIDDEN);
  
  // Sidebar Header
  lv_obj_t *sidebar_header = lv_obj_create(menu_sidebar);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(sidebar_header, 220, 70);
  #else
  lv_obj_set_size(sidebar_header, 220, 60);
  #endif
  lv_obj_set_pos(sidebar_header, 0, 0);
  lv_obj_set_style_bg_color(sidebar_header, MENU_HEADER, 0);
  lv_obj_set_style_radius(sidebar_header, 0, 0);
  lv_obj_set_style_border_width(sidebar_header, 0, 0);
  lv_obj_clear_flag(sidebar_header, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *sidebar_title = lv_label_create(sidebar_header);
  lv_label_set_text(sidebar_title, "Menu");
  lv_obj_set_style_text_font(sidebar_title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sidebar_title, MENU_TEXT, 0);
  lv_obj_center(sidebar_title);
  
  // Menu Items Container
  lv_obj_t *menu_items = lv_obj_create(menu_sidebar);
  #ifdef BOARD_WAVESHARE_AMOLED_1_8
  lv_obj_set_size(menu_items, 220, DISPLAY_HEIGHT - 70);
  lv_obj_set_pos(menu_items, 0, 70);
  #else
  lv_obj_set_size(menu_items, 220, 380);
  lv_obj_set_pos(menu_items, 0, 60);
  #endif
  lv_obj_set_style_bg_opa(menu_items, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(menu_items, 0, 0);
  lv_obj_set_style_pad_all(menu_items, 0, 0);
  lv_obj_set_flex_flow(menu_items, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(menu_items, 2, 0);
  lv_obj_clear_flag(menu_items, LV_OBJ_FLAG_SCROLLABLE);
  
  // Create menu items
  menu_btn_control = create_menu_item(menu_items, LV_SYMBOL_HOME, "Steuerung", menu_control_cb, false);
  
  // Settings divider
  lv_obj_t *divider = lv_label_create(menu_items);
  lv_label_set_text(divider, "Einstellungen");
  lv_obj_set_style_text_font(divider, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(divider, MENU_TEXT_DIM, 0);
  lv_obj_set_style_pad_left(divider, 15, 0);
  lv_obj_set_style_pad_top(divider, 10, 0);
  
  menu_btn_redsea = create_menu_item(menu_items, LV_SYMBOL_TINT, "Red Sea", menu_redsea_cb, true);
  menu_btn_tunze = create_menu_item(menu_items, LV_SYMBOL_REFRESH, "Tunze Hub", menu_tunze_cb, true);
  menu_btn_tasmota = create_menu_item(menu_items, LV_SYMBOL_POWER, "Tasmota", menu_tasmota_cb, true);
  menu_btn_device = create_menu_item(menu_items, LV_SYMBOL_HOME, "Geraeteinfo", menu_device_cb, true);
  menu_btn_reset = create_menu_item(menu_items, LV_SYMBOL_WARNING, "Werksreset", menu_reset_cb, true);
  
  // Set initial active state
  set_active_menu(0);
  
  // Show control section by default
  show_control_section();
  
  menu_sidebar_visible = false;
  
  // Load the screen
  lv_scr_load(menu_screen);
}

// ============================================================
// Show Menu Screen
// ============================================================
void showMenuScreen() {
  createMenuScreen();
  lv_scr_load(menu_screen);
}

// ============================================================
// Update Menu UI (call periodically to refresh status)
// ============================================================
void updateMenuUI() {
  // Refresh current section if it's the control section
  if (active_menu_item == 0 && menu_content != NULL) {
    // Only update if state changed
    static bool last_state = false;
    if (feedingModeActive != last_state) {
      last_state = feedingModeActive;
      show_control_section();
    }
  }
}

// ============================================================
// Get Menu Screen (for navigation)
// ============================================================
lv_obj_t* getMenuScreen() {
  return menu_screen;
}

#endif // MENU_UI_H
