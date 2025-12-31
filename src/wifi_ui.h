/**
 * @file wifi_ui.h
 * @brief LVGL WiFi Setup UI with virtual keyboard
 * 
 * Provides a beautiful touch-based WiFi configuration interface
 * Works alongside the web-based configuration portal
 */

#ifndef WIFI_UI_H
#define WIFI_UI_H

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include "credentials.h"
#include "board_config.h"

// ============================================================
// External References
// ============================================================
extern bool wifiConfigMode;
extern void stopConfigPortal();

// ============================================================
// WiFi UI Colors (matching main theme)
// ============================================================
#define WIFI_UI_BG           lv_color_hex(0x1a1a2e)
#define WIFI_UI_CARD         lv_color_hex(0x16213e)
#define WIFI_UI_HEADER       lv_color_hex(0x0f3460)
#define WIFI_UI_ACCENT       lv_color_hex(0x00d9ff)
#define WIFI_UI_SUCCESS      lv_color_hex(0x00ff87)
#define WIFI_UI_ERROR        lv_color_hex(0xff6b6b)
#define WIFI_UI_TEXT         lv_color_hex(0xffffff)
#define WIFI_UI_TEXT_DIM     lv_color_hex(0x8892b0)
#define WIFI_UI_BUTTON       lv_color_hex(0x533483)

// ============================================================
// WiFi UI State
// ============================================================
static lv_obj_t *wifi_screen = NULL;
static lv_obj_t *wifi_keyboard = NULL;
static lv_obj_t *wifi_ssid_ta = NULL;
static lv_obj_t *wifi_pass_ta = NULL;
static lv_obj_t *wifi_status_label = NULL;
static lv_obj_t *wifi_ip_label = NULL;
static lv_obj_t *wifi_network_list = NULL;
static lv_obj_t *wifi_scan_btn = NULL;
static lv_obj_t *wifi_connect_btn = NULL;
static lv_obj_t *wifi_spinner = NULL;

static bool wifi_scanning = false;
static bool wifi_connecting = false;
static bool wifi_edit_mode = false;  // true = show edit form, false = show info
static lv_obj_t *active_textarea = NULL;

// UI containers for switching views
static lv_obj_t *wifi_info_container = NULL;
static lv_obj_t *wifi_edit_container = NULL;

// ============================================================
// Forward Declarations
// ============================================================
static void wifi_keyboard_event_cb(lv_event_t *e);
static void wifi_ta_event_cb(lv_event_t *e);
static void wifi_scan_btn_event_cb(lv_event_t *e);
static void wifi_connect_btn_event_cb(lv_event_t *e);
static void wifi_change_btn_event_cb(lv_event_t *e);
static void wifi_show_edit_view();
static void wifi_show_info_view();
static void wifi_network_selected_cb(lv_event_t *e);
static void wifi_back_btn_event_cb(lv_event_t *e);

// ============================================================
// Styles for WiFi UI
// ============================================================
static lv_style_t wifi_style_card;
static lv_style_t wifi_style_btn;
static lv_style_t wifi_style_ta;
static lv_style_t wifi_style_list_item;

// ============================================================
// German QWERTZ Keyboard Map
// ============================================================
static const char *kb_map_de[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", LV_SYMBOL_BACKSPACE, "\n",
  "q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "\n",
  "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
  LV_SYMBOL_UP, "y", "x", "c", "v", "b", "n", "m", ".", "-", "\n",
  "#1", LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_KEYBOARD, ""
};

static const lv_btnmatrix_ctrl_t kb_ctrl_de[] = {
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6,
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 4, 4, 4, 4,
  6 | LV_BTNMATRIX_CTRL_CHECKABLE, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 4, 10, 4, 5
};

static const char *kb_map_de_upper[] = {
  "!", "\"", "@", "$", "%", "&", "/", "(", ")", "=", LV_SYMBOL_BACKSPACE, "\n",
  "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "\n",
  "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
  LV_SYMBOL_UP, "Y", "X", "C", "V", "B", "N", "M", "?", "_", "\n",
  "#1", LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_KEYBOARD, ""
};

static const char *kb_map_de_special[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", LV_SYMBOL_BACKSPACE, "\n",
  "+", "-", "*", "/", "=", "#", "'", ":", ";", "~", "\n",
  "<", ">", "[", "]", "{", "}", "\\", "|", "`", "\n",
  "abc", "@", "$", "%", "&", "^", "!", "?", ".", ",", "\n",
  "abc", LV_SYMBOL_LEFT, " ", LV_SYMBOL_RIGHT, LV_SYMBOL_KEYBOARD, ""
};

static bool kb_is_upper = false;
static bool kb_is_special = false;

