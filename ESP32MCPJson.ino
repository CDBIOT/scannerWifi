#ifdef ESP8266
#include <ESP8266WebServer.h> 
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>

#include "FS.h"

#else
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <SPIFFS.h>
#include "FS.h"
#endif

#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <TimeLib.h>
#include <mcp_can.h>
#include <mcp_can_dfs.h>
#include <SPI.h>
#include<Wire.h>
// Tamanho do Objeto JSON
const   size_t    JSON_SIZE = JSON_OBJECT_SIZE(5) + 130;
const byte        DNSSERVER_PORT                  = 53;

IPAddress apIP(192, 168, 1, 1);
DNSServer         dnsServer;
WebServer server(80);   
         
// Variáveis Globais ------------------------------------
char              id[30];       // Identificação do dispositivo
boolean           ledOn;        // Estado do LED
word              bootCount;    // Número de inicializações
char              ssid[30];     // Rede WiFi
char              pw[30];       // Senha da Rede WiFi


// Mode 1 PIDs
#define PID_ENGINE_LOAD 0x04
#define PID_COOLANT_TEMP 0x05
#define PID_FUEL_PRESSURE 0x0A
#define PID_INTAKE_MAP 0x0B
#define PID_RPM 0x0C
#define PID_SPEED 0x0D
#define PID_INTAKE_TEMP 0x0F
#define PID_MAF_FLOW 0x10
#define PID_THROTTLE 0x11
#define PID_RUNTIME 0x1F
#define PID_FUEL_LEVEL 0x2F
#define PID_DISTANCE 0x31
#define PID_BAROMETRIC 0x33
#define PID_CONTROL_MODULE_VOLTAGE 0x42
#define PID_AMBIENT_TEMP 0x46
#define PID_ETHANOL_FUEL 0x52
#define PID_FUEL_RAIL_PRESSURE 0x59
#define PID_ENGINE_OIL_TEMP 0x5C
#define PID_FUEL_INJECTION_TIMING 0x5D

// CAN Interrupt and Chip Select Pins
#define CAN0_INT 2                              /* Set INT to pin 2 (This rarely changes)   */
MCP_CAN CAN0(15);                                /* Set CS to pin 9 (Old shields use pin 10) */

#define standard 0
// 7E0/8 = Engine ECM
// 7E1/9 = Transmission ECM
#if standard == 1
  #define LISTEN_ID 0x7EA
  #define REPLY_ID 0x7E0
  #define FUNCTIONAL_ID 0x7DF
#else
  #define LISTEN_ID 0x98DAF101
  #define REPLY_ID 0x98DA01F1
  #define FUNCTIONAL_ID 0x98DB33F1
#endif
// CAN TX Variables
unsigned long prevTx = 0;
unsigned int invlTx = 2000;
const char* mqtt_server = "broker.mqtt-dashboard.com";


byte txData[] = {0x02,0x01,0x20,0x55,0x55,0x55,0x55,0x55};
//adicionais //mode // PID //

// CAN RX Variables
unsigned long rxID;
byte dlc;
byte rxBuf[8];
char msgString[128];                        // Array to store serial string

#define LEDPIN          LED_BUILTIN     // LED pin

//Mapeamento de pinos do NodeMCU
#define led0    16
#define led1    5
#define led2    4
#define led3    0

int valueD0 = HIGH;
int valueD1 = HIGH;
int valueD2 = HIGH;
int valueD3 = HIGH;
 
int cont = 0;
int valor = analogRead(A0);
float temperatura = ((valor*250)/1023);

// Funções Genéricas ------------------------------------
void log(String s) {
  // Gera log na Serial
  Serial.println(s);
}

String hexStr(const unsigned long &h, const byte &l = 8) {
  // Retorna valor em formato hexadecimal
  String s;
  s= String(h, HEX);
  s.toUpperCase();
  s = ("00000000" + s).substring(s.length() + 8 - l);
  return s;
}

String deviceID() {
    return "CDB SCAN-" + hexStr(ESP.getEfuseMac());
}

String ipStr(const IPAddress &ip) {
  // Retorna IPAddress em formato "n.n.n.n"
  String sFn = "";
  for (byte bFn = 0; bFn < 3; bFn++) {
    sFn += String((ip >> (8 * bFn)) & 0xFF) + ".";
  }
  sFn += String(((ip >> 8 * 3)) & 0xFF);
  return sFn;
}
//
//void ledSet() {
//  // Define estado do LED
//  digitalWrite(LED_PIN, ledOn ? LED_ON : LED_OFF);
//}

void  configReset() {
  // Define configuração padrão
  strlcpy(id, "IeC Device", sizeof(id)); 
  strlcpy(ssid, "CDB", sizeof(ssid)); 
  strlcpy(pw, "abcde12345", sizeof(pw)); 
  ledOn     = false;
  bootCount = 0;
}
boolean configRead() {
  // Lê configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Configura.json"), "r");
  if (deserializeJson(jsonConfig, file)) {
    // Falha na leitura, assume valores padrão
    
  configReset();

    log(F("Falha lendo CONFIG, assumindo valores padrão."));
    return false;
  } else {
    // Sucesso na leitura
    strlcpy(id, jsonConfig["id"]      | "", sizeof(id)); 
    strlcpy(ssid, jsonConfig["ssid"]  | "", sizeof(ssid)); 
    strlcpy(pw, jsonConfig["pw"]      | "", sizeof(pw)); 
    ledOn     = jsonConfig["led"]     | false;
    bootCount = jsonConfig["boot"]    | 0;

    file.close();

    log(F("\nLendo config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
}

boolean configSave() {
  // Grava configuração
  StaticJsonDocument<JSON_SIZE> jsonConfig;

  File file = SPIFFS.open(F("/Configura.json"), "w+");
  if (file) {
    // Atribui valores ao JSON e grava
    jsonConfig["ssid"]  = ssid;
    jsonConfig["pw"]    = pw;

    serializeJsonPretty(jsonConfig, file);
    file.close();

    log(F("\nGravando config:"));
    serializeJsonPretty(jsonConfig, Serial);
    log("");

    return true;
  }
  return false;
}


// Requisições Web --------------------------------------
void handleHome() {
  // Home
  File file = SPIFFS.open(F("/Home.htm"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();
    

    // Atualiza conteúdo dinâmico
    
    s.replace(F("#led#")      , ledOn ? F("Ligado") : F("Desligado"));
    s.replace(F("#id#")     , id);
    s.replace(F("#ssid#")   , ssid);
    s.replace(F("#pw#")   , pw);
    s.replace(F("#sysIP#")    , ipStr(WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP()));
    s.replace(F("#clientIP#") , ipStr(server.client().remoteIP()));
    
    // Envia dados
    server.send(200, F("text/html"), s);
    log("Home - Cliente: " + ipStr(server.client().remoteIP()) + (server.uri() != "/" ? " [" + server.uri() + "]" : ""));
  } else {
    server.send(500, F("text/plain"), F("Home - ERROR 500"));
    log(F("Home - ERRO lendo arquivo"));
  }
}


void handleMonitor() {
   // Monitor
  File file = SPIFFS.open(F("/Monitor.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    // Envia dados
    server.send(200, F("text/html"), s);
    log("JS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Monitor - ERROR 500"));
    log(F("JS - ERRO lendo arquivo"));

  }
}

void handleGrafico() {
   // Grafico
  File file = SPIFFS.open(F("/Grafico.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();
    
    // Envia dados
    server.send(200, F("text/html"), s);
    log("JS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Grafico - ERROR 500"));
    log(F("JS - ERRO lendo arquivo"));

  }
}

void handleRelogio() {
   // Configura
  File file = SPIFFS.open(F("/Relogio.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();

    // Envia dados
    server.send(200, F("text/html"), s);
    log("JS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Home - ERROR 500"));
    log(F("JS - ERRO lendo arquivo"));
}
}

void handleConfigura() {
   // Configura
  File file = SPIFFS.open(F("/Config.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();
    
    // Atualiza conteúdo dinâmico
    s.replace(F("#id#")     , id);
   // s.replace(F("#ledOn#")  ,  ledOn ? " checked" : "");
   //s.replace(F("#ledOff#") , !ledOn ? " checked" : "");
    s.replace(F("#ssid#")   , ssid);
    
     // Envia dados
    server.send(200, F("text/html"), s);
    log("Configura - Cliente: "  + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Temperatura- ERROR 500"));
    log(F("Temperatura - ERRO lendo arquivo"));
}

}
void handleConfigSave() {
  // Grava Config
  // Verifica número de campos recebidos
#ifdef ESP8266
  // ESP8266 gera o campo adicional "plain" via post
  if (server.args() == 5) {
#else
  // ESP32 envia apenas os 4 campos
  if (server.args() == 4) {
#endif
    String s;

    // Grava id
    s = server.arg("id");
    s.trim();
    if (s == "") {
      s = deviceID();
   }
   strlcpy(id, s.c_str(), sizeof(id));

    // Grava ssid
    s = server.arg("ssid");
    s.trim();
    strlcpy(ssid, s.c_str(), sizeof(ssid));

    // Grava pw
    s = server.arg("pw");
    s.trim();
    if (s != "") {
      // Atualiza senha, se informada
      strlcpy(pw, s.c_str(), sizeof(pw));
    }

    // Grava ledOn
  //  ledOn = server.arg("led").toInt();

    // Atualiza LED
    //ledSet();

    // Grava configuração
    if (configSave()) {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração salva.');history.back()</script></html>"));
      log("ConfigSave - Cliente: " + ipStr(server.client().remoteIP()));
    } else {
      server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha salvando configuração.');history.back()</script></html>"));
      log(F("ConfigSave - ERRO salvando Config"));
    }
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Erro de parâmetros.');history.back()</script></html>"));
  }
}


void handleReconfig() {
  // Reinicia Config
  configReset();

  // Atualiza LED
 // ledSet();

  // Grava configuração
  if (configSave()) {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Configuração reiniciada.');window.location = '/'</script></html>"));
    log("Reconfig - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(200, F("text/html"), F("<html><meta charset='UTF-8'><script>alert('Falha reiniciando configuração.');history.back()</script></html>"));
    log(F("Reconfig - ERRO reiniciando Config"));
  }
}

void handleReboot() {
  // Reboot
  File file = SPIFFS.open(F("/Reboot.htm"), "r");
  if (file) {
    server.streamFile(file, F("text/html"));
    file.close();
    log("Reboot - Cliente: " + ipStr(server.client().remoteIP()));
    delay(100);
    ESP.restart();
  } else {
    server.send(500, F("text/plain"), F("Reboot - ERROR 500"));
    log(F("Reboot - ERRO lendo arquivo"));
  }
}
void handleCSS() {
  // Arquivo CSS
  File file = SPIFFS.open(F("/Style.css"), "r");
  if (file) {
    // Define cache para 3 dias
    server.sendHeader(F("Cache-Control"), F("public, max-age=172800"));
    server.streamFile(file, F("text/css"));
    file.close();
    log("CSS - Cliente: " + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("CSS - ERROR 500"));
    log(F("CSS - ERRO lendo arquivo"));
  }
}



void handleTemp()
{
  
  File file = SPIFFS.open(F("/Temperatura.html"), "r");
  if (file) {
    file.setTimeout(100);
    String s = file.readString();
    file.close();
   
    // Envia dados
    server.send(200, F("text/html"), s);
    log("Configura - Cliente: "  + ipStr(server.client().remoteIP()));
  } else {
    server.send(500, F("text/plain"), F("Temperatura- ERROR 500"));
    log(F("Temperatura - ERRO lendo arquivo"));
}
}

void getTemp()
{
  int valor = analogRead(A0);
  float t = ((valor*250)/1023);
  
  String json = "{\"temperature\":"+String(t)+"}";//Cria um json com os dados da temperatura
  server.send (200, "application/json", json);
  
}

void getCAN()
{
 pinMode(CAN0_INT, INPUT);                          // Configuring pin for /INT input
 
   if (CAN0.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
     Serial.println("Error Initializing MCP2515");
  }
  if (CAN0.setMode(MCP_NORMAL) != CAN_OK) {
     Serial.println("Error Setting Mode MCP2515");
  }
  else{
    Serial.println("Error Initializing MCP2515... Permanent failure!  Check your code & connections");
   yield();
    //while(1);
    }

#if standard == 1
  // Standard ID Filters
  CAN0.init_Mask(0,0x7F00000);                // Init first mask...
  CAN0.init_Filt(0,0x7DF0000);                // Init first filter...
  CAN0.init_Filt(1,0x7E10000);                // Init second filter...

  CAN0.init_Mask(1,0x7F00000);                // Init second mask...
  CAN0.init_Filt(2,0x7DF0000);                // Init third filter...
  CAN0.init_Filt(3,0x7E10000);                // Init fouth filter...
  CAN0.init_Filt(4,0x7DF0000);                // Init fifth filter...
  CAN0.init_Filt(5,0x7E10000);                // Init sixth filter...

#else
  // Extended ID Filters
  CAN0.init_Mask(0,0x90FF0000);                // Init first mask...
  CAN0.init_Filt(0,0x90DA0000);                // Init first filter...
  CAN0.init_Filt(1,0x90DB0000);                // Init second filter...

  CAN0.init_Mask(1,0x90FF0000);                // Init second mask...
  CAN0.init_Filt(2,0x90DA0000);                // Init third filter...
  CAN0.init_Filt(3,0x90DB0000);                // Init fouth filter...
  CAN0.init_Filt(4,0x90DA0000);                // Init fifth filter...
  CAN0.init_Filt(5,0x90DB0000);                // Init sixth filter...
#endif

if((millis() - prevTx) >= invlTx){
    prevTx = millis();
    static byte pids[] = {PID_RPM,PID_SPEED,PID_COOLANT_TEMP ,PID_FUEL_PRESSURE,PID_INTAKE_MAP,
                          PID_THROTTLE , PID_FUEL_LEVEL ,PID_BAROMETRIC , PID_CONTROL_MODULE_VOLTAGE , PID_AMBIENT_TEMP};
    static byte index = 0;
    byte pid = pids[index];
    int value;
    {
    //byte txData[] = {0x02,0x01,index,0x55,0x55,0x55,0x55,0x55};
    txData[2] = pid;
    //adicionais //mode // PID // data
    index = (index +1) % sizeof(pids);
    if(CAN0.sendMsgBuf(FUNCTIONAL_ID, 8, txData) == CAN_OK){
      Serial.println("Message Sent Successfully!");    
    } else {
      Serial.println("Error Sending Message...");
    }
    }
  }
  String json;
    
  if(!digitalRead(CAN0_INT)){                         // If CAN0_INT pin is low, read receive buffer
    CAN0.readMsgBuf(&rxID, &dlc, rxBuf);             // Get CAN data
  
    // Display received CAN data as we receive it.
    if((rxID & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Ext ID: 0x%.8lX  DLC: %1d  Data:", (rxID & 0x1FFFFFFF), dlc);
    else
      sprintf(msgString, "Std ID: 0x%.3lX  DLC: %1d  Data:", rxID, dlc);
 
    Serial.print(msgString);
    //String json = "{\"CAN\":"+String(msgString)+"}";
   // server.send (200, "application/json", json);
 
    if((rxID & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      Serial.print(msgString);
   //   String json = "{\"CAN\":"+String(msgString)+"}";
   //  server.send (200, "application/json", json);
    } else 
   
    {
        sprintf(msgString, " 0x%.81X", rxID);
      //  json += msgString;
       // server.send (200, "application/json_id", json);
        
      for(byte i = 0; i<dlc; i++){
        sprintf(msgString, " 0x%.2X", rxBuf[i]);
        Serial.print(msgString);
       //json += msgString;
       // String json = "{\"CAN\":"+String(msgString)+"}";
      }
     uint data0 = rxBuf[0];
      
    StaticJsonDocument<JSON_SIZE> doc;
    JsonObject CAN = doc.to<JsonObject>();
    JsonArray data = CAN.createNestedArray("data");
    data.add(hexStr(rxBuf[0]));
    data.add(hexStr(rxBuf[1]));
    data.add(hexStr(rxBuf[2]));
    data.add(hexStr(rxBuf[3]));
    data.add(hexStr(rxBuf[4]));
    serializeJson(CAN, json);

 // String clientId = "cdbiot123";
 //clientId += String(random(0xffff), HEX);// exemplo de conversão para HEX

    // json = "{\"CAN\":"+String(jsonCANMsg)+"}";
     server.send (200, "application/json", json);
    }
  }
}

void handleRoot() 
{
  String html =
 " <html>"

 "</html>";
  //Envia o html para o cliente com o código 200, que é o código quando a requisição foi realizada com sucesso
  server.send(200, "text/html",html);
}

void setup(){

  Serial.begin(9600);
  while(!Serial);
  
  // SPIFFS
  if (!SPIFFS.begin()) {
    log(F("SPIFFS ERRO"));
    while (true);
  }
  
  pinMode(led0, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);

  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  
  configRead();
  configSave();
   
  // WiFi Access Point
  #ifdef ESP8266
    // Configura WiFi para ESP8266
    WiFi.hostname(deviceID());
    WiFi.softAP(deviceID(), deviceID());
  #else
    // Configura WiFi para ESP32
    WiFi.setHostname(deviceID().c_str());
    WiFi.softAP(deviceID().c_str(), deviceID().c_str());
  #endif
  log("WiFi AP " + deviceID() + " - IP " + ipStr(WiFi.softAPIP()));

  // Habilita roteamento DNS
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(DNSSERVER_PORT, "*", WiFi.softAPIP());

  // WiFi Station
  
  WiFi.begin(ssid, pw);
  log("Conectando WiFi " + String(ssid));
  byte b = 0;
  while(WiFi.status() != WL_CONNECTED && b < 60) {
    b++;
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    // WiFi Station conectado
    log("WiFi conectado (" + String(WiFi.RSSI()) + ") IP " + ipStr(WiFi.localIP()));
  } else {
    log(F("WiFi não conectado"));
  }
  
server.on("/Temperatura", handleTemp);  
server.on("/temperature", HTTP_GET, getTemp);
server.on("/CAN", HTTP_GET, getCAN); 
server.on("/Monitor",handleMonitor);
server.on("/Grafico",handleGrafico);
server.on("/configSave", handleConfigSave);
server.on("/Config", handleConfigura);
server.on("/Relogio", handleRelogio);
server.on("/css" , handleCSS);
server.on("/Home", handleHome);
server.on("/reboot", handleReboot);
server.on("/reconfig", handleReconfig);
server.on("/", handleHome);
//server.onNotFound(onNotFound);
server.begin();
Serial.println("Servidor HTTP iniciado");
}

void loop() 
{
   // WatchDog ----------------------------------------
  yield();
 //lcdmessage();
 dnsServer.processNextRequest();
 server.handleClient();
}
  
