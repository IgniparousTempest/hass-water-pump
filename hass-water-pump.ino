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

int pin_sw=2;  //for switch (D4)
int pin_pump=4;  //for pump (D2)
int pin_led=5;  //for status LED (D1)
boolean pump_status;
boolean sw_status;
Ticker timer; // Timer to switch off pump

void setup() {
  pinMode(pin_sw, INPUT);
  pinMode(pin_pump, OUTPUT);
  pinMode(pin_led, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.hostname("HASS_Water_Pump");
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
  // For debug only
  server.on("/toggle", HTTP_GET, [](){
    pump_toggle();
    server.send(200, "text/json", response_json());
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
  
  //initialise pump
  pump_off();
}

String response_json() {
    String is_active = pump_status ? "true" : "false";
    return "{\"is_active\": \"" + is_active + "\"}";
}

String home_page() {
    String status_text = pump_status ? "<span style='color: green; font-weight: bold;'>pumping</span>" : "<span style='color: orange; font-weight: bold;'>idle</span>";
    String data_text = pump_status ? "{\"active\": true, \"duration\": 600}" : "{\"active\": false}";
    String html = "<!DOCTYPE HTML>\n";
    html += "<html>\n";
    html += "  <head>\n";
    html += "    <title>HASS Water Pump</title>\n";
    html += "    <script src='https://code.jquery.com/jquery-3.3.1.min.js' integrity='sha256-FgpCb/KJQlLNfOu91ta32oNMZxltwRo8QtmkMRdAu9=' crossorigin='anonymous'></script>\n";
    html += "    <script>\n";
    html += "      $(function () {\n";
    html += "        $('form').on('submit', function (e) {\n";
    html += "          e.preventDefault();\n";
    html += "          $.post({\n";
    html += "            url: '/pump',\n";
    html += "            data: '" + data_text + "',\n";
    html += "            success: function () {\n";
    html += "              location.reload();\n";
    html += "            }\n";
    html += "          });\n";
    html += "        });\n";
    html += "      });\n";
    html += "    </script>\n";
    html += "  </head>\n";
    html += "  <body>\n";
    html += "    <h1>ESP8266 Web Server Water Pump</h1>\n";
    html += "    <p>Pump is " + status_text + "</p>\n";
    html += "    <form>\n";
    html += "      <button name='submit' value='submit'>Toggle Pump</button>\n";
    html += "    </form>\n";
    html += "  </body>\n";
    html += "</html>";
    return html;
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
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(pin_led, LOW);
    digitalWrite(pin_pump, LOW);
    timer.detach();
}

/**
 * Turn the pump on.
 * @param duration_in_seconds The duration to turn the pump on for.
 */
void pump_on(int duration_in_seconds) {
    Serial.println("Turning pump on for " + String(duration_in_seconds) + " seconds");
    pump_status = true;
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(pin_led, HIGH);
    digitalWrite(pin_pump, HIGH);
    timer.once(duration_in_seconds, pump_off);
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  // Restart device if WiFi is disconnected and it is not pumping
  // TODO: Is there not a onDisconnected handler that can do this better?
  if (WiFi.status() == WL_DISCONNECTED && !pump_status)
     ESP.restart();

  //TODO: Use button to toggle pump state

  delay(200);
}

