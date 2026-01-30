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
 *  
 *  Modified by Djordje Herceg
 *  15.8.2025.
 *  - Added styles for input field labels and placeholders
 *  - Added the InputField and InputGroup structs to help group settings into WiFi and Other
 *  - Removed user_config_html. Use the InputField instead to add new fields to the Web form. 
 *  - Added pre-loading existing settings into the Config page
 *  - The sketch uses almost 1Mb of flash, you might want to increase the firmware partition size in ESP32
 */


#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define JSON_BUFFER_LENGTH 3072
#define JSON_CHAR_LENGTH 1024



namespace ConfigPortal {

struct InputField {
  const char* inputGroup;  // Name of the input group. Subsequent fields in the same group are grouped in html.
  const char* type;
  const char* name;
  const char* placeholder;
  const char* value;  // optional
  bool checked;       // optional
};

struct InputGroup {
  InputField* fields;
  size_t count;
};

enum PostAction { Save,
                  SaveAndRestart };

// Configuration definitions
StaticJsonDocument<JSON_BUFFER_LENGTH> cfg;
char cfgFile[] = "/config.json";
char* ssid_pfix = (char*)"CaptivePortal";
// Declare input group
extern InputGroup userInputs;

// Configuration Changing/Changed callbacks
typedef bool (*ConfigChangingCallback)();  // return false if invalid
typedef void (*ConfigChangedCallback)();
ConfigChangingCallback onConfigChanging = nullptr;
ConfigChangedCallback onConfigChanged = nullptr;
// Registration functions
void registerConfigChanging(ConfigChangingCallback cb) {
  onConfigChanging = cb;
}
void registerConfigChanged(ConfigChangedCallback cb) {
  onConfigChanged = cb;
}


// Web and DNS Server resources
DNSServer dnsServer;
WebServer webServer(80);
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
extern char* ssid_pfix;

// Forward declarations
void web_sendConfigPage();


String html_begin = ""
                    "<html><head>"
                    "<meta charset='UTF-8'>"
                    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                    "<title>IOT Device Setup</title>"
                    "<link rel='stylesheet' href='/style.css'>"
                    "</head><body>"
                    "<h1>Device Setup Page</h1>"
                    "<form action='/post' method='POST'>";

String html_end = "<p>"
                  "<button type='submit' name='btnAction' value='save'>Save</button>"
                  "<button type='submit' name ='btnAction' value='saveandrestart'>Save & Restart</button>"
                  "</form>"
                  "</body></html>";

String postSave_html = ""
                       "<html><head><title>Reboot Device</title><link rel='stylesheet' href='/style.css'></head>"
                       "<body><h2>Device Configuration Finished</h2><h2>Click the Reboot Button</h2>"
                       "<p>The WiFi connection to the device will be closed.</p>"
                       "<p>Please reconnect to <strong>ESP32</strong> manually if needed.</p>"
                       "<p><button type='button' onclick=\"location.href='/reboot'\">Reboot</button>"
                       "</body></html>";

String redirect_html = ""
                       "<html><head><meta http-equiv='refresh' content='0; URL=http:/pre_boot' /></head>"
                       "<body><p>Redirecting</body></html>";

String style_css =
  "body{font-family:Arial,sans-serif;margin:20px;background-color:#f4f4f4}"
  "h1{text-align:center;color:#333}"
  "section{background:#fff;padding:15px;margin-bottom:20px;border-radius:8px;box-shadow:0 0 5px rgba(0,0,0,0.1)}"
  ".field{position:relative;margin-top:20px}"
  ".field input,.field select{width:100%;padding:12px 8px;border:1px solid #ccc;border-radius:4px;background:none;font-size:14px}"
  ".field input[type=color]{height:42px}"
  ".field label{position:absolute;top:50%;left:10px;transform:translateY(-50%);background:#fff;padding:0 4px;color:#666;transition:.2s ease all;pointer-events:none}"
  ".field input:focus+label,.field input:not(:placeholder-shown)+label,.field select:focus+label{top:-8px;font-size:12px;color:#0078d7}"
  "button{margin-top:20px;padding:10px 20px;background:#0078d7;color:#fff;border:none;border-radius:4px;cursor:pointer}"
  "button:hover{background:#005fa3}";










void saveConfig() {
  File f = LittleFS.open(cfgFile, "w");
  serializeJson(cfg, f);
  f.close();
}

void reset_config() {
  deserializeJson(cfg, "{meta:{}}");
  saveConfig();
}

void maskConfig(char* buff) {
  JsonDocument temp_cfg;
  temp_cfg.set(cfg);  // copy cfg to temp_cfg
  if (cfg.containsKey("wifi_pwd")) temp_cfg["wifi_pwd"] = "********";
  if (cfg.containsKey("token")) temp_cfg["token"] = "********";
  serializeJson(temp_cfg, buff, JSON_CHAR_LENGTH);
}

IRAM_ATTR void web_reboot() {
  WiFi.disconnect();
  ESP.restart();
}

void loadConfig() {
  // check Factory Reset Request and reset if requested or load the config
  if (!LittleFS.begin()) { LittleFS.format(); }  // before the reset_config and reading

  if (LittleFS.exists(cfgFile)) {
    File f = LittleFS.open(cfgFile, "r");
    DeserializationError error = deserializeJson(cfg, f.readString());
    f.close();

    if (error) {
      deserializeJson(cfg, "{meta:{}}");
    } else {
      Serial.println("CONFIG JSON Successfully loaded");
      char maskBuffer[JSON_CHAR_LENGTH];
      maskConfig(maskBuffer);
      Serial.println(String(maskBuffer));
    }
  } else {
    deserializeJson(cfg, "{meta:{}}");
  }
}

void web_postConfig() {
  int args = webServer.args();
  PostAction act;
  for (int i = 0; i < args; i++) {
    if (webServer.argName(i).equals("btnAction")) {
      // action buttons, Save or Save&Restart
      if (webServer.arg(i).equals("save")) {
        act = Save;
      } else if (webServer.arg(i).equals("saveandrestart")) {
        act = SaveAndRestart;
      }

    } else {
      // ordinary input values
      if (webServer.argName(i).indexOf(String("meta.")) == 0) {
        String temp = webServer.arg(i);
        temp.trim();
        cfg["meta"][webServer.argName(i).substring(5)] = temp;
      } else {
        String temp = webServer.arg(i);
        temp.trim();
        cfg[webServer.argName(i)] = temp;
      }
    }
  }
  cfg["config"] = "done";

  // Call "changing" callback before saving, to verify the values
  if (onConfigChanging != nullptr) {
    bool ok = onConfigChanging();
    if (!ok) {
      Serial.println("Config rejected by callback");
      //webServer.send(400, "text/plain", "Invalid configuration");
      web_sendConfigPage();  // resend the Configuration page again
      // TODO: pass error descriptions to the Config page, to be shown to the user

      return;
    }
  }

  saveConfig();

  // Call "changed" callback after saving
  if (onConfigChanged != nullptr) {
    onConfigChanged();
  }

  if (act == Save) {
    web_sendConfigPage();
  } else {
    // redirect uri augmentation here
    //
    webServer.send(200, "text/html", redirect_html);
  }
}

void web_pre_reboot() {
  int args = webServer.args();
  for (int i = 0; i < args; i++) {
    Serial.printf("%s -> %s\n", webServer.argName(i).c_str(), webServer.arg(i).c_str());
  }
  webServer.send(200, "text/html", postSave_html);
}







// HTML methods

void html_beginGroup(String& html, const char* label = nullptr) {
  html += "<fieldset style='margin-top:1em; padding:0.5em; border:1px solid #ccc;'>";

  if (label && strlen(label) > 0) {
    html += "<legend style='font-weight:bold;'>";
    html += label;
    html += "</legend>";
  }
}

void html_endGroup(String& html) {
  html += "</fieldset>";
}

/**
 * @brief Appends an HTML <input> element to the provided HTML string.
 *
 * Supported input types: "text", "password", "checkbox", "radio", "email", "number", "date", "ssid".
 * This function dynamically adds an <input> field to the HTML string,
 * allowing customization of the field's name, placeholder text, default value,
 * and optionally the "checked" attribute for checkbox or radio inputs.
 *
 * @param html         Reference to the HTML string to append to.
 * @param type         "text", "password", "checkbox", "radio", "email", "number", "date", "ssid"
 * @param fieldName    Name attribute of the input field.
 * @param placeholder  Placeholder text shown inside the input field (optional).
 * @param fieldValue   Default value of the input field (optional).
 * @param checked      Whether the input should be marked as checked (for checkbox/radio).
 */
void html_appendInput(String& html, const char* type, const char* fieldName, const char* placeholderText, const char* fieldValue, bool checked = false) {
  bool isCheckbox = (strcmp(type, "checkbox") == 0);
  bool isRadio = (strcmp(type, "radio") == 0);
  bool isSSID = (strcmp(type, "ssid") == 0);
  bool isTextual = !isCheckbox && !isRadio && !isSSID;

  if (isTextual) {
    // TEXTUAL INPUT
    html += "<section><div class='field'>";
    html += "<input type='";
    html += type;
    html += "' name='";
    html += fieldName;
    html += "' id='";
    html += fieldName;
    html += "' placeholder=' '";

    if (fieldValue && strlen(fieldValue) > 0) {
      html += " value='";
      html += fieldValue;
      html += "'";
    }
    html += ">";

    if (placeholderText && strlen(placeholderText) > 0) {
      html += "<label for='";
      html += fieldName;
      html += "'>";
      html += placeholderText;
      html += "</label>";
    }
    html += "</div></section>";

  } else if (isSSID) {
    // SSID DROPDOWN
    html += "<section><div class='field'>";
    int n = WiFi.scanNetworks();
    if (n == 0) {
      html += "<input type='text' value='No networks found' readonly />";
    } else {
      html += "<select name='";
      html += fieldName;
      html += "' id='";
      html += fieldName;
      html += "'>";
      for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        ssid.trim();
        html += "<option value='";
        html += ssid;
        html += "'";
        if (fieldValue && ssid == fieldValue) html += " selected";
        html += ">";
        html += ssid;
        html += "</option>";
      }
      html += "</select>";
    }
    WiFi.scanDelete();
    html += "<label for='";
    html += fieldName;
    html += "'>";
    if (placeholderText && strlen(placeholderText) > 0) html += placeholderText;
    else html += "Select WiFi Network";
    html += "</label></div></section>";

  } else {
    // CHECKBOX / RADIO
    html += "<p>";
    if (isCheckbox) {
      html += "<input type='hidden' name='";
      html += fieldName;
      html += "' value='0'>";
    }
    html += "<input type='";
    html += type;
    html += "' name='";
    html += fieldName;
    html += "' value='1'";
    if (checked) html += " checked";
    html += ">";

    if (placeholderText && strlen(placeholderText) > 0) {
      html += " ";
      html += placeholderText;
    }
    html += "</p>";
  }
}

