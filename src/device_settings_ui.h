/**
 * @file device_settings_ui.h
 * @brief Device Settings UI for ESP32-4848S040 (Large Display)
 * 
 * Allows configuration of Red Sea, Tunze, and Tasmota settings
 * directly on the touch display (480x480).
 * 
 * Only available on BOARD_ESP32_4848S040 (large display)
 */

#ifndef DEVICE_SETTINGS_UI_H
#define DEVICE_SETTINGS_UI_H

#ifdef BOARD_ESP32_4848S040

#include <Arduino.h>
#include <lvgl.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "board_config.h"
#include "credentials.h"

// External references
extern String redsea_USERNAME;
extern String redsea_PASSWORD;
extern String redsea_AQUARIUM_ID;
extern String redsea_AQUARIUM_NAME;
extern String TUNZE_USERNAME;
extern String TUNZE_PASSWORD;
extern String TUNZE_DEVICE_ID;
extern String TUNZE_DEVICE_NAME;
extern bool ENABLE_redsea;
extern bool ENABLE_TUNZE;
extern void saveCredentials();

// API functions from redsea_api.h and tunze_api.h
extern String redseaGetAquariums();
extern String tunzeGetDevices();

// Tasmota functions
bool tasmotaIsEnabled();
void tasmotaSetEnabled(bool enabled);
int tasmotaGetPulseTime();
void tasmotaSetPulseTime(int seconds);
void tasmotaSaveConfig();

// Tasmota scan functions
extern String tasmotaStartScan();
extern String tasmotaGetScanResults();
extern void tasmotaAddDevice(const String& ip, const String& name, bool enabled, bool turnOn);
extern void tasmotaRemoveDevice(const String& ip);
extern String tasmotaGetDevicesJson();

// ============================================================
// Colors
// ============================================================
#define DS_BG             lv_color_hex(0x1a1a2e)
#define DS_CARD           lv_color_hex(0x16213e)
#define DS_HEADER         lv_color_hex(0x0f3460)
#define DS_ACCENT         lv_color_hex(0x2196F3)
#define DS_SUCCESS        lv_color_hex(0x00ff87)
#define DS_ERROR          lv_color_hex(0xff6b6b)
#define DS_WARNING        lv_color_hex(0xffa502)
#define DS_TEXT           lv_color_hex(0xffffff)
#define DS_TEXT_DIM       lv_color_hex(0xb8c4d8)  // Brighter for better readability
#define DS_REDSEA         lv_color_hex(0xe94560)
#define DS_TUNZE          lv_color_hex(0x00d9ff)
#define DS_TASMOTA        lv_color_hex(0xffa502)
#define DS_INPUT_BG       lv_color_hex(0x0a1628)

// ============================================================
// State
// ============================================================
static lv_obj_t *ds_screen = NULL;
static lv_obj_t *ds_keyboard = NULL;
static lv_obj_t *ds_current_textarea = NULL;
static int ds_current_service = 0;  // 0=RedSea, 1=Tunze, 2=Tasmota

// Input fields
static lv_obj_t *ds_username_ta = NULL;
static lv_obj_t *ds_password_ta = NULL;
static lv_obj_t *ds_device_ta = NULL;
static lv_obj_t *ds_enable_switch = NULL;

// Tasmota specific
static lv_obj_t *ds_tasmota_pulse_ta = NULL;
static lv_obj_t *ds_tasmota_scan_btn = NULL;
static lv_obj_t *ds_tasmota_device_list = NULL;
static lv_obj_t *ds_tasmota_scan_progress = NULL;
static bool ds_tasmota_scanning = false;

// Device selection
static lv_obj_t *ds_device_dropdown = NULL;
static lv_obj_t *ds_load_btn = NULL;
static lv_obj_t *ds_loading_spinner = NULL;
static lv_obj_t *ds_device_label = NULL;

// Storage for loaded devices (max 10 devices)
#define DS_MAX_DEVICES 10
static String ds_device_ids[DS_MAX_DEVICES];
static String ds_device_names[DS_MAX_DEVICES];
static int ds_device_count = 0;
static bool ds_loading = false;

// Forward declarations
lv_obj_t* getMainScreen();
void ds_show_service_settings(int service);  // Public function
static void ds_save_current_settings();
static void ds_load_devices_task(lv_timer_t *timer);

// ============================================================
// Keyboard Event Handler
// ============================================================
static void ds_keyboard_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *kb = lv_event_get_target(e);
  
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    // Hide keyboard
    lv_obj_add_flag(ds_keyboard, LV_OBJ_FLAG_HIDDEN);
    ds_current_textarea = NULL;
  }
}

