/**
 * @file tasmota_api.h
 * @brief Tasmota Device Discovery and Control API
 * 
 * Discovers Tasmota devices on the local network and controls them
 * during feeding mode. Supports automatic turn-on via PulseTime.
 */

#ifndef TASMOTA_API_H
#define TASMOTA_API_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include <esp_attr.h>
#include <lvgl.h>

// Forward declarations from main
extern bool feedingModeActive;

// ============================================================
// Tasmota Device Structure
// ============================================================
struct TasmotaDevice {
  String ip;
  String name;
  String hostname;
  bool enabled;      // User selected for feeding break
  bool turnOn;       // true = turn ON during feeding, false = turn OFF (default)
  bool powerState;   // Current power state
  bool reachable;    // Device is reachable
};

// ============================================================
// Global Tasmota State (in DRAM to avoid cache issues)
// ============================================================
static std::vector<TasmotaDevice> tasmotaDevices;
static DRAM_ATTR int tasmotaPulseTime = 900;  // Default: 15 minutes (in seconds)
static DRAM_ATTR bool tasmotaEnabled = false;
static DRAM_ATTR bool tasmotaFeedingActive = false;  // Feeding mode active for Tasmota
static DRAM_ATTR unsigned long tasmotaFeedingStartTime = 0;  // When feeding started
static DRAM_ATTR bool tasmotaDebug = false;  // Debug output disabled

// ============================================================
// Preferences Reference
// ============================================================
extern Preferences preferences;

// ============================================================
// Getter Functions for Menu UI
// ============================================================
bool tasmotaIsEnabled() { return tasmotaEnabled; }
int tasmotaGetPulseTime() { return tasmotaPulseTime; }

// ============================================================
// Helper: URL Encode
// ============================================================
static String urlEncode(const String& str) {
  String encoded = "";
  char c;
  for (size_t i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

// ============================================================
// Send Command to Tasmota Device (with optional retry)
// ============================================================
static String tasmotaSendCommand(const String& ip, const String& command, int retries = 3) {
  String response = "";
  
  for (int attempt = 0; attempt < retries; attempt++) {
    // Check WiFi connection before trying
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[TASMOTA] WiFi disconnected, waiting...");
      delay(100);
      yield();
      lv_timer_handler();
      continue;
    }
    
    // Yield to other tasks
    yield();
    lv_timer_handler();
    delay(10);
    
    HTTPClient http;
    String url = "http://" + ip + "/cm?cmnd=" + urlEncode(command);
    
    if (tasmotaDebug) {
      if (attempt > 0) {
        Serial.printf("[TASMOTA DEBUG] >> %s CMD: %s (retry %d)\n", ip.c_str(), command.c_str(), attempt);
      } else {
        Serial.printf("[TASMOTA DEBUG] >> %s CMD: %s\n", ip.c_str(), command.c_str());
      }
      Serial.printf("[TASMOTA DEBUG]    URL: %s\n", url.c_str());
    }
    
    http.begin(url);
    http.setTimeout(800);  // 800ms timeout (reduced)
    http.setConnectTimeout(500);  // 500ms connect timeout (reduced)
    
    int httpCode = http.GET();
    
    // Yield to other tasks after HTTP call
    yield();
    lv_timer_handler();
    delay(5);
    
    if (httpCode == HTTP_CODE_OK) {
      response = http.getString();
      if (tasmotaDebug) {
        Serial.printf("[TASMOTA DEBUG] << %s Response: %s\n", ip.c_str(), response.c_str());
      }
      http.end();
      return response;  // Success - return immediately
    } else if (tasmotaDebug) {
      // Only log errors in debug mode to reduce serial spam
      Serial.printf("[TASMOTA DEBUG] %s HTTP %d\n", ip.c_str(), httpCode);
    }
    
    http.end();
    
    // Wait before retry with watchdog feeding
    if (attempt < retries - 1) {
      Serial.printf("[TASMOTA] Retry %d/%d for %s...\n", attempt + 1, retries - 1, ip.c_str());
      yield();
      lv_timer_handler();
      delay(100);
      yield();
      lv_timer_handler();
    }
  }
  
  yield();
  return response;  // Empty string on failure
}

// ============================================================
// Check if IP is a Tasmota Device
// ============================================================
static bool tasmotaCheckDevice(const String& ip, TasmotaDevice& device) {
  // Yield to other tasks before HTTP request
  delay(20);
  
  HTTPClient http;
  // Status command returns device info
  String url = "http://" + ip + "/cm?cmnd=Status";
  
  http.begin(url);
  http.setTimeout(2000);  // 2s timeout for weak WiFi
  http.setConnectTimeout(1500);  // 1.5s connection timeout
  
  int httpCode = http.GET();
  delay(10);  // Yield after HTTP
  
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    http.end();
    
    // Debug output
    Serial.printf("  %s: HTTP OK, parsing...\n", ip.c_str());
    
    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
      Serial.printf("  %s: JSON parse error: %s\n", ip.c_str(), error.c_str());
      return false;
    }
    
    // Check if it's a Tasmota device (has Status object with DeviceName or FriendlyName)
    JsonObject status = doc["Status"];
    if (!status.isNull()) {
      device.ip = ip;
      
      // Get device name
      if (status["DeviceName"].is<const char*>()) {
        device.name = status["DeviceName"].as<String>();
      } else if (status["FriendlyName"].is<JsonArray>() && status["FriendlyName"][0].is<const char*>()) {
        device.name = status["FriendlyName"][0].as<String>();
      } else {
        device.name = "Tasmota";
      }
      
      device.hostname = doc["StatusNET"]["Hostname"] | "";
      device.reachable = true;
      device.enabled = false;
      
      // Get power state from Status.Power
      int power = status["Power"] | -1;
      device.powerState = (power == 1);
      
      Serial.printf("  %s: Found! Name=%s, Power=%d\n", ip.c_str(), device.name.c_str(), power);
      return true;
    } else {
      Serial.printf("  %s: No Status object in response\n", ip.c_str());
    }
  } else if (httpCode > 0) {
    Serial.printf("  %s: HTTP %d\n", ip.c_str(), httpCode);
    http.end();
  } else {
    http.end();
  }
  
  return false;
}

