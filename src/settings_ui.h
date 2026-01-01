/**
 * @file settings_ui.h
 * @brief Settings Menu UI with WiFi and Factory Reset
 */

#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include <Arduino.h>
#include <lvgl.h>
#include <Preferences.h>
#include "board_config.h"
#include "version.h"

// ============================================================
// External References
// ============================================================
extern void showWiFiScreen();
extern void saveWiFiCredentials(const String &ssid, const String &password);

// ============================================================
// Settings UI Colors
// ============================================================
#define SETTINGS_BG           lv_color_hex(0x1a1a2e)
#define SETTINGS_CARD         lv_color_hex(0x16213e)
#define SETTINGS_HEADER       lv_color_hex(0x0f3460)
#define SETTINGS_ACCENT       lv_color_hex(0x00d9ff)
#define SETTINGS_SUCCESS      lv_color_hex(0x00ff87)
#define SETTINGS_ERROR        lv_color_hex(0xff6b6b)
#define SETTINGS_WARNING      lv_color_hex(0xffa502)
#define SETTINGS_TEXT         lv_color_hex(0xffffff)
#define SETTINGS_TEXT_DIM     lv_color_hex(0xb8c4d8)  // Brighter for readability

// ============================================================
// Settings UI State
// ============================================================
static lv_obj_t *settings_screen = NULL;
static lv_obj_t *reset_confirm_msgbox = NULL;

// Forward declaration for main screen
lv_obj_t* getMainScreen();

// ============================================================
// Settings Styles
// ============================================================
static lv_style_t settings_style_card;
static lv_style_t settings_style_btn;

static void create_settings_styles() {
  // Card style
  lv_style_init(&settings_style_card);
  lv_style_set_bg_color(&settings_style_card, SETTINGS_CARD);
  lv_style_set_bg_opa(&settings_style_card, LV_OPA_COVER);
  lv_style_set_radius(&settings_style_card, 15);
  lv_style_set_pad_all(&settings_style_card, 15);
  lv_style_set_border_width(&settings_style_card, 0);
  
  // Button style
  lv_style_init(&settings_style_btn);
  lv_style_set_bg_color(&settings_style_btn, lv_color_hex(0x2d3a55));
  lv_style_set_radius(&settings_style_btn, 12);
  lv_style_set_text_color(&settings_style_btn, SETTINGS_TEXT);
  lv_style_set_pad_all(&settings_style_btn, 15);
}

// ============================================================
// Factory Reset Handler
// ============================================================
static bool pending_restart = false;

static void settings_reset_msgbox_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_current_target(e);
  const char *btn_text = lv_msgbox_get_active_btn_text(obj);
  
  if (btn_text != NULL) {
    if (strcmp(btn_text, "Reset") == 0) {
      Serial.println("Werksreset wird durchgefuehrt...");
      
      // Clear the main preferences namespace (contains wifi_ssid, wifi_pass)
      Preferences prefs;
      prefs.begin("feeding-break", false);  // NOTE: This is the actual namespace used in main.cpp!
      prefs.clear();
      prefs.end();
      
      Serial.println("Preferences geloescht.");
      
      // Disconnect WiFi (don't erase internal flash - causes watchdog)
      WiFi.disconnect(false);
      
      Serial.println("WiFi getrennt. Neustart...");
      
      // Close msgbox first
      lv_msgbox_close(reset_confirm_msgbox);
      reset_confirm_msgbox = NULL;
      
      // Set flag for restart in main loop (safer than restarting in callback)
      pending_restart = true;
      
    } else {
      // Cancel - close msgbox
      lv_msgbox_close(reset_confirm_msgbox);
      reset_confirm_msgbox = NULL;
    }
  }
}

// Call this in main loop to check for pending restart
void checkPendingRestart() {
  if (pending_restart) {
    delay(100);  // Small delay to let UI update
    ESP.restart();
  }
}