static void create_wifi_styles() {
  // Card style
  lv_style_init(&wifi_style_card);
  lv_style_set_bg_color(&wifi_style_card, WIFI_UI_CARD);
  lv_style_set_bg_opa(&wifi_style_card, LV_OPA_COVER);
  lv_style_set_radius(&wifi_style_card, 15);
  lv_style_set_pad_all(&wifi_style_card, 15);
  lv_style_set_border_width(&wifi_style_card, 0);
  
  // Button style
  lv_style_init(&wifi_style_btn);
  lv_style_set_bg_color(&wifi_style_btn, WIFI_UI_ACCENT);
  lv_style_set_bg_grad_color(&wifi_style_btn, lv_color_hex(0x0099cc));
  lv_style_set_bg_grad_dir(&wifi_style_btn, LV_GRAD_DIR_VER);
  lv_style_set_radius(&wifi_style_btn, 10);
  lv_style_set_shadow_width(&wifi_style_btn, 10);
  lv_style_set_shadow_color(&wifi_style_btn, WIFI_UI_ACCENT);
  lv_style_set_shadow_opa(&wifi_style_btn, LV_OPA_30);
  lv_style_set_text_color(&wifi_style_btn, lv_color_hex(0x000000));
  
  // Textarea style
  lv_style_init(&wifi_style_ta);
  lv_style_set_bg_color(&wifi_style_ta, lv_color_hex(0x0f1729));
  lv_style_set_text_color(&wifi_style_ta, WIFI_UI_TEXT);
  lv_style_set_border_color(&wifi_style_ta, WIFI_UI_ACCENT);
  lv_style_set_border_width(&wifi_style_ta, 2);
  lv_style_set_radius(&wifi_style_ta, 8);
  lv_style_set_pad_all(&wifi_style_ta, 10);
  
  // List item style
  lv_style_init(&wifi_style_list_item);
  lv_style_set_bg_color(&wifi_style_list_item, lv_color_hex(0x1e2a45));
  lv_style_set_bg_opa(&wifi_style_list_item, LV_OPA_COVER);
  lv_style_set_radius(&wifi_style_list_item, 8);
  lv_style_set_pad_all(&wifi_style_list_item, 10);
  lv_style_set_text_color(&wifi_style_list_item, WIFI_UI_TEXT);
}

// ============================================================
// Keyboard Event Handler for German QWERTZ
// ============================================================
static void wifi_keyboard_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *kb = lv_event_get_target(e);
  
  if (kb == NULL) return;
  
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    if (active_textarea != NULL) {
      lv_obj_clear_state(active_textarea, LV_STATE_FOCUSED);
    }
    active_textarea = NULL;
  }
  else if (code == LV_EVENT_VALUE_CHANGED) {
    uint16_t btn_id = lv_btnmatrix_get_selected_btn(kb);
    if (btn_id == LV_BTNMATRIX_BTN_NONE) return;
    
    const char *txt = lv_btnmatrix_get_btn_text(kb, btn_id);
    if (txt == NULL) return;
    
    // Handle special buttons
    if (strcmp(txt, LV_SYMBOL_UP) == 0) {
      // Toggle shift
      kb_is_upper = !kb_is_upper;
      kb_is_special = false;
      if (kb_is_upper) {
        lv_btnmatrix_set_map(kb, kb_map_de_upper);
      } else {
        lv_btnmatrix_set_map(kb, kb_map_de);
      }
      lv_btnmatrix_set_ctrl_map(kb, kb_ctrl_de);
    }
    else if (strcmp(txt, "#1") == 0 || strcmp(txt, "abc") == 0) {
      // Toggle special characters
      kb_is_special = !kb_is_special;
      kb_is_upper = false;
      if (kb_is_special) {
        lv_btnmatrix_set_map(kb, kb_map_de_special);
      } else {
        lv_btnmatrix_set_map(kb, kb_map_de);
      }
      lv_btnmatrix_set_ctrl_map(kb, kb_ctrl_de);
    }
    else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
      if (active_textarea) {
        lv_textarea_del_char(active_textarea);
        lv_obj_add_state(active_textarea, LV_STATE_FOCUSED);  // Cursor sichtbar halten
      }
    }
    else if (strcmp(txt, LV_SYMBOL_LEFT) == 0) {
      // Cursor left
      if (active_textarea) {
        lv_textarea_cursor_left(active_textarea);
        lv_obj_add_state(active_textarea, LV_STATE_FOCUSED);  // Cursor sichtbar halten
      }
    }
    else if (strcmp(txt, LV_SYMBOL_RIGHT) == 0) {
      // Cursor right
      if (active_textarea) {
        lv_textarea_cursor_right(active_textarea);
        lv_obj_add_state(active_textarea, LV_STATE_FOCUSED);  // Cursor sichtbar halten
      }
    }
    else if (strcmp(txt, LV_SYMBOL_KEYBOARD) == 0) {
      // Hide keyboard
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
      if (active_textarea) lv_obj_clear_state(active_textarea, LV_STATE_FOCUSED);
      active_textarea = NULL;
    }
    else if (strlen(txt) > 0 && txt[0] != '\n') {
      // Regular character - add to textarea
      if (active_textarea) {
        lv_textarea_add_text(active_textarea, txt);
        lv_obj_add_state(active_textarea, LV_STATE_FOCUSED);  // Cursor sichtbar halten
      }
      
      // One-shot shift: return to lowercase after typing
      if (kb_is_upper && !kb_is_special) {
        kb_is_upper = false;
        lv_btnmatrix_set_map(kb, kb_map_de);
        lv_btnmatrix_set_ctrl_map(kb, kb_ctrl_de);
      }
    }
  }
}

