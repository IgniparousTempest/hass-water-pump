#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//------------------------------for local web
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <Ticker.h>
#include "secrets.h"

MDNSResponder mdns;
ESP8266WebServer server(80);
//----------------------------for local web

int pinout=2;  //for relay, and internal led
int pinsw=0;  //for switch
boolean pump_status;
boolean sw_status;
Ticker timer; // Timer to switch off pump

void setup() {
  pinMode(pinout, OUTPUT);
  pinMode(pinsw, INPUT);
  Serial.begin(115200);
  Serial.println("Booting");
   WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("esp8266", WiFi.localIP())) 
    Serial.println("MDNS responder started");
 
  server.on("/", [](){
    server.send(200, "text/html", home_page());
  });
  server.on("/pump", HTTP_GET, [](){
    server.send(200, "text/json", response_json());
  });
  server.on("/pump", HTTP_POST, [](){
    StaticJsonBuffer<200> request_buffer;
    JsonObject& request = request_buffer.parseObject(server.arg("plain"));
    boolean desired_light_state = request["active"];
    if (desired_light_state) {
      int duration = request["duration"];
      pump_on(duration);
    }
    else
      pump_off();
    server.send(200, "text/json", response_json());
  });

  server.begin();
  //-------------------------------------------add your code here
}

String response_json() {
    String is_active = pump_status ? "true" : "false";
    return "{\"is_active\": \"" + is_active + "\"}";
}

String home_page() {
    String status_text = pump_status ? "<span style='color: green'>pumping</span>" : "<span style='color: orange'>idle</span>";
    return "<h1>ESP8266 Web Server Pump Switch</h1>" +
           "<p>Pump is " + status_text + "</p>" +
           "<form action='' method='post'>" +
           "  <button name='foo' value='upvote'>Toggle Pump</button>" +
           "</form>";
}

void pump_toggle() {
    Serial.println("Toggling pump...");
    if (pump_status) 
      pump_off();
    else
      // Stay on for maximum of 10 minutes
      pump_on(10*60);
}

/**
 * Turns pump off
 */
void pump_off() {
    Serial.println("Turning pump off");
    pump_status = false;
    digitalWrite(pinout, LOW);
    timer.detach();
}

/**
 * Turn the pump on.
 * @param duration_in_seconds The duration to turn the pump on for.
 */
void pump_on(int duration_in_seconds) {
    Serial.println("Turning pump on");
    pump_status = true;
    digitalWrite(pinout, HIGH);
    timer.once(duration_in_seconds, pump_off);
}

void loop() {
  ArduinoOTA.handle();
  //-------------------------------------------add your code here
  server.handleClient();

  //TODO: Use button to toggle pump state
  delay(200);
  
}

