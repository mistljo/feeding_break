#ifndef REDSEA_API_H
#define REDSEA_API_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"

// External references
extern String redsea_USERNAME;
extern String redsea_PASSWORD;
extern String redsea_AQUARIUM_ID;
extern String redseaToken;

bool redseaLogin() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String tokenUrl = String(redsea_API_BASE) + "/oauth/token";
  
  http.begin(client, tokenUrl);
  http.setTimeout(10000);  // 10s timeout
  http.setConnectTimeout(5000);  // 5s connect timeout
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", redsea_CLIENT_AUTH);
  
  String postData = "grant_type=password&username=";
  postData += redsea_USERNAME;
  postData += "&password=";
  String encodedPassword = redsea_PASSWORD;
  encodedPassword.replace("&", "%26");
  encodedPassword.replace("#", "%23");
  postData += encodedPassword;
  
  Serial.println("Requesting OAuth token...");
  int httpCode = http.POST(postData);
  
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      redseaToken = doc["access_token"].as<String>();
      Serial.println("✓ OAuth token received");
      http.end();
      return true;
    } else {
      Serial.print("✗ JSON parsing failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("✗ HTTP request failed with code: ");
    Serial.println(httpCode);
    Serial.println(http.getString());
  }
  
  http.end();
  return false;
}

bool redseaCheckFeedingStatus() {
  if (redseaToken.isEmpty()) {
    Serial.println("No OAuth token - logging in first...");
    if (!redseaLogin()) return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String statusUrl = String(redsea_API_BASE) + "/aquarium/" + redsea_AQUARIUM_ID;
  
  http.begin(client, statusUrl);
  http.setTimeout(8000);  // 8s timeout
  http.setConnectTimeout(4000);  // 4s connect timeout
  http.addHeader("Authorization", "Bearer " + redseaToken);
  
  Serial.println("Checking current feeding status...");
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      bool isActive = doc["properties"]["feeding"] | false;
      Serial.print("✓ Cloud status: Feeding mode is ");
      Serial.println(isActive ? "ACTIVE" : "INACTIVE");
      http.end();
      return isActive;
    } else {
      Serial.print("✗ JSON parse error: ");
      Serial.println(error.c_str());
    }
  } else if (httpCode == 401) {
    http.end();
    redseaToken = "";
    return redseaCheckFeedingStatus();
  }
  
  http.end();
  return false;
}

bool redseaStartFeeding() {
  if (redseaToken.isEmpty()) {
    Serial.println("No OAuth token - logging in first...");
    if (!redseaLogin()) return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String feedingUrl = String(redsea_API_BASE) + "/aquarium/" + redsea_AQUARIUM_ID + "/feeding/start";
  
  http.begin(client, feedingUrl);
  http.setTimeout(10000);  // 10s timeout
  http.setConnectTimeout(5000);  // 5s connect timeout
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + redseaToken);
  
  String postData = "{}";
  
  Serial.println("Starting Red Sea feeding mode...");
  int httpCode = http.POST(postData);
  
  if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
    Serial.println("✓ Red Sea feeding mode activated");
    http.end();
    return true;
  } else if (httpCode == 400) {
    String response = http.getString();
    if (response.indexOf("already active") >= 0) {
      Serial.println("⚠ Feeding mode is already active in cloud");
      http.end();
      return true;
    }
    Serial.print("✗ Bad request: ");
    Serial.println(response);
    http.end();
    return false;
  } else if (httpCode == 401) {
    Serial.println("✗ Token expired - re-authenticating...");
    http.end();
    redseaToken = "";
    return redseaStartFeeding();
  } else {
    Serial.print("✗ Feeding mode request failed with code: ");
    Serial.println(httpCode);
    Serial.println(http.getString());
    http.end();
    return false;
  }
}

bool redseaStopFeeding() {
  if (redseaToken.isEmpty()) {
    Serial.println("No OAuth token - logging in first...");
    if (!redseaLogin()) return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String feedingUrl = String(redsea_API_BASE) + "/aquarium/" + redsea_AQUARIUM_ID + "/feeding/stop";
  
  http.begin(client, feedingUrl);
  http.setTimeout(10000);  // 10s timeout
  http.setConnectTimeout(5000);  // 5s connect timeout
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + redseaToken);
  
  String postData = "{}";
  
  Serial.println("Stopping Red Sea feeding mode...");
  int httpCode = http.POST(postData);
  
  if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
    Serial.println("✓ Red Sea feeding mode deactivated");
    http.end();
    return true;
  } else if (httpCode == 401) {
    Serial.println("✗ Token expired - re-authenticating...");
    http.end();
    redseaToken = "";
    return redseaStopFeeding();
  } else {
    Serial.print("✗ Stop feeding request failed with code: ");
    Serial.println(httpCode);
    Serial.println(http.getString());
    http.end();
    return false;
  }
}

