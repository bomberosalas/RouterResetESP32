#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include "time.h"

// ================= CONFIG =================
#define WDT_TIMEOUT 20 
#define LOG_SIZE 50

// MEJORA: Tu servidor NTP Local (ESP32 con GPS)
const char* ntpServer = "192.168.0.xx"; 
const long  gmtOffset_sec = -14400;     // UTC-4 (Chile Continental)
const int   daylightOffset_sec = 0;     // 3600 si es horario de verano

// -------- TELEGRAM ----------
String BOT_TOKEN = "YOUR_TOKEN";
String CHAT_ID   = "YOUR_CHATID";

// -------- WIFI ----------
const char* ssid = "MySSID";
const char* password = "WIFI_PASSWORD";

IPAddress local_IP(192,168,0,XX);
IPAddress gateway(192,168,0,XX);
IPAddress subnet(255,255,255,0);
IPAddress primaryDNS(1,1,1,1);
IPAddress secondaryDNS(1,0,0,1);

// ================= VARIABLES ESTADO =================
WebServer server(80);
WiFiUDP udp;
const uint8_t WOL_MAC[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
IPAddress wolBroadcast(192,168,0,255);

#define RELAY_PIN 4
#define LED_PIN 8
#define RELAY_ON LOW
#define RELAY_OFF HIGH

enum RouterState {ON, REBOOTING};
RouterState state = ON;

unsigned long rebootStart = 0;
unsigned long lastCheck = 0;
const unsigned long checkInterval = 3600000; // 1 hora

bool pendingReset = false;
unsigned long warningStart = 0;
unsigned long lastResetTime = 0;
const unsigned long GRACE_PERIOD = 300000; 

int internetFailures = 0;
int totalResets = 0;

// ================= LOGS CON HORA LOCAL =================
String logs[LOG_SIZE];
int logIndex = 0;

String getTimestamp(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "[Sincronizando...]";
  }
  char timeStringBuff[15];
  strftime(timeStringBuff, sizeof(timeStringBuff), "[%H:%M:%S]", &timeinfo);
  return String(timeStringBuff);
}

void addLog(String msg){
  logs[logIndex] = getTimestamp() + " " + msg;
  logIndex = (logIndex + 1) % LOG_SIZE;
}

// ================= TELEGRAM =================
void sendTelegram(String msg){
  if(WiFi.status()!=WL_CONNECTED) return;
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + msg;
  http.begin(url);
  http.GET();
  http.end();
}

// ================= WOL =================
bool sendWOL(){
  uint8_t packet[102];
  memset(packet,0xFF,6);
  for(int i=0;i<16;i++){
    memcpy(packet+6+(i*6),WOL_MAC,6);
  }
  udp.beginPacket(wolBroadcast,9);
  udp.write(packet,102);
  udp.endPacket();
  addLog("WOL enviado");
  sendTelegram("💻 WOL enviado");
  return true;
}

// ================= RED E INTERNET =================
void connectWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(ssid,password);

  unsigned long start = millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<15000){
    esp_task_wdt_reset(); 
    delay(500);
  }

  if(WiFi.status()==WL_CONNECTED){
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    addLog("WiFi OK");
    sendTelegram(" 👌 WiFi conectado (NTP Local OK)");
  }
}

bool checkInternet(){
  if(WiFi.status()!=WL_CONNECTED) return false;
  HTTPClient http;
  http.setTimeout(1500);
  int success = 0;

  http.begin("http://clients3.google.com/generate_204");
  if(http.GET() == 204) success++;
  http.end();

  if(success < 1){
    delay(200);
    http.begin("http://1.1.1.1");
    if(http.GET() > 0) success++;
    http.end();
  }
  return success >= 1;
}

void checkInternetLogic(){
  if (millis() - lastResetTime < GRACE_PERIOD) return;
  
  if(millis() - lastCheck >= checkInterval){
    lastCheck = millis();
    addLog("--- Chequeo automático ---");

    int okCount = 0;
    for(int i=0; i<3; i++){
      esp_task_wdt_reset(); 
      if(checkInternet()) okCount++;
      if(i < 2) delay(2000);
    }

    if(okCount < 2){
      internetFailures++;
      pendingReset = true;
      warningStart = millis();
      addLog("Internet FALLIDO");
      sendTelegram("⚠️ Internet caído, reinicio en 60s");
    } else {
      addLog("Internet OK");
    }
  }
}

// ================= LÓGICA DE REINICIO =================
void autoResetLogic(){
  if(!pendingReset) return;
  if(millis()-warningStart > 60000){
    pendingReset = false;
    state = REBOOTING;
    rebootStart = millis();
    totalResets++;
    lastResetTime = millis(); 
    addLog("AUTO RESET");
    sendTelegram("🔄 Auto reset ejecutado");
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(RELAY_PIN, RELAY_OFF);
  }
}

