#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// ============================================================
// CREDENTIALS MANAGEMENT
// ============================================================
// This file manages credential storage in ESP32 Flash memory.
// NO hardcoded passwords or personal data!
//
// All credentials are:
// - Loaded from Preferences (Flash)
// - Configured via Web Interface at runtime
// - Encrypted with device-unique key
// - NOT stored in this source code
//
// Safe for GitHub: ✓ Yes
// ============================================================

#include <Preferences.h>
#include "crypto.h"

// External references to global variables
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
extern Preferences preferences;

void loadCredentials() {
  // Load Red Sea credentials
  String savedredseaUser = preferences.getString("redsea_user", "");
  String savedredseaPass = preferences.getString("redsea_pass", "");
  String savedredseaAquaId = preferences.getString("redsea_aqua_id", "");
  String savedredseaAquaName = preferences.getString("rs_aqua_name", "");
  
  if (savedredseaUser.length() > 0) redsea_USERNAME = savedredseaUser;
  if (savedredseaPass.length() > 0) redsea_PASSWORD = decryptString(savedredseaPass);
  if (savedredseaAquaId.length() > 0) redsea_AQUARIUM_ID = savedredseaAquaId;
  if (savedredseaAquaName.length() > 0) redsea_AQUARIUM_NAME = savedredseaAquaName;
  
  // Load Tunze credentials
  String savedTunzeUser = preferences.getString("tunze_user", "");
  String savedTunzePass = preferences.getString("tunze_pass", "");
  String savedTunzeDevId = preferences.getString("tunze_dev_id", "");
  String savedTunzeDevName = preferences.getString("tz_dev_name", "");
  
  if (savedTunzeUser.length() > 0) TUNZE_USERNAME = savedTunzeUser;
  if (savedTunzePass.length() > 0) TUNZE_PASSWORD = decryptString(savedTunzePass);
  if (savedTunzeDevId.length() > 0) TUNZE_DEVICE_ID = savedTunzeDevId;
  if (savedTunzeDevName.length() > 0) TUNZE_DEVICE_NAME = savedTunzeDevName;
  
  // Load system enable/disable settings - default to false (only enable if configured)
  ENABLE_redsea = preferences.getBool("enable_redsea", false);
  ENABLE_TUNZE = preferences.getBool("enable_tunze", false);
  
  Serial.println("✓ Credentials loaded from flash (encrypted)");
}

void saveCredentials() {
  // Save Red Sea credentials (password encrypted)
  preferences.putString("redsea_user", redsea_USERNAME);
  preferences.putString("redsea_pass", encryptString(redsea_PASSWORD));
  preferences.putString("redsea_aqua_id", redsea_AQUARIUM_ID);
  preferences.putString("rs_aqua_name", redsea_AQUARIUM_NAME);
  
  // Save Tunze credentials (password encrypted)
  preferences.putString("tunze_user", TUNZE_USERNAME);
  preferences.putString("tunze_pass", encryptString(TUNZE_PASSWORD));
  preferences.putString("tunze_dev_id", TUNZE_DEVICE_ID);
  preferences.putString("tz_dev_name", TUNZE_DEVICE_NAME);
  
  // Save system enable/disable settings
  preferences.putBool("enable_redsea", ENABLE_redsea);
  preferences.putBool("enable_tunze", ENABLE_TUNZE);
  
  Serial.println("✓ Credentials saved to flash (encrypted)");
}

bool loadWiFiCredentials(String &ssid, String &password) {
  ssid = preferences.getString("wifi_ssid", "");
  String encPass = preferences.getString("wifi_pass", "");
  password = decryptString(encPass);
  return (ssid.length() > 0);
}

void saveWiFiCredentials(const String &ssid, const String &password) {
  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_pass", encryptString(password));
  Serial.println("✓ WiFi credentials saved (encrypted)");
}

#endif // CREDENTIALS_H