// ============================================================
// Password Show/Hide Toggle
// ============================================================
static void wifi_pass_toggle_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    
    bool is_password = lv_textarea_get_password_mode(wifi_pass_ta);
    lv_textarea_set_password_mode(wifi_pass_ta, !is_password);
    
    // Update icon
    if (is_password) {
      lv_label_set_text(label, LV_SYMBOL_EYE_OPEN);
    } else {
      lv_label_set_text(label, LV_SYMBOL_EYE_CLOSE);
    }
  }
}

// ============================================================
// Textarea Event Handler (show keyboard on focus)
// ============================================================
static void wifi_ta_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  
  if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
    if (wifi_keyboard != NULL && ta != NULL) {
      // Entferne Fokus vom vorherigen Textfeld
      if (active_textarea != NULL && active_textarea != ta) {
        lv_obj_clear_state(active_textarea, LV_STATE_FOCUSED);
      }
      
      active_textarea = ta;
      lv_obj_add_state(ta, LV_STATE_FOCUSED);  // Setze Fokus auf neues Feld
      lv_obj_clear_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
      
      // Scroll textarea into view above keyboard
      lv_obj_scroll_to_view(ta, LV_ANIM_ON);
    }
  }
}

// ============================================================
// Network Selected Callback
// ============================================================
static void wifi_network_selected_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == NULL) return;
    
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    
    if (label != NULL && wifi_ssid_ta != NULL && wifi_pass_ta != NULL && wifi_keyboard != NULL) {
      const char *ssid = lv_label_get_text(label);
      // Extract just the SSID (remove signal info)
      String ssidStr = String(ssid);
      int spaceIdx = ssidStr.indexOf("  ");
      if (spaceIdx > 0) {
        ssidStr = ssidStr.substring(0, spaceIdx);
      }
      
      lv_textarea_set_text(wifi_ssid_ta, ssidStr.c_str());
      
      // Focus on password field
      lv_obj_add_state(wifi_pass_ta, LV_STATE_FOCUSED);
      active_textarea = wifi_pass_ta;
      lv_obj_clear_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

// ============================================================
// Scan Networks
// ============================================================
static void wifi_do_scan() {
  wifi_scanning = true;
  
  // Show spinner, hide list
  if (wifi_spinner) lv_obj_clear_flag(wifi_spinner, LV_OBJ_FLAG_HIDDEN);
  if (wifi_network_list) lv_obj_add_flag(wifi_network_list, LV_OBJ_FLAG_HIDDEN);
  if (wifi_status_label) lv_label_set_text(wifi_status_label, "Suche Netzwerke...");
  
  // Start async scan
  WiFi.scanNetworks(true);
}

static void wifi_update_scan_results() {
  int16_t n = WiFi.scanComplete();
  
  if (n == WIFI_SCAN_RUNNING) {
    return; // Still scanning
  }
  
  wifi_scanning = false;
  
  // Hide spinner
  if (wifi_spinner) lv_obj_add_flag(wifi_spinner, LV_OBJ_FLAG_HIDDEN);
  
  if (n == WIFI_SCAN_FAILED || n < 0) {
    if (wifi_status_label) lv_label_set_text(wifi_status_label, "Scan fehlgeschlagen");
    return;
  }
  
  // Clear old list items
  if (wifi_network_list) {
    lv_obj_clean(wifi_network_list);
    lv_obj_clear_flag(wifi_network_list, LV_OBJ_FLAG_HIDDEN);
    
    if (n == 0) {
      lv_label_set_text(wifi_status_label, "Keine Netzwerke gefunden");
    } else {
      char statusText[32];
      snprintf(statusText, sizeof(statusText), "%d Netzwerke gefunden", n);
      lv_label_set_text(wifi_status_label, statusText);
      
      // Add networks to list (sorted by signal strength)
      for (int i = 0; i < n && i < 10; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        
        // Check for duplicates
        bool isDup = false;
        for (int j = 0; j < i; j++) {
          if (WiFi.SSID(j) == ssid) {
            isDup = true;
            break;
          }
        }
        if (isDup) continue;
        
        // Create list button
        lv_obj_t *btn = lv_btn_create(wifi_network_list);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, 50);
        lv_obj_add_style(btn, &wifi_style_list_item, 0);
        lv_obj_add_event_cb(btn, wifi_network_selected_cb, LV_EVENT_CLICKED, NULL);
        
        // Network info
        char netInfo[64];
        int rssi = WiFi.RSSI(i);
        const char *lock = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? LV_SYMBOL_EYE_CLOSE : "";
        const char *signal = LV_SYMBOL_WIFI;
        
        snprintf(netInfo, sizeof(netInfo), "%s  %s %s %ddBm", 
                 ssid.c_str(), signal, lock, rssi);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, netInfo);
        lv_obj_center(label);
      }
    }
  }
  
  WiFi.scanDelete();
}