// ============================================================
// Background Scan State (in DRAM to avoid cache issues)
// ============================================================
static DRAM_ATTR volatile bool tasmotaScanRunning = false;
static DRAM_ATTR volatile bool tasmotaScanComplete = false;
static DRAM_ATTR volatile int tasmotaScanProgress = 0;  // 0-254
static DRAM_ATTR volatile int tasmotaScanFound = 0;
static std::vector<TasmotaDevice> tasmotaScanResults;

// ============================================================
// Background Scan Task
// ============================================================
static void tasmotaScanTask(void* parameter) {
  Serial.println("\n=== Scanning for Tasmota devices (background) ===");
  
  tasmotaScanResults.clear();
  tasmotaScanProgress = 0;
  tasmotaScanFound = 0;
  
  // Get local IP
  IPAddress localIP = WiFi.localIP();
  String baseIP = String(localIP[0]) + "." + String(localIP[1]) + "." + String(localIP[2]) + ".";
  
  Serial.printf("Scanning network: %s0/24\n", baseIP.c_str());
  
  // Scan all IPs
  for (int i = 1; i < 255; i++) {
    if (!tasmotaScanRunning) {
      Serial.println("Scan cancelled");
      break;
    }
    
    tasmotaScanProgress = i;
    String ip = baseIP + String(i);
    
    // Skip our own IP
    if (ip == localIP.toString()) continue;
    
    TasmotaDevice device;
    if (tasmotaCheckDevice(ip, device)) {
      Serial.printf("✓ Found Tasmota: %s (%s)\n", device.name.c_str(), ip.c_str());
      tasmotaScanResults.push_back(device);
      tasmotaScanFound = tasmotaScanResults.size();
    }
    
    // Progress every 25 IPs
    if (i % 25 == 0) {
      Serial.printf("  Scanned %d/254 IPs... (found: %d)\n", i, tasmotaScanFound);
    }
    
    // Small delay to be nice to other tasks
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  
  Serial.printf("=== Scan complete: %d devices found ===\n\n", tasmotaScanResults.size());
  
  tasmotaScanRunning = false;
  tasmotaScanComplete = true;
  
  // Delete this task
  vTaskDelete(NULL);
}

// ============================================================
// Start Network Scan (non-blocking)
// ============================================================
static String tasmotaStartScan() {
  if (tasmotaScanRunning) {
    return "{\"success\":false,\"message\":\"Scan already running\"}";
  }
  
  tasmotaScanRunning = true;
  tasmotaScanComplete = false;
  
  // Create background task with lower priority
  xTaskCreatePinnedToCore(
    tasmotaScanTask,    // Task function
    "tasmota_scan",     // Name
    8192,               // Stack size
    NULL,               // Parameters
    1,                  // Priority (low)
    NULL,               // Task handle
    0                   // Core 0
  );
  
  return "{\"success\":true,\"message\":\"Scan started\"}";
}

// ============================================================
// Get Scan Results
// ============================================================
static String tasmotaGetScanResults() {
  JsonDocument doc;
  
  if (tasmotaScanRunning) {
    doc["success"] = true;
    doc["scanning"] = true;
    doc["progress"] = tasmotaScanProgress;
    doc["found"] = tasmotaScanFound;
    doc["message"] = "Scan in progress...";
  } else if (tasmotaScanComplete) {
    doc["success"] = true;
    doc["scanning"] = false;
    doc["count"] = tasmotaScanResults.size();
    
    JsonArray devices = doc["devices"].to<JsonArray>();
    for (const auto& dev : tasmotaScanResults) {
      JsonObject d = devices.add<JsonObject>();
      d["ip"] = dev.ip;
      d["name"] = dev.name;
      d["hostname"] = dev.hostname;
      d["powerState"] = dev.powerState;
      d["reachable"] = true;
      d["enabled"] = false;
      d["turnOn"] = false;
      d["hostname"] = dev.hostname;
      d["powerState"] = dev.powerState;
      d["reachable"] = true;
      d["enabled"] = false;
    }
  } else {
    doc["success"] = true;
    doc["scanning"] = false;
    doc["message"] = "No scan results. Start a scan first.";
  }
  
  String result;
  serializeJson(doc, result);
  return result;
}

// Legacy function - now starts background scan
static String tasmotaScanNetwork() {
  return tasmotaStartScan();
}

// ============================================================
// Get Power State of a Device (using Status 0 to not affect PulseTime)
// Returns: 1 = ON, 0 = OFF, -1 = error/unknown
// ============================================================
static int tasmotaGetPowerStateEx(const String& ip) {
  // Use "Status 0" instead of "Power" to avoid resetting PulseTime timer
  String response = tasmotaSendCommand(ip, "Status 0");
  
  if (response.length() == 0) {
    return -1;  // Error - no response
  }
  
  JsonDocument doc;
  if (deserializeJson(doc, response) == DeserializationError::Ok) {
    // Try StatusSTS.POWER first (most reliable, string "ON"/"OFF")
    if (doc["StatusSTS"].is<JsonObject>()) {
      String stspower = doc["StatusSTS"]["POWER"] | "";
      if (tasmotaDebug) Serial.printf("[TASMOTA DEBUG] StatusSTS.POWER = '%s'\n", stspower.c_str());
      if (stspower == "ON") return 1;
      if (stspower == "OFF") return 0;
    }
    
    // Try Status.Power (can be int or string depending on Tasmota version)
    if (doc["Status"].is<JsonObject>()) {
      JsonVariant pv = doc["Status"]["Power"];
      if (pv.is<int>()) {
        int power = pv.as<int>();
        if (tasmotaDebug) Serial.printf("[TASMOTA DEBUG] Status.Power (int) = %d\n", power);
        if (power == 1) return 1;
        if (power == 0) return 0;
      } else if (pv.is<const char*>()) {
        String powerStr = pv.as<String>();
        if (tasmotaDebug) Serial.printf("[TASMOTA DEBUG] Status.Power (str) = '%s'\n", powerStr.c_str());
        if (powerStr == "1" || powerStr == "ON") return 1;
        if (powerStr == "0" || powerStr == "OFF") return 0;
      }
    }
    
    // Fallback: try direct POWER field (for simple Power command response)
    String powerStr = doc["POWER"] | doc["POWER1"] | "";
    if (powerStr.length() > 0) {
      if (tasmotaDebug) Serial.printf("[TASMOTA DEBUG] POWER field = '%s'\n", powerStr.c_str());
      if (powerStr == "ON") return 1;
      if (powerStr == "OFF") return 0;
    }
  }
  
  return -1;  // Error - invalid response
}

// Legacy wrapper for compatibility
static bool tasmotaGetPowerState(const String& ip) {
  int state = tasmotaGetPowerStateEx(ip);
  return (state == 1);
}

// ============================================================
// Turn Device ON (with retry)
// ============================================================
static bool tasmotaTurnOn(const String& ip) {
  Serial.printf("Tasmota %s: Turning ON\n", ip.c_str());
  String response = tasmotaSendCommand(ip, "Power ON", 3);  // 3 retries
  return response.indexOf("ON") >= 0;
}

// ============================================================
// Turn Device OFF (with optional PulseTime for auto-on)
// ============================================================
static bool tasmotaTurnOff(const String& ip, int autoOnSeconds = 0) {
  Serial.printf("Tasmota %s: Turning OFF", ip.c_str());
  
  if (autoOnSeconds > 0) {
    // Use PowerOnState 5 (inverted PulseTime) for auto-on after OFF
    // PulseTime 112-64900 = 1-64788 seconds (value - 100 = seconds)
    int pulseValue = autoOnSeconds + 100;
    if (pulseValue > 64900) pulseValue = 64900;
    
    Serial.printf(" (auto-on in %d sec via PowerOnState 5)\n", autoOnSeconds);
    
    // Set PowerOnState 5 = inverted PulseTime (OFF -> wait -> ON)
    tasmotaSendCommand(ip, "PowerOnState 5", 2);
    
    // Set PulseTime
    String cmd = "PulseTime " + String(pulseValue);
    tasmotaSendCommand(ip, cmd, 2);
  } else {
    Serial.println();
    // Disable auto-on
    tasmotaSendCommand(ip, "PulseTime 0", 2);
  }
  
  String response = tasmotaSendCommand(ip, "Power OFF", 3);  // 3 retries for Power OFF
  return response.indexOf("OFF") >= 0;
}

// ============================================================
// Start Feeding Mode - Turn OFF/ON selected devices based on setting
// ============================================================
void tasmotaStartFeeding() {
  if (!tasmotaEnabled || tasmotaDevices.empty()) {
    Serial.println("⊘ Tasmota disabled or no devices configured");
    return;
  }
  
  Serial.println("\n=== Tasmota: Starting Feeding Mode ===");
  
  for (auto& device : tasmotaDevices) {
    if (device.enabled) {
      if (device.turnOn) {
        // Turn ON during feeding (inverted)
        if (tasmotaTurnOn(device.ip)) {
          device.powerState = true;
          Serial.printf("✓ %s (%s) turned ON (inverted)\n", device.name.c_str(), device.ip.c_str());
        } else {
          Serial.printf("✗ %s (%s) failed to turn ON\n", device.name.c_str(), device.ip.c_str());
        }
      } else {
        // Turn OFF during feeding (normal) with PulseTime for automatic turn-on
        if (tasmotaTurnOff(device.ip, tasmotaPulseTime)) {
          device.powerState = false;
          Serial.printf("✓ %s (%s) turned OFF\n", device.name.c_str(), device.ip.c_str());
        } else {
          Serial.printf("✗ %s (%s) failed to turn OFF\n", device.name.c_str(), device.ip.c_str());
        }
      }
    }
  }
  
  tasmotaFeedingActive = true;
  tasmotaFeedingStartTime = millis();
  Serial.println("=== Tasmota: Feeding Mode Started ===\n");
}

// ============================================================
// Stop Feeding Mode - Reverse the action
// ============================================================
void tasmotaStopFeeding() {
  if (!tasmotaEnabled || tasmotaDevices.empty()) {
    Serial.println("⊘ Tasmota disabled or no devices configured");
    return;
  }
  
  Serial.println("\n=== Tasmota: Stopping Feeding Mode ===");
  Serial.printf("Devices count: %d, tasmotaFeedingActive: %s\n", 
                tasmotaDevices.size(), tasmotaFeedingActive ? "true" : "false");
  
  for (auto& device : tasmotaDevices) {
    Serial.printf("Device: %s, enabled=%d, turnOn=%d\n", 
                  device.name.c_str(), device.enabled, device.turnOn);
    
    if (device.enabled) {
      // Disable auto-on: PulseTime 0 and restore PowerOnState 3
      Serial.printf("Sending PulseTime 0 to %s\n", device.ip.c_str());
      tasmotaSendCommand(device.ip, "PulseTime 0");
      Serial.printf("Sending PowerOnState 3 to %s\n", device.ip.c_str());
      tasmotaSendCommand(device.ip, "PowerOnState 3");  // Restore to "last state"
      
      if (device.turnOn) {
        // Was ON during feeding, turn OFF now (inverted)
        Serial.printf("turnOn=true -> Turning OFF %s\n", device.ip.c_str());
        if (tasmotaTurnOff(device.ip, 0)) {
          device.powerState = false;
          Serial.printf("✓ %s (%s) turned OFF (inverted)\n", device.name.c_str(), device.ip.c_str());
        } else {
          Serial.printf("✗ %s (%s) failed to turn OFF\n", device.name.c_str(), device.ip.c_str());
        }
      } else {
        // Was OFF during feeding, turn ON now (normal)
        Serial.printf("turnOn=false -> Turning ON %s\n", device.ip.c_str());
        if (tasmotaTurnOn(device.ip)) {
          device.powerState = true;
          Serial.printf("✓ %s (%s) turned ON\n", device.name.c_str(), device.ip.c_str());
        } else {
          Serial.printf("✗ %s (%s) failed to turn ON\n", device.name.c_str(), device.ip.c_str());
        }
      }
    }
  }
  
  tasmotaFeedingActive = false;
  Serial.println("=== Tasmota: Feeding Mode Stopped ===\n");
}

// ============================================================
// Save Tasmota Configuration
// ============================================================
void tasmotaSaveConfig() {
  preferences.putBool("tasmota_en", tasmotaEnabled);
  preferences.putInt("tasmota_pulse", tasmotaPulseTime);
  
  // Save device list as JSON
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (const auto& device : tasmotaDevices) {
    if (device.enabled) {
      JsonObject d = arr.add<JsonObject>();
      d["ip"] = device.ip;
      d["name"] = device.name;
      d["turnOn"] = device.turnOn;
    }
  }
  
  String deviceJson;
  serializeJson(doc, deviceJson);
  preferences.putString("tasmota_devs", deviceJson);
  
  Serial.println("✓ Tasmota config saved");
}

// ============================================================
// Load Tasmota Configuration
// ============================================================
void tasmotaLoadConfig() {
  tasmotaEnabled = preferences.getBool("tasmota_en", false);
  tasmotaPulseTime = preferences.getInt("tasmota_pulse", 900);
  
  String deviceJson = preferences.getString("tasmota_devs", "[]");
  
  JsonDocument doc;
  if (deserializeJson(doc, deviceJson) == DeserializationError::Ok) {
    tasmotaDevices.clear();
    
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject d : arr) {
      TasmotaDevice device;
      device.ip = d["ip"].as<String>();
      device.name = d["name"].as<String>();
      device.turnOn = d["turnOn"] | false;
      device.enabled = true;
      device.reachable = false;  // Will be checked later
      device.powerState = false;
      tasmotaDevices.push_back(device);
    }
  }
  
  Serial.printf("✓ Tasmota config loaded: %s, %d devices, %d sec pulse\n",
                tasmotaEnabled ? "enabled" : "disabled",
                tasmotaDevices.size(),
                tasmotaPulseTime);
}