static void settings_reset_btn_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    // Show confirmation dialog - shorter button texts
    static const char *btns[] = {"Abbrechen", "Reset", ""};
    
    reset_confirm_msgbox = lv_msgbox_create(NULL, 
      LV_SYMBOL_WARNING " Werksreset", 
      "Alle Einstellungen werden\ngeloescht!\n\nWiFi Zugangsdaten\nAlle Konfigurationen\n\nFortfahren?",
      btns, true);
    
    lv_obj_set_size(reset_confirm_msgbox, 450, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(reset_confirm_msgbox, SETTINGS_CARD, 0);
    lv_obj_set_style_text_color(reset_confirm_msgbox, SETTINGS_TEXT, 0);
    lv_obj_set_style_border_color(reset_confirm_msgbox, SETTINGS_ERROR, 0);
    lv_obj_set_style_border_width(reset_confirm_msgbox, 2, 0);
    lv_obj_set_style_pad_all(reset_confirm_msgbox, 25, 0);
    lv_obj_center(reset_confirm_msgbox);
    
    // Style the button matrix (buttons) with more spacing and bigger buttons
    lv_obj_t *btns_obj = lv_msgbox_get_btns(reset_confirm_msgbox);
    lv_obj_set_width(btns_obj, 400);
    lv_obj_set_style_pad_column(btns_obj, 20, 0);
    lv_obj_set_height(btns_obj, 55);
    lv_obj_set_style_text_font(btns_obj, &lv_font_montserrat_16, LV_PART_ITEMS);
    
    lv_obj_add_event_cb(reset_confirm_msgbox, settings_reset_msgbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
  }
}

// ============================================================
// WiFi Settings Button Handler
// ============================================================
static void settings_wifi_btn_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    showWiFiScreen();
  }
}

// ============================================================
// Back Button Handler
// ============================================================
static void settings_back_btn_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *main_scr = getMainScreen();
    if (main_scr != NULL) {
      lv_scr_load_anim(main_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
    }
  }
}

