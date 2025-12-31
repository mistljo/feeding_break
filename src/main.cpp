#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// Project headers
#include "config.h"
#include "credentials.h"
#include "redsea_api.h"
#include "tunze_api.h"
#include "tasmota_api.h"
#include "wifi_setup.h"
#include "display_lvgl.h"  // LVGL Display (replaces old display.h)

// Dynamic credentials (loaded from Preferences)
String redsea_USERNAME;
String redsea_PASSWORD;
String redsea_AQUARIUM_ID;
String redsea_AQUARIUM_NAME;
String TUNZE_USERNAME;
String TUNZE_PASSWORD;
String TUNZE_DEVICE_ID;
String TUNZE_DEVICE_NAME;

// Feature Configuration - default false until configured
bool ENABLE_redsea = false;
bool ENABLE_TUNZE = false;

// Global variables
String redseaToken = "";
String tunzeSID = "";
WebSocketsClient tunzeWebSocket;
bool tunzeConnected = false;
unsigned long tunzeMessageId = 5000;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
// debounceDelay is defined in config.h
bool feedingModeActive = false;

// WiFi Configuration
Preferences preferences;
AsyncWebServer *webServer = nullptr;
AsyncWebServer *configServer = nullptr;
DNSServer *dnsServer = nullptr;
bool wifiConfigMode = false;
unsigned long restartScheduledTime = 0;

// WiFi Reconnect handling
unsigned long wifiLastReconnectAttempt = 0;
const unsigned long wifiReconnectInterval = 30000;  // Try to reconnect every 30 seconds
bool wifiReconnecting = false;

// Time/NTP Configuration
String ntpServer = "pool.ntp.org";
String tzString = "CET-1CEST,M3.5.0,M10.5.0/3";  // Central European Time with DST

// Factory Reset
unsigned long factoryResetPressStart = 0;
bool factoryResetLEDBlinking = false;
bool factoryResetProcessed = false;

// Forward declarations
void setupWebServer();
void handleFactoryReset();
bool performFactoryReset();
void handleButton();
void startFeedingMode();
void stopFeedingMode();
void setupNTP();
void loadTimeConfig();
void saveTimeConfig();
String getTimezoneString(int tzIndex, bool dst);
int getCurrentTimezoneIndex();

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for Serial to be ready (CH340 board)
  
  // Suppress I2C master error logs (FT3168 touch occasionally sleeps)
  esp_log_level_set("i2c.master", ESP_LOG_NONE);
  
  Serial.println("\n\n=================================");
  Serial.println("Feeding Break Controller v2.0");
  Serial.println("With Touch Display Support");
  Serial.println("=================================\n");
  Serial.flush();
  
  // IMPORTANT: Set relay pins HIGH immediately to prevent clicking!
  // Relays are active LOW, so HIGH = OFF
  if (RELAY1_PIN >= 0) {
    pinMode(RELAY1_PIN, OUTPUT);
    digitalWrite(RELAY1_PIN, HIGH);
  }
  if (RELAY2_PIN >= 0) {
    pinMode(RELAY2_PIN, OUTPUT);
    digitalWrite(RELAY2_PIN, HIGH);
  }
  if (RELAY3_PIN >= 0) {
    pinMode(RELAY3_PIN, OUTPUT);
    digitalWrite(RELAY3_PIN, HIGH);
  }
  
  // Configure hardware pins (only if defined)
  if (BUTTON_PIN >= 0) pinMode(BUTTON_PIN, INPUT_PULLUP);
  if (FACTORY_RESET_PIN >= 0) pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
  if (LED_PIN >= 0) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
  }
  
  // Initialize preferences
  preferences.begin("feeding-break", false);
  
  // Load credentials from flash
  loadCredentials();
  tasmotaLoadConfig();  // Load Tasmota configuration
  
  // Setup WiFi BEFORE display to avoid watchdog issues
  setupWiFi();
  
  // Initialize Display after WiFi (LVGL timer can cause watchdog issues during WiFi connect)
  setupDisplay();
  
  // WiFi status shown via serial (LVGL UI shows status cards)
  Serial.print("WiFi verbunden! IP: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server
  setupWebServer();
  
  // Connect to Tunze Hub if enabled AND WiFi is connected (not in config portal mode)
  if (ENABLE_TUNZE && WiFi.status() == WL_CONNECTED && !wifiConfigMode) {
    Serial.println("\nConnecting to Tunze Hub...");
    delay(100);  // Small delay to ensure network stack is ready
    tunzeConnect();
  } else if (ENABLE_TUNZE) {
    Serial.println("\nTunze Hub: waiting for WiFi connection...");
  } else {
    Serial.println("\nTunze Hub disabled in configuration");
  }
  
  // Setup NTP time sync
  loadTimeConfig();
  if (WiFi.status() == WL_CONNECTED && !wifiConfigMode) {
    setupNTP();
  }
  
  Serial.println("\n=================================");
  Serial.println("System ready!");
  Serial.println("=================================");
  Serial.print("Web Interface: http://");
  Serial.println(WiFi.localIP());
  if (ENABLE_redsea) {
    Serial.println("Note: redsea login will happen on first button press.");
  }
  if (LED_PIN >= 0) digitalWrite(LED_PIN, HIGH); // System ready indicator
  
  // Show WiFi setup screen if not configured
  showWiFiSetupIfNeeded();
  
  // LVGL display ready - updateDisplay() will handle UI refresh
  updateDisplay();
}

// External function from settings_ui.h
extern void checkPendingRestart();

// WiFi Reconnect function
void handleWiFiReconnect() {
  // Skip if in config mode or already connected
  if (wifiConfigMode || WiFi.status() == WL_CONNECTED) {
    wifiReconnecting = false;
    return;
  }
  
  // Check if it's time to try reconnecting
  if (millis() - wifiLastReconnectAttempt < wifiReconnectInterval) {
    return;
  }
  
  if (!wifiReconnecting) {
    Serial.println("\n⚠ WiFi connection lost - attempting to reconnect...");
    wifiReconnecting = true;
  }
  
  wifiLastReconnectAttempt = millis();
  
  // Try to reconnect
  WiFi.disconnect();
  delay(100);
  WiFi.begin();  // Use stored credentials
  
  // Wait up to 10 seconds
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi reconnected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    wifiReconnecting = false;
    
    // Reconnect Tunze if enabled
    if (ENABLE_TUNZE && !tunzeConnected) {
      Serial.println("Reconnecting to Tunze Hub...");
      tunzeConnect();
    }
  } else {
    Serial.println("\n✗ WiFi reconnect failed - will retry in 30 seconds");
  }
}

void loop() {
  handleButton();
  handleFactoryReset();
  handleConfigPortal();  // Process WiFi config portal (non-blocking)
  handleWiFiReconnect(); // Monitor WiFi and auto-reconnect
  updateDisplay();       // LVGL tick + UI update (includes touch handling)
  checkPendingRestart(); // Check if factory reset requested restart
  
  // Keep WebSocket connection alive
  tunzeWebSocket.loop();
  
  delay(5);  // Shorter delay for smoother LVGL animations
}

// setupWiFi, startConfigPortal, stopConfigPortal are now in wifi_setup.h
// But wifi_setup.h's startConfigPortal() has blocking while(true) loop
// which includes the HTML server setup and maintenance