// ============================================================
// Get Tasmota Settings as JSON
// ============================================================
String tasmotaGetSettings() {
  JsonDocument doc;
  doc["enabled"] = tasmotaEnabled;
  doc["pulseTime"] = tasmotaPulseTime;  // camelCase for JavaScript
  
  JsonArray devices = doc["devices"].to<JsonArray>();
  for (const auto& device : tasmotaDevices) {
    JsonObject d = devices.add<JsonObject>();
    d["ip"] = device.ip;
    d["name"] = device.name;
    d["enabled"] = device.enabled;
    d["turnOn"] = device.turnOn;
    d["power"] = device.powerState ? "ON" : "OFF";
  }
  
  String result;
  serializeJson(doc, result);
  return result;
}

// ============================================================
// Update Tasmota Settings from JSON
// ============================================================
bool tasmotaUpdateSettings(const String& json) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) {
    return false;
  }
  
  tasmotaEnabled = doc["enabled"] | false;
  // Accept both pulseTime (JS) and pulse_time (internal)
  tasmotaPulseTime = doc["pulseTime"] | doc["pulse_time"] | 900;
  
  // Update device list
  if (doc["devices"].is<JsonArray>()) {
    tasmotaDevices.clear();
    
    JsonArray arr = doc["devices"].as<JsonArray>();
    for (JsonObject d : arr) {
      TasmotaDevice device;
      device.ip = d["ip"].as<String>();
      device.name = d["name"].as<String>();
      device.enabled = d["enabled"] | true;
      device.turnOn = d["turnOn"] | false;
      device.reachable = true;
      device.powerState = false;
      tasmotaDevices.push_back(device);
    }
  }
  
  tasmotaSaveConfig();
  return true;
}

