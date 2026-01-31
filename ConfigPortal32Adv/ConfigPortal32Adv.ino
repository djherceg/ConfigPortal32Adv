#include <Arduino.h>
#include <ESPmDNS.h>
#include "ConfigPortal32Adv.h"
#include <LittleFS.h>


String user_config_html = "";

// groupName, input type, name, placeholder string, value (optional), checked (optional)
ConfigPortal::InputField myInputs[] = {
  // Basic text input
  { "WiFi", ConfigPortal::input_text, "wifi_ssid", "SSID", "", false },
  { "WiFi", ConfigPortal::input_password, "wifi_pwd", "Password", "", false },
  // SSID list
  { "WiFi", ConfigPortal::input_ssid, "wifi_ssid", "Select WiFi network", nullptr, true },
  // Basic text input
  { "Basic", ConfigPortal::input_text, "dev_name", "Device Name", "ESP32-C3", false },
  // Basic text input - Client id
  { "Basic", "text", "client_name", "Client Name", "ESP32-C3", false },
  // Basic text input - Client password
  { "Basic", "text", "client_pwd", "Client Password", "pwd", false },
  // Basic text input - MQTT Broker Address
  { "Basic", "text", "mqtt_url", "Broker address", "broker.net", false },
  // Basic text input - MQTT Broker Address
  { "Basic", "number", "mqtt_port", "Broker port", "8883", false },
  // Checkbox
  { "Special", "checkbox", "debug", "Enable Debug", nullptr, true },
  // Password field
  { "Special", "password", "admin_pw", "Admin Password", "", false },
  // Email input
  { "Special", "email", "user_email", "Email Address", "user@example.com", false },
  // Number input
  { "Special", "number", "max_clients", "Max Clients", "5", false },
  // Date input
  { "Date and time", "date", "install_date", "Installation Date", "2025-08-15", false },
  // Time input
  { "Date and time", "time", "start_time", "Start Time", "08:00", false },
  // Color picker
  { "Visuals", "color", "theme_color", "Theme Color", "#ff0000", false }
};

ConfigPortal::InputGroup ConfigPortal::userInputs = {
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

bool ConfigChanging() {
  // What we can do here:
  // - Check a value with a specific key in the ConfigPortal::cfg JSON document
  // - Modify the value if needed
  // - Store the value back in the cfg JSON document
  // - The modified cfg then gets saved to the file system by ConfigPortal
  Serial.println("ConfigChanging invoked");

  // check max_clients
  if (ConfigPortal::cfg.containsKey("max_clients")) {
    String v = ConfigPortal::cfg["max_clients"];
    int val = v.toInt();
    if (val < 0) val = 0;
    if (val > 5) val = 5;
    ConfigPortal::cfg["max_clients"] = String(val);
  }

  return true;
}

void ConfigChanged() {
  // This is invoked after the configuration is saved to the file system 
  // The consistent configuration lives in the ConfigPortal::cfg JSON document
  Serial.println("ConfigChanged invoked");
}


void setup() {
  Serial.begin(115200);

  testLittleFS();

  // init the portal
  ConfigPortal::ssid_pfix = "CaptivePortal";

  ConfigPortal::loadConfig();
  ConfigPortal::registerConfigChanging(ConfigChanging);
  ConfigPortal::registerConfigChanged(ConfigChanged);
  ConfigPortal::serverStart();
  
  // TODO: This needs to be tuned in production
  return;

  // *** If no "config" is found or "config" is not "done", run configDevice ***
  if (!ConfigPortal::cfg.containsKey("config") || strcmp((const char*)ConfigPortal::cfg["config"], "done")) {
    ConfigPortal::serverStart();
  }

  // Normal startup, no config
  WiFi.mode(WIFI_STA);
  WiFi.begin((const char*)ConfigPortal::cfg["wifi_ssid"], (const char*)ConfigPortal::cfg["wifi_pwd"]);
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
  ConfigPortal::serverLoop();
}