void setupWebServer() {
  // Create webserver instance
  webServer = new AsyncWebServer(80);
  
  // Main unified page
  webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Feeding Break Controller</title>";
    html += "<style>";
    html += "*{margin:0;padding:0;box-sizing:border-box}";
    html += "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:0}";
    html += ".container{max-width:600px;margin:80px auto 20px auto;background:#fff;border-radius:15px;box-shadow:0 10px 40px rgba(0,0,0,0.2);overflow:hidden}";
    html += ".header{background:linear-gradient(135deg,#2196F3,#1976D2);color:#fff;padding:20px;text-align:center;position:fixed;top:0;left:0;right:0;z-index:100;display:flex;align-items:center;justify-content:space-between}";
    html += ".header h1{font-size:24px;margin:0;flex:1;text-align:center}";
    html += ".hamburger{width:30px;height:25px;cursor:pointer;display:flex;flex-direction:column;justify-content:space-between;position:relative;z-index:101}";
    html += ".hamburger span{display:block;height:3px;background:#fff;border-radius:3px;transition:all 0.3s}";
    html += ".sidebar{width:280px;background:#f5f5f5;border-right:1px solid #ddd;padding-top:80px;position:fixed;left:0;top:0;bottom:0;transform:translateX(-100%);transition:transform 0.3s;z-index:99;overflow-y:auto}";
    html += ".sidebar.active{transform:translateX(0)}";
    html += ".sidebar-item{padding:15px 20px;cursor:pointer;border-left:4px solid transparent;transition:all 0.3s;color:#555;font-weight:bold;display:flex;align-items:center;gap:10px}";
    html += ".sidebar-item:hover{background:#fff;border-left-color:#2196F3;color:#2196F3}";
    html += ".sidebar-item.active{background:#fff;border-left-color:#2196F3;color:#2196F3}";
    html += ".sidebar-section{padding:10px 20px;color:#999;font-size:12px;font-weight:bold;text-transform:uppercase;letter-spacing:1px;margin-top:10px}";
    html += ".sidebar-item.sub{padding-left:40px;font-size:14px}";
    html += ".overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.5);z-index:98;display:none}";
    html += ".overlay.active{display:block}";
    html += ".content{padding:30px}";
    html += ".section{display:none}";
    html += ".section.active{display:block}";
    html += ".section h2{color:#2196F3;margin-bottom:15px;font-size:20px}";
    html += ".status-card{text-align:center;padding:40px;margin:20px 0;border-radius:10px;transition:all 0.3s}";
    html += ".status-card.status-active{background:linear-gradient(135deg,#4CAF50,#45a049);color:#fff;box-shadow:0 4px 15px rgba(76,175,80,0.4)}";
    html += ".status-card.inactive{background:linear-gradient(135deg,#f44336,#d32f2f);color:#fff;box-shadow:0 4px 15px rgba(244,67,54,0.4)}";
    html += ".status-icon{font-size:60px;margin-bottom:15px}";
    html += ".status-text{font-size:24px;font-weight:bold;margin-bottom:10px}";
    html += ".status-detail{font-size:14px;opacity:0.9}";
    html += ".btn-group{display:flex;gap:10px;margin:20px 0}";
    html += "button{width:100%;padding:15px;font-size:16px;font-weight:bold;border:none;border-radius:8px;cursor:pointer;transition:all 0.3s;box-shadow:0 4px 10px rgba(0,0,0,0.2);margin-top:10px}";
    html += "button:hover{transform:translateY(-2px);box-shadow:0 6px 15px rgba(0,0,0,0.3)}";
    html += ".btn-start{background:linear-gradient(135deg,#4CAF50,#45a049);color:#fff}";
    html += ".btn-stop{background:linear-gradient(135deg,#f44336,#d32f2f);color:#fff}";
    html += ".btn-save{background:linear-gradient(135deg,#4CAF50,#45a049);color:#fff}";
    html += ".btn-danger{background:linear-gradient(135deg,#f44336,#d32f2f);color:#fff}";
    html += ".form-group{margin:15px 0}";
    html += "label{display:block;font-weight:bold;color:#555;margin-bottom:5px}";
    html += "input{width:100%;padding:12px;border:2px solid #ddd;border-radius:6px;font-size:14px;transition:border 0.3s}";
    html += "input:focus{outline:none;border-color:#2196F3}";
    html += "select{width:100%;padding:12px;border:2px solid #ddd;border-radius:6px;font-size:14px;transition:border 0.3s}";
    html += ".message{padding:15px;margin:15px 0;border-radius:6px;display:none}";
    html += ".message.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}";
    html += ".message.error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}";
    html += ".pwd-toggle{cursor:pointer;color:#2196F3;font-size:12px;margin-top:5px;display:inline-block}";
    html += ".toggle-container{display:flex;align-items:center;margin-bottom:15px;padding:10px;background:#f9f9f9;border-radius:6px}";
    html += ".toggle-container label{margin:0;flex:1}";
    html += "input[type='checkbox']{width:auto;margin-left:10px}";
    html += ".info-grid{display:grid;gap:10px;margin:15px 0}";
    html += ".info-item{background:#f9f9f9;padding:12px;border-radius:6px;display:flex;justify-content:space-between;align-items:center}";
    html += ".info-label{font-weight:bold;color:#555}";
    html += ".info-value{color:#2196F3;font-weight:bold}";
    html += ".warning{background:#fff3cd;border:1px solid #ffc107;color:#856404;padding:12px;border-radius:6px;margin:10px 0}";
    html += ".device-card{border:1px solid #333;border-radius:8px;margin:10px 0;overflow:hidden;background:#1a1a1a}";
    html += ".device-header{padding:12px 15px;background:#2d2d2d;cursor:pointer;display:flex;justify-content:space-between;align-items:center;transition:background 0.2s}";
    html += ".device-header:hover{background:#363636}";
    html += ".device-name{font-weight:600;color:#3498db;font-size:16px}";
    html += ".device-toggle{color:#999;font-size:12px;transition:transform 0.3s}";
    html += ".device-details{padding:15px;background:#1e1e1e}";
    html += ".device-info-row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #2a2a2a}";
    html += ".device-info-row:last-child{border-bottom:none}";
    html += ".device-info-label{color:#999;font-size:13px;flex:0 0 120px}";
    html += ".device-info-value{color:#fff;font-size:13px;text-align:right;word-break:break-all}";
    html += ".device-owner{color:#3498db;font-weight:600;margin:10px 0 5px 0;padding-top:10px;border-top:2px solid #333}";
    html += "@media(max-width:768px){.container{margin:80px 10px 20px 10px;border-radius:10px}.content{padding:20px}.sidebar{width:250px}}";
    html += "</style>";
    html += "<script>";
    html += "let isUpdating=false;";
    html += "let aquariums=[];";
    html += "function toggleMenu(){";
    html += "document.getElementById('sidebar').classList.toggle('active');";
    html += "document.getElementById('overlay').classList.toggle('active');";
    html += "}";
    html += "function showSection(sectionId){";
    html += "document.querySelectorAll('.section').forEach(s=>s.classList.remove('active'));";
    html += "document.querySelectorAll('.sidebar-item').forEach(i=>i.classList.remove('active'));";
    html += "document.getElementById(sectionId).classList.add('active');";
    html += "event.target.classList.add('active');";
    html += "toggleMenu();";
    html += "if(sectionId==='section-control')updateStatus();";
    html += "if(sectionId==='section-device'){loadScreensaverSettings();loadTimeSettings();}";
    html += "}";
    html += "var tasmotaStatusTimer=null;";
    html += "function updateStatus(){";
    html += "const controller=new AbortController();";
    html += "const timeoutId=setTimeout(()=>controller.abort(),5000);";
    html += "fetch('/api/status',{signal:controller.signal}).then(r=>r.json()).then(data=>{";
    html += "clearTimeout(timeoutId);";
    html += "const card=document.getElementById('statusCard');";
    html += "const icon=document.getElementById('statusIcon');";
    html += "const text=document.getElementById('statusText');";
    html += "const detail=document.getElementById('statusDetail');";
    html += "const startBtn=document.getElementById('startBtn');";
    html += "const stopBtn=document.getElementById('stopBtn');";
    html += "if(data.feeding_active){";
    html += "card.className='status-card status-active';";
    html += "icon.textContent='🟢';";
    html += "text.textContent='Fütterungsmodus AKTIV';";
    html += "detail.textContent='Pumpen sind pausiert';";
    html += "startBtn.style.display='none';";
    html += "stopBtn.style.display='block';";
    html += "startTasmotaStatusPolling();";
    html += "}else{";
    html += "card.className='status-card inactive';";
    html += "icon.textContent='🔴';";
    html += "text.textContent='Fütterungsmodus INAKTIV';";
    html += "detail.textContent='Normaler Betrieb';";
    html += "startBtn.style.display='block';";
    html += "stopBtn.style.display='none';";
    html += "stopTasmotaStatusPolling();";
    html += "hideTasmotaStatus();";
    html += "}";
    html += "if(document.getElementById('deviceIPCtrl'))document.getElementById('deviceIPCtrl').textContent=data.ip;";
    html += "if(document.getElementById('wifiSignalCtrl'))document.getElementById('wifiSignalCtrl').textContent=data.wifi_rssi+' dBm';";
    html += "if(document.getElementById('deviceIP'))document.getElementById('deviceIP').textContent=data.ip;";
    html += "if(document.getElementById('wifiSignal'))document.getElementById('wifiSignal').textContent=data.wifi_rssi+' dBm';";
    html += "}).catch(e=>{";
    html += "clearTimeout(timeoutId);";
    html += "if(e.name!=='AbortError')console.error('Status update failed:',e);";
    html += "});";
    html += "}";
    html += "function startTasmotaStatusPolling(){";
    html += "if(tasmotaStatusTimer)return;";
    html += "updateTasmotaStatus();";
    html += "tasmotaStatusTimer=setInterval(updateTasmotaStatus,8000);";
    html += "}";
    html += "function stopTasmotaStatusPolling(){";
    html += "if(tasmotaStatusTimer){clearInterval(tasmotaStatusTimer);tasmotaStatusTimer=null;}";
    html += "}";
    html += "function hideTasmotaStatus(){";
    html += "var el=document.getElementById('tasmotaStatusBox');";
    html += "if(el)el.style.display='none';";
    html += "}";
    html += "function updateTasmotaStatus(){";
    html += "fetch('/api/tasmota-status').then(r=>r.json()).then(data=>{";
    html += "var el=document.getElementById('tasmotaStatusBox');";
    html += "if(!el)return;";
    html += "if(!data.enabled||data.devices.length===0){el.style.display='none';return;}";
    html += "el.style.display='block';";
    html += "var h='<div style=\"font-weight:bold;margin-bottom:10px;color:#fff\">🔌 Tasmota Geräte ('+data.completedDevices+'/'+data.totalDevices+' fertig)</div>';";
    html += "data.devices.forEach(function(d){";
    html += "var stateIcon=d.powerState?'🟢':'🔴';";
    html += "var stateText=d.powerState?'AN':'AUS';";
    html += "var statusCol=d.completed?'#4CAF50':'#ff9800';";
    html += "var statusText=d.completed?'✓ Fertig':'⏳ Warte...';";
    html += "h+='<div style=\"display:flex;justify-content:space-between;align-items:center;padding:8px;background:#1a1a1a;border-radius:4px;margin:4px 0\">';";
    html += "h+='<span style=\"color:#fff\">'+d.name+'</span>';";
    html += "h+='<span style=\"display:flex;gap:10px;align-items:center\">';";
    html += "h+='<span style=\"color:'+(d.powerState?'#4CAF50':'#f44336')+'\">'+stateIcon+' '+stateText+'</span>';";
    html += "h+='<span style=\"color:'+statusCol+';font-size:12px\">'+statusText+'</span>';";
    html += "h+='</span></div>';";
    html += "});";
    html += "if(data.allComplete){";
    html += "h+='<div style=\"margin-top:10px;padding:10px;background:#1b5e20;border-radius:4px;text-align:center;color:#fff\">✓ Alle Geräte zurückgesetzt!</div>';";
    html += "setTimeout(function(){updateStatus();stopTasmotaStatusPolling();hideTasmotaStatus();},1500);";
    html += "}";
    html += "el.innerHTML=h;";
    html += "}).catch(function(e){console.error(e);});";
    html += "}";
    html += "function toggleFeeding(action){";
    html += "if(isUpdating)return;";
    html += "isUpdating=true;";
    html += "const statusCard=document.getElementById('statusCard');";
    html += "statusCard.style.opacity='0.6';";
    html += "fetch('/api/feeding/'+action,{method:'POST'})";
    html += ".then(r=>r.json())";
    html += ".then(data=>{";
    html += "if(data.success){";
    html += "updateStatus();";
    html += "}else{";
    html += "alert('Fehler: '+data.message);";
    html += "}";
    html += "statusCard.style.opacity='1';";
    html += "isUpdating=false;";
    html += "}).catch(e=>{";
    html += "alert('Netzwerkfehler: '+e);";
    html += "statusCard.style.opacity='1';";
    html += "isUpdating=false;";
    html += "});";
    html += "}";
    html += "function loadSettings(){";
    html += "fetch('/api/settings').then(r=>r.json()).then(data=>{";
    html += "document.getElementById('redseaUser').value=data.redsea_username;";
    html += "document.getElementById('redseaPass').value=data.redsea_password;";
    html += "document.getElementById('redseaAquaId').value=data.redsea_aquarium_id;";
    html += "if(data.redsea_aquarium_id&&data.redsea_aquarium_name){";
    html += "document.getElementById('redseaAquaName').textContent=data.redsea_aquarium_name;";
    html += "}else{";
    html += "document.getElementById('redseaAquaName').textContent=data.redsea_aquarium_id?'Gespeichert':'Nicht gesetzt';";
    html += "}";
    html += "document.getElementById('enableredsea').checked=data.enable_redsea;";
    html += "document.getElementById('tunzeUser').value=data.tunze_username;";
    html += "document.getElementById('tunzePass').value=data.tunze_password;";
    html += "document.getElementById('tunzeDevId').value=data.tunze_device_id;";
    html += "const tunzeNameField=document.getElementById('tunzeDeviceName');";
    html += "if(data.tunze_device_id&&data.tunze_device_name){";
    html += "tunzeNameField.textContent=data.tunze_device_name;";
    html += "tunzeNameField.setAttribute('data-device-name',data.tunze_device_name);";
    html += "}else{";
    html += "tunzeNameField.textContent=data.tunze_device_id?'Gespeichert':'Nicht gesetzt';";
    html += "tunzeNameField.removeAttribute('data-device-name');";
    html += "}";
    html += "document.getElementById('enableTunze').checked=data.enable_tunze;";
    html += "}).catch(e=>console.error('Error:',e));";
    html += "}";
    html += "function loadAquariums(event){";
    html += "const btn=event.target;";
    html += "const select=document.getElementById('redseaAquaSelect');";
    html += "const user=document.getElementById('redseaUser').value;";
    html += "const pass=document.getElementById('redseaPass').value;";
    html += "if(!user||!pass){alert('Bitte zuerst Benutzername und Passwort eingeben und speichern!');return;}";
    html += "btn.disabled=true;";
    html += "btn.textContent='⏳ Laden...';";
    html += "const tempData={redsea_username:user,redsea_password:pass,redsea_aquarium_id:document.getElementById('redseaAquaId').value,tunze_username:document.getElementById('tunzeUser').value,tunze_password:document.getElementById('tunzePass').value,tunze_device_id:document.getElementById('tunzeDevId').value,enable_redsea:document.getElementById('enableredsea').checked,enable_tunze:document.getElementById('enableTunze').checked};";
    html += "fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(tempData)}).then(()=>{";
    html += "fetch('/api/aquariums').then(r=>r.json()).then(data=>{";
    html += "if(data.success&&data.aquariums){";
    html += "aquariums=data.aquariums;";
    html += "select.innerHTML='<option value=\"\">-- Aquarium auswählen --</option>';";
    html += "aquariums.forEach(a=>{";
    html += "const opt=document.createElement('option');";
    html += "opt.value=a.id;";
    html += "opt.textContent=a.name;";
    html += "select.appendChild(opt);";
    html += "});";
    html += "const currentId=document.getElementById('redseaAquaId').value;";
    html += "if(currentId)select.value=currentId;";
    html += "populateDeviceInfo(data.aquariums);";
    html += "btn.textContent='✓ Geladen ('+aquariums.length+')';";
    html += "setTimeout(()=>{btn.textContent='🔄 Laden';btn.disabled=false;},2000);";
    html += "}else{";
    html += "alert('Fehler: '+(data.message||'Keine Aquarien gefunden'));";
    html += "btn.textContent='✗ Fehler';";
    html += "setTimeout(()=>{btn.textContent='🔄 Laden';btn.disabled=false;},2000);";
    html += "}";
    html += "}).catch(e=>{";
    html += "alert('Netzwerkfehler: '+e);";
    html += "btn.textContent='✗ Fehler';";
    html += "setTimeout(()=>{btn.textContent='🔄 Laden';btn.disabled=false;},2000);";
    html += "});";
    html += "}).catch(e=>{alert('Fehler beim Speichern: '+e);btn.textContent='🔄 Laden';btn.disabled=false;});";
    html += "}";
    html += "function selectAquarium(){";
    html += "const select=document.getElementById('redseaAquaSelect');";
    html += "const idField=document.getElementById('redseaAquaId');";
    html += "const nameField=document.getElementById('redseaAquaName');";
    html += "if(select.value){";
    html += "idField.value=select.value;";
    html += "nameField.textContent=select.options[select.selectedIndex].text;";
    html += "}else{";
    html += "idField.value='';";
    html += "nameField.textContent='Nicht gesetzt';";
    html += "}";
    html += "}";
    html += "function loadTunzeDevices(event){";
    html += "const btn=event.target;";
    html += "const select=document.getElementById('tunzeDeviceSelect');";
    html += "const user=document.getElementById('tunzeUser').value;";
    html += "const pass=document.getElementById('tunzePass').value;";
    html += "if(!user||!pass){alert('Bitte zuerst Benutzername und Passwort eingeben und speichern!');return;}";
    html += "btn.disabled=true;";
    html += "btn.textContent='⏳ Laden...';";
    html += "const tempData={redsea_username:document.getElementById('redseaUser').value,redsea_password:document.getElementById('redseaPass').value,redsea_aquarium_id:document.getElementById('redseaAquaId').value,tunze_username:user,tunze_password:pass,tunze_device_id:document.getElementById('tunzeDevId').value,enable_redsea:document.getElementById('enableredsea').checked,enable_tunze:document.getElementById('enableTunze').checked};";
    html += "fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(tempData)}).then(()=>{";
    html += "fetch('/api/tunze-devices').then(r=>r.json()).then(data=>{";
    html += "if(data.success&&data.devices){";
    html += "const devices=data.devices;";
    html += "select.innerHTML='<option value=\"\">-- Device auswählen --</option>';";
    html += "devices.forEach(d=>{";
    html += "const opt=document.createElement('option');";
    html += "opt.value=d.imei;";
    html += "opt.textContent=d.name+' ('+d.model+')';";
    html += "select.appendChild(opt);";
    html += "});";
    html += "const currentId=document.getElementById('tunzeDevId').value;";
    html += "if(currentId)select.value=currentId;";
    html += "populateTunzeDeviceInfo(devices);";
    html += "btn.textContent='✓ Geladen ('+devices.length+')';";
    html += "setTimeout(()=>{btn.textContent='🔄 Laden';btn.disabled=false;},2000);";
    html += "}else{";
    html += "alert('Fehler: '+(data.message||'Keine Devices gefunden'));";
    html += "btn.textContent='✗ Fehler';";
    html += "setTimeout(()=>{btn.textContent='🔄 Laden';btn.disabled=false;},2000);";
    html += "}";
    html += "}).catch(e=>{";
    html += "alert('Netzwerkfehler: '+e);";
    html += "btn.textContent='✗ Fehler';";
    html += "setTimeout(()=>{btn.textContent='🔄 Laden';btn.disabled=false;},2000);";
    html += "});";
    html += "}).catch(e=>{alert('Fehler beim Speichern: '+e);btn.textContent='🔄 Laden';btn.disabled=false;});";
    html += "}";
    html += "function selectTunzeDevice(){";
    html += "const select=document.getElementById('tunzeDeviceSelect');";
    html += "const idField=document.getElementById('tunzeDevId');";
    html += "const nameField=document.getElementById('tunzeDeviceName');";
    html += "if(select.value){";
    html += "idField.value=select.value;";
    html += "const selectedText=select.options[select.selectedIndex].text;";
    html += "nameField.textContent=selectedText;";
    html += "nameField.setAttribute('data-device-name',selectedText);";
    html += "}else{";
    html += "idField.value='';";
    html += "nameField.textContent='Nicht gesetzt';";
    html += "nameField.removeAttribute('data-device-name');";
    html += "}";
    html += "}";
    html += "function populateTunzeDeviceInfo(devices){";
    html += "const container=document.getElementById('tunzeDeviceInfo');";
    html += "if(!devices||devices.length===0){container.innerHTML='<p style=\"color:#999;text-align:center\">Keine Geräte gefunden</p>';return;}";
    html += "let content='';";
    html += "devices.forEach((d,idx)=>{";
    html += "content+='<div class=\"device-card\">';";
    html += "content+='<div class=\"device-header\" onclick=\"toggleDeviceDetails(\\'tunze-'+idx+'\\')\">';";
    html += "content+='<span class=\"device-name\">'+d.name+'</span>';";
    html += "content+='<span class=\"device-toggle\" id=\"toggle-tunze-'+idx+'\">▼</span>';";
    html += "content+='</div>';";
    html += "content+='<div class=\"device-details\" id=\"details-tunze-'+idx+'\" style=\"display:none\">';";
    html += "content+='<div class=\"device-info-row\"><span class=\"device-info-label\">IMEI:</span><span class=\"device-info-value\">'+d.imei+'</span></div>';";
    html += "content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Typ:</span><span class=\"device-info-value\">'+d.type+'</span></div>';";
    html += "content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Modell:</span><span class=\"device-info-value\">'+d.model+'</span></div>';";
    html += "if(d.serial)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Seriennr.:</span><span class=\"device-info-value\">'+d.serial+'</span></div>';";
    html += "if(d.firmware)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Firmware:</span><span class=\"device-info-value\">'+d.firmware+'</span></div>';";
    html += "if(d.slot)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Slot:</span><span class=\"device-info-value\">'+d.slot+'</span></div>';";
    html += "content+='</div>';";
    html += "content+='</div>';";
    html += "});";
    html += "container.innerHTML=content;";
    html += "}";
    html += "function populateDeviceInfo(aquariums){";
    html += "const container=document.getElementById('redseaDeviceInfo');";
    html += "if(!aquariums||aquariums.length===0){container.innerHTML='<p style=\"color:#999;text-align:center\">Keine Geräte gefunden</p>';return;}";
    html += "let content='';";
    html += "aquariums.forEach((a,idx)=>{";
    html += "content+='<div class=\"device-card\">';";
    html += "content+='<div class=\"device-header\" onclick=\"toggleDeviceDetails('+idx+')\">';";
    html += "content+='<span class=\"device-name\">'+a.name+'</span>';";
    html += "content+='<span class=\"device-toggle\" id=\"toggle-'+idx+'\">▼</span>';";
    html += "content+='</div>';";
    html += "content+='<div class=\"device-details\" id=\"details-'+idx+'\" style=\"display:none\">';";
    html += "content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Aquarium ID:</span><span class=\"device-info-value\">'+a.id+'</span></div>';";
    html += "if(a.measuring_unit)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Volumen-Einheit:</span><span class=\"device-info-value\">'+a.measuring_unit+'</span></div>';";
    html += "if(a.water_volume)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Brutto-Volumen:</span><span class=\"device-info-value\">'+a.water_volume+' '+(a.measuring_unit||'')+'</span></div>';";
    html += "if(a.net_water_volume)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Netto-Volumen:</span><span class=\"device-info-value\">'+a.net_water_volume+' '+(a.measuring_unit||'')+'</span></div>';";
    html += "if(a.online!==undefined)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Online:</span><span class=\"device-info-value\">'+(a.online?'✓ Ja':'✗ Nein')+'</span></div>';";
    html += "if(a.timezone_offset!==undefined)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Zeitzone:</span><span class=\"device-info-value\">UTC'+(a.timezone_offset>=0?'+':'')+Math.round(a.timezone_offset/60)+'h</span></div>';";
    html += "if(a.owner){";
    html += "content+='<div class=\"device-owner\">👤 Besitzer</div>';";
    html += "if(a.owner.name)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Name:</span><span class=\"device-info-value\">'+a.owner.name+'</span></div>';";
    html += "if(a.owner.email)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">E-Mail:</span><span class=\"device-info-value\">'+a.owner.email+'</span></div>';";
    html += "if(a.owner.country)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Land:</span><span class=\"device-info-value\">'+a.owner.country+'</span></div>';";
    html += "}";
    html += "if(a.devices&&a.devices.length>0){";
    html += "content+='<div class=\"device-owner\" style=\"margin-top:10px\">🔧 Geräte</div>';";
    html += "a.devices.forEach(d=>{";
    html += "content+='<div style=\"padding:8px;border-left:3px solid #3498db;margin:5px 0;background:#1e1e1e\">';";
    html += "if(d.name)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Name:</span><span class=\"device-info-value\">'+d.name+'</span></div>';";
    html += "if(d.type)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Typ:</span><span class=\"device-info-value\">'+d.type+'</span></div>';";
    html += "if(d.serial)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Seriennr.:</span><span class=\"device-info-value\">'+d.serial+'</span></div>';";
    html += "if(d.firmware)content+='<div class=\"device-info-row\"><span class=\"device-info-label\">Firmware:</span><span class=\"device-info-value\">'+d.firmware+'</span></div>';";
    html += "content+='</div>';";
    html += "});";
    html += "}";
    html += "content+='</div>';";
    html += "content+='</div>';";
    html += "});";
    html += "container.innerHTML=content;";
    html += "}";
    html += "function toggleDeviceDetails(idx){";
    html += "const details=document.getElementById('details-'+idx);";
    html += "const toggle=document.getElementById('toggle-'+idx);";
    html += "if(details.style.display==='none'){";
    html += "details.style.display='block';";
    html += "toggle.textContent='▲';";
    html += "}else{";
    html += "details.style.display='none';";
    html += "toggle.textContent='▼';";
    html += "}";
    html += "}";
    html += "function saveSettings(){";
    html += "const data={";
    html += "redsea_username:document.getElementById('redseaUser').value,";
    html += "redsea_password:document.getElementById('redseaPass').value,";
    html += "redsea_aquarium_id:document.getElementById('redseaAquaId').value,";
    html += "redsea_aquarium_name:document.getElementById('redseaAquaName').textContent==='Nicht gesetzt'?'':document.getElementById('redseaAquaName').textContent,";
    html += "enable_redsea:document.getElementById('enableredsea').checked,";
    html += "tunze_username:document.getElementById('tunzeUser').value,";
    html += "tunze_password:document.getElementById('tunzePass').value,";
    html += "tunze_device_id:document.getElementById('tunzeDevId').value,";
    html += "tunze_device_name:(document.getElementById('tunzeDeviceName').getAttribute('data-device-name')||''),";
    html += "enable_tunze:document.getElementById('enableTunze').checked";
    html += "};";
    html += "const saveBtn=event.target;";
    html += "const originalText=saveBtn.textContent;";
    html += "saveBtn.disabled=true;";
    html += "saveBtn.textContent='💾 Speichere...';";
    html += "fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})";
    html += ".then(r=>r.json()).then(result=>{";
    html += "if(result.success){";
    html += "alert('✓ Einstellungen erfolgreich gespeichert!');";
    html += "saveBtn.textContent='✓ Gespeichert';";
    html += "setTimeout(()=>{saveBtn.textContent=originalText;saveBtn.disabled=false;},2000);";
    html += "}else{";
    html += "alert('✗ Fehler beim Speichern');";
    html += "saveBtn.textContent=originalText;";
    html += "saveBtn.disabled=false;";
    html += "}";
    html += "}).catch(e=>{";
    html += "alert('✗ Netzwerkfehler: '+e);";
    html += "saveBtn.textContent=originalText;";
    html += "saveBtn.disabled=false;";
    html += "});";
    html += "}";
    html += "function togglePassword(id){";
    html += "const input=document.getElementById(id);";
    html += "input.type=input.type==='password'?'text':'password';";
    html += "}";
    html += "function confirmFactoryReset(){";
    html += "if(!confirm('⚠️ WARNUNG: Alle Einstellungen und WiFi-Daten werden gelöscht!\\n\\nMöchten Sie wirklich fortfahren?'))return;";
    html += "if(!confirm('🚨 LETZTE WARNUNG!\\n\\nDas Gerät wird auf Werkseinstellungen zurückgesetzt und neu gestartet.\\n\\nWirklich fortfahren?'))return;";
    html += "const resetBtn=document.getElementById('resetBtn');";
    html += "resetBtn.disabled=true;";
    html += "resetBtn.textContent='🔄 Wird zurückgesetzt...';";
    html += "fetch('/api/factory-reset',{method:'POST'})";
    html += ".then(r=>r.json()).then(result=>{";
    html += "if(result.success){";
    html += "alert('✓ Werksreset erfolgreich! Das Gerät startet neu.');";
    html += "}else{";
    html += "alert('✗ Fehler beim Werksreset');";
    html += "resetBtn.disabled=false;";
    html += "resetBtn.textContent='⚠️ Werksreset';";
    html += "}";
    html += "}).catch(e=>{";
    html += "alert('✗ Fehler: '+e);";
    html += "resetBtn.disabled=false;";
    html += "resetBtn.textContent='⚠️ Werksreset';";
    html += "});";
    html += "}";
    html += "function loadScreensaverSettings(){";
    html += "fetch('/api/screensaver-settings').then(r=>r.json()).then(data=>{";
    html += "document.getElementById('screensaverTimeout').value=data.timeout;";
    html += "}).catch(e=>console.error('Error loading screensaver settings:',e));";
    html += "}";
    html += "function saveScreensaverSettings(){";
    html += "const timeout=parseInt(document.getElementById('screensaverTimeout').value)||0;";
    html += "const msg=document.getElementById('screensaverMessage');";
    html += "fetch('/api/screensaver-settings',{";
    html += "method:'POST',";
    html += "headers:{'Content-Type':'application/json'},";
    html += "body:JSON.stringify({timeout:timeout})";
    html += "}).then(r=>r.json()).then(result=>{";
    html += "msg.className='message success';";
    html += "msg.textContent='✓ Screensaver-Einstellungen gespeichert!';";
    html += "msg.style.display='block';";
    html += "setTimeout(()=>msg.style.display='none',3000);";
    html += "}).catch(e=>{";
    html += "msg.className='message error';";
    html += "msg.textContent='✗ Fehler beim Speichern';";
    html += "msg.style.display='block';";
    html += "});";
    html += "}";
    // Time settings JavaScript functions
    html += "function loadTimeSettings(){";
    html += "fetch('/api/time-settings').then(r=>r.json()).then(data=>{";
    html += "document.getElementById('timezoneSelect').value=data.timezone_index;";
    html += "document.getElementById('currentTime').textContent=data.current_time;";
    html += "}).catch(e=>console.error('Time settings load error:',e));";
    html += "}";
    html += "function saveTimeSettings(){";
    html += "const tzIndex=parseInt(document.getElementById('timezoneSelect').value);";
    html += "const msg=document.getElementById('timeMessage');";
    html += "fetch('/api/time-settings',{";
    html += "method:'POST',";
    html += "headers:{'Content-Type':'application/json'},";
    html += "body:JSON.stringify({timezone_index:tzIndex})";
    html += "}).then(r=>r.json()).then(result=>{";
    html += "msg.className='message success';";
    html += "msg.textContent='✓ Zeiteinstellungen gespeichert! Zeit wird synchronisiert...';";
    html += "msg.style.display='block';";
    html += "setTimeout(()=>{msg.style.display='none';loadTimeSettings();},3000);";
    html += "}).catch(e=>{";
    html += "msg.className='message error';";
    html += "msg.textContent='✗ Fehler beim Speichern';";
    html += "msg.style.display='block';";
    html += "});";
    html += "}";
    // Tasmota JavaScript functions
    html += "var tasmotaDevices=[];";
    html += "function loadTasmotaSettings(){";
    html += "fetch('/api/tasmota-settings').then(function(r){return r.json();}).then(function(data){";
    html += "document.getElementById('enableTasmota').checked=data.enabled;";
    html += "document.getElementById('tasmotaPulseTime').value=Math.round(data.pulseTime/60);";
    html += "tasmotaDevices=data.devices||[];";
    html += "renderTasmotaDevices();";
    html += "}).catch(function(e){console.error('Tasmota load error:',e);});";
    html += "}";
    html += "function renderTasmotaDevices(){";
    html += "var c=document.getElementById('tasmotaDeviceList');";
    html += "if(!tasmotaDevices||tasmotaDevices.length===0){";
    html += "c.innerHTML='<p style=\"color:#999;text-align:center;padding:20px\">Keine Geraete. Klicke Scannen.</p>';";
    html += "return;}";
    html += "var h='';";
    html += "for(var i=0;i<tasmotaDevices.length;i++){";
    html += "var d=tasmotaDevices[i];";
    html += "var col=d.reachable?(d.powerState?'#4CAF50':'#f44336'):'#999';";
    html += "var st=d.reachable?(d.powerState?'AN':'AUS'):'Offline';";
    html += "var actionText=d.turnOn?'Einschalten':'Ausschalten';";
    html += "var actionCol=d.turnOn?'#4CAF50':'#f44336';";
    html += "h+='<div style=\"padding:12px;border-bottom:1px solid #333;background:#1a1a1a;margin:2px 0;border-radius:4px\">';";
    html += "h+='<div style=\"display:flex;justify-content:space-between;align-items:center\">';";
    html += "h+='<label style=\"display:flex;align-items:center;gap:8px\"><input type=\"checkbox\" data-idx=\"'+i+'\" '+(d.enabled?'checked':'')+' onchange=\"tasmotaDeviceToggle(this)\"> <b style=\"color:#fff\">'+d.name+'</b></label>';";
    html += "h+='<span style=\"color:'+col+';font-size:12px\">'+st+'</span></div>';";
    html += "h+='<div style=\"font-size:11px;color:#888;margin:4px 0 8px 26px\">'+d.ip+'</div>';";
    html += "h+='<div style=\"margin-left:26px;display:flex;align-items:center;gap:8px\">';";
    html += "h+='<span style=\"color:#aaa;font-size:12px\">Bei Futtermodus:</span>';";
    html += "h+='<select data-idx=\"'+i+'\" onchange=\"tasmotaActionChange(this)\" style=\"padding:4px 8px;border-radius:4px;border:1px solid #444;background:#2a2a2a;color:#fff;font-size:12px\">';";
    html += "h+='<option value=\"off\" '+(d.turnOn?'':'selected')+'>🔴 Ausschalten</option>';";
    html += "h+='<option value=\"on\" '+(d.turnOn?'selected':'')+'>🟢 Einschalten</option>';";
    html += "h+='</select></div></div>';}";
    html += "c.innerHTML=h;}";
    html += "function tasmotaActionChange(el){var i=parseInt(el.dataset.idx);if(tasmotaDevices[i])tasmotaDevices[i].turnOn=(el.value==='on');}";
    html += "var scanPollTimer=null;";
    html += "function scanTasmota(){";
    html += "var btn=document.getElementById('scanTasmotaBtn');";
    html += "var status=document.getElementById('scanStatus');";
    html += "btn.disabled=true;btn.textContent='Starte...';";
    html += "status.style.display='block';status.innerHTML='Starte Netzwerkscan...';";
    html += "fetch('/api/tasmota-scan').then(function(r){return r.json();}).then(function(data){";
    html += "if(data.success){pollScanResults();}";
    html += "else{btn.textContent='Fehler';status.style.display='none';setTimeout(function(){btn.textContent='Netzwerk scannen';btn.disabled=false;},2000);}";
    html += "}).catch(function(e){btn.textContent='Fehler';status.style.display='none';setTimeout(function(){btn.textContent='Netzwerk scannen';btn.disabled=false;},2000);});";
    html += "}";
    html += "function pollScanResults(){";
    html += "fetch('/api/tasmota-scan-results').then(function(r){return r.json();}).then(function(data){";
    html += "var btn=document.getElementById('scanTasmotaBtn');";
    html += "var status=document.getElementById('scanStatus');";
    html += "if(data.scanning){";
    html += "var pct=Math.round((data.progress/254)*100);";
    html += "btn.textContent='Scanne '+pct+'%';";
    html += "status.innerHTML='<div style=\"margin-bottom:8px;color:#fff\">Scanne IP '+data.progress+'/254</div><div style=\"background:#333;border-radius:4px;height:8px;overflow:hidden\"><div style=\"background:#4CAF50;height:100%;width:'+pct+'%\"></div></div><div style=\"margin-top:5px;color:#4CAF50\">Gefunden: '+data.found+' Geraete</div>';";
    html += "setTimeout(pollScanResults,800);}";
    html += "else{status.style.display='none';";
    html += "if(data.devices&&data.devices.length>0){tasmotaDevices=data.devices;renderTasmotaDevices();btn.textContent='Gefunden: '+data.count;}";
    html += "else{btn.textContent='Nichts gefunden';}";
    html += "setTimeout(function(){btn.textContent='Netzwerk scannen';btn.disabled=false;},2500);}";
    html += "}).catch(function(e){console.error(e);setTimeout(pollScanResults,2000);});";
    html += "}";
    html += "function tasmotaDeviceToggle(el){var i=parseInt(el.dataset.idx);if(tasmotaDevices[i])tasmotaDevices[i].enabled=el.checked;}";
    html += "function tasmotaToggleChanged(){}";
    html += "function testTasmota(ip){alert('Test: '+ip);}";
    html += "function saveTasmotaSettings(){";
    html += "var d={enabled:document.getElementById('enableTasmota').checked,";
    html += "pulseTime:parseInt(document.getElementById('tasmotaPulseTime').value||'15')*60,";
    html += "devices:tasmotaDevices};";
    html += "fetch('/api/tasmota-settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})";
    html += ".then(function(r){return r.json();}).then(function(res){";
    html += "if(res.success){alert('Gespeichert!');}else{alert('Fehler');}";
    html += "}).catch(function(e){alert('Fehler: '+e);});";
    html += "}";
    html += "updateStatus();";
    html += "loadSettings();";
    html += "loadTasmotaSettings();";
    html += "setInterval(updateStatus,5000);";
    html += "</script>";
    html += "</head><body>";
    html += "<div class='header'>";
    html += "<div class='hamburger' onclick='toggleMenu()'><span></span><span></span><span></span></div>";
    html += "<h1>🐠 Feeding Break</h1>";
    html += "<div style='width:30px'></div>";
    html += "</div>";
    html += "<div id='overlay' class='overlay' onclick='toggleMenu()'></div>";
    html += "<div id='sidebar' class='sidebar'>";
    html += "<div class='sidebar-item active' onclick='showSection(\"section-control\")'>🎮 Steuerung</div>";
    html += "<div class='sidebar-section'>Einstellungen</div>";
    html += "<div class='sidebar-item sub' onclick='showSection(\"section-redsea\")'>🌊 Red Sea</div>";
    html += "<div class='sidebar-item sub' onclick='showSection(\"section-tunze\")'>🌀 Tunze Hub</div>";
    html += "<div class='sidebar-item sub' onclick='showSection(\"section-tasmota\")'>🔌 Tasmota</div>";
    html += "<div class='sidebar-item sub' onclick='showSection(\"section-device\")'>📱 Geräteinfo</div>";
    html += "<div class='sidebar-item sub' onclick='showSection(\"section-reset\")'>⚠️ Werksreset</div>";
    html += "</div>";
    html += "<div class='container'>";
    html += "<div class='content'>";
    
    // Control Section (Default)
    html += "<div id='section-control' class='section active'>";
    html += "<div id='statusCard' class='status-card inactive'>";
    html += "<div id='statusIcon' class='status-icon'>🔴</div>";
    html += "<div id='statusText' class='status-text'>Lade Status...</div>";
    html += "<div id='statusDetail' class='status-detail'></div>";
    html += "</div>";
    html += "<div class='btn-group'>";
    html += "<button id='startBtn' class='btn-start' onclick='toggleFeeding(\"start\")'>▶ Starten</button>";
    html += "<button id='stopBtn' class='btn-stop' onclick='toggleFeeding(\"stop\")' style='display:none'>⏹ Stoppen</button>";
    html += "</div>";
    html += "<div id='tasmotaStatusBox' style='display:none;margin:15px 0;padding:15px;background:#2a2a2a;border-radius:8px;border:1px solid #444'></div>";
    html += "</div>";
    
    // Red Sea Section
    html += "<div id='section-redsea' class='section'>";
    html += "<h2>🌊 Red Sea Cloud</h2>";
    html += "<div class='toggle-container'>";
    html += "<label>Red Sea Cloud aktivieren</label>";
    html += "<input type='checkbox' id='enableredsea' checked>";
    html += "</div>";
    html += "<form onsubmit='return false;'>";
    html += "<div class='form-group'>";
    html += "<label>Benutzername / E-Mail</label>";
    html += "<input type='text' id='redseaUser' placeholder='E-Mail-Adresse' autocomplete='username'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Passwort</label>";
    html += "<input type='password' id='redseaPass' placeholder='Passwort' autocomplete='current-password'>";
    html += "<span class='pwd-toggle' onclick='togglePassword(\"redseaPass\")'>👁️ Anzeigen/Verstecken</span>";
    html += "</div>";
    html += "</form>";
    html += "<div class='form-group'>";
    html += "<label>Aquarium</label>";
    html += "<div style='display:flex;gap:10px'>";
    html += "<select id='redseaAquaSelect' style='flex:1' onchange='selectAquarium()'>";
    html += "<option value=''>-- Aquarium auswählen --</option>";
    html += "</select>";
    html += "<button type='button' id='loadAquaBtn' onclick='loadAquariums(event)' style='width:auto;padding:12px 20px;margin:0'>🔄 Laden</button>";
    html += "</div>";
    html += "<div style='margin-top:10px;padding:10px;background:#f9f9f9;border-radius:6px;display:flex;justify-content:space-between'>";
    html += "<span style='color:#666;font-size:13px'>Ausgewähltes Aquarium:</span>";
    html += "<span id='redseaAquaName' style='color:#2196F3;font-weight:600;font-size:13px'>Nicht gesetzt</span>";
    html += "</div>";
    html += "<input type='hidden' id='redseaAquaId'>";
    html += "</div>";
    html += "<div id='redseaDeviceInfo' style='margin:20px 0'></div>";
    html += "<button class='btn-save' onclick='saveSettings()'>💾 Speichern</button>";
    html += "</div>";
    
    // Tunze Section
    html += "<div id='section-tunze' class='section'>";
    html += "<h2>🌀 Tunze Hub</h2>";
    html += "<div class='toggle-container'>";
    html += "<label>Tunze Hub aktivieren</label>";
    html += "<input type='checkbox' id='enableTunze' checked>";
    html += "</div>";
    html += "<form onsubmit='return false;'>";
    html += "<div class='form-group'>";
    html += "<label>Benutzername / E-Mail</label>";
    html += "<input type='text' id='tunzeUser' placeholder='E-Mail-Adresse' autocomplete='username'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Passwort</label>";
    html += "<input type='password' id='tunzePass' placeholder='Passwort' autocomplete='current-password'>";
    html += "<span class='pwd-toggle' onclick='togglePassword(\"tunzePass\")'>👁️ Anzeigen/Verstecken</span>";
    html += "</div>";
    html += "</form>";
    html += "<div class='form-group'>";
    html += "<label>Device (Controller/Gateway)</label>";
    html += "<div style='display:flex;gap:10px'>";
    html += "<select id='tunzeDeviceSelect' style='flex:1' onchange='selectTunzeDevice()'>";
    html += "<option value=''>-- Device auswählen --</option>";
    html += "</select>";
    html += "<button type='button' id='loadTunzeBtn' onclick='loadTunzeDevices(event)' style='width:auto;padding:12px 20px;margin:0'>🔄 Laden</button>";
    html += "</div>";
    html += "<div style='margin-top:10px;padding:10px;background:#f9f9f9;border-radius:6px;display:flex;justify-content:space-between'>";
    html += "<span style='color:#666;font-size:13px'>Ausgewähltes Device:</span>";
    html += "<span id='tunzeDeviceName' style='color:#2196F3;font-weight:600;font-size:13px'>Nicht gesetzt</span>";
    html += "</div>";
    html += "<input type='hidden' id='tunzeDevId'>";
    html += "</div>";
    html += "<div id='tunzeDeviceInfo' style='margin:20px 0'></div>";
    html += "<button class='btn-save' onclick='saveSettings()'>💾 Speichern</button>";
    html += "</div>";
    
    // Tasmota Section
    html += "<div id='section-tasmota' class='section'>";
    html += "<h2>🔌 Tasmota Steckdosen</h2>";
    html += "<p style='color:#666;margin-bottom:15px'>Schalte WLAN-Steckdosen mit Tasmota-Firmware während der Fütterung aus (z.B. Skimmer, UV-C).</p>";
    html += "<div class='toggle-container'>";
    html += "<label>Tasmota Steuerung aktivieren</label>";
    html += "<input type='checkbox' id='enableTasmota' onchange='tasmotaToggleChanged()'>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Auto-Einschalten nach (Minuten)</label>";
    html += "<input type='number' id='tasmotaPulseTime' placeholder='15' min='1' max='600' value='15'>";
    html += "<small style='color:#666;display:block;margin-top:5px'>Geräte schalten nach dieser Zeit automatisch wieder ein (0 = nur manuell)</small>";
    html += "</div>";
    html += "<div class='form-group'>";
    html += "<label>Gefundene Geräte</label>";
    html += "<button type='button' id='scanTasmotaBtn' onclick='scanTasmota()' style='width:100%;margin-bottom:10px;background:linear-gradient(135deg,#2196F3,#1976D2);color:#fff'>🔍 Netzwerk scannen</button>";
    html += "<div id='scanStatus' style='display:none;background:#1a1a1a;padding:15px;border-radius:6px;margin-bottom:10px;text-align:center'></div>";
    html += "<div id='tasmotaDeviceList' style='border:1px solid #ddd;border-radius:6px;max-height:300px;overflow-y:auto'>";
    html += "<p style='color:#999;text-align:center;padding:20px'>Klicke auf 'Netzwerk scannen' um Tasmota-Geräte zu finden</p>";
    html += "</div>";
    html += "</div>";
    html += "<button class='btn-save' onclick='saveTasmotaSettings()'>💾 Speichern</button>";
    html += "</div>";
    
    // Device Information Section
    html += "<div id='section-device' class='section'>";
    html += "<h2>📱 Geräteinformationen</h2>";
    html += "<div class='info-grid'>";
    html += "<div class='info-item'><span class='info-label'>Device IP</span><span id='deviceIP' class='info-value'>-</span></div>";
    html += "<div class='info-item'><span class='info-label'>WiFi Signal</span><span id='wifiSignal' class='info-value'>-</span></div>";
    html += "<div class='info-item'><span class='info-label'>Aktuelle Zeit</span><span id='currentTime' class='info-value'>-</span></div>";
    html += "</div>";
    html += "<h2 style='margin-top:20px'>🕐 Zeiteinstellungen</h2>";
    html += "<div class='form-group'>";
    html += "<label>Zeitzone</label>";
    html += "<select id='timezoneSelect'>";
    html += "<option value='0'>UTC</option>";
    html += "<option value='1'>Westeuropa (UK, Portugal)</option>";
    html += "<option value='2'>Mitteleuropa (DE, AT, CH)</option>";
    html += "<option value='3'>Osteuropa</option>";
    html += "<option value='4'>Moskau</option>";
    html += "<option value='5'>US Eastern</option>";
    html += "<option value='6'>US Central</option>";
    html += "<option value='7'>US Pacific</option>";
    html += "</select>";
    html += "<p style='font-size:12px;color:#888;margin-top:5px'>Sommer-/Winterzeit wird automatisch umgestellt</p>";
    html += "</div>";
    html += "<button class='btn-save' onclick='saveTimeSettings()'>💾 Zeit speichern</button>";
    html += "<div id='timeMessage' class='message'></div>";
    html += "<h2 style='margin-top:20px'>⏱️ Bildschirmschoner</h2>";
    html += "<div class='form-group'>";
    html += "<label>Timeout (Sekunden)</label>";
    html += "<input type='number' id='screensaverTimeout' min='0' max='3600' placeholder='60'>";
    html += "<small style='color:#666;display:block;margin-top:5px'>0 = deaktiviert</small>";
    html += "</div>";
    html += "<button class='btn-save' onclick='saveScreensaverSettings()'>💾 Speichern</button>";
    html += "<div id='screensaverMessage' class='message'></div>";
    html += "</div>";
    
    // Factory Reset Section
    html += "<div id='section-reset' class='section'>";
    html += "<h2>⚠️ Werksreset</h2>";
    html += "<div class='warning'>";
    html += "<strong>⚠️ Achtung:</strong> Der Werksreset löscht alle gespeicherten Einstellungen, WiFi-Daten und Zugangsdaten. Das Gerät wird auf die Werkseinstellungen zurückgesetzt und neu gestartet.";
    html += "</div>";
    html += "<button id='resetBtn' class='btn-danger' onclick='confirmFactoryReset()'>⚠️ Werksreset</button>";
    html += "</div>";
    html += "</div>";
    html += "</div></body></html>";
    request->send(200, "text/html", html);
  });
  
  // Redirect /settings to main page
  webServer->on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/");
  });
  
  // API: Get Status
  webServer->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"feeding_active\":" + String(feedingModeActive ? "true" : "false") + ",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"redsea_enabled\":" + String(ENABLE_redsea ? "true" : "false") + ",";
    json += "\"tunze_enabled\":" + String(ENABLE_TUNZE ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // API: Start Feeding
  webServer->on("/api/feeding/start", HTTP_POST, [](AsyncWebServerRequest *request){
    startFeedingMode();
    String json = "{\"success\":true,\"message\":\"Feeding mode started\"}";
    request->send(200, "application/json", json);
  });
  
  // API: Stop Feeding
  webServer->on("/api/feeding/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    stopFeedingMode();
    String json = "{\"success\":true,\"message\":\"Feeding mode stopped\"}";
    request->send(200, "application/json", json);
  });
  
  // API: Get Settings
  webServer->on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"redsea_username\":\"" + redsea_USERNAME + "\",";
    json += "\"redsea_password\":\"" + redsea_PASSWORD + "\",";
    json += "\"redsea_aquarium_id\":\"" + redsea_AQUARIUM_ID + "\",";
    json += "\"redsea_aquarium_name\":\"" + redsea_AQUARIUM_NAME + "\",";
    json += "\"enable_redsea\":" + String(ENABLE_redsea ? "true" : "false") + ",";
    json += "\"tunze_username\":\"" + TUNZE_USERNAME + "\",";
    json += "\"tunze_password\":\"" + TUNZE_PASSWORD + "\",";
    json += "\"tunze_device_id\":\"" + TUNZE_DEVICE_ID + "\",";
    json += "\"tunze_device_name\":\"" + TUNZE_DEVICE_NAME + "\",";
    json += "\"enable_tunze\":" + String(ENABLE_TUNZE ? "true" : "false") + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI());
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // API: Get Aquariums from redsea
  webServer->on("/api/aquariums", HTTP_GET, [](AsyncWebServerRequest *request){
    String result = redseaGetAquariums();
    request->send(200, "application/json", result);
  });
  
  // API: Get Tunze Devices
  webServer->on("/api/tunze-devices", HTTP_GET, [](AsyncWebServerRequest *request){
    String result = tunzeGetDevices();
    request->send(200, "application/json", result);
  });
  
  // API: Save Settings
  webServer->on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      // Parse JSON body
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (const char*)data);
      
      if (!error) {
        // Update credentials
        redsea_USERNAME = doc["redsea_username"].as<String>();
        redsea_PASSWORD = doc["redsea_password"].as<String>();
        redsea_AQUARIUM_ID = doc["redsea_aquarium_id"].as<String>();
        if (doc["redsea_aquarium_name"].is<String>()) {
          redsea_AQUARIUM_NAME = doc["redsea_aquarium_name"].as<String>();
        }
        ENABLE_redsea = doc["enable_redsea"] | false;
        TUNZE_USERNAME = doc["tunze_username"].as<String>();
        TUNZE_PASSWORD = doc["tunze_password"].as<String>();
        TUNZE_DEVICE_ID = doc["tunze_device_id"].as<String>();
        if (doc["tunze_device_name"].is<String>()) {
          TUNZE_DEVICE_NAME = doc["tunze_device_name"].as<String>();
        }
        ENABLE_TUNZE = doc["enable_tunze"] | false;
        
        // Save to flash
        saveCredentials();
        
        // Clear tokens to force re-login
        redseaToken = "";
        tunzeSID = "";
        
        String json = "{\"success\":true,\"message\":\"Settings saved\"}";
        request->send(200, "application/json", json);
      } else {
        String json = "{\"success\":false,\"message\":\"JSON parse error\"}";
        request->send(400, "application/json", json);
      }
    }
  );
  
  // API: Factory Reset
  webServer->on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("Factory reset requested via API");
    String json = "{\"success\":true,\"message\":\"Factory reset initiated\"}";
    request->send(200, "application/json", json);
    
    // Perform factory reset after a short delay
    delay(500);
    performFactoryReset();
  });
  
  // API endpoint for WiFi connection status
  webServer->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
    json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI());
    json += "}";
    request->send(200, "application/json", json);
  });
  
  // Favicon handler (prevent 500 error)
  webServer->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(204); // No Content
  });
  
  // === Tasmota API Endpoints ===
  
  // Get Tasmota settings
  webServer->on("/api/tasmota-settings", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", tasmotaGetSettings());
  });
  
  // Save Tasmota settings
  webServer->on("/api/tasmota-settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String json = String((char*)data).substring(0, len);
      bool success = tasmotaUpdateSettings(json);
      String result = "{\"success\":" + String(success ? "true" : "false") + "}";
      request->send(200, "application/json", result);
    }
  );
  
  // Scan network for Tasmota devices (starts background task)
  webServer->on("/api/tasmota-scan", HTTP_GET, [](AsyncWebServerRequest *request){
    String result = tasmotaStartScan();
    request->send(200, "application/json", result);
  });
  
  // Get scan results
  webServer->on("/api/tasmota-scan-results", HTTP_GET, [](AsyncWebServerRequest *request){
    String result = tasmotaGetScanResults();
    request->send(200, "application/json", result);
  });
  
  // Get Tasmota feeding status
  webServer->on("/api/tasmota-status", HTTP_GET, [](AsyncWebServerRequest *request){
    String result = tasmotaGetFeedingStatus();
    request->send(200, "application/json", result);
  });
  
  // Screensaver settings endpoints
  webServer->on("/api/screensaver-settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{\"timeout\":" + String(getScreensaverTimeout()) + "}";
    request->send(200, "application/json", json);
  });
  
  webServer->on("/api/screensaver-settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String json = String((char*)data).substring(0, len);
      JsonDocument doc;
      deserializeJson(doc, json);
      int timeout = doc["timeout"].as<int>();
      setScreensaverTimeout(timeout);
      saveScreensaverTimeout();
      String result = "{\"success\":true}";
      request->send(200, "application/json", result);
    }
  );
  
  // Time/NTP settings endpoints
  webServer->on("/api/time-settings", HTTP_GET, [](AsyncWebServerRequest *request){
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%d.%m.%Y %H:%M:%S", &timeinfo);
    
    String json = "{\"timezone_index\":" + String(getCurrentTimezoneIndex()) + 
                  ",\"ntp_server\":\"" + ntpServer + "\"" +
                  ",\"current_time\":\"" + String(timeStr) + "\"}";
    request->send(200, "application/json", json);
  });
  
  webServer->on("/api/time-settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String json = String((char*)data).substring(0, len);
      JsonDocument doc;
      deserializeJson(doc, json);
      
      int tzIndex = doc["timezone_index"].as<int>();
      if (doc["ntp_server"].is<const char*>()) {
        ntpServer = doc["ntp_server"].as<String>();
      }
      
      tzString = getTimezoneString(tzIndex, true);  // Always use DST rules
      saveTimeConfig();
      setupNTP();  // Reconfigure with new settings
      
      String result = "{\"success\":true}";
      request->send(200, "application/json", result);
    }
  );
  
  // Test a Tasmota device
  webServer->on("/api/tasmota-test", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String json = String((char*)data).substring(0, len);
      JsonDocument doc;
      deserializeJson(doc, json);
      String ip = doc["ip"].as<String>();
      bool success = tasmotaTestDevice(ip);
      String result = "{\"success\":" + String(success ? "true" : "false") + "}";
      request->send(200, "application/json", result);
    }
  );
  
  webServer->begin();
  Serial.println("✓ Web server started");
}

