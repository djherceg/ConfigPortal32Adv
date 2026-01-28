#include <Arduino.h>
#include <ESPmDNS.h>
#include "ConfigPortal32Adv.h"
#include <LittleFS.h>

char* ssid_pfix = (char*)"CaptivePortal";
String user_config_html = "";

InputField myInputs[] = {
  // Basic text input
  { "text", "dev_name", "Device Name", "ESP32-C3", false },

  // Basic text input - Client id
  { "text", "client_name", "Client Name", "ESP32-C3", false },

  // Basic text input - Client password
  { "text", "client_pwd", "Client Password", "pwd", false },

  // Basic text input - MQTT Broker Address
  { "text", "mqtt_url", "Broker address", "broker.net", false },

  // Basic text input - MQTT Broker Address
  { "number", "mqtt_port", "Broker port", "8883", false },

  // SSID list

  { "ssid", "ssid", "Select WiFi network", nullptr, true },

  // Checkbox
  { "checkbox", "debug", "Enable Debug", nullptr, true },

  // Password field
  { "password", "admin_pw", "Admin Password", "", false },

  // Email input
  { "email", "user_email", "Email Address", "user@example.com", false },

  // Number input
  { "number", "max_clients", "Max Clients", "5", false },

  // Date input
  { "date", "install_date", "Installation Date", "2025-08-15", false },

  // Time input
  { "time", "start_time", "Start Time", "08:00", false },

  // Color picker
  { "color", "theme_color", "Theme Color", "#ff0000", false }

};

InputGroup userInputs = {
  myInputs,
  sizeof(myInputs) / sizeof(myInputs[0])
};



/*
 *  ConfigPortal library to extend and implement the WiFi connected IOT device
 *
 *  Yoonseok Hur
 *
 *  Usage Scenario:
 *  0. copy the example template in the README.md
 *  1. Modify the ssid_pfix to help distinquish your Captive Portal SSID
 *          char   ssid_pfix[];
 *  2. Modify user_config_html to guide and get the user config data through the Captive Portal
 *          String user_config_html;
 *  2. declare the user config variable before setup
 *  3. In the setup(), read the cfg["meta"]["your field"] and assign to your config variable
 *
 */

void testLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  File file = LittleFS.open("/test_example.txt", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("File Content:");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}


void setup() {
  Serial.begin(115200);

  testLittleFS();

  loadConfig();
  // TODO: the configDevice() call needs to be removed in production
  configDevice();
  
  // *** If no "config" is found or "config" is not "done", run configDevice ***
  if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
    configDevice();
  }
  
  // Normal startup, no config
  WiFi.mode(WIFI_STA);
  WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // main setup
  Serial.printf("\nIP address : ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("miniwifi")) {
    Serial.println("MDNS responder started");
  }
}

void loop() {
}