static void wifi_scan_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    wifi_do_scan();
  }
}

// ============================================================
// Connect Button Handler
// ============================================================
static bool pending_wifi_connect = false;
static String pending_ssid;
static String pending_pass;

static void wifi_connect_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    if (wifi_ssid_ta == NULL || wifi_pass_ta == NULL) return;
    
    const char *ssid = lv_textarea_get_text(wifi_ssid_ta);
    const char *pass = lv_textarea_get_text(wifi_pass_ta);
    
    if (strlen(ssid) == 0) {
      if (wifi_status_label) {
        lv_label_set_text(wifi_status_label, "Bitte SSID eingeben!");
        lv_obj_set_style_text_color(wifi_status_label, WIFI_UI_ERROR, 0);
      }
      return;
    }
    
    // Hide keyboard
    if (wifi_keyboard) lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Save credentials first
    saveWiFiCredentials(String(ssid), String(pass));
    
    // Update status
    if (wifi_status_label) {
      lv_label_set_text(wifi_status_label, "Gespeichert! Neustart...");
      lv_obj_set_style_text_color(wifi_status_label, WIFI_UI_SUCCESS, 0);
    }
    
    // Show spinner
    if (wifi_spinner) lv_obj_clear_flag(wifi_spinner, LV_OBJ_FLAG_HIDDEN);
    
    // Schedule restart after short delay to let UI update
    // This avoids the flash cache conflict
    delay(500);
    ESP.restart();
  }
}

// ============================================================
// Back Button Handler
// ============================================================
// Forward declaration of settings screen
extern void showSettingsScreen();
lv_obj_t* getMainScreen();

static void wifi_back_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  
  if (code == LV_EVENT_CLICKED) {
    // Return to settings screen
    showSettingsScreen();
  }
}

// ============================================================
// Update WiFi Connection Status
// ============================================================
static void wifi_update_connection_status() {
  if (!wifi_connecting) return;
  
  static int attempts = 0;
  static unsigned long lastCheck = 0;
  
  if (millis() - lastCheck < 500) return;
  lastCheck = millis();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connecting = false;
    attempts = 0;
    
    // Hide spinner
    if (wifi_spinner) lv_obj_add_flag(wifi_spinner, LV_OBJ_FLAG_HIDDEN);
    
    // Update status
    char ipStr[64];
    snprintf(ipStr, sizeof(ipStr), LV_SYMBOL_OK " Verbunden: %s", WiFi.localIP().toString().c_str());
    lv_label_set_text(wifi_status_label, ipStr);
    lv_obj_set_style_text_color(wifi_status_label, WIFI_UI_SUCCESS, 0);
    
    // Update IP label
    if (wifi_ip_label) {
      snprintf(ipStr, sizeof(ipStr), "IP: %s", WiFi.localIP().toString().c_str());
      lv_label_set_text(wifi_ip_label, ipStr);
    }
    
    wifiConfigMode = false;
    
    // Auto-return to main screen after 2 seconds
    // (Could add a timer here)
    
  } else {
    attempts++;
    
    if (attempts > 20) { // 10 seconds timeout
      wifi_connecting = false;
      attempts = 0;
      
      // Hide spinner
      if (wifi_spinner) lv_obj_add_flag(wifi_spinner, LV_OBJ_FLAG_HIDDEN);
      
      lv_label_set_text(wifi_status_label, LV_SYMBOL_WARNING " Verbindung fehlgeschlagen!");
      lv_obj_set_style_text_color(wifi_status_label, WIFI_UI_ERROR, 0);
    }
  }
}

// ============================================================
// Change WiFi Button Handler (switch to edit mode)
// ============================================================
static void wifi_change_btn_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    wifi_edit_mode = true;
    if (wifi_info_container) lv_obj_add_flag(wifi_info_container, LV_OBJ_FLAG_HIDDEN);
    if (wifi_edit_container) lv_obj_clear_flag(wifi_edit_container, LV_OBJ_FLAG_HIDDEN);
    // Start scan when entering edit mode
    wifi_do_scan();
  }
}