void handleFactoryReset() {
  // Skip if no factory reset pin defined (use web interface instead)
  if (FACTORY_RESET_PIN < 0) return;
  
  int resetButtonState = digitalRead(FACTORY_RESET_PIN);
  
  if (resetButtonState == LOW) {
    // Button is pressed
    if (factoryResetPressStart == 0) {
      // Just started pressing
      factoryResetPressStart = millis();
      Serial.println("Factory reset button pressed...");
    }
    
    unsigned long pressDuration = millis() - factoryResetPressStart;
    
    // After 5 seconds, start blinking LED fast
    if (pressDuration >= 5000 && !factoryResetLEDBlinking) {
      factoryResetLEDBlinking = true;
      Serial.println("Hold for 5 more seconds to factory reset...");
    }
    
    // Blink LED fast when past 5 seconds
    if (factoryResetLEDBlinking && LED_PIN >= 0) {
      digitalWrite(LED_PIN, (millis() / 100) % 2); // Fast blink (100ms)
    }
    
    // After 10 seconds total, perform factory reset
    if (pressDuration >= 10000 && !factoryResetProcessed) {
      factoryResetProcessed = true;
      performFactoryReset();
    }
  } else {
    // Button released - reset state
    if (factoryResetPressStart > 0) {
      unsigned long pressDuration = millis() - factoryResetPressStart;
      if (pressDuration < 10000) {
        Serial.println("Factory reset cancelled (released too early)");
      }
      factoryResetPressStart = 0;
      factoryResetLEDBlinking = false;
      factoryResetProcessed = false;
      if (LED_PIN >= 0) digitalWrite(LED_PIN, feedingModeActive ? LOW : HIGH); // Restore normal LED state
    }
  }
}

