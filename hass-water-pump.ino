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
    html += "<html lang='en'>\n";
    html += "  <head>\n";
    html += "    <title>HASS Water Pump</title>\n";
    html += "    <meta charset='utf-8'>\n";
    html += "    <meta name='viewport' content='width=device-width, initial-scale=1'>\n";
    html += "    <link rel='stylesheet' href='https://bootswatch.com/4/yeti/bootstrap.min.css'>\n";
    html += "    <script src='https://code.jquery.com/jquery-3.3.1.min.js'\n";
    html += "      integrity='sha256-FgpCb/KJQlLNfOu91ta32o/NMZxltwRo8QtmkMRdAu8='\n";
    html += "      crossorigin='anonymous'></script>\n";
    html += "    <script src='https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.13.0/popper.min.js'></script>\n";
    html += "    <script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0/js/bootstrap.min.js'></script>\n";
    html += "    <script>\n";
    html += "      function onButtonPost() {\n";
    html += "        $.post({\n";
    html += "          url: '/pump',\n";
    html += "          data: '" + data_text + "',\n";
    html += "          success: function () {\n";
    html += "            location.reload();\n";
    html += "          }\n";
    html += "        });\n";
    html += "      };\n";
    html += "    </script>\n";
    html += "  </head>\n";
    html += "  <body>\n";
    html += "  \n";
    html += "    <nav class='navbar navbar-expand-lg navbar-dark bg-dark'>\n";
    html += "      <a class='navbar-brand' href='/'>ESP8266 Water Pump</a>\n";
    html += "      <button class='navbar-toggler' type='button' data-toggle='collapse' data-target='#navbarSupportedContent' aria-controls='navbarSupportedContent' aria-expanded='false' aria-label='Toggle navigation'>\n";
    html += "      <span class='navbar-toggler-icon'></span>\n";
    html += "      </button>\n";
    html += "      <div class='collapse navbar-collapse' id='navbarSupportedContent'>\n";
    html += "        <ul class='navbar-nav mr-auto'>\n";
    html += "          <li class='nav-item active'>\n";
    html += "            <a class='nav-link' href='/'>Home <span class='sr-only'>(current)</span></a>\n";
    html += "          </li>\n";
    html += "        </ul>\n";
    html += "      </div>\n";
    html += "    </nav>\n";
    html += "    \n";
    html += "    <div class='container-fluid'>\n";
    html += "      <div class='row'>\n";
    html += "        <main role='main' class='col-md-9 ml-sm-auto col-lg-10 pt-3 px-4'>\n";
    html += "          <div class='d-flex justify-content-between flex-wrap flex-md-nowrap align-items-center pb-2 mb-3 border-bottom'>\n";
    html += "            <h1 class='h2'>Dashboard</h1>\n";
    html += "          </div>\n";
    html += "          <p>Pump is <span class='text-success font-weight-bold'>pumping</span></p>\n";
    html += "          <button type='button' class='btn btn-primary' onclick='onButtonPost();'>Toggle Pump</button>\n";
    html += "        </main>\n";
    html += "      </div>\n";
    html += "    </div>\n";
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