// ============================================================
// Test a single device (toggle power)
// ============================================================
String tasmotaTestDevice(const String& ip) {
  Serial.printf("Testing Tasmota device: %s\n", ip.c_str());
  
  // Get current state
  bool currentState = tasmotaGetPowerState(ip);
  
  // Toggle
  if (currentState) {
    tasmotaTurnOff(ip, 0);
  } else {
    tasmotaTurnOn(ip);
  }
  
  delay(1000);
  
  // Toggle back
  if (currentState) {
    tasmotaTurnOn(ip);
  } else {
    tasmotaTurnOff(ip, 0);
  }
  
  JsonDocument doc;
  doc["success"] = true;
  doc["message"] = "Device toggled successfully";
  
  String result;
  serializeJson(doc, result);
  return result;
}

// ============================================================
// Update Power States of all enabled devices
// ============================================================
void tasmotaUpdatePowerStates() {
  if (tasmotaDebug) {
    Serial.println("[TASMOTA DEBUG] Updating power states...");
  }
  for (auto& device : tasmotaDevices) {
    // Yield to other tasks for each device
    delay(50);
    
    if (device.enabled) {
      bool oldState = device.powerState;
      int newState = tasmotaGetPowerStateEx(device.ip);
      
      if (newState >= 0) {
        // Valid response - update state
        device.powerState = (newState == 1);
        device.reachable = true;
        if (tasmotaDebug && oldState != device.powerState) {
          Serial.printf("[TASMOTA DEBUG] %s state changed: %s -> %s\n", 
            device.name.c_str(), 
            oldState ? "ON" : "OFF", 
            device.powerState ? "ON" : "OFF");
        }
      } else {
        // Error - keep old state, mark as possibly unreachable
        if (tasmotaDebug) {
          Serial.printf("[TASMOTA DEBUG] %s query failed - keeping state: %s\n", 
            device.name.c_str(), 
            device.powerState ? "ON" : "OFF");
        }
      }
    }
  }
}