bool performFactoryReset() {
  Serial.println("\n=================================");
  Serial.println("PERFORMING FACTORY RESET");
  Serial.println("=================================");
  
  // Visual feedback - rapid blink (only if LED available)
  if (LED_PIN >= 0) {
    for (int i = 0; i < 20; i++) {
      digitalWrite(LED_PIN, i % 2);
      delay(50);
    }
  }
  
  // Clear WiFi credentials
  Serial.println("Clearing WiFi credentials...");
  preferences.remove("wifi_ssid");
  preferences.remove("wifi_pass");
  
  // Clear other preferences
  Serial.println("Clearing stored preferences...");
  preferences.clear();
  
  // Clear redsea and Tunze tokens
  redseaToken = "";
  tunzeSID = "";
  
  Serial.println("✓ Factory reset complete!");
  Serial.println("Restarting in 3 seconds...");
  
  // Final LED indication (only if LED available)
  if (LED_PIN >= 0) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
    }
  }
  
  delay(3000);
  ESP.restart();
  
  return true;
}

// All Red Sea and Tunze API functions are now in redsea_api.h and tunze_api.h

void handleButton() {
  // Skip if no button pin defined (use touch display instead)
  if (BUTTON_PIN < 0) return;
  
  static bool buttonProcessed = false;
  static unsigned long lastCheck = 0;
  
  // Limit check frequency to avoid flooding
  if (millis() - lastCheck < 50) return;
  lastCheck = millis();
  
  int reading = digitalRead(BUTTON_PIN);
  
  if (reading == LOW && !buttonProcessed) {
    // Button is pressed and hasn't been processed yet
    delay(50); // Simple debounce
    if (digitalRead(BUTTON_PIN) == LOW) { // Confirm still pressed
      Serial.println("\n=== BOOT BUTTON PRESSED ===");
      Serial.print("Local status: ");
      Serial.println(feedingModeActive ? "ACTIVE" : "INACTIVE");
      
      // Check cloud status to sync (only if redsea enabled)
      if (ENABLE_redsea) {
        bool cloudStatus = redseaCheckFeedingStatus();
        if (cloudStatus != feedingModeActive) {
          Serial.println("⚠ Syncing with cloud status...");
          feedingModeActive = cloudStatus;
        }
      }
      
      if (feedingModeActive) {
        stopFeedingMode();
      } else {
        startFeedingMode();
      }
      
      buttonProcessed = true;
    }
  }
  
  if (reading == HIGH) {
    // Button released, ready for next press
    buttonProcessed = false;
  }
}