// ============================================================
// Text Area Focus Handler
// ============================================================
static void ds_textarea_focus_cb(lv_event_t *e) {
  lv_obj_t *ta = lv_event_get_target(e);
  if (ds_keyboard != NULL) {
    ds_current_textarea = ta;
    lv_keyboard_set_textarea(ds_keyboard, ta);
    lv_obj_clear_flag(ds_keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Scroll textarea into view
    lv_obj_scroll_to_view(ta, LV_ANIM_ON);
  }
}

// ============================================================
// Back Button Handler
// ============================================================
static void ds_back_btn_cb(lv_event_t *e) {
  lv_obj_t *main_scr = getMainScreen();
  if (main_scr != NULL) {
    lv_scr_load_anim(main_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
  }
}

// ============================================================
// Service Tab Handler
// ============================================================
static void ds_tab_cb(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_target(e);
  int service = (int)(intptr_t)lv_event_get_user_data(e);
  ds_show_service_settings(service);
}

// ============================================================
// Save Button Handler
// ============================================================
static void ds_save_btn_cb(lv_event_t *e) {
  ds_save_current_settings();
  
  // Show success message
  lv_obj_t *mbox = lv_msgbox_create(NULL, LV_SYMBOL_OK " Gespeichert", 
    "Einstellungen wurden\nerfolgreich gespeichert!", NULL, true);
  lv_obj_set_style_bg_color(mbox, DS_CARD, 0);
  lv_obj_set_style_text_color(mbox, DS_TEXT, 0);
  lv_obj_center(mbox);
  
  // Auto-close after 2 seconds
  lv_timer_create([](lv_timer_t *timer) {
    lv_obj_t *mbox = (lv_obj_t*)timer->user_data;
    if (mbox != NULL && lv_obj_is_valid(mbox)) {
      lv_msgbox_close(mbox);
    }
    lv_timer_del(timer);
  }, 2000, mbox);
}

// ============================================================
// Enable Switch Handler
// ============================================================
static void ds_enable_switch_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  
  if (ds_current_service == 0) {
    ENABLE_redsea = enabled;
  } else if (ds_current_service == 1) {
    ENABLE_TUNZE = enabled;
  } else if (ds_current_service == 2) {
    tasmotaSetEnabled(enabled);
  }
}

// ============================================================
// Create Input Field
// ============================================================
static lv_obj_t* ds_create_input(lv_obj_t *parent, const char *label_text, 
                                  const char *placeholder, bool is_password = false) {
  // Container
  lv_obj_t *cont = lv_obj_create(parent);
  lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(cont, 0, 0);
  lv_obj_set_style_pad_all(cont, 5, 0);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  
  // Label
  lv_obj_t *lbl = lv_label_create(cont);
  lv_label_set_text(lbl, label_text);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl, DS_TEXT, 0);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
  
  // Text Area
  lv_obj_t *ta = lv_textarea_create(cont);
  lv_obj_set_size(ta, LV_PCT(100), 45);
  lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 0, 25);
  lv_obj_set_style_bg_color(ta, DS_INPUT_BG, 0);
  lv_obj_set_style_border_color(ta, DS_ACCENT, 0);
  lv_obj_set_style_border_width(ta, 2, 0);
  lv_obj_set_style_radius(ta, 8, 0);
  lv_obj_set_style_text_color(ta, DS_TEXT, 0);
  lv_obj_set_style_text_font(ta, &lv_font_montserrat_16, 0);
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_textarea_set_one_line(ta, true);
  
  if (is_password) {
    lv_textarea_set_password_mode(ta, true);
  }
  
  // Focus event to show keyboard
  lv_obj_add_event_cb(ta, ds_textarea_focus_cb, LV_EVENT_FOCUSED, NULL);
  
  return ta;
}

// ============================================================
// Save Current Settings
// ============================================================
static void ds_save_current_settings() {
  if (ds_current_service == 0) {
    // Red Sea
    if (ds_username_ta) redsea_USERNAME = String(lv_textarea_get_text(ds_username_ta));
    // Only update password if user entered a new one
    if (ds_password_ta) {
      String newPass = String(lv_textarea_get_text(ds_password_ta));
      if (newPass.length() > 0) {
        redsea_PASSWORD = newPass;
      }
    }
    ENABLE_redsea = lv_obj_has_state(ds_enable_switch, LV_STATE_CHECKED);
    saveCredentials();
    Serial.println("Red Sea settings saved from display");
  } 
  else if (ds_current_service == 1) {
    // Tunze
    if (ds_username_ta) TUNZE_USERNAME = String(lv_textarea_get_text(ds_username_ta));
    // Only update password if user entered a new one
    if (ds_password_ta) {
      String newPass = String(lv_textarea_get_text(ds_password_ta));
      if (newPass.length() > 0) {
        TUNZE_PASSWORD = newPass;
      }
    }
    ENABLE_TUNZE = lv_obj_has_state(ds_enable_switch, LV_STATE_CHECKED);
    saveCredentials();
    Serial.println("Tunze settings saved from display");
  }
  else if (ds_current_service == 2) {
    // Tasmota
    bool enabled = lv_obj_has_state(ds_enable_switch, LV_STATE_CHECKED);
    tasmotaSetEnabled(enabled);
    
    if (ds_tasmota_pulse_ta) {
      int pulseMinutes = atoi(lv_textarea_get_text(ds_tasmota_pulse_ta));
      tasmotaSetPulseTime(pulseMinutes * 60);  // Convert to seconds
    }
    tasmotaSaveConfig();
    Serial.println("Tasmota settings saved from display");
  }
}

// ============================================================
// Device Dropdown Selection Handler
// ============================================================
static void ds_dropdown_cb(lv_event_t *e) {
  lv_obj_t *dropdown = lv_event_get_target(e);
  uint16_t sel = lv_dropdown_get_selected(dropdown);
  
  if (sel > 0 && sel <= ds_device_count) {
    int idx = sel - 1;  // First entry is "-- Auswählen --"
    
    if (ds_current_service == 0) {
      // Red Sea
      redsea_AQUARIUM_ID = ds_device_ids[idx];
      redsea_AQUARIUM_NAME = ds_device_names[idx];
      Serial.printf("Selected aquarium: %s (ID: %s)\n", redsea_AQUARIUM_NAME.c_str(), redsea_AQUARIUM_ID.c_str());
      
      // Update label
      if (ds_device_label) {
        lv_label_set_text(ds_device_label, redsea_AQUARIUM_NAME.c_str());
        lv_obj_set_style_text_color(ds_device_label, DS_REDSEA, 0);
      }
    } else if (ds_current_service == 1) {
      // Tunze
      TUNZE_DEVICE_ID = ds_device_ids[idx];
      TUNZE_DEVICE_NAME = ds_device_names[idx];
      Serial.printf("Selected device: %s (ID: %s)\n", TUNZE_DEVICE_NAME.c_str(), TUNZE_DEVICE_ID.c_str());
      
      // Update label
      if (ds_device_label) {
        lv_label_set_text(ds_device_label, TUNZE_DEVICE_NAME.c_str());
        lv_obj_set_style_text_color(ds_device_label, DS_TUNZE, 0);
      }
    }
  }
}

