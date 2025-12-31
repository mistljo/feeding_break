#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "credentials.h"

// External references
extern AsyncWebServer *configServer;
extern DNSServer *dnsServer;
extern bool wifiConfigMode;
extern unsigned long restartScheduledTime;

void setupWiFi();
void startConfigPortal();
void stopConfigPortal();
void handleConfigPortal();

void setupWiFi() {
  String ssid, password;
  
  if (loadWiFiCredentials(ssid, password)) {
    Serial.println("Initializing WiFi...");
    Serial.print("Found saved WiFi credentials\nConnecting to: ");
    Serial.println(ssid);
    
    // Use WiFi events for non-blocking connection
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Non-blocking wait with vTaskDelay to allow other tasks
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      vTaskDelay(pdMS_TO_TICKS(250));  // Use FreeRTOS delay instead of Arduino delay
      Serial.print(".");
      Serial.flush();
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✓ WiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal strength: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("✗ WiFi connection failed");
      Serial.println("Starting configuration portal...");
      startConfigPortal();
    }
  } else {
    Serial.println("No saved WiFi credentials found");
    Serial.println("Starting configuration portal...");
    Serial.flush();
    startConfigPortal();
  }
}

void startConfigPortal() {
  Serial.println("\n=================================");
  Serial.println("WiFi Configuration Mode");
  Serial.println("=================================");
  Serial.println("Starting Access Point...");
  Serial.flush();
  
  wifiConfigMode = true;
  
  // Blink LED slowly in config mode (shorter)
  if (LED_PIN >= 0) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }
  
  // IMPORTANT: Properly initialize WiFi and let LWIP stack fully start
  WiFi.mode(WIFI_OFF);
  delay(300);
  
  // Start AP mode - this initializes the TCP/IP stack properly
  WiFi.mode(WIFI_AP);
  delay(500);  // Critical: Give LWIP/TCP stack time to fully initialize
  
  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(500);  // Wait for AP to be fully ready
  
  if (!apStarted) {
    Serial.println("✗ Failed to start Access Point!");
    return;
  }
  
  IPAddress IP = WiFi.softAPIP();
  
  // Verify AP is really ready
  while (IP[0] == 0) {
    Serial.println("Waiting for AP IP...");
    delay(500);
    IP = WiFi.softAPIP();
  }
  
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(IP.toString());
  Serial.println("=================================\n");
  
  // Small additional delay to ensure TCP/IP stack is fully initialized
  delay(500);
  
  // Start DNS server for captive portal
  dnsServer = new DNSServer();
  dnsServer->start(53, "*", IP);
  
  // Small delay before creating web server
  delay(100);
  
  // Create configuration web server
  configServer = new AsyncWebServer(80);
  
  // Captive Portal: Redirect all requests to root
  configServer->onNotFound([](AsyncWebServerRequest *request){
    request->redirect("http://" + WiFi.softAPIP().toString() + "/");
  });
  
  // Serve configuration page
  configServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>WiFi Setup</title>";
    html += "<style>";
    html += "body{font-family:Arial;margin:0;padding:20px;background:#0066cc}";
    html += ".container{max-width:400px;margin:0 auto;background:#fff;padding:30px;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.3)}";
    html += "h1{color:#0066cc;margin-top:0;text-align:center}";
    html += "label{display:block;margin:15px 0 5px;font-weight:bold;color:#333}";
    html += "input,select{width:100%;padding:10px;border:2px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}";
    html += "input:focus,select:focus{outline:none;border-color:#0066cc}";
    html += "button{width:100%;padding:12px;background:#0066cc;color:#fff;border:none;border-radius:5px;font-size:16px;font-weight:bold;cursor:pointer;margin-top:20px}";
    html += "button:hover{background:#0052a3}";
    html += ".scan{background:#28a745;margin-bottom:10px}";
    html += ".scan:hover{background:#218838}";
    html += "#networks{display:none;margin:10px 0}";
    html += ".network{padding:10px;background:#f8f9fa;margin:5px 0;border-radius:5px;cursor:pointer;border:2px solid #ddd}";
    html += ".network:hover{background:#e9ecef;border-color:#0066cc}";
    html += ".loading{text-align:center;padding:20px;display:none}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>🐠 WiFi Setup</h1>";
    html += "<button class='scan' onclick='scanNetworks()'>📡 Scan Networks</button>";
    html += "<div class='loading' id='loading'>Scanning...</div>";
    html += "<div id='networks'></div>";
    html += "<form action='/save' method='POST'>";
    html += "<label>WiFi Network:</label>";
    html += "<input type='text' name='ssid' id='ssid' placeholder='Enter SSID' required>";
    html += "<label>Password:</label>";
    html += "<input type='password' name='password' placeholder='Enter Password' required>";
    html += "<button type='submit'>💾 Save & Connect</button>";
    html += "</form></div>";
    html += "<script>";
    html += "function scanNetworks(){";
    html += "document.getElementById('loading').style.display='block';";
    html += "document.getElementById('networks').style.display='none';";
    html += "pollScan();";
    html += "}";
    html += "function pollScan(){";
    html += "fetch('/scan').then(r=>r.json()).then(data=>{";
    html += "if(data.status=='started'||data.status=='scanning'){";
    html += "setTimeout(pollScan,2000);";
    html += "}else if(data.networks){";
    html += "document.getElementById('loading').style.display='none';";
    html += "let html='';";
    html += "data.networks.forEach(n=>{";
    html += "html+='<div class=\"network\" onclick=\"selectNetwork(\\''+n.ssid+'\\')\">';";
    html += "html+='📶 '+n.ssid+' ('+n.rssi+' dBm)'+(n.secure?' 🔒':'');";
    html += "html+='</div>';";
    html += "});";
    html += "document.getElementById('networks').innerHTML=html;";
    html += "document.getElementById('networks').style.display='block';";
    html += "}";
    html += "}).catch(e=>{document.getElementById('loading').innerHTML='Error: '+e;});";
    html += "}";
    html += "function selectNetwork(ssid){document.getElementById('ssid').value=ssid;}";
    html += "</script></body></html>";
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
  });
  
  // Handle WiFi scan
  configServer->on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int16_t scanResult = WiFi.scanComplete();
    
    if (scanResult == WIFI_SCAN_RUNNING) {
      request->send(202, "application/json", "{\"status\":\"scanning\"}");
      return;
    }
    
    if (scanResult >= 0) {
      String json = "{\"networks\":[";
      bool first = true;
      
      for (int i = 0; i < scanResult; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        
        bool isDuplicate = false;
        for (int j = 0; j < i; j++) {
          if (WiFi.SSID(j) == ssid && WiFi.RSSI(i) <= WiFi.RSSI(j)) {
            isDuplicate = true;
            break;
          }
        }
        
        if (!isDuplicate) {
          if (!first) json += ",";
          first = false;
          json += "{\"ssid\":\"" + ssid + "\",";
          json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
          json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
        }
      }
      
      json += "]}";
      WiFi.scanDelete();
      request->send(200, "application/json", json);
    } else {
      WiFi.scanNetworks(true);
      request->send(202, "application/json", "{\"status\":\"started\"}");
    }
  });
  
  // Handle save
  configServer->on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid = "";
    String password = "";
    
    if (request->hasParam("ssid", true)) {
      ssid = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
      password = request->getParam("password", true)->value();
    }
    
    if (ssid.length() > 0) {
      saveWiFiCredentials(ssid, password);
      
      String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
      html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
      html += "<title>Verbinden...</title>";
      html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#0066cc;color:#fff}";
      html += "h1{font-size:48px}";
      html += ".spinner{border:8px solid #f3f3f3;border-top:8px solid #fff;border-radius:50%;width:60px;height:60px;animation:spin 1s linear infinite;margin:30px auto}";
      html += "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}";
      html += "</style>";
      html += "<script>";
      html += "let attempts=0;";
      html += "function checkConnection(){";
      html += "attempts++;";
      html += "fetch('/api/status').then(r=>r.json()).then(data=>{";
      html += "if(data.connected && data.ip){";
      html += "document.getElementById('status').innerHTML='✓ Verbunden!<br>Weiterleitung zu '+data.ip+'...';";
      html += "setTimeout(function(){window.location.href='http://'+data.ip+'/';},2000);";
      html += "}else if(attempts<20){";
      html += "setTimeout(checkConnection,1000);";
      html += "}else{";
      html += "document.getElementById('status').innerHTML='⚠ Verbindung fehlgeschlagen<br>Bitte manuell zu 192.168.x.x verbinden';";
      html += "}";
      html += "}).catch(e=>{if(attempts<20)setTimeout(checkConnection,1000);});";
      html += "}";
      html += "setTimeout(checkConnection,5000);";
      html += "</script>";
      html += "</head><body>";
      html += "<h1>💾 Gespeichert!</h1>";
      html += "<p>SSID: " + ssid + "</p>";
      html += "<div class='spinner'></div>";
      html += "<p id='status'>Verbinde mit WiFi...</p>";
      html += "</body></html>";
      
      request->send(200, "text/html", html);
      
      restartScheduledTime = millis() + 3000;
      Serial.println("Restart scheduled in 3 seconds...");
    } else {
      request->send(400, "text/plain", "SSID required");
    }
  });
  
  configServer->begin();
  Serial.println("✓ Configuration portal started");
  Serial.println("Use display or web interface to configure WiFi");
  
  // NON-BLOCKING: Config portal runs alongside display
  // DNS and restart handled in handleConfigPortal()
}

// Process config portal requests (call in loop)
void handleConfigPortal() {
  if (!wifiConfigMode) return;
  
  if (dnsServer) {
    dnsServer->processNextRequest();
  }
  
  if (restartScheduledTime > 0 && millis() >= restartScheduledTime) {
    Serial.println("Restarting now...");
    ESP.restart();
  }
  
  // LED blinking in config mode
  if (LED_PIN >= 0) {
    digitalWrite(LED_PIN, (millis() / 500) % 2);
  }
}

void stopConfigPortal() {
  if (dnsServer) {
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;
  }
  
  if (configServer) {
    configServer->end();
    delete configServer;
    configServer = nullptr;
  }
  
  WiFi.softAPdisconnect(true);
  wifiConfigMode = false;
  
  Serial.println("✓ Config portal stopped");
}

#endif // WIFI_SETUP_H