void startFeedingMode() {
  // Blink LED to indicate button press (only if LED available)
  if (LED_PIN >= 0) {
    for (int i = 0; i < 6; i++) {
      digitalWrite(LED_PIN, i % 2 == 0 ? LOW : HIGH);
      delay(100);
    }
  }
  
  Serial.println("Starting feeding mode...");
  
  bool redseaSuccess = true;
  bool tunzeSuccess = true;
  
  // Start redsea feeding mode (if enabled)
  if (ENABLE_redsea) {
    redseaSuccess = redseaStartFeeding();
  } else {
    Serial.println("⊘ redsea disabled - skipping");
  }
  
  // Start Tunze feeding mode (if enabled)
  if (ENABLE_TUNZE) {
    tunzeStartFeeding();
  } else {
    Serial.println("⊘ Tunze disabled - skipping");
  }
  
  // Start Tasmota feeding mode (turn off devices)
  tasmotaStartFeeding();
  
  // Update status if any system succeeded
  if (redseaSuccess || !ENABLE_redsea) {
    feedingModeActive = true;
    Serial.println("✓ Feeding mode STARTED");
    Serial.println("=== FEEDING MODE ACTIVE ===\n");
  } else {
    Serial.println("✗ Feeding mode start failed");
    Serial.println("=== FEEDING MODE INACTIVE ===\n");
  }
  
  // Force UI update
  updateDisplay();
}