// ============================================================
// Load Devices (Async Task)
// ============================================================
static void ds_load_devices_task(lv_timer_t *timer) {
  int service = (int)(intptr_t)timer->user_data;
  
  Serial.printf("Loading devices for service %d...\n", service);
  
  String result;
  if (service == 0) {
    // Update username, only update password if user entered a new one
    if (ds_username_ta) redsea_USERNAME = String(lv_textarea_get_text(ds_username_ta));
    if (ds_password_ta) {
      String newPass = String(lv_textarea_get_text(ds_password_ta));
      if (newPass.length() > 0) redsea_PASSWORD = newPass;
    }
    result = redseaGetAquariums();
  } else {
    if (ds_username_ta) TUNZE_USERNAME = String(lv_textarea_get_text(ds_username_ta));
    if (ds_password_ta) {
      String newPass = String(lv_textarea_get_text(ds_password_ta));
      if (newPass.length() > 0) TUNZE_PASSWORD = newPass;
    }
    result = tunzeGetDevices();
  }
  
  Serial.println("API Result: " + result.substring(0, 200));
  
  // Parse result
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, result);
  
  ds_device_count = 0;
  String dropdown_options = "-- Auswaehlen --\n";
  
  if (!error && doc["success"].as<bool>()) {
    JsonArray items;
    if (service == 0) {
      items = doc["aquariums"].as<JsonArray>();
    } else {
      items = doc["devices"].as<JsonArray>();
    }
    
    for (JsonObject item : items) {
      if (ds_device_count >= DS_MAX_DEVICES) break;
      
      if (service == 0) {
        ds_device_ids[ds_device_count] = item["id"].as<String>();
        ds_device_names[ds_device_count] = item["name"].as<String>();
      } else {
        ds_device_ids[ds_device_count] = item["imei"].as<String>();
        ds_device_names[ds_device_count] = item["name"].as<String>();
      }
      
      dropdown_options += ds_device_names[ds_device_count];
      ds_device_count++;
      
      if (ds_device_count < DS_MAX_DEVICES) {
        dropdown_options += "\n";
      }
    }
    
    Serial.printf("Found %d devices\n", ds_device_count);
  } else {
    Serial.println("Failed to parse API result or login failed");
  }
  
  // Update UI (must be done carefully - widgets might be deleted)
  if (ds_device_dropdown != NULL && lv_obj_is_valid(ds_device_dropdown)) {
    lv_dropdown_set_options(ds_device_dropdown, dropdown_options.c_str());
    
    // Select current device if exists
    String currentId = (service == 0) ? redsea_AQUARIUM_ID : TUNZE_DEVICE_ID;
    for (int i = 0; i < ds_device_count; i++) {
      if (ds_device_ids[i] == currentId) {
        lv_dropdown_set_selected(ds_device_dropdown, i + 1);
        break;
      }
    }
  }
  
  // Hide spinner, show button
  if (ds_loading_spinner != NULL && lv_obj_is_valid(ds_loading_spinner)) {
    lv_obj_add_flag(ds_loading_spinner, LV_OBJ_FLAG_HIDDEN);
  }
  if (ds_load_btn != NULL && lv_obj_is_valid(ds_load_btn)) {
    lv_obj_clear_flag(ds_load_btn, LV_OBJ_FLAG_HIDDEN);
    
    if (ds_device_count > 0) {
      lv_obj_t *btn_lbl = lv_obj_get_child(ds_load_btn, 0);
      if (btn_lbl) {
        char txt[32];
        snprintf(txt, sizeof(txt), LV_SYMBOL_OK " %d gefunden", ds_device_count);
        lv_label_set_text(btn_lbl, txt);
      }
    } else {
      lv_obj_t *btn_lbl = lv_obj_get_child(ds_load_btn, 0);
      if (btn_lbl) {
        lv_label_set_text(btn_lbl, LV_SYMBOL_CLOSE " Fehler");
      }
    }
  }
  
  ds_loading = false;
  lv_timer_del(timer);
}

// ============================================================
// Load Button Handler
// ============================================================
static void ds_load_btn_cb(lv_event_t *e) {
  if (ds_loading) return;
  ds_loading = true;
  
  // Show spinner, hide button text
  if (ds_load_btn) {
    lv_obj_t *btn_lbl = lv_obj_get_child(ds_load_btn, 0);
    if (btn_lbl) {
      lv_label_set_text(btn_lbl, "Laden...");
    }
  }
  
  // Start async task (small delay to let UI update)
  lv_timer_create(ds_load_devices_task, 100, (void*)(intptr_t)ds_current_service);
}

// ============================================================
// Show Service Settings Content (Public function)
// ============================================================
static lv_obj_t *ds_content_area = NULL;
static lv_obj_t *ds_tab_redsea = NULL;
static lv_obj_t *ds_tab_tunze = NULL;
static lv_obj_t *ds_tab_tasmota = NULL;

static void ds_update_tab_styles() {
  // Reset all tabs
  lv_obj_set_style_bg_color(ds_tab_redsea, DS_CARD, 0);
  lv_obj_set_style_bg_color(ds_tab_tunze, DS_CARD, 0);
  lv_obj_set_style_bg_color(ds_tab_tasmota, DS_CARD, 0);
  
  // Highlight active tab
  if (ds_current_service == 0) {
    lv_obj_set_style_bg_color(ds_tab_redsea, DS_REDSEA, 0);
  } else if (ds_current_service == 1) {
    lv_obj_set_style_bg_color(ds_tab_tunze, DS_TUNZE, 0);
  } else {
    lv_obj_set_style_bg_color(ds_tab_tasmota, DS_TASMOTA, 0);
  }
}