// END HTML METHODS


/*
  Function to scan and build HTML dropdown options

  usage:
    html += "<select name='ssid' id='ssid'>";
    html += getWiFiDropdownOptions();
    html += "</select>";
*/
String getWiFiDropdownOptions() {
  String options = "";

  // Start scan
  int n = WiFi.scanNetworks();
  if (n == 0) {
    options += "<option>No networks found</option>";
  } else {
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      ssid.trim();  // remove stray spaces
      options += "<option value=\"" + ssid + "\">" + ssid + "</option>";
    }
  }

  // Clear scan results to free memory
  WiFi.scanDelete();

  return options;
}



void web_sendConfigPage() {
  // creates the config page using the configuration document cfg
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");
  String html;
  html.reserve(10000);  // allocate 10K for HTML content to avoid heap fragmentation
  html += html_begin;

  // read the config file (if it exists) and populate the web input controls
  //loadConfig();
  bool cfgOK = false;
  // *** If no "config" is found or "config" is not "done", run configDevice ***
  if (cfg.containsKey("config") && (strcmp((const char*)cfg["config"], "done")) == 0) {
    cfgOK = true;
  }

  const char* groupname = nullptr;  // current group name, used for HTML grouping

  if (userInputs.fields && userInputs.count > 0) {
    for (size_t i = 0; i < userInputs.count; ++i) {
      // current input field definition
      InputField& f = userInputs.fields[i];
      // start a new group if necessary
      if ((groupname == nullptr) || (strcmp(groupname, f.inputGroup) != 0)) {
        if (groupname != nullptr) {
          html_endGroup(html);  // end the previous group first, if it exists
        }
        groupname = f.inputGroup;  // then start the new group
        html_beginGroup(html, f.inputGroup);
      }

      // generate HTML code for the current input field
      html_appendInput(html, f.type, f.name, f.placeholder, cfgOK ? (const char*)cfg[f.name] : f.value, f.checked);
    }
    html_endGroup(html);
  }

  html += html_end;
  webServer.sendContent(html);
}