void stopFeedingMode() {
  // Blink LED fast to indicate stop (only if LED available)
  if (LED_PIN >= 0) {
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_PIN, i % 2 == 0 ? LOW : HIGH);
      delay(50);
    }
  }
  
  Serial.println("Stopping feeding mode...");
  
  bool redseaSuccess = true;
  bool tunzeSuccess = true;
  
  // Stop redsea feeding mode (if enabled)
  if (ENABLE_redsea) {
    redseaSuccess = redseaStopFeeding();
  } else {
    Serial.println("⊘ redsea disabled - skipping");
  }
  
  // Stop Tunze feeding mode (if enabled)
  if (ENABLE_TUNZE) {
    tunzeStopFeeding();
  } else {
    Serial.println("⊘ Tunze disabled - skipping");
  }
  
  // Stop Tasmota feeding mode (turn on devices)
  tasmotaStopFeeding();
  
  // Always update status (even if API calls failed, we want local state to reflect stop)
  feedingModeActive = false;
  Serial.println("✓ Feeding mode STOPPED");
  Serial.println("=== FEEDING MODE INACTIVE ===\n");
  
  // Force UI update
  updateDisplay();
}

// ============================================================
// NTP Time Configuration
// ============================================================
void loadTimeConfig() {
  Preferences prefs;
  prefs.begin("feeding-break", true);  // true = read-only
  
  // Load with default values, suppress errors for first run
  String loadedTz = prefs.getString("timezone", "");
  String loadedNtp = prefs.getString("ntp_server", "");
  
  prefs.end();
  
  // Check if we need to initialize with defaults
  bool needsSave = false;
  
  if (loadedTz.length() > 0) {
    tzString = loadedTz;
  } else {
    tzString = "CET-1CEST,M3.5.0,M10.5.0/3";  // Default for Germany
    needsSave = true;
  }
  
  if (loadedNtp.length() > 0) {
    ntpServer = loadedNtp;
  } else {
    ntpServer = "pool.ntp.org";  // Default
    needsSave = true;
  }
  
  // Save defaults on first boot to avoid error logs next time
  if (needsSave) {
    Preferences prefsWrite;
    prefsWrite.begin("feeding-break", false);
    prefsWrite.putString("timezone", tzString);
    prefsWrite.putString("ntp_server", ntpServer);
    prefsWrite.end();
    Serial.println("Time config initialized with defaults");
  }
  
  Serial.println("Time config loaded: TZ=" + tzString);
}