void updateRouter(){
  if(state == REBOOTING && millis()-rebootStart > 25000){
    digitalWrite(RELAY_PIN, RELAY_ON);
    digitalWrite(LED_PIN, LOW);
    state = ON;
    addLog("Router ON");
    sendTelegram(" ✅ Router encendido");
  }
}

// ================= DASHBOARD WEB =================
void setupWeb(){
  server.on("/", [](){
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width'>"
    "<title>Router Monitor - Carlos Salas</title>"
    "<style>"
    "body{background:#111;color:#eee;font-family:Arial;text-align:center;}"
    ".card{background:#222;padding:20px;border-radius:10px;margin:10px;}"
    ".ok{color:#4caf50;} .warn{color:#ff9800;} .err{color:#f44336;}"
    "button{padding:10px;margin:5px;border:none;border-radius:6px;font-size:16px;cursor:pointer;}"
    ".btn-green{background:#4caf50;color:white;} .btn-red{background:#f44336;color:white;} .btn-blue{background:#2196f3;color:white;}"
    "#logs{text-align:left; font-size:13px; max-height:300px; overflow-y:auto; background:#000; padding:10px; border-radius:5px;}"
    "</style></head><body>"
    "<h2>Router Monitor V2</h2>"
    "<div class='card'><p id='state'></p><p>IP: " + WiFi.localIP().toString() + "</p>"
    "<p>RSSI: " + String(WiFi.RSSI()) + " dBm</p>"
    "<p>Fallos Internet: <span id='fails'></span></p>"
    "<p>Resets Totales: <span id='resets'></span></p></div>"
    "<div class='card'>"
    "<button id='btnReset' onclick=\"fetch('/reset')\">Reset Manual</button>"
    "<button class='btn-blue' onclick=\"fetch('/wol')\">Wake on LAN</button>"
    "<button onclick=\"fetch('/cancel')\">Cancelar Reinicio</button></div>"
    "<div class='card'><h3>Historial de Eventos</h3><div id='logs'></div></div>"
    "<script>"
    "async function load(){"
    "try{"
    "let r=await fetch('/data'); let d=await r.json();"
    "let st=document.getElementById('state'); let btn=document.getElementById('btnReset');"
    "if(d.state=='ON'){ st.innerHTML='SISTEMA ONLINE'; st.className='ok'; btn.className='btn-green'; }"
    "else{ st.innerHTML='REINICIANDO ROUTER...'; st.className='err'; btn.className='btn-red'; }"
    "if(d.pending){ st.innerHTML='INTERNET CAÍDO - ESPERANDO'; st.className='warn'; }"
    "document.getElementById('fails').innerText=d.fails;"
    "document.getElementById('resets').innerText=d.resets;"
    "let logs=''; d.logs.forEach(l=>{ if(l && l!='null') logs+='<p style=\"margin:2px 0\">'+l+'</p>'; });"
    "document.getElementById('logs').innerHTML=logs;"
    "}catch(e){}}"
    "setInterval(load,3000);load();"
    "</script></body></html>";
    server.send(200,"text/html",html);
  });

  server.on("/data", [](){
    String json="{";
    json += "\"state\":\""+String(state==ON?"ON":"REBOOT")+"\",";
    json += "\"fails\":"+String(internetFailures)+",";
    json += "\"resets\":"+String(totalResets)+",";
    json += "\"pending\":"+String(pendingReset?"true":"false")+",";
    json += "\"logs\":[";
    for(int i=0;i<LOG_SIZE;i++){
      int idx = (logIndex + i) % LOG_SIZE; // Ordenar cronológicamente
      if(logs[idx] != "") {
        json += "\"" + logs[idx] + "\"";
        if(i < LOG_SIZE - 1) json += ",";
      }
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  server.on("/reset", [](){
    state = REBOOTING; rebootStart = millis(); totalResets++;
    lastResetTime = millis(); digitalWrite(LED_PIN, HIGH);
    addLog("Reset manual iniciado");
    sendTelegram("👆 Reset manual solicitado vía Web");
    digitalWrite(RELAY_PIN, RELAY_OFF);
    server.send(200,"text/plain","OK");
  });

  server.on("/wol", [](){
    sendWOL();
    server.send(200,"text/plain","WOL OK");
  });

  server.on("/cancel", [](){
    pendingReset=false;
    addLog("Reinicio cancelado por usuario");
    sendTelegram("⛔ Reset cancelado desde el panel");
    server.send(200,"text/plain","OK");
  });

  server.begin();
}

// ================= SETUP & LOOP =================
void setup(){
  Serial.begin(115200);

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_ON);
  digitalWrite(LED_PIN, LOW);

  connectWiFi();
  setupWeb();
  udp.begin(9);
  
  addLog("Sistema iniciado correctamente");
}

void loop(){
  esp_task_wdt_reset();
  server.handleClient();
  checkInternetLogic();
  autoResetLogic();
  updateRouter();
}