// ============================================================
// Create Settings Screen
// ============================================================
void createSettingsScreen() {
  if (settings_screen != NULL) {
    lv_obj_del(settings_screen);
  }
  
  create_settings_styles();
  
  // Create screen
  settings_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(settings_screen, SETTINGS_BG, 0);
  lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, 0);
  
  // ========== Header ==========
  lv_obj_t *header = lv_obj_create(settings_screen);
  lv_obj_set_size(header, DISPLAY_WIDTH, 60);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, SETTINGS_HEADER, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  // Back button
  lv_obj_t *back_btn = lv_btn_create(header);
  lv_obj_set_size(back_btn, 50, 40);
  lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x0a2540), 0);
  lv_obj_set_style_radius(back_btn, 8, 0);
  lv_obj_add_event_cb(back_btn, settings_back_btn_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *back_icon = lv_label_create(back_btn);
  lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(back_icon, SETTINGS_TEXT, 0);
  lv_obj_center(back_icon);
  
  // Title
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, LV_SYMBOL_SETTINGS "  Einstellungen");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, SETTINGS_TEXT, 0);
  lv_obj_center(title);
  
  // ========== Content ==========
  lv_obj_t *content = lv_obj_create(settings_screen);
  lv_obj_set_size(content, 460, 400);
  lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 70);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, 10, 0);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(content, 15, 0);
  
  // ========== WiFi Settings Card ==========
  lv_obj_t *wifi_card = lv_obj_create(content);
  lv_obj_set_size(wifi_card, 440, 100);
  lv_obj_add_style(wifi_card, &settings_style_card, 0);
  lv_obj_clear_flag(wifi_card, LV_OBJ_FLAG_SCROLLABLE);
  
  // WiFi Icon
  lv_obj_t *wifi_icon = lv_label_create(wifi_card);
  lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(wifi_icon, SETTINGS_ACCENT, 0);
  lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 10, 0);
  
  // WiFi Title & Status
  lv_obj_t *wifi_title = lv_label_create(wifi_card);
  lv_label_set_text(wifi_title, "WiFi Einstellungen");
  lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(wifi_title, SETTINGS_TEXT, 0);
  lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 55, 10);
  
  lv_obj_t *wifi_status = lv_label_create(wifi_card);
  if (WiFi.status() == WL_CONNECTED) {
    char statusText[64];
    snprintf(statusText, sizeof(statusText), "Verbunden: %s", WiFi.localIP().toString().c_str());
    lv_label_set_text(wifi_status, statusText);
    lv_obj_set_style_text_color(wifi_status, SETTINGS_SUCCESS, 0);
  } else {
    lv_label_set_text(wifi_status, "Nicht verbunden");
    lv_obj_set_style_text_color(wifi_status, SETTINGS_TEXT_DIM, 0);
  }
  lv_obj_set_style_text_font(wifi_status, &lv_font_montserrat_14, 0);
  lv_obj_align(wifi_status, LV_ALIGN_BOTTOM_LEFT, 55, -10);
  
  // WiFi Button (arrow)
  lv_obj_t *wifi_btn = lv_btn_create(wifi_card);
  lv_obj_set_size(wifi_btn, 60, 60);
  lv_obj_align(wifi_btn, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0x0a2540), 0);
  lv_obj_set_style_radius(wifi_btn, 10, 0);
  lv_obj_add_event_cb(wifi_btn, settings_wifi_btn_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *wifi_arrow = lv_label_create(wifi_btn);
  lv_label_set_text(wifi_arrow, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(wifi_arrow, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(wifi_arrow, SETTINGS_TEXT, 0);
  lv_obj_center(wifi_arrow);
  
  // Make whole card clickable
  lv_obj_add_flag(wifi_card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(wifi_card, settings_wifi_btn_cb, LV_EVENT_CLICKED, NULL);
  
  // ========== Factory Reset Card ==========
  lv_obj_t *reset_card = lv_obj_create(content);
  lv_obj_set_size(reset_card, 440, 100);
  lv_obj_add_style(reset_card, &settings_style_card, 0);
  lv_obj_set_style_border_color(reset_card, SETTINGS_ERROR, 0);
  lv_obj_set_style_border_width(reset_card, 1, 0);
  lv_obj_set_style_border_opa(reset_card, LV_OPA_50, 0);
  lv_obj_clear_flag(reset_card, LV_OBJ_FLAG_SCROLLABLE);
  
  // Reset Icon
  lv_obj_t *reset_icon = lv_label_create(reset_card);
  lv_label_set_text(reset_icon, LV_SYMBOL_WARNING);
  lv_obj_set_style_text_font(reset_icon, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(reset_icon, SETTINGS_ERROR, 0);
  lv_obj_align(reset_icon, LV_ALIGN_LEFT_MID, 10, 0);
  
  // Reset Title & Description
  lv_obj_t *reset_title = lv_label_create(reset_card);
  lv_label_set_text(reset_title, "Werksreset");
  lv_obj_set_style_text_font(reset_title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(reset_title, SETTINGS_TEXT, 0);
  lv_obj_align(reset_title, LV_ALIGN_TOP_LEFT, 55, 10);
  
  lv_obj_t *reset_desc = lv_label_create(reset_card);
  lv_label_set_text(reset_desc, "Alle Einstellungen loeschen");
  lv_obj_set_style_text_font(reset_desc, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(reset_desc, SETTINGS_TEXT_DIM, 0);
  lv_obj_align(reset_desc, LV_ALIGN_BOTTOM_LEFT, 55, -10);
  
  // Reset Button
  lv_obj_t *reset_btn = lv_btn_create(reset_card);
  lv_obj_set_size(reset_btn, 60, 60);
  lv_obj_align(reset_btn, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_set_style_bg_color(reset_btn, SETTINGS_ERROR, 0);
  lv_obj_set_style_radius(reset_btn, 10, 0);
  lv_obj_add_event_cb(reset_btn, settings_reset_btn_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *reset_arrow = lv_label_create(reset_btn);
  lv_label_set_text(reset_arrow, LV_SYMBOL_TRASH);
  lv_obj_set_style_text_font(reset_arrow, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(reset_arrow, SETTINGS_TEXT, 0);
  lv_obj_center(reset_arrow);
  
  // ========== Version Info ==========
  lv_obj_t *version_label = lv_label_create(content);
  char version_text[64];
  snprintf(version_text, sizeof(version_text), "%s %s", APP_NAME, BUILD_VERSION);
  lv_label_set_text(version_label, version_text);
  lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(version_label, SETTINGS_TEXT_DIM, 0);
}

// ============================================================
// Show Settings Screen
// ============================================================
void showSettingsScreen() {
  if (settings_screen == NULL) {
    createSettingsScreen();
  } else {
    // Refresh screen (recreate to update status)
    createSettingsScreen();
  }
  
  lv_scr_load_anim(settings_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

#endif // SETTINGS_UI_H