void saveTimeConfig() {
  Preferences prefs;
  prefs.begin("feeding-break", false);
  prefs.putString("timezone", tzString);
  prefs.putString("ntp_server", ntpServer);
  prefs.end();
  Serial.println("Time config saved");
}

void setupNTP() {
  Serial.println("Setting up NTP time sync...");
  configTzTime(tzString.c_str(), ntpServer.c_str());
  
  // Wait for time to be set (max 10 seconds)
  int retry = 0;
  while (time(nullptr) < 1000000000 && retry < 20) {
    for (int i = 0; i < 5; i++) {
      delay(100);
      lv_timer_handler();  // Keep LVGL alive
    }
    Serial.print(".");
    yield();
    retry++;
  }
  
  if (time(nullptr) > 1000000000) {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    Serial.println();
    Serial.print("NTP time synced: ");
    Serial.println(&timeinfo, "%d.%m.%Y %H:%M:%S");
  } else {
    Serial.println("\nNTP sync failed - will retry later");
  }
}

// Timezone presets for easy selection
String getTimezoneString(int tzIndex, bool dst) {
  // Common timezones with DST rules
  switch(tzIndex) {
    case 0:  // UTC
      return "UTC0";
    case 1:  // Western European (UK, Portugal)
      return dst ? "WET0WEST,M3.5.0/1,M10.5.0" : "WET0";
    case 2:  // Central European (Germany, Austria, Switzerland)
      return dst ? "CET-1CEST,M3.5.0,M10.5.0/3" : "CET-1";
    case 3:  // Eastern European
      return dst ? "EET-2EEST,M3.5.0/3,M10.5.0/4" : "EET-2";
    case 4:  // Moscow
      return "MSK-3";
    case 5:  // US Eastern
      return dst ? "EST5EDT,M3.2.0,M11.1.0" : "EST5";
    case 6:  // US Central
      return dst ? "CST6CDT,M3.2.0,M11.1.0" : "CST6";
    case 7:  // US Pacific
      return dst ? "PST8PDT,M3.2.0,M11.1.0" : "PST8";
    default:
      return dst ? "CET-1CEST,M3.5.0,M10.5.0/3" : "CET-1";
  }
}

String getTimezoneName(int tzIndex) {
  switch(tzIndex) {
    case 0: return "UTC";
    case 1: return "Western Europe (UK)";
    case 2: return "Central Europe (DE/AT/CH)";
    case 3: return "Eastern Europe";
    case 4: return "Moscow";
    case 5: return "US Eastern";
    case 6: return "US Central";
    case 7: return "US Pacific";
    default: return "Central Europe";
  }
}

int getCurrentTimezoneIndex() {
  if (tzString.startsWith("UTC")) return 0;
  if (tzString.startsWith("WET")) return 1;
  if (tzString.startsWith("CET")) return 2;
  if (tzString.startsWith("EET")) return 3;
  if (tzString.startsWith("MSK")) return 4;
  if (tzString.startsWith("EST")) return 5;
  if (tzString.startsWith("CST")) return 6;
  if (tzString.startsWith("PST")) return 7;
  return 2; // Default Central European
}
