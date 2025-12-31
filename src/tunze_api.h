#ifndef TUNZE_API_H
#define TUNZE_API_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include "config.h"

// External references
extern String TUNZE_USERNAME;
extern String TUNZE_PASSWORD;
extern String TUNZE_DEVICE_ID;
extern String tunzeSID;
extern WebSocketsClient tunzeWebSocket;
extern bool tunzeConnected;
extern unsigned long tunzeMessageId;

// Forward declaration
void tunzeWebSocketEvent(WStype_t type, uint8_t * payload, size_t length);

bool tunzeLogin() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return false;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  http.begin(client, "https://tunze-hub.com/action/login");
  http.setTimeout(10000);  // 10s timeout
  http.setConnectTimeout(5000);  // 5s connect timeout
  http.addHeader("Content-Type", "application/json");
  
  const char* headerKeys[] = {"Set-Cookie"};
  http.collectHeaders(headerKeys, 1);
  
  JsonDocument doc;
  doc["username"] = TUNZE_USERNAME.c_str();
  doc["password"] = TUNZE_PASSWORD.c_str();
  String postData;
  serializeJson(doc, postData);
  
  Serial.println("Logging into Tunze Hub...");
  int httpCode = http.POST(postData);
  
  if (httpCode == 200 || httpCode == 302) {
    String setCookie = http.header("Set-Cookie");
    
    int sidStart = setCookie.indexOf("SID=");
    if (sidStart >= 0) {
      sidStart += 4;
      int sidEnd = setCookie.indexOf(';', sidStart);
      if (sidEnd < 0) sidEnd = setCookie.length();
      tunzeSID = setCookie.substring(sidStart, sidEnd);
      Serial.println("✓ Tunze login successful");
      Serial.print("SID: ");
      Serial.println(tunzeSID.substring(0, 20) + "...");
      http.end();
      return true;
    } else {
      Serial.println("✗ No SID in Set-Cookie header");
    }
  }
  
  Serial.print("✗ Tunze login failed with code: ");
  Serial.println(httpCode);
  http.end();
  return false;
}

String tunzeGetDevices() {
  if (tunzeSID.isEmpty()) {
    Serial.println("No Tunze SID - logging in first...");
    if (!tunzeLogin()) {
      return "{\"success\":false,\"message\":\"Login failed\"}";
    }
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  http.begin(client, "https://tunze-hub.com/action/getDevices");
  http.setTimeout(8000);  // 8s timeout
  http.setConnectTimeout(4000);  // 4s connect timeout
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Cookie", "SID=" + tunzeSID);
  
  Serial.println("Fetching Tunze devices...");
  int httpCode = http.POST("{}");
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Tunze Devices Response:");
    Serial.println(payload);
    Serial.println("---");
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      String result = "{\"success\":true,\"devices\":[";
      
      bool first = true;
      
      // Parse gateways (controllers)
      if (doc["gateways"].is<JsonArray>()) {
        JsonArray gateways = doc["gateways"];
        for (JsonObject gw : gateways) {
          if (!first) result += ",";
          first = false;
          
          result += "{";
          result += "\"imei\":\"" + gw["imei"].as<String>() + "\"";
          result += ",\"name\":\"" + gw["name"].as<String>() + "\"";
          result += ",\"type\":\"Gateway\"";
          result += ",\"model\":\"" + gw["type"].as<String>() + "\"";
          result += ",\"firmware\":\"" + gw["firmware"]["version"].as<String>() + "\"";
          result += ",\"serial\":\"" + gw["sn"].as<String>() + "\"";
          result += "}";
          
          Serial.print("Gateway: ");
          Serial.print(gw["name"].as<String>());
          Serial.print(" (");
          Serial.print(gw["imei"].as<String>());
          Serial.println(")");
        }
      }
      
      // Parse endpoints (devices)
      if (doc["endpoints"].is<JsonArray>()) {
        JsonArray endpoints = doc["endpoints"];
        for (JsonObject ep : endpoints) {
          if (!first) result += ",";
          first = false;
          
          String deviceName = ep["name"].as<String>();
          if (deviceName.length() == 0) {
            deviceName = "Device " + ep["type"].as<String>();
          }
          
          result += "{";
          result += "\"imei\":\"" + ep["imei"].as<String>() + "\"";
          result += ",\"name\":\"" + deviceName + "\"";
          result += ",\"type\":\"Endpoint\"";
          result += ",\"model\":\"" + ep["type"].as<String>() + "\"";
          result += ",\"slot\":\"" + ep["slot"].as<String>() + "\"";
          result += "}";
          
          Serial.print("Endpoint: ");
          Serial.print(deviceName);
          Serial.print(" (");
          Serial.print(ep["type"].as<String>());
          Serial.println(")");
        }
      }
      
      result += "]}";
      http.end();
      return result;
    } else {
      Serial.print("✗ JSON parse error: ");
      Serial.println(error.c_str());
    }
  } else if (httpCode == 401) {
    http.end();
    tunzeSID = "";
    return tunzeGetDevices();
  } else {
    Serial.print("✗ Failed to fetch Tunze devices with code: ");
    Serial.println(httpCode);
  }
  
  http.end();
  return "{\"success\":false,\"message\":\"Failed to fetch devices\"}";
}

