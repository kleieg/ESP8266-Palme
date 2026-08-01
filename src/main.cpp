//
//Benutzt für Palme unD LED 
// Anpassungen: in config.h den Namen
//
//

// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <AsyncElegantOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <SoftwareSerial.h>

#include "WLAN_Credentials.h"
#include "config.h"
#include "wifi_mqtt.h"


// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
long My_time = 0;
long Start_time;
long Up_time;
long U_days;
long U_hours;
long U_min;
long U_sec;

// Timers auxiliar variables
long now = millis();
int LEDblink = 0;
bool led = 1;

// Create AsyncWebServer object on port 80
AsyncWebServer Asynserver(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// end of variables -----------------------------------------------------

// Initialize LittleFS
void initLittleFS()
{
  if (!LittleFS.begin())
  {
    LOG_PRINTLN("An error has occurred while mounting LittleFS");
  }
  LOG_PRINTLN("LittleFS mounted successfully");
}


String getOutputStates()
{
  JSONVar myArray;

  U_days = Up_time / 86400;
  U_hours = (Up_time % 86400) / 3600;
  U_min = (Up_time % 3600) / 60;
  U_sec = (Up_time % 60);

  myArray["cards"][0]["c_text"] = Hostname;
  myArray["cards"][1]["c_text"] = WiFi.dnsIP().toString() + "   /   " + String(VERSION);
  myArray["cards"][2]["c_text"] = String(WiFi.RSSI());
  myArray["cards"][3]["c_text"] = String(MQTT_INTERVAL) + "ms";
  myArray["cards"][4]["c_text"] = String(U_days) + " days " + String(U_hours) + ":" + String(U_min) + ":" + String(U_sec);
  myArray["cards"][5]["c_text"] = "WiFi = " + String(WiFi_reconnect) + "   MQTT = " + String(Mqtt_reconnect);
  myArray["cards"][6]["c_text"] = " to reboot click ok";

  myArray["gpios"][0]["output"] = String(0);
  myArray["gpios"][0]["state"] = String(digitalRead(GPIO_switch));

  String jsonString = JSON.stringify(myArray);
  LOG_PRINTLN("Refresh Webserver");
  LOG_PRINTF("%s\n",jsonString.c_str()); 
  return jsonString;
}

void notifyClients(String state)
{
  ws.textAll(state);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    // according to AsyncWebServer documentation this is ok
    data[len] = 0;

    LOG_PRINTLN("Data received: ");
    LOG_PRINTF("%s\n",data);

    JSONVar json = JSON.parse((const char *)data);
    if (json == nullptr)
    {
      LOG_PRINTLN("Request is not valid json, ignoring");
      return;
    }
    if (!json.hasOwnProperty("action"))
    {
      LOG_PRINTLN("Request is not valid json, ignoring");
      return;
    }
    if (!strcmp(json["action"], "states"))
    {
      notifyClients(getOutputStates());
    }
    else if (!strcmp(json["action"], "reboot"))
    {
      LOG_PRINTLN("Reset..");
      ESP.restart();
    }
    else if (!strcmp(json["action"], "relais"))
    {
      if (!json.hasOwnProperty("data"))
      {
        LOG_PRINTLN("Relais request is missing data, ignoring");
        return;
      }
      if (!json["data"].hasOwnProperty("relais"))
      {
        LOG_PRINTLN("Relais request is missing relais number, ignoring");
        return;
      }
      if (JSONVar::typeof_(json["data"]["relais"]) != "number")
      {
        LOG_PRINTLN("Relais request contains invali relais number, ignoring");
        return;
      }
      int relais = json["data"]["relais"];
      if (relais < 0 || relais >= NUM_OUTPUTS)
      {
        LOG_PRINTLN("Relais request contains invali relais number, ignoring");
        return;
      }
      
      digitalWrite(GPIO_switch, !digitalRead(GPIO_switch));
      notifyClients(getOutputStates());
      LOG_PRINTLN("switch Relais");

    }
  }

  Mqtt_lastSend = now - MQTT_INTERVAL - 10;  // --> MQTT send !!
}