// ============================================================
// Create WiFi Info View (when connected)
// ============================================================
static void create_wifi_info_view(lv_obj_t *parent) {
  wifi_info_container = lv_obj_create(parent);
  lv_obj_set_size(wifi_info_container, 460, 350);
  lv_obj_align(wifi_info_container, LV_ALIGN_TOP_MID, 0, 65);
  lv_obj_set_style_bg_opa(wifi_info_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wifi_info_container, 0, 0);
  lv_obj_set_style_pad_all(wifi_info_container, 10, 0);
  lv_obj_set_flex_flow(wifi_info_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(wifi_info_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(wifi_info_container, 15, 0);
  
  // Connection Status Card
  lv_obj_t *status_card = lv_obj_create(wifi_info_container);
  lv_obj_set_size(status_card, 440, 140);
  lv_obj_add_style(status_card, &wifi_style_card, 0);
  lv_obj_set_style_border_color(status_card, WIFI_UI_SUCCESS, 0);
  lv_obj_set_style_border_width(status_card, 2, 0);
  lv_obj_clear_flag(status_card, LV_OBJ_FLAG_SCROLLABLE);
  
  // Status icon
  lv_obj_t *status_icon = lv_label_create(status_card);
  lv_label_set_text(status_icon, LV_SYMBOL_OK);
  lv_obj_set_style_text_font(status_icon, &lv_font_montserrat_36, 0);
  lv_obj_set_style_text_color(status_icon, WIFI_UI_SUCCESS, 0);
  lv_obj_align(status_icon, LV_ALIGN_LEFT_MID, 15, 0);
  
  // SSID
  lv_obj_t *ssid_title = lv_label_create(status_card);
  lv_label_set_text(ssid_title, "Verbunden mit:");
  lv_obj_set_style_text_font(ssid_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ssid_title, WIFI_UI_TEXT_DIM, 0);
  lv_obj_align(ssid_title, LV_ALIGN_TOP_LEFT, 70, 15);
  
  lv_obj_t *ssid_label = lv_label_create(status_card);
  lv_label_set_text(ssid_label, WiFi.SSID().c_str());
  lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ssid_label, WIFI_UI_TEXT, 0);
  lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 70, 40);
  
  // IP Address
  char ipStr[32];
  snprintf(ipStr, sizeof(ipStr), "IP: %s", WiFi.localIP().toString().c_str());
  lv_obj_t *ip_label = lv_label_create(status_card);
  lv_label_set_text(ip_label, ipStr);
  lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ip_label, WIFI_UI_ACCENT, 0);
  lv_obj_align(ip_label, LV_ALIGN_TOP_LEFT, 70, 70);
  
  // Signal Strength Card
  lv_obj_t *signal_card = lv_obj_create(wifi_info_container);
  lv_obj_set_size(signal_card, 440, 100);
  lv_obj_add_style(signal_card, &wifi_style_card, 0);
  lv_obj_clear_flag(signal_card, LV_OBJ_FLAG_SCROLLABLE);
  
  lv_obj_t *signal_icon = lv_label_create(signal_card);
  lv_label_set_text(signal_icon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(signal_icon, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(signal_icon, WIFI_UI_ACCENT, 0);
  lv_obj_align(signal_icon, LV_ALIGN_LEFT_MID, 15, 0);
  
  int rssi = WiFi.RSSI();
  const char *quality;
  lv_color_t quality_color;
  if (rssi > -50) { quality = "Ausgezeichnet"; quality_color = WIFI_UI_SUCCESS; }
  else if (rssi > -60) { quality = "Sehr gut"; quality_color = WIFI_UI_SUCCESS; }
  else if (rssi > -70) { quality = "Gut"; quality_color = WIFI_UI_ACCENT; }
  else if (rssi > -80) { quality = "Mittel"; quality_color = lv_color_hex(0xffa502); }
  else { quality = "Schwach"; quality_color = WIFI_UI_ERROR; }
  
  lv_obj_t *signal_title = lv_label_create(signal_card);
  lv_label_set_text(signal_title, "Signalstaerke:");
  lv_obj_set_style_text_font(signal_title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(signal_title, WIFI_UI_TEXT_DIM, 0);
  lv_obj_align(signal_title, LV_ALIGN_TOP_LEFT, 60, 15);
  
  char signalStr[32];
  snprintf(signalStr, sizeof(signalStr), "%s (%d dBm)", quality, rssi);
  lv_obj_t *signal_value = lv_label_create(signal_card);
  lv_label_set_text(signal_value, signalStr);
  lv_obj_set_style_text_font(signal_value, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(signal_value, quality_color, 0);
  lv_obj_align(signal_value, LV_ALIGN_TOP_LEFT, 60, 45);
  
  // Change WiFi Button
  lv_obj_t *change_btn = lv_btn_create(wifi_info_container);
  lv_obj_set_size(change_btn, 250, 55);
  lv_obj_set_style_bg_color(change_btn, WIFI_UI_BUTTON, 0);
  lv_obj_set_style_radius(change_btn, 12, 0);
  lv_obj_add_event_cb(change_btn, wifi_change_btn_event_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *change_label = lv_label_create(change_btn);
  lv_label_set_text(change_label, LV_SYMBOL_EDIT "  WiFi aendern");
  lv_obj_set_style_text_font(change_label, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(change_label, WIFI_UI_TEXT, 0);
  lv_obj_center(change_label);
}

// ============================================================
// Create WiFi Edit View (form with keyboard)
// ============================================================
static void create_wifi_edit_view(lv_obj_t *parent) {
  wifi_edit_container = lv_obj_create(parent);
  lv_obj_set_size(wifi_edit_container, 460, 280);
  lv_obj_align(wifi_edit_container, LV_ALIGN_TOP_MID, 0, 65);
  lv_obj_set_style_bg_opa(wifi_edit_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wifi_edit_container, 0, 0);
  lv_obj_set_style_pad_all(wifi_edit_container, 5, 0);
  lv_obj_set_flex_flow(wifi_edit_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(wifi_edit_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  
  // AP Info Card (only in config mode)
  if (wifiConfigMode) {
    lv_obj_t *ap_card = lv_obj_create(wifi_edit_container);
    lv_obj_set_size(ap_card, 450, 80);
    lv_obj_add_style(ap_card, &wifi_style_card, 0);
    lv_obj_clear_flag(ap_card, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *ap_title = lv_label_create(ap_card);
    lv_label_set_text(ap_title, "Access Point aktiv:");
    lv_obj_set_style_text_color(ap_title, WIFI_UI_TEXT_DIM, 0);
    lv_obj_set_style_text_font(ap_title, &lv_font_montserrat_14, 0);
    lv_obj_align(ap_title, LV_ALIGN_TOP_LEFT, 10, 8);
    
    wifi_ip_label = lv_label_create(ap_card);
    lv_obj_set_width(wifi_ip_label, 410);  // Genug Platz fuer langen Text
    lv_label_set_long_mode(wifi_ip_label, LV_LABEL_LONG_SCROLL_CIRCULAR);  // Scrollen wenn zu lang
    char apInfo[80];
    snprintf(apInfo, sizeof(apInfo), "AP: %s   IP: %s", AP_SSID, WiFi.softAPIP().toString().c_str());
    lv_label_set_text(wifi_ip_label, apInfo);
    lv_obj_set_style_text_color(wifi_ip_label, WIFI_UI_ACCENT, 0);
    lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
    lv_obj_align(wifi_ip_label, LV_ALIGN_TOP_LEFT, 10, 35);
  }
  
  // SSID Input
  lv_obj_t *ssid_label = lv_label_create(wifi_edit_container);
  lv_label_set_text(ssid_label, "WLAN Netzwerk:");
  lv_obj_set_style_text_color(ssid_label, WIFI_UI_TEXT, 0);
  lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_14, 0);
  lv_obj_set_width(ssid_label, 450);
  
  wifi_ssid_ta = lv_textarea_create(wifi_edit_container);
  lv_obj_set_size(wifi_ssid_ta, 450, 45);
  lv_obj_add_style(wifi_ssid_ta, &wifi_style_ta, 0);
  lv_textarea_set_placeholder_text(wifi_ssid_ta, "Netzwerk auswaehlen oder eingeben");
  lv_textarea_set_one_line(wifi_ssid_ta, true);
  lv_obj_add_event_cb(wifi_ssid_ta, wifi_ta_event_cb, LV_EVENT_ALL, NULL);
  
  // Password Input
  lv_obj_t *pass_label = lv_label_create(wifi_edit_container);
  lv_label_set_text(pass_label, "Passwort:");
  lv_obj_set_style_text_color(pass_label, WIFI_UI_TEXT, 0);
  lv_obj_set_style_text_font(pass_label, &lv_font_montserrat_14, 0);
  lv_obj_set_width(pass_label, 450);
  
  // Password row
  lv_obj_t *pass_row = lv_obj_create(wifi_edit_container);
  lv_obj_set_size(pass_row, 450, 50);
  lv_obj_set_style_bg_opa(pass_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(pass_row, 0, 0);
  lv_obj_set_style_pad_all(pass_row, 0, 0);
  lv_obj_set_flex_flow(pass_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(pass_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(pass_row, LV_OBJ_FLAG_SCROLLABLE);
  
  wifi_pass_ta = lv_textarea_create(pass_row);
  lv_obj_set_size(wifi_pass_ta, 390, 45);
  lv_obj_add_style(wifi_pass_ta, &wifi_style_ta, 0);
  lv_textarea_set_placeholder_text(wifi_pass_ta, "WiFi Passwort");
  lv_textarea_set_one_line(wifi_pass_ta, true);
  lv_textarea_set_password_mode(wifi_pass_ta, true);
  lv_obj_add_event_cb(wifi_pass_ta, wifi_ta_event_cb, LV_EVENT_ALL, NULL);
  
  // Show/Hide password button
  lv_obj_t *pass_toggle_btn = lv_btn_create(pass_row);
  lv_obj_set_size(pass_toggle_btn, 50, 45);
  lv_obj_set_style_bg_color(pass_toggle_btn, lv_color_hex(0x2d3a55), 0);
  lv_obj_set_style_radius(pass_toggle_btn, 8, 0);
  lv_obj_add_event_cb(pass_toggle_btn, wifi_pass_toggle_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *pass_toggle_icon = lv_label_create(pass_toggle_btn);
  lv_label_set_text(pass_toggle_icon, LV_SYMBOL_EYE_CLOSE);
  lv_obj_set_style_text_color(pass_toggle_icon, WIFI_UI_TEXT, 0);
  lv_obj_center(pass_toggle_icon);
  
  // Buttons Row
  lv_obj_t *btn_row = lv_obj_create(wifi_edit_container);
  lv_obj_set_size(btn_row, 450, 55);
  lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_row, 0, 0);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
  
  // Scan button
  wifi_scan_btn = lv_btn_create(btn_row);
  lv_obj_set_size(wifi_scan_btn, 140, 50);
  lv_obj_set_style_bg_color(wifi_scan_btn, WIFI_UI_BUTTON, 0);
  lv_obj_set_style_radius(wifi_scan_btn, 10, 0);
  lv_obj_add_event_cb(wifi_scan_btn, wifi_scan_btn_event_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *scan_label = lv_label_create(wifi_scan_btn);
  lv_label_set_text(scan_label, LV_SYMBOL_REFRESH " Scan");
  lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_16, 0);
  lv_obj_center(scan_label);
  
  // Connect button
  wifi_connect_btn = lv_btn_create(btn_row);
  lv_obj_set_size(wifi_connect_btn, 200, 50);
  lv_obj_add_style(wifi_connect_btn, &wifi_style_btn, 0);
  lv_obj_add_event_cb(wifi_connect_btn, wifi_connect_btn_event_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *connect_label = lv_label_create(wifi_connect_btn);
  lv_label_set_text(connect_label, LV_SYMBOL_OK " Verbinden");
  lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_16, 0);
  lv_obj_center(connect_label);
  
  // Status Label
  wifi_status_label = lv_label_create(parent);
  lv_label_set_text(wifi_status_label, "Bereit");
  lv_obj_set_style_text_color(wifi_status_label, WIFI_UI_TEXT_DIM, 0);
  lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_14, 0);
  lv_obj_align(wifi_status_label, LV_ALIGN_TOP_MID, 0, 350);
  
  // Spinner
  wifi_spinner = lv_spinner_create(parent, 1000, 60);
  lv_obj_set_size(wifi_spinner, 40, 40);
  lv_obj_align(wifi_spinner, LV_ALIGN_TOP_MID, 100, 345);
  lv_obj_add_flag(wifi_spinner, LV_OBJ_FLAG_HIDDEN);
  
  // Network List
  wifi_network_list = lv_obj_create(parent);
  lv_obj_set_size(wifi_network_list, 460, 100);
  lv_obj_align(wifi_network_list, LV_ALIGN_TOP_MID, 0, 375);
  lv_obj_set_style_bg_color(wifi_network_list, WIFI_UI_CARD, 0);
  lv_obj_set_style_bg_opa(wifi_network_list, LV_OPA_80, 0);
  lv_obj_set_style_radius(wifi_network_list, 15, 0);
  lv_obj_set_style_border_width(wifi_network_list, 0, 0);
  lv_obj_set_style_pad_all(wifi_network_list, 10, 0);
  lv_obj_set_flex_flow(wifi_network_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(wifi_network_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(wifi_network_list, LV_OBJ_FLAG_HIDDEN);
  
  // Keyboard (German QWERTZ using btnmatrix)
  wifi_keyboard = lv_btnmatrix_create(parent);
  lv_obj_set_size(wifi_keyboard, DISPLAY_WIDTH, 200);
  lv_obj_align(wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  
  // Set German keyboard map
  lv_btnmatrix_set_map(wifi_keyboard, kb_map_de);
  lv_btnmatrix_set_ctrl_map(wifi_keyboard, kb_ctrl_de);
  
  lv_obj_add_event_cb(wifi_keyboard, wifi_keyboard_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
  
  // Keyboard styling
  lv_obj_set_style_bg_color(wifi_keyboard, lv_color_hex(0x1e2a45), 0);
  lv_obj_set_style_bg_color(wifi_keyboard, lv_color_hex(0x2d3a55), LV_PART_ITEMS);
  lv_obj_set_style_text_color(wifi_keyboard, WIFI_UI_TEXT, LV_PART_ITEMS);
  lv_obj_set_style_border_width(wifi_keyboard, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_color(wifi_keyboard, lv_color_hex(0x3d4a65), LV_PART_ITEMS);
  lv_obj_set_style_radius(wifi_keyboard, 5, LV_PART_ITEMS);
  lv_obj_set_style_pad_all(wifi_keyboard, 4, 0);
}

// ============================================================
// Create WiFi Setup Screen
// ============================================================
void createWiFiScreen() {
  if (wifi_screen != NULL) {
    lv_obj_del(wifi_screen);
    wifi_screen = NULL;
    wifi_info_container = NULL;
    wifi_edit_container = NULL;
  }
  
  create_wifi_styles();
  
  // Determine initial mode: info view if connected, edit view otherwise
  wifi_edit_mode = (WiFi.status() != WL_CONNECTED) || wifiConfigMode;
  
  // Create screen
  wifi_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(wifi_screen, WIFI_UI_BG, 0);
  lv_obj_set_style_bg_opa(wifi_screen, LV_OPA_COVER, 0);
  
  // ========== Header ==========
  lv_obj_t *header = lv_obj_create(wifi_screen);
  lv_obj_set_size(header, DISPLAY_WIDTH, 60);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, WIFI_UI_HEADER, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  // Back button (nur anzeigen wenn nicht im Config-Modus)
  if (!wifiConfigMode) {
    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 50, 40);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x0a2540), 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_add_event_cb(back_btn, wifi_back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, WIFI_UI_TEXT, 0);
    lv_obj_center(back_icon);
  }
  
  // Title
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, LV_SYMBOL_WIFI "  WiFi");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, WIFI_UI_TEXT, 0);
  lv_obj_center(title);
  
  // Create both views
  create_wifi_info_view(wifi_screen);
  create_wifi_edit_view(wifi_screen);
  
  // Show appropriate view and hide/show edit-only elements
  if (wifi_edit_mode) {
    lv_obj_add_flag(wifi_info_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(wifi_edit_container, LV_OBJ_FLAG_HIDDEN);
    // Show edit-mode elements
    if (wifi_status_label) lv_obj_clear_flag(wifi_status_label, LV_OBJ_FLAG_HIDDEN);
    if (wifi_spinner) lv_obj_clear_flag(wifi_spinner, LV_OBJ_FLAG_HIDDEN); // Will be hidden by code
    if (wifi_network_list) lv_obj_clear_flag(wifi_network_list, LV_OBJ_FLAG_HIDDEN); // Will be hidden by code
    if (wifi_keyboard) lv_obj_clear_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN); // Will be hidden by code
  } else {
    lv_obj_clear_flag(wifi_info_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_edit_container, LV_OBJ_FLAG_HIDDEN);
    // Hide edit-mode elements in info view
    if (wifi_status_label) lv_obj_add_flag(wifi_status_label, LV_OBJ_FLAG_HIDDEN);
    if (wifi_spinner) lv_obj_add_flag(wifi_spinner, LV_OBJ_FLAG_HIDDEN);
    if (wifi_network_list) lv_obj_add_flag(wifi_network_list, LV_OBJ_FLAG_HIDDEN);
    if (wifi_keyboard) lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

// ============================================================
// Show WiFi Setup Screen
// ============================================================
void showWiFiScreen() {
  // Always recreate screen to refresh status
  createWiFiScreen();
  
  lv_scr_load_anim(wifi_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
  
  // Auto-start network scan only in edit mode
  if (wifi_edit_mode) {
    delay(500);  // Wait for screen transition
    wifi_do_scan();
  }
}

// ============================================================
// Update WiFi UI (call in loop when WiFi screen is active)
// ============================================================
void updateWiFiUI() {
  if (wifi_screen == NULL) return;
  if (lv_scr_act() != wifi_screen) return;
  
  // Update scan results
  if (wifi_scanning) {
    wifi_update_scan_results();
  }
  
  // Update connection status
  if (wifi_connecting) {
    wifi_update_connection_status();
  }
}

// ============================================================
// Check if WiFi screen is active
// ============================================================
bool isWiFiScreenActive() {
  return (wifi_screen != NULL && lv_scr_act() == wifi_screen);
}

#endif // WIFI_UI_H