String redseaGetAquariums() {
  if (redseaToken.isEmpty()) {
    Serial.println("No OAuth token - logging in first...");
    if (!redseaLogin()) {
      return "{\"success\":false,\"message\":\"Login failed\"}";
    }
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String aquariumUrl = String(redsea_API_BASE) + "/aquarium";
  
  http.begin(client, aquariumUrl);
  http.setTimeout(8000);  // 8s timeout
  http.setConnectTimeout(4000);  // 4s connect timeout
  http.addHeader("Authorization", "Bearer " + redseaToken);
  
  Serial.println("Fetching aquarium list...");
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Raw API Response:");
    Serial.println(payload);
    Serial.println("---");
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      if (!doc.is<JsonArray>()) {
        Serial.println("✗ Response is not an array");
        http.end();
        return "{\"success\":false,\"message\":\"Unexpected JSON structure\"}";
      }
      
      JsonArray aquariums = doc.as<JsonArray>();
      Serial.print("Number of aquariums found: ");
      Serial.println(aquariums.size());
      
      String result = "{\"success\":true,\"aquariums\":[";
      
      bool first = true;
      int count = 0;
      
      for (JsonObject aquarium : aquariums) {
        if (!first) result += ",";
        first = false;
        
        String aqua_id = aquarium["id"].as<String>();
        String aqua_name = aquarium["name"].as<String>();
        String aqua_uid = aquarium["uid"].as<String>();
        
        if (aqua_name.length() == 0) {
          aqua_name = "Aquarium " + aqua_id;
        }
        
        count++;
        Serial.print("Aquarium #");
        Serial.print(count);
        Serial.print(": ");
        Serial.println(aqua_name);
        
        result += "{\"id\":\"" + aqua_uid + "\",\"name\":\"" + aqua_name + "\"";
        
        // Add aquarium info
        if (aquarium["measuring_unit"].is<const char*>()) result += ",\"measuring_unit\":\"" + aquarium["measuring_unit"].as<String>() + "\"";
        if (aquarium["water_volume"].is<int>()) result += ",\"water_volume\":" + String(aquarium["water_volume"].as<int>());
        if (aquarium["net_water_volume"].is<int>()) result += ",\"net_water_volume\":" + String(aquarium["net_water_volume"].as<int>());
        if (aquarium["online"].is<bool>()) result += ",\"online\":" + String(aquarium["online"].as<bool>() ? "true" : "false");
        if (aquarium["timezone_offset"].is<int>()) result += ",\"timezone_offset\":" + String(aquarium["timezone_offset"].as<int>());
        
        // Add system information as "device" info
        if (aquarium["system_series"].is<const char*>() || aquarium["serial_number"].is<const char*>()) {
          result += ",\"devices\":[{";
          result += "\"name\":\"" + aquarium["system_model"].as<String>() + "\"";
          result += ",\"type\":\"" + aquarium["system_type"].as<String>() + "\"";
          result += ",\"serial\":\"" + aquarium["serial_number"].as<String>() + "\"";
          result += ",\"firmware\":\"System Series: " + aquarium["system_series"].as<String>() + "\"";
          result += "}]";
          Serial.print("  System: ");
          Serial.println(aquarium["system_series"].as<String>());
        }
        
        result += "}";
      }
      
      result += "]}";
      Serial.print("Total aquariums: ");
      Serial.println(count);
      http.end();
      return result;
    } else {
      Serial.print("✗ JSON parse error: ");
      Serial.println(error.c_str());
    }
  } else if (httpCode == 401) {
    http.end();
    redseaToken = "";
    return redseaGetAquariums();
  } else {
    Serial.print("✗ Failed to fetch aquariums with code: ");
    Serial.println(httpCode);
  }
  
  http.end();
  return "{\"success\":false,\"message\":\"Failed to fetch aquariums\"}";
}

#endif // REDSEA_API_H