void serverStart() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  char ap_name[100];
  sprintf(ap_name, "%s_%08X", ssid_pfix, (unsigned int)ESP.getEfuseMac());
  WiFi.softAP(ap_name);
  WiFi.setTxPower(WIFI_POWER_2dBm);  // lowest value to prevent overheating

  dnsServer.start(DNS_PORT, "*", apIP);


  webServer.on("/post", web_postConfig);
  webServer.on("/reboot", web_reboot);
  webServer.on("/pre_boot", web_pre_reboot);
  webServer.on("/style.css", []() {
    webServer.send(200, "text/css", style_css);
  });

  webServer.onNotFound([]() {
    web_sendConfigPage();
  });

  webServer.begin();
  Serial.println("starting the config");
}

void serverLoop() {
  yield();
  dnsServer.processNextRequest();
  webServer.handleClient();

  // if (userConfigLoop != NULL) {
  //   (*userConfigLoop)();
  // }

  vTaskDelay(pdMS_TO_TICKS(10));  // RTOS-friendly delay
}

void serverStop() {
  dnsServer.stop();
  webServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("servers stopped, resources released");
}



}




// ---- OBSOLETE CODE -----

//void (*userConfigLoop)() = NULL;

// void byte2buff(char* msg, byte* payload, unsigned int len) {
//   unsigned int i, j;
//   for (i = j = 0; i < len;) {
//     msg[j++] = payload[i++];
//   }
//   msg[j] = '\0';
// }


