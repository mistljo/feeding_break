/**
 * @file screensaver_ui.h
 * @brief Screensaver with Analog Clock using lv_meter widget
 * 
 * LVGL 8.4 compatible - uses built-in meter widget for proper clock display
 */

#ifndef SCREENSAVER_UI_H
#define SCREENSAVER_UI_H

#include <lvgl.h>
#include <time.h>
#include <math.h>

// Forward declarations
void hideScreensaver();

// Colors (Rolex Submariner style)
#define CLOCK_BG        lv_color_hex(0x000000)  // Black
#define CLOCK_DIAL      lv_color_hex(0x1a1a2e)  // Dark blue dial
#define CLOCK_MARKERS   lv_color_hex(0xFFFFFF)  // White markers
#define CLOCK_HAND      lv_color_hex(0xFFFFFF)  // White hands
#define CLOCK_SECOND    lv_color_hex(0x00b894)  // Green second hand
#define CLOCK_CENTER    lv_color_hex(0xe74c3c)  // Red center

// Timing constants
#define SCREENSAVER_TOUCH_IGNORE_MS 300  // Ignore touches for 300ms after exit

// Objects
static lv_obj_t *screensaver_screen = NULL;
static lv_obj_t *clock_meter = NULL;
static lv_meter_indicator_t *indic_hour = NULL;
static lv_meter_indicator_t *indic_min = NULL;
static lv_meter_indicator_t *indic_sec = NULL;
static lv_obj_t *date_label = NULL;
static lv_timer_t *clock_timer = NULL;
static bool screensaver_active = false;
static unsigned long screensaver_exit_time = 0;  // Time when screensaver was exited

// ============================================================
// Update Clock
// ============================================================
static void update_clock() {
  if (!screensaver_active || !clock_meter || !indic_hour || !indic_min || !indic_sec) return;
  
  time_t now = time(NULL);
  struct tm *timeinfo = localtime(&now);
  if (!timeinfo) return;
  
  int hour = timeinfo->tm_hour % 12;
  int minute = timeinfo->tm_min;
  int second = timeinfo->tm_sec;
  
  // Update needle positions
  // Hour: 0-11 maps to 0-60 (each hour = 5 ticks + minute offset)
  int hour_val = hour * 5 + minute / 12;  // 0-60 range
  lv_meter_set_indicator_value(clock_meter, indic_hour, hour_val);
  
  // Minute: 0-59 maps to 0-60
  lv_meter_set_indicator_value(clock_meter, indic_min, minute);
  
  // Second: 0-59 maps to 0-60
  lv_meter_set_indicator_value(clock_meter, indic_sec, second);
  
  // Update date label
  if (date_label) {
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%02d.%02d.%04d", 
             timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
    lv_label_set_text(date_label, date_str);
  }
}

// ============================================================
// Clock Timer Callback
// ============================================================
static void clock_timer_cb(lv_timer_t *timer) {
  update_clock();
}

// ============================================================
// Touch Event to Dismiss Screensaver
// ============================================================
static void screensaver_touch_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
    // Stop the event from propagating further
    lv_event_stop_bubbling(e);
    hideScreensaver();
  }
}