void tunzeConnect() {
  // Make sure WiFi is actually connected before attempting any network operations
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ Cannot connect to Tunze Hub - WiFi not connected");
    return;
  }
  
  if (tunzeSID.isEmpty()) {
    Serial.println("No Tunze SID - logging in first...");
    if (!tunzeLogin()) {
      return;
    }
  }
  
  Serial.println("Connecting to Tunze Hub WebSocket...");
  
  String cookieHeader = "Cookie: SID=" + tunzeSID;
  tunzeWebSocket.setExtraHeaders(cookieHeader.c_str());
  
  tunzeWebSocket.beginSSL(TUNZE_HUB_HOST, TUNZE_HUB_PORT, TUNZE_HUB_PATH);
  tunzeWebSocket.onEvent(tunzeWebSocketEvent);
  tunzeWebSocket.setReconnectInterval(5000);
}

void tunzeWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("✗ Tunze WebSocket disconnected");
      tunzeConnected = false;
      break;
      
    case WStype_CONNECTED:
      Serial.println("✓ Tunze WebSocket connected");
      {
        String authMsg = "{\"auth\":[[\"dev\",\"" + TUNZE_DEVICE_ID + "\"]]}";
        if (DEBUG_TUNZE) {
          Serial.print("Sending auth: ");
          Serial.println(authMsg);
        }
        tunzeWebSocket.sendTXT(authMsg);
      }
      break;
      
    case WStype_TEXT:
      {
        String message = String((char*)payload);
        if (DEBUG_TUNZE) {
          Serial.print("Tunze <- ");
          Serial.println(message);
        }
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (!error) {
          // Respond to ping
          if (doc["ping"].is<JsonArray>()) {
            unsigned long timestamp = millis();
            String pongMsg = "{\"pong\":[" + String(timestamp) + "]}";
            if (DEBUG_TUNZE) {
              Serial.print("Tunze -> ");
              Serial.println(pongMsg);
            }
            tunzeWebSocket.sendTXT(pongMsg);
          }
          
          // Check for auth confirmation
          if (doc["auth"].is<JsonObject>()) {
            Serial.println("✓ Tunze device authenticated");
            tunzeConnected = true;
          }
        }
      }
      break;
      
    case WStype_ERROR:
      Serial.println("✗ Tunze WebSocket error");
      break;
  }
}

void tunzeStartFeeding() {
  if (!tunzeConnected) {
    Serial.println("⚠ Tunze not connected - connecting now...");
    tunzeConnect();
    delay(2000);
  }
  
  if (!tunzeConnected) {
    Serial.println("✗ Tunze connection failed - skipping");
    return;
  }
  
  tunzeMessageId++;
  String feedMsg = "{\"mid\":" + String(tunzeMessageId) + ",\"";
  feedMsg += TUNZE_DEVICE_ID;
  feedMsg += "-1002-0001\":[[\"acts\",200,600]]}";
  
  if (DEBUG_TUNZE) {
    Serial.print("Tunze -> ");
    Serial.println(feedMsg);
  }
  tunzeWebSocket.sendTXT(feedMsg);
  Serial.println("✓ Tunze feeding mode started (10 min)");
}

void tunzeStopFeeding() {
  if (!tunzeConnected) {
    Serial.println("⚠ Tunze not connected - cannot stop");
    return;
  }
  
  tunzeMessageId++;
  String stopMsg = "{\"mid\":" + String(tunzeMessageId) + ",\"";
  stopMsg += TUNZE_DEVICE_ID;
  stopMsg += "-1002-0001\":[[\"deas\"]]}";
  
  if (DEBUG_TUNZE) {
    Serial.print("Tunze -> ");
    Serial.println(stopMsg);
  }
  tunzeWebSocket.sendTXT(stopMsg);
  Serial.println("✓ Tunze feeding mode stopped");
}

#endif // TUNZE_API_H
