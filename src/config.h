#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// SECURITY NOTICE
// ============================================================
// This file contains API endpoints and default configuration.
// NO PERSONAL DATA should be hardcoded here!
// All credentials (WiFi, Red Sea, Tunze) are stored in:
// - Flash memory (ESP32 Preferences)
// - Configured via Web Interface or Touch Display
//
// Safe for GitHub: ✓ Yes
// ============================================================

// ============================================================
// Board Configuration - Hardware specific settings
// ============================================================
#include "board_config.h"

// ============================================================
// API Configuration - Cloud Services
// ============================================================

// Red Sea Cloud API Configuration
const char* redsea_API_BASE = "https://cloud.reef-beat.com";
const char* redsea_CLIENT_AUTH = "Basic Z0ZqSHRKcGE6Qzlmb2d3cmpEV09SVDJHWQ==";

// Tunze Hub Configuration
const char* TUNZE_HUB_HOST = "tunze-hub.com";
const int TUNZE_HUB_PORT = 443;
const char* TUNZE_HUB_PATH = "/ws";

// ============================================================
// WiFi Configuration Portal
// ============================================================
// IMPORTANT: Change these default values before deploying!
// The AP will be created when no WiFi credentials are stored
const char* AP_SSID = "FeedingBreak_Setup";
const char* AP_PASSWORD = "ChangeMe123!";  // Change this for security!

// ============================================================
// Debug Configuration
// ============================================================
const bool DEBUG_TUNZE = false;   // Set to true to see all Tunze WebSocket messages

// ============================================================
// Timing Configuration
// ============================================================
const unsigned long debounceDelay = 50;  // Button debounce delay in ms

#endif // CONFIG_H