// ============================================================
// Create Screensaver Screen
// ============================================================
void createScreensaver() {
  // Create screen
  screensaver_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(screensaver_screen, CLOCK_BG, 0);
  lv_obj_add_event_cb(screensaver_screen, screensaver_touch_cb, LV_EVENT_CLICKED, NULL);
  
  // Create meter (clock face)
  clock_meter = lv_meter_create(screensaver_screen);
  lv_obj_set_size(clock_meter, 380, 380);
  lv_obj_center(clock_meter);
  
  // Style the meter
  lv_obj_set_style_bg_color(clock_meter, CLOCK_DIAL, LV_PART_MAIN);
  lv_obj_set_style_border_width(clock_meter, 8, LV_PART_MAIN);
  lv_obj_set_style_border_color(clock_meter, lv_color_hex(0x2d3436), LV_PART_MAIN);
  lv_obj_set_style_pad_all(clock_meter, 10, LV_PART_MAIN);
  
  // Remove ticks from default indicator area
  lv_obj_set_style_bg_opa(clock_meter, LV_OPA_COVER, LV_PART_INDICATOR);
  
  // Add scale for clock (0-60 for minutes/seconds positioning)
  lv_meter_scale_t *scale = lv_meter_add_scale(clock_meter);
  lv_meter_set_scale_range(clock_meter, scale, 0, 60, 360, 270);  // Full circle, start at 12
  
  // Ticks only, NO auto-generated number labels
  lv_meter_set_scale_ticks(clock_meter, scale, 60, 2, 8, CLOCK_MARKERS);
  lv_meter_set_scale_major_ticks(clock_meter, scale, 5, 4, 12, CLOCK_MARKERS, -10);  // -10 = hide labels inside (not visible)
  
  // Hide the auto-generated tick labels by making them transparent
  lv_obj_set_style_text_opa(clock_meter, LV_OPA_TRANSP, LV_PART_TICKS);
  
  // Add hour labels 1-12 manually
  const char* hour_labels[] = {"12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"};
  int radius = 150;  // Distance from center for labels
  for (int i = 0; i < 12; i++) {
    lv_obj_t *label = lv_label_create(clock_meter);
    lv_label_set_text(label, hour_labels[i]);
    lv_obj_set_style_text_color(label, CLOCK_MARKERS, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    // Calculate position: 12 is at top (270°), each hour is 30° clockwise
    float angle = (270 + i * 30) * 3.14159f / 180.0f;
    int x = (int)(radius * cosf(angle));
    int y = (int)(radius * sinf(angle));
    lv_obj_align(label, LV_ALIGN_CENTER, x, y);
  }
  
  // Hour hand (short and thick)
  indic_hour = lv_meter_add_needle_line(clock_meter, scale, 6, CLOCK_HAND, -60);
  
  // Minute hand (long and medium)
  indic_min = lv_meter_add_needle_line(clock_meter, scale, 4, CLOCK_HAND, -30);
  
  // Second hand (longest and thin, green)
  indic_sec = lv_meter_add_needle_line(clock_meter, scale, 2, CLOCK_SECOND, -20);
  
  // Date display
  date_label = lv_label_create(screensaver_screen);
  lv_label_set_text(date_label, "24.12.2025");
  lv_obj_set_style_text_color(date_label, lv_color_hex(0xb2bec3), 0);
  lv_obj_set_style_text_font(date_label, &lv_font_montserrat_18, 0);
  lv_obj_align(date_label, LV_ALIGN_BOTTOM_MID, 0, -30);
  
  // Hint to touch
  lv_obj_t *hint_label = lv_label_create(screensaver_screen);
  lv_label_set_text(hint_label, "Touch to exit");
  lv_obj_set_style_text_color(hint_label, lv_color_hex(0x636e72), 0);
  lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_12, 0);
  lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -10);
  
  // Create timer for updating clock (1 second)
  clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
  lv_timer_pause(clock_timer);
  
  // Initial update
  update_clock();
}

// ============================================================
// Show Screensaver
// ============================================================
void showScreensaver() {
  if (screensaver_screen && !screensaver_active) {
    screensaver_active = true;
    lv_scr_load(screensaver_screen);
    update_clock();
    lv_timer_resume(clock_timer);
  }
}

// ============================================================
// Hide Screensaver
// ============================================================
void hideScreensaver() {
  extern lv_obj_t* getMenuScreen();
  if (screensaver_active) {
    screensaver_active = false;
    screensaver_exit_time = millis();  // Record exit time
    lv_timer_pause(clock_timer);
    lv_scr_load(getMenuScreen());
  }
}

// ============================================================
// Check if touch should be ignored (just exited screensaver)
// ============================================================
bool shouldIgnoreTouchAfterScreensaver() {
  if (screensaver_exit_time > 0 && (millis() - screensaver_exit_time) < SCREENSAVER_TOUCH_IGNORE_MS) {
    return true;  // Ignore touches after screensaver exit
  }
  return false;
}

// ============================================================
// Clear screensaver exit time (call after handling ignored touch)
// ============================================================
void clearScreensaverExitTime() {
  screensaver_exit_time = 0;
}

// ============================================================
// Check if Screensaver is Active
// ============================================================
bool isScreensaverActive() {
  return screensaver_active;
}

#endif // SCREENSAVER_UI_H