void ds_show_service_settings(int service) {
  ds_current_service = service;
  ds_update_tab_styles();
  
  // Clear content area
  lv_obj_clean(ds_content_area);
  
  // Reset textarea pointers
  ds_username_ta = NULL;
  ds_password_ta = NULL;
  ds_device_ta = NULL;
  ds_enable_switch = NULL;
  ds_tasmota_pulse_ta = NULL;
  
  // Reset device selection pointers
  ds_device_dropdown = NULL;
  ds_load_btn = NULL;
  ds_loading_spinner = NULL;
  ds_device_label = NULL;
  ds_loading = false;
  
  // Reset Tasmota pointers
  ds_tasmota_scan_btn = NULL;
  ds_tasmota_device_list = NULL;
  ds_tasmota_scan_progress = NULL;
  ds_tasmota_scanning = false;
  
  // Create scrollable container for content
  lv_obj_t *scroll_cont = lv_obj_create(ds_content_area);
  lv_obj_set_size(scroll_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(scroll_cont, 0, 0);
  lv_obj_set_style_pad_all(scroll_cont, 10, 0);
  lv_obj_set_flex_flow(scroll_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(scroll_cont, 10, 0);
  
  if (service == 0) {
    // ============ RED SEA ============
    // Enable Switch
    lv_obj_t *switch_cont = lv_obj_create(scroll_cont);
    lv_obj_set_size(switch_cont, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(switch_cont, DS_CARD, 0);
    lv_obj_set_style_radius(switch_cont, 10, 0);
    lv_obj_set_style_border_width(switch_cont, 0, 0);
    lv_obj_clear_flag(switch_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *switch_lbl = lv_label_create(switch_cont);
    lv_label_set_text(switch_lbl, "Red Sea aktivieren");
    lv_obj_set_style_text_font(switch_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(switch_lbl, DS_TEXT, 0);
    lv_obj_align(switch_lbl, LV_ALIGN_LEFT_MID, 15, 0);
    
    ds_enable_switch = lv_switch_create(switch_cont);
    lv_obj_align(ds_enable_switch, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_style_bg_color(ds_enable_switch, DS_SUCCESS, (lv_style_selector_t)LV_PART_INDICATOR | (lv_style_selector_t)LV_STATE_CHECKED);
    if (ENABLE_redsea) lv_obj_add_state(ds_enable_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(ds_enable_switch, ds_enable_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Username
    ds_username_ta = ds_create_input(scroll_cont, "E-Mail / Benutzername", "E-Mail eingeben...");
    if (redsea_USERNAME.length() > 0) {
      lv_textarea_set_text(ds_username_ta, redsea_USERNAME.c_str());
    }
    
    // Password
    ds_password_ta = ds_create_input(scroll_cont, "Passwort", "Passwort eingeben...", true);
    if (redsea_PASSWORD.length() > 0) {
      // Don't show actual password - just indicate one exists
      lv_textarea_set_placeholder_text(ds_password_ta, "••••••••  (gespeichert)");
    }
    
    // Device Selection Section
    // Load button + dropdown container
    lv_obj_t *dev_sel_cont = lv_obj_create(scroll_cont);
    lv_obj_set_size(dev_sel_cont, LV_PCT(100), 130);
    lv_obj_set_style_bg_color(dev_sel_cont, DS_CARD, 0);
    lv_obj_set_style_radius(dev_sel_cont, 10, 0);
    lv_obj_set_style_border_width(dev_sel_cont, 0, 0);
    lv_obj_clear_flag(dev_sel_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(dev_sel_cont, 10, 0);
    
    lv_obj_t *sel_lbl = lv_label_create(dev_sel_cont);
    lv_label_set_text(sel_lbl, "Aquarium auswaehlen:");
    lv_obj_set_style_text_font(sel_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sel_lbl, DS_TEXT_DIM, 0);
    lv_obj_align(sel_lbl, LV_ALIGN_TOP_LEFT, 5, 0);
    
    // Load Button
    ds_load_btn = lv_btn_create(dev_sel_cont);
    lv_obj_set_size(ds_load_btn, 140, 40);
    lv_obj_align(ds_load_btn, LV_ALIGN_TOP_RIGHT, -5, -5);
    lv_obj_set_style_bg_color(ds_load_btn, DS_REDSEA, 0);
    lv_obj_set_style_radius(ds_load_btn, 8, 0);
    lv_obj_add_event_cb(ds_load_btn, ds_load_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_lbl = lv_label_create(ds_load_btn);
    lv_label_set_text(btn_lbl, LV_SYMBOL_DOWNLOAD " Laden");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_lbl);
    
    // Dropdown for device selection
    ds_device_dropdown = lv_dropdown_create(dev_sel_cont);
    lv_obj_set_size(ds_device_dropdown, LV_PCT(100) - 10, 40);
    lv_obj_align(ds_device_dropdown, LV_ALIGN_TOP_LEFT, 0, 35);
    lv_dropdown_set_options(ds_device_dropdown, "-- Auswaehlen --");
    lv_obj_set_style_bg_color(ds_device_dropdown, DS_BG, 0);
    lv_obj_set_style_text_color(ds_device_dropdown, DS_TEXT, 0);
    lv_obj_set_style_text_font(ds_device_dropdown, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(ds_device_dropdown, DS_REDSEA, 0);
    lv_obj_set_style_border_width(ds_device_dropdown, 1, 0);
    lv_obj_set_style_radius(ds_device_dropdown, 8, 0);
    lv_dropdown_set_dir(ds_device_dropdown, LV_DIR_BOTTOM);
    lv_obj_add_event_cb(ds_device_dropdown, ds_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Selected device label (below dropdown)
    ds_device_label = lv_label_create(dev_sel_cont);
    lv_obj_align(ds_device_label, LV_ALIGN_BOTTOM_LEFT, 5, 0);
    lv_obj_set_style_text_font(ds_device_label, &lv_font_montserrat_16, 0);
    if (redsea_AQUARIUM_NAME.length() > 0) {
      lv_label_set_text(ds_device_label, redsea_AQUARIUM_NAME.c_str());
      lv_obj_set_style_text_color(ds_device_label, DS_REDSEA, 0);
    } else {
      lv_label_set_text(ds_device_label, "Nicht konfiguriert");
      lv_obj_set_style_text_color(ds_device_label, DS_TEXT_DIM, 0);
    }
    
  } else if (service == 1) {
    // ============ TUNZE ============
    // Enable Switch
    lv_obj_t *switch_cont = lv_obj_create(scroll_cont);
    lv_obj_set_size(switch_cont, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(switch_cont, DS_CARD, 0);
    lv_obj_set_style_radius(switch_cont, 10, 0);
    lv_obj_set_style_border_width(switch_cont, 0, 0);
    lv_obj_clear_flag(switch_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *switch_lbl = lv_label_create(switch_cont);
    lv_label_set_text(switch_lbl, "Tunze Hub aktivieren");
    lv_obj_set_style_text_font(switch_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(switch_lbl, DS_TEXT, 0);
    lv_obj_align(switch_lbl, LV_ALIGN_LEFT_MID, 15, 0);
    
    ds_enable_switch = lv_switch_create(switch_cont);
    lv_obj_align(ds_enable_switch, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_style_bg_color(ds_enable_switch, DS_SUCCESS, (lv_style_selector_t)LV_PART_INDICATOR | (lv_style_selector_t)LV_STATE_CHECKED);
    if (ENABLE_TUNZE) lv_obj_add_state(ds_enable_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(ds_enable_switch, ds_enable_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Username
    ds_username_ta = ds_create_input(scroll_cont, "E-Mail / Benutzername", "E-Mail eingeben...");
    if (TUNZE_USERNAME.length() > 0) {
      lv_textarea_set_text(ds_username_ta, TUNZE_USERNAME.c_str());
    }
    
    // Password
    ds_password_ta = ds_create_input(scroll_cont, "Passwort", "Passwort eingeben...", true);
    if (TUNZE_PASSWORD.length() > 0) {
      // Don't show actual password - just indicate one exists
      lv_textarea_set_placeholder_text(ds_password_ta, "••••••••  (gespeichert)");
    }
    
    // Device Selection Section
    // Load button + dropdown container
    lv_obj_t *dev_sel_cont = lv_obj_create(scroll_cont);
    lv_obj_set_size(dev_sel_cont, LV_PCT(100), 130);
    lv_obj_set_style_bg_color(dev_sel_cont, DS_CARD, 0);
    lv_obj_set_style_radius(dev_sel_cont, 10, 0);
    lv_obj_set_style_border_width(dev_sel_cont, 0, 0);
    lv_obj_clear_flag(dev_sel_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(dev_sel_cont, 10, 0);
    
    lv_obj_t *sel_lbl = lv_label_create(dev_sel_cont);
    lv_label_set_text(sel_lbl, "Tunze Device auswaehlen:");
    lv_obj_set_style_text_font(sel_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sel_lbl, DS_TEXT_DIM, 0);
    lv_obj_align(sel_lbl, LV_ALIGN_TOP_LEFT, 5, 0);
    
    // Load Button
    ds_load_btn = lv_btn_create(dev_sel_cont);
    lv_obj_set_size(ds_load_btn, 140, 40);
    lv_obj_align(ds_load_btn, LV_ALIGN_TOP_RIGHT, -5, -5);
    lv_obj_set_style_bg_color(ds_load_btn, DS_TUNZE, 0);
    lv_obj_set_style_radius(ds_load_btn, 8, 0);
    lv_obj_add_event_cb(ds_load_btn, ds_load_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_lbl = lv_label_create(ds_load_btn);
    lv_label_set_text(btn_lbl, LV_SYMBOL_DOWNLOAD " Laden");
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_lbl);
    
    // Dropdown for device selection
    ds_device_dropdown = lv_dropdown_create(dev_sel_cont);
    lv_obj_set_size(ds_device_dropdown, LV_PCT(100) - 10, 40);
    lv_obj_align(ds_device_dropdown, LV_ALIGN_TOP_LEFT, 0, 35);
    lv_dropdown_set_options(ds_device_dropdown, "-- Auswaehlen --");
    lv_obj_set_style_bg_color(ds_device_dropdown, DS_BG, 0);
    lv_obj_set_style_text_color(ds_device_dropdown, DS_TEXT, 0);
    lv_obj_set_style_text_font(ds_device_dropdown, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(ds_device_dropdown, DS_TUNZE, 0);
    lv_obj_set_style_border_width(ds_device_dropdown, 1, 0);
    lv_obj_set_style_radius(ds_device_dropdown, 8, 0);
    lv_dropdown_set_dir(ds_device_dropdown, LV_DIR_BOTTOM);
    lv_obj_add_event_cb(ds_device_dropdown, ds_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Selected device label (below dropdown)
    ds_device_label = lv_label_create(dev_sel_cont);
    lv_obj_align(ds_device_label, LV_ALIGN_BOTTOM_LEFT, 5, 0);
    lv_obj_set_style_text_font(ds_device_label, &lv_font_montserrat_16, 0);
    if (TUNZE_DEVICE_NAME.length() > 0) {
      lv_label_set_text(ds_device_label, TUNZE_DEVICE_NAME.c_str());
      lv_obj_set_style_text_color(ds_device_label, DS_TUNZE, 0);
    } else {
      lv_label_set_text(ds_device_label, "Nicht konfiguriert");
      lv_obj_set_style_text_color(ds_device_label, DS_TEXT_DIM, 0);
    }
    
  } else {
    // ============ TASMOTA ============
    bool tasmotaEnabled = tasmotaIsEnabled();
    int pulseTime = tasmotaGetPulseTime();
    
    // Enable Switch
    lv_obj_t *switch_cont = lv_obj_create(scroll_cont);
    lv_obj_set_size(switch_cont, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(switch_cont, DS_CARD, 0);
    lv_obj_set_style_radius(switch_cont, 10, 0);
    lv_obj_set_style_border_width(switch_cont, 0, 0);
    lv_obj_clear_flag(switch_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *switch_lbl = lv_label_create(switch_cont);
    lv_label_set_text(switch_lbl, "Tasmota aktivieren");
    lv_obj_set_style_text_font(switch_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(switch_lbl, DS_TEXT, 0);
    lv_obj_align(switch_lbl, LV_ALIGN_LEFT_MID, 15, 0);
    
    ds_enable_switch = lv_switch_create(switch_cont);
    lv_obj_align(ds_enable_switch, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_style_bg_color(ds_enable_switch, DS_SUCCESS, (lv_style_selector_t)LV_PART_INDICATOR | (lv_style_selector_t)LV_STATE_CHECKED);
    if (tasmotaEnabled) lv_obj_add_state(ds_enable_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(ds_enable_switch, ds_enable_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Pulse Time Input
    lv_obj_t *pulse_cont = lv_obj_create(scroll_cont);
    lv_obj_set_size(pulse_cont, LV_PCT(100), 80);
    lv_obj_set_style_bg_opa(pulse_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pulse_cont, 0, 0);
    lv_obj_set_style_pad_all(pulse_cont, 5, 0);
    lv_obj_clear_flag(pulse_cont, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *pulse_lbl = lv_label_create(pulse_cont);
    lv_label_set_text(pulse_lbl, "Auto-Einschalten nach (Min.)");
    lv_obj_set_style_text_font(pulse_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(pulse_lbl, DS_TEXT, 0);
    lv_obj_align(pulse_lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    
    ds_tasmota_pulse_ta = lv_textarea_create(pulse_cont);
    lv_obj_set_size(ds_tasmota_pulse_ta, LV_PCT(100), 45);
    lv_obj_align(ds_tasmota_pulse_ta, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_obj_set_style_bg_color(ds_tasmota_pulse_ta, DS_INPUT_BG, 0);
    lv_obj_set_style_border_color(ds_tasmota_pulse_ta, DS_ACCENT, 0);
    lv_obj_set_style_border_width(ds_tasmota_pulse_ta, 2, 0);
    lv_obj_set_style_radius(ds_tasmota_pulse_ta, 8, 0);
    lv_obj_set_style_text_color(ds_tasmota_pulse_ta, DS_TEXT, 0);
    lv_obj_set_style_text_font(ds_tasmota_pulse_ta, &lv_font_montserrat_16, 0);
    lv_textarea_set_accepted_chars(ds_tasmota_pulse_ta, "0123456789");
    lv_textarea_set_one_line(ds_tasmota_pulse_ta, true);
    lv_textarea_set_max_length(ds_tasmota_pulse_ta, 3);
    
    char pulse_str[8];
    snprintf(pulse_str, sizeof(pulse_str), "%d", pulseTime / 60);
    lv_textarea_set_text(ds_tasmota_pulse_ta, pulse_str);
    
    lv_obj_add_event_cb(ds_tasmota_pulse_ta, ds_textarea_focus_cb, LV_EVENT_FOCUSED, NULL);
    
    // Info Card
    lv_obj_t *info_card = lv_obj_create(scroll_cont);
    lv_obj_set_size(info_card, LV_PCT(100), 70);
    lv_obj_set_style_bg_color(info_card, DS_CARD, 0);
    lv_obj_set_style_radius(info_card, 10, 0);
    lv_obj_set_style_border_width(info_card, 0, 0);
    lv_obj_set_style_pad_all(info_card, 15, 0);
    lv_obj_clear_flag(info_card, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *info_title = lv_label_create(info_card);
    lv_label_set_text(info_title, LV_SYMBOL_POWER " Tasmota Steckdosen");
    lv_obj_set_style_text_font(info_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(info_title, DS_TASMOTA, 0);
    lv_obj_align(info_title, LV_ALIGN_TOP_LEFT, 0, 0);
    
    lv_obj_t *info_text = lv_label_create(info_card);
    lv_label_set_text(info_text, "Geraete werden im Fuetterungsmodus\nautomatisch aus- und eingeschaltet.");
    lv_obj_set_style_text_font(info_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_text, DS_TEXT_DIM, 0);
    lv_obj_align(info_text, LV_ALIGN_TOP_LEFT, 0, 22);
    
    // Scan Button
    ds_tasmota_scan_btn = lv_btn_create(scroll_cont);
    lv_obj_set_size(ds_tasmota_scan_btn, LV_PCT(100), 45);
    lv_obj_set_style_bg_color(ds_tasmota_scan_btn, DS_TASMOTA, 0);
    lv_obj_set_style_radius(ds_tasmota_scan_btn, 8, 0);
    lv_obj_add_event_cb(ds_tasmota_scan_btn, [](lv_event_t *e) {
      if (ds_tasmota_scanning) return;
      ds_tasmota_scanning = true;
      
      // Update button text
      lv_obj_t *btn_lbl = lv_obj_get_child(ds_tasmota_scan_btn, 0);
      if (btn_lbl) lv_label_set_text(btn_lbl, "Scanne...");
      
      // Start scan
      tasmotaStartScan();
      
      // Create timer to poll results
      lv_timer_create([](lv_timer_t *timer) {
        String result = tasmotaGetScanResults();
        JsonDocument doc;
        deserializeJson(doc, result);
        
        bool scanning = doc["scanning"].as<bool>();
        int progress = doc["progress"].as<int>();
        int found = doc["found"].as<int>();
        
        // Update button text with progress
        if (ds_tasmota_scan_btn && lv_obj_is_valid(ds_tasmota_scan_btn)) {
          lv_obj_t *btn_lbl = lv_obj_get_child(ds_tasmota_scan_btn, 0);
          if (btn_lbl) {
            char txt[40];
            if (scanning) {
              snprintf(txt, sizeof(txt), "Scanne %d/254... (%d)", progress, found);
            } else {
              snprintf(txt, sizeof(txt), LV_SYMBOL_OK " %d Geraete gefunden", found);
            }
            lv_label_set_text(btn_lbl, txt);
          }
        }
        
        // If scan complete, update device list
        if (!scanning) {
          ds_tasmota_scanning = false;
          
          // Clear and rebuild device list
          if (ds_tasmota_device_list && lv_obj_is_valid(ds_tasmota_device_list)) {
            lv_obj_clean(ds_tasmota_device_list);
            
            JsonArray devices = doc["devices"].as<JsonArray>();
            for (JsonObject dev : devices) {
              String ip = dev["ip"].as<String>();
              String name = dev["name"].as<String>();
              
              // Create device item
              lv_obj_t *item = lv_obj_create(ds_tasmota_device_list);
              lv_obj_set_size(item, LV_PCT(100), 50);
              lv_obj_set_style_bg_color(item, DS_CARD, 0);
              lv_obj_set_style_radius(item, 8, 0);
              lv_obj_set_style_border_width(item, 0, 0);
              lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
              
              lv_obj_t *name_lbl = lv_label_create(item);
              lv_label_set_text(name_lbl, name.c_str());
              lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
              lv_obj_set_style_text_color(name_lbl, DS_TEXT, 0);
              lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 10, -8);
              
              lv_obj_t *ip_lbl = lv_label_create(item);
              lv_label_set_text(ip_lbl, ip.c_str());
              lv_obj_set_style_text_font(ip_lbl, &lv_font_montserrat_12, 0);
              lv_obj_set_style_text_color(ip_lbl, DS_TEXT_DIM, 0);
              lv_obj_align(ip_lbl, LV_ALIGN_LEFT_MID, 10, 10);
              
              // Add button
              lv_obj_t *add_btn = lv_btn_create(item);
              lv_obj_set_size(add_btn, 70, 35);
              lv_obj_align(add_btn, LV_ALIGN_RIGHT_MID, -5, 0);
              lv_obj_set_style_bg_color(add_btn, DS_SUCCESS, 0);
              lv_obj_set_style_radius(add_btn, 6, 0);
              
              // Store IP in button user data
              String *ipPtr = new String(ip);
              String *namePtr = new String(name);
              lv_obj_set_user_data(add_btn, ipPtr);
              
              lv_obj_t *add_lbl = lv_label_create(add_btn);
              lv_label_set_text(add_lbl, LV_SYMBOL_PLUS);
              lv_obj_set_style_text_color(add_lbl, lv_color_hex(0x1a1a2e), 0);
              lv_obj_center(add_lbl);
              
              lv_obj_add_event_cb(add_btn, [](lv_event_t *e) {
                lv_obj_t *btn = lv_event_get_target(e);
                String *ip = (String*)lv_obj_get_user_data(btn);
                if (ip) {
                  tasmotaAddDevice(*ip, *ip, true, true);
                  tasmotaSaveConfig();
                  Serial.printf("Added Tasmota device: %s\n", ip->c_str());
                  
                  // Show feedback
                  lv_obj_t *lbl = lv_obj_get_child(btn, 0);
                  if (lbl) lv_label_set_text(lbl, LV_SYMBOL_OK);
                  lv_obj_set_style_bg_color(btn, DS_TEXT_DIM, 0);
                }
              }, LV_EVENT_CLICKED, NULL);
            }
          }
          
          // Reset button text after delay
          lv_timer_create([](lv_timer_t *t) {
            if (ds_tasmota_scan_btn && lv_obj_is_valid(ds_tasmota_scan_btn)) {
              lv_obj_t *btn_lbl = lv_obj_get_child(ds_tasmota_scan_btn, 0);
              if (btn_lbl) lv_label_set_text(btn_lbl, LV_SYMBOL_REFRESH " Netzwerk scannen");
            }
            lv_timer_del(t);
          }, 3000, NULL);
          
          lv_timer_del(timer);
        }
      }, 500, NULL);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *scan_btn_lbl = lv_label_create(ds_tasmota_scan_btn);
    lv_label_set_text(scan_btn_lbl, LV_SYMBOL_REFRESH " Netzwerk scannen");
    lv_obj_set_style_text_font(scan_btn_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(scan_btn_lbl, lv_color_hex(0x1a1a2e), 0);
    lv_obj_center(scan_btn_lbl);
    
    // Device list container
    ds_tasmota_device_list = lv_obj_create(scroll_cont);
    lv_obj_set_size(ds_tasmota_device_list, LV_PCT(100), 150);
    lv_obj_set_style_bg_opa(ds_tasmota_device_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ds_tasmota_device_list, 0, 0);
    lv_obj_set_style_pad_all(ds_tasmota_device_list, 0, 0);
    lv_obj_set_flex_flow(ds_tasmota_device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ds_tasmota_device_list, 5, 0);
    
    // Placeholder text
    lv_obj_t *placeholder = lv_label_create(ds_tasmota_device_list);
    lv_label_set_text(placeholder, "Druecke 'Netzwerk scannen' um\nTasmota-Geraete zu finden");
    lv_obj_set_style_text_font(placeholder, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(placeholder, DS_TEXT_DIM, 0);
  }
  
  // Save Button (at bottom of scroll area)
  lv_obj_t *save_btn = lv_btn_create(scroll_cont);
  lv_obj_set_size(save_btn, LV_PCT(100), 50);
  lv_obj_set_style_bg_color(save_btn, DS_SUCCESS, 0);
  lv_obj_set_style_radius(save_btn, 10, 0);
  lv_obj_add_event_cb(save_btn, ds_save_btn_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *save_lbl = lv_label_create(save_btn);
  lv_label_set_text(save_lbl, LV_SYMBOL_SAVE " Speichern");
  lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(save_lbl, lv_color_hex(0x1a1a2e), 0);
  lv_obj_center(save_lbl);
}

// ============================================================
// Create Device Settings Screen
// ============================================================
void createDeviceSettingsScreen() {
  if (ds_screen != NULL) {
    lv_obj_del(ds_screen);
    ds_screen = NULL;
  }
  
  // Create main screen
  ds_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ds_screen, DS_BG, 0);
  lv_obj_set_style_bg_opa(ds_screen, LV_OPA_COVER, 0);
  
  // ========== Header ==========
  lv_obj_t *header = lv_obj_create(ds_screen);
  lv_obj_set_size(header, DISPLAY_WIDTH, 60);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, DS_HEADER, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  // Back button
  lv_obj_t *back_btn = lv_btn_create(header);
  lv_obj_set_size(back_btn, 50, 40);
  lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x0a2540), 0);
  lv_obj_set_style_radius(back_btn, 8, 0);
  lv_obj_add_event_cb(back_btn, ds_back_btn_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *back_icon = lv_label_create(back_btn);
  lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(back_icon, DS_TEXT, 0);
  lv_obj_center(back_icon);
  
  // Title
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, LV_SYMBOL_SETTINGS " Geraete-Einstellungen");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, DS_TEXT, 0);
  lv_obj_center(title);
  
  // ========== Tab Bar ==========
  lv_obj_t *tab_bar = lv_obj_create(ds_screen);
  lv_obj_set_size(tab_bar, DISPLAY_WIDTH, 50);
  lv_obj_align(tab_bar, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_set_style_bg_color(tab_bar, DS_CARD, 0);
  lv_obj_set_style_radius(tab_bar, 0, 0);
  lv_obj_set_style_border_width(tab_bar, 0, 0);
  lv_obj_set_style_pad_all(tab_bar, 5, 0);
  lv_obj_set_flex_flow(tab_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(tab_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);
  
  // Red Sea Tab
  ds_tab_redsea = lv_btn_create(tab_bar);
  lv_obj_set_size(ds_tab_redsea, 145, 40);
  lv_obj_set_style_bg_color(ds_tab_redsea, DS_REDSEA, 0);  // Active by default
  lv_obj_set_style_radius(ds_tab_redsea, 8, 0);
  lv_obj_add_event_cb(ds_tab_redsea, ds_tab_cb, LV_EVENT_CLICKED, (void*)0);
  
  lv_obj_t *redsea_lbl = lv_label_create(ds_tab_redsea);
  lv_label_set_text(redsea_lbl, "Red Sea");
  lv_obj_set_style_text_font(redsea_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(redsea_lbl, DS_TEXT, 0);
  lv_obj_center(redsea_lbl);
  
  // Tunze Tab
  ds_tab_tunze = lv_btn_create(tab_bar);
  lv_obj_set_size(ds_tab_tunze, 145, 40);
  lv_obj_set_style_bg_color(ds_tab_tunze, DS_CARD, 0);
  lv_obj_set_style_radius(ds_tab_tunze, 8, 0);
  lv_obj_add_event_cb(ds_tab_tunze, ds_tab_cb, LV_EVENT_CLICKED, (void*)1);
  
  lv_obj_t *tunze_lbl = lv_label_create(ds_tab_tunze);
  lv_label_set_text(tunze_lbl, "Tunze");
  lv_obj_set_style_text_font(tunze_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(tunze_lbl, DS_TEXT, 0);
  lv_obj_center(tunze_lbl);
  
  // Tasmota Tab
  ds_tab_tasmota = lv_btn_create(tab_bar);
  lv_obj_set_size(ds_tab_tasmota, 145, 40);
  lv_obj_set_style_bg_color(ds_tab_tasmota, DS_CARD, 0);
  lv_obj_set_style_radius(ds_tab_tasmota, 8, 0);
  lv_obj_add_event_cb(ds_tab_tasmota, ds_tab_cb, LV_EVENT_CLICKED, (void*)2);
  
  lv_obj_t *tasmota_lbl = lv_label_create(ds_tab_tasmota);
  lv_label_set_text(tasmota_lbl, "Tasmota");
  lv_obj_set_style_text_font(tasmota_lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(tasmota_lbl, DS_TEXT, 0);
  lv_obj_center(tasmota_lbl);
  
  // ========== Content Area ==========
  ds_content_area = lv_obj_create(ds_screen);
  lv_obj_set_size(ds_content_area, DISPLAY_WIDTH - 20, 230);  // Leave space for keyboard
  lv_obj_align(ds_content_area, LV_ALIGN_TOP_MID, 0, 115);
  lv_obj_set_style_bg_opa(ds_content_area, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ds_content_area, 0, 0);
  lv_obj_set_style_pad_all(ds_content_area, 0, 0);
  
  // ========== Keyboard ==========
  ds_keyboard = lv_keyboard_create(ds_screen);
  lv_obj_set_size(ds_keyboard, DISPLAY_WIDTH, 180);
  lv_obj_align(ds_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(ds_keyboard, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
  lv_obj_set_style_bg_color(ds_keyboard, DS_CARD, 0);
  lv_obj_set_style_text_color(ds_keyboard, DS_TEXT, LV_PART_ITEMS);
  lv_obj_add_event_cb(ds_keyboard, ds_keyboard_cb, LV_EVENT_ALL, NULL);
  
  // Show Red Sea settings by default
  ds_current_service = 0;
  ds_show_service_settings(0);
}

// ============================================================
// Show Device Settings Screen
// ============================================================
void showDeviceSettingsScreen() {
  createDeviceSettingsScreen();
  lv_scr_load_anim(ds_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

#endif // BOARD_ESP32_4848S040

#endif // DEVICE_SETTINGS_UI_H
