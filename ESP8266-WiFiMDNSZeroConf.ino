/***********************************************************************
* PURPOSE: Configuration of ESPP8266 based IoT devices without hard-coded WiFi settings or other MDNS server settings. 
*   To allow WiFi settings to be provided through the device's own AP
*   To discover MDNS servers within the local network without any manual configuration 
*   This allows for a deployment IoT devices with one-time WiFi configuration, 
*     and to connect to any MDNS servers in the network with no additional configuration (ZeroConf).
* AUTHOR: Ramprasad Polana
* DATE: Nov 2018
* COPYRIGHT: Ramprasad Polana 2018.  All rights reserved.
* 
* DESCRIPTION: 
*   1. auto connect to previously saved WiFi connection
*   2. auto switch to AP mode for config in case no previously saved WiFi connection or cannot connect to saved WiFi connection
*   3. auto discover mqtt (or any other type) server in the local network using MDNS, and save them for later use
* IMPLEMENTATION NOTES:
*   Config details are stored in and read from /config.json file using SPIFFS
*   WiFi AP ssid is generated as "ESP_<chipID>", and random hardcoded password (presumably communicated to enduser printed on the hardware unit)
*   If previously saved server details exist in config, no discovery is done as those saved details are read in
*   In case of successful discovery, only first MDNS server of given type is saved
*   Server resolution details (IP and port) are also saved in  oonfig
*   All code is doe in setup to get all config right before entering loop, no useful code in loop
* USAGE:
*   Combine with other useful code in loop
*   First run, WiFi AP will be available to save WiFi connection in /config.json
*   Second run onwards, the saved connection is used to autoconnect to WiFi 
*   First run, MDNS servers discovery is done and their config is saved in /config.json
*   Second run onwards, saved MDNS server details are read back from /config.json
*   To reset saved settings in /config.json, erase ESP flash memory through Ardiono IDE before uploading sketch
* CHAAGE HISTORY:
* ISSUES:
*   When saving config of mqtt server, the json save might overwrite other previously saved settings in /config.json?? Need to check
*   wiFiManager.reset() can be used to erase previous WiFi connection parameters, but not sure if it resets ssved MDNS settings in /config.json
*   
***********************************************************************/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library 
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal

#include <WiFiManager.h>          // For WiFi config AP
#include <ESP8266mDNS.h>          // For discovery of MDNS services

#define HOSTNAME_SIZE 32
#define IP_SIZE 16
#define PORT_SIZE 8

char hostname_string[HOSTNAME_SIZE+1] = {0};

// mqtt server details
char mqtt_server[HOSTNAME_SIZE+1] = {0};
char mqtt_server_ip[IP_SIZE+1] = {0};
char mqtt_server_port[PORT_SIZE+1] = {0};

void read_config(){
//read configuration from FS json
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strncpy(mqtt_server, json["mqtt_server"], HOSTNAME_SIZE);
          mqtt_server[HOSTNAME_SIZE] = 0;
          strncpy(mqtt_server_ip, json["mqtt_server_ip"], IP_SIZE);
          mqtt_server_ip[IP_SIZE] = 0;
          strncpy(mqtt_server_port, json["mqtt_server_port"], PORT_SIZE);
          mqtt_server_port[PORT_SIZE] = 0;
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read  
}
void save_config(){
  //save the custom parameters to FS
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["mqtt_server"] = mqtt_server;
  json["mqtt_server_ip"] = mqtt_server_ip;
  json["mqtt_server_port"] = mqtt_server_port;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }
  json.printTo(configFile);
  configFile.close();
  json.printTo(Serial);
  Serial.println("");
  //end save
}

void discover_mqtt(char *hostname_param, char *mqtt_server) {
  if (!MDNS.begin(hostname_param)) {
    Serial.print("Error setting up mDNS responder for hostname: ");
      Serial.println(hostname_param);
  }
  Serial.println("mDNS responder started");

  Serial.println("Sending mDNS query");
  int n = MDNS.queryService("mqtt", "tcp"); 
  Serial.println("mDNS query done");
  if (n == 0) {
    Serial.println("no services found");
    return;
  }
  else {
    Serial.print(n);
    Serial.println(" service(s) found");
    for (int i = 0; i < n; ++i) {
      // Print details for each service found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(MDNS.hostname(i));
      Serial.print(" (");
      Serial.print(MDNS.IP(i));
      Serial.print(":");
      Serial.print(MDNS.port(i));
      Serial.println(")");
      if (MDNS.hostname(i).length() > HOSTNAME_SIZE) {
        Serial.println("ERROR: hostname length exceeds size allocated, being truncated!!");
      }
      strncpy(mqtt_server, MDNS.hostname(i).c_str(), HOSTNAME_SIZE);
      mqtt_server[HOSTNAME_SIZE] = 0;
      strncpy(mqtt_server_ip, MDNS.IP(i).toString().c_str(), IP_SIZE);
      mqtt_server_ip[IP_SIZE] = 0;
      sprintf(mqtt_server_port, "%d\0", MDNS.port(i));
      mqtt_server_port[PORT_SIZE] = 0;
      break;
      // try to connect and check if fingerprint matches
    }
  }
}

char* ssid_ap = "ESP_";
const char* password_ap = "9B2nKxeQ"; // random password
WiFiManager wifiManager;

void setup() {
  Serial.begin(115200); // Initialize serial communication 
  Serial.println("\r\nESP8266 Simple WiFi Config");
  Serial.println("\r\nsetup()");

  delay(2000); // this was necessary for WiFi as well as the MDNS to work correctly
  
  sprintf(hostname_string, "ESP_%06X", ESP.getChipId());
  Serial.print("Hostname: ");
  Serial.println(hostname_string);
  WiFi.hostname(hostname_string);
  
  sprintf(ssid_ap, "ESP_%06X", ESP.getChipId());
  //first parameter is name of access point, second is the password
  wifiManager.autoConnect(ssid_ap, password_ap);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error connecting to WiFi using autoconnect through WiFiManager!!");
    return;
  }
  Serial.print("WiFi Connected to: ");
  String ssid = WiFi.SSID();
  Serial.print(ssid.c_str());
//  String password = WiFi.psk();
//  Serial.print(" / ");
//  Serial.print(password.c_str());
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // additional config
  read_config();
  if(strlen(mqtt_server) == 0){
    discover_mqtt(hostname_string, mqtt_server);
    if(strlen(mqtt_server) > 0){
      save_config();
    }
  } else {
    Serial.print("MQTT server from previously saved config: ");
    Serial.print(mqtt_server);
    Serial.print(", at IP/port ");
    Serial.print(mqtt_server_ip);
    Serial.print(":");
    Serial.println(mqtt_server_port);  
  }
}

int loop_delay = 5000;
int count = 0;

void loop() {
 
  Serial.print("Started in loop:");  
  Serial.print(count * loop_delay);  
  Serial.println("ms");
  count ++;
  
  delay(loop_delay);
}