// bool getHTML(String* html, char* fname) {
//   if (LittleFS.exists(fname)) {
//     String buff;
//     File f = LittleFS.open(fname, "r");
//     buff = f.readString();
//     buff.trim();
//     f.close();
//     *html = buff;
//     return true;
//   } else {
//     return false;
//   }
// }



// void configDevice() {
//   DNSServer dnsServer;
//   const byte DNS_PORT = 53;
//   IPAddress apIP(192, 168, 1, 1);
//   WiFi.mode(WIFI_AP);
//   WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
//   char ap_name[100];
//   sprintf(ap_name, "%s_%08X", ssid_pfix, (unsigned int)ESP.getEfuseMac());
//   WiFi.softAP(ap_name);
//   WiFi.setTxPower(WIFI_POWER_2dBm);  // lowest value to prevent overheating
//   //WiFi.setTxPower(WIFI_POWER_5dBm);

//   dnsServer.start(DNS_PORT, "*", apIP);

//   //if (getHTML(&postSave_html, (char*)"/postSave.html")) {
//   // argument redirection
//   //} else
//   //  postSave_html = postSave_html_default;
//   //}

//   webServer.on("/save", web_saveEnv);
//   webServer.on("/reboot", web_reboot);
//   webServer.on("/pre_boot", web_pre_reboot);
//   webServer.on("/style.css", []() {
//     webServer.send(200, "text/css", style_css);
//   });

//   webServer.onNotFound([]() {
//     web_sendConfigPage();
//   });

//   webServer.begin();
//   Serial.println("starting the config");
//   while (1) {
//     yield();
//     dnsServer.processNextRequest();
//     webServer.handleClient();
//     // if (userConfigLoop != NULL) {
//     //   (*userConfigLoop)();
//     // }

//     vTaskDelay(pdMS_TO_TICKS(10));  // RTOS-friendly delay
//   }
// }