void onEvent(AsyncWebSocket *Asynserver, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
  {
    LOG_PRINTF("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  }
  case WS_EVT_DISCONNECT:
    LOG_PRINTF("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}


// receive MQTT messages
void MQTT_call(String &topic, String &payload)  {
  
  LOG_PRINTF("%s","Message arrived on topic: ");
  LOG_PRINTF("%s\n",topic);
  LOG_PRINTF("%s","Data : ");

/*
  String MQTT_message;
  for (unsigned int i = 0; i < length; i++) {
    MQTT_message += (char)message[i];
  }
  LOG_PRINTLN(MQTT_message);
*/

  LOG_PRINTLN(payload);

  String windowTopic = Hostname + "/CMD/Flamingo";
// String strTopic = String(topic);

  if (topic == windowTopic ){

    if(payload == "true"){
      LOG_PRINTF("%s\n", "MQTT switch on");
      digitalWrite(GPIO_switch, HIGH);
    }
    if(payload == "false"){
      LOG_PRINTF("%s\n", "MQTT switch off");
      digitalWrite(GPIO_switch, LOW);
    }
  }

  notifyClients(getOutputStates());
  Mqtt_lastSend = now - MQTT_INTERVAL - 10;  // --> MQTT send !!
}


void MQTTsend()
{
  JSONVar mqtt_data, sensors, actuators;

  String mqtt_tag = Hostname + "/STATUS";
  LOG_PRINTF("%s\n", mqtt_tag.c_str());

  mqtt_data["Time"] = My_time;
  mqtt_data["RSSI"] = WiFi.RSSI();
  mqtt_data["IP"] = WiFi.localIP().toString().c_str();

  if  ( digitalRead(GPIO_switch) ) {
    actuators["Flamingo"] = true;
  }
  else{
    actuators["Flamingo"] = false;
  }
  mqtt_data["Actuators"] = actuators;

  String mqtt_string = JSON.stringify(mqtt_data);

  LOG_PRINTLN("Refresh MQTT");
  LOG_PRINTF("%s\n", mqtt_string.c_str());

  mqttClient.publish(mqtt_tag.c_str(), mqtt_string.c_str());

  notifyClients(getOutputStates());
}

void setup()
{
  // Serial port for debugging purposes
  LOG_INIT()

  delay(4000); // wait for serial log to be reday

  pinMode(GPIO_switch, OUTPUT);
  digitalWrite(GPIO_switch, LOW);

  pinMode(GPIO_LED_INTERN, OUTPUT);
  digitalWrite(GPIO_LED_INTERN, LOW);

  LOG_PRINTF("start init\n");
  initLittleFS();
  initWiFi();

 // init Websocket
  ws.onEvent(onEvent);
  Asynserver.addHandler(&ws);

  LOG_PRINTF("setup MQTT\n");
  initMQTT();
  mqttClient.onMessage(MQTT_call);

  // Route for root / web page
  LOG_PRINTLN("set Webpage");
  Asynserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html", false); });

  Asynserver.serveStatic("/", LittleFS, "/");

  // init NTP
  LOG_PRINTLN("init NTP");
  timeClient.begin();
  timeClient.setTimeOffset(0);

  // update UPCtime for Starttime
  timeClient.update();
  Start_time = timeClient.getEpochTime();

  // Start ElegantOTA
  LOG_PRINTLN("start Elegant OTA");
  AsyncElegantOTA.begin(&Asynserver);

  // Start server
  LOG_PRINTLN("start server");
  Asynserver.begin();

  LOG_PRINTLN("setup finished");
}

void loop()
{
  ws.cleanupClients();

  // update UPCtime
  timeClient.update();
  My_time = timeClient.getEpochTime();
  Up_time = My_time - Start_time;


  now = millis();

  // LED blinken
  if (now - LEDblink > LED_BLINK_INTERVAL)
  {
    LEDblink = now;
    if (led == 0)
    {
      digitalWrite(GPIO_LED_INTERN, 1);
      led = 1;
    }
    else
    {
      digitalWrite(GPIO_LED_INTERN, 0);
      led = 0;
    }
  }


  // check WiFi
  if (WiFi.status() != WL_CONNECTED  ) {
    // try reconnect every 5 seconds
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;              // prevents mqtt reconnect running also
      // Attempt to reconnect
      reconnect_wifi();
    }
  }


  // check if MQTT broker is still connected
  if (!mqttClient.connected()) {
    // try reconnect every 5 seconds
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      reconnect_mqtt();
    }
  } else {
    // Client connected

    mqttClient.loop();

    // send data to MQTT broker
    if (now - Mqtt_lastSend > MQTT_INTERVAL) {
    Mqtt_lastSend = now;
    MQTTsend();
    } 
  }   
}