// ============================================================
// Check if all Tasmota devices are back to normal state
// Returns true if feeding mode should be considered complete
// ============================================================
bool tasmotaCheckFeedingComplete() {
  if (!tasmotaFeedingActive || !tasmotaEnabled || tasmotaDevices.empty()) {
    return false;
  }
  
  int enabledCount = 0;
  int completedCount = 0;
  
  for (auto& device : tasmotaDevices) {
    if (device.enabled) {
      enabledCount++;
      
      // Update power state
      bool currentPower = tasmotaGetPowerState(device.ip);
      device.powerState = currentPower;
      
      // Check if device is back to "normal" state
      // turnOn=false means device should be OFF during feeding, so complete when ON
      // turnOn=true means device should be ON during feeding, so complete when OFF
      if (device.turnOn) {
        // Inverted: was ON during feeding, complete when OFF
        if (!currentPower) completedCount++;
      } else {
        // Normal: was OFF during feeding, complete when ON (PulseTime triggered)
        if (currentPower) completedCount++;
      }
    }
  }
  
  return (enabledCount > 0 && completedCount == enabledCount);
}

// ============================================================
// Get Tasmota Feeding Status as JSON
// ============================================================
String tasmotaGetFeedingStatus() {
  // Yield to other tasks at start
  delay(10);
  
  JsonDocument doc;
  
  doc["active"] = tasmotaFeedingActive;
  doc["enabled"] = tasmotaEnabled;
  doc["pulseTime"] = tasmotaPulseTime;
  
  if (tasmotaFeedingActive) {
    unsigned long elapsed = (millis() - tasmotaFeedingStartTime) / 1000;
    doc["elapsedSeconds"] = elapsed;
    
    if (tasmotaDebug) {
      Serial.printf("[TASMOTA DEBUG] Feeding status poll - elapsed: %lu sec\n", elapsed);
    }
    
    // Yield before status update
    delay(20);
    
    // Update power states from devices when feeding is active
    tasmotaUpdatePowerStates();
  }
  
  int enabledCount = 0;
  int completedCount = 0;
  
  JsonArray devices = doc["devices"].to<JsonArray>();
  
  for (const auto& device : tasmotaDevices) {
    if (device.enabled) {
      enabledCount++;
      
      JsonObject d = devices.add<JsonObject>();
      d["ip"] = device.ip;
      d["name"] = device.name;
      d["powerState"] = device.powerState;
      d["turnOn"] = device.turnOn;  // What action during feeding
      
      // Determine expected state and if complete
      bool expectedDuringFeeding = device.turnOn;  // ON if turnOn, OFF if !turnOn
      bool isInFeedingState = (device.powerState == expectedDuringFeeding);
      
      d["inFeedingState"] = isInFeedingState;
      d["completed"] = !isInFeedingState;  // Completed when NOT in feeding state anymore
      
      if (!isInFeedingState) completedCount++;
    }
  }
  
  doc["totalDevices"] = enabledCount;
  doc["completedDevices"] = completedCount;
  doc["allComplete"] = (enabledCount > 0 && completedCount == enabledCount);
  
  // Auto-end feeding mode when all devices are back to normal
  if (tasmotaFeedingActive && enabledCount > 0 && completedCount == enabledCount) {
    Serial.println("\n=== Tasmota: All devices restored - auto-ending feeding mode ===");
    tasmotaFeedingActive = false;
    feedingModeActive = false;  // Update global state
    
    // Clean up: disable PulseTime and restore PowerOnState
    for (auto& device : tasmotaDevices) {
      if (device.enabled) {
        tasmotaSendCommand(device.ip, "PulseTime 0");
        tasmotaSendCommand(device.ip, "PowerOnState 3");
      }
    }
    Serial.println("=== Feeding mode auto-stopped ===\n");
  }
  
  String result;
  serializeJson(doc, result);
  return result;
}

#endif // TASMOTA_API_H
