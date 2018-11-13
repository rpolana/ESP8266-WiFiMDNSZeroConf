#  Configuration of ESPP8266 based IoT devices without hard-coded WiFi settings or other MDNS server settings. 

## PURPOSE
To allow WiFi settings to be provided through the device's own AP
To discover MDNS servers within the local network without any manual configuration.

This allows for a deployment IoT devices with one-time WiFi configuration, and to connect to any MDNS servers in the network with no additional configuration (ZeroConf).

## DESCRIPTION 
1. auto connect to previously saved WiFi connection
2. auto switch to AP mode for config in case no previously saved WiFi connection or cannot connect to saved WiFi connection
3. auto discover mqtt (or any other type) server in the local network using MDNS, and save them for later use

## IMPLEMENTATION NOTES
* Config details are stored in and read from /config.json file using SPIFFS
* WiFi AP ssid is generated as "ESP_<chipID>", and random hardcoded password (presumably communicated to enduser printed on the hardware unit)
* If previously saved server details exist in config, no discovery is done as those saved details are read in
* In case of successful discovery, only first MDNS server of given type is saved
* Server resolution details (IP and port) are also saved in  oonfig
* All code is doe in setup to get all config right before entering loop, no useful code in loop

## USAGE
* Combine with other useful code in loop
* First run, WiFi AP will be available to save WiFi connection in /config.json
* Second run onwards, the saved connection is used to autoconnect to WiFi 
* First run, MDNS servers discovery is done and their config is saved in /config.json
* Second run onwards, saved MDNS server details are read back from /config.json
* To reset saved settings in /config.json, erase ESP flash memory through Ardiono IDE before uploading sketch

## TO DO
* When saving config of mqtt server, the json save might overwrite other previously saved settings in /config.json?? Need to check
* wiFiManager.reset() can be used to erase previous WiFi connection parameters, but not sure if it resets ssved MDNS settings in /config.json
  
