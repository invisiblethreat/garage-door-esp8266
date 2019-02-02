#include <FS.h>          //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

//flag for saving data
bool shouldSaveConfig = true;

// Pin used to control the relay
int Relay = 3;

// init our webserver
ESP8266WebServer server(80);

void setup() {
  // Set the inital state of the relay
  pinMode(Relay, OUTPUT);    //Set Pin3 as output
  digitalWrite(Relay, HIGH);    //Set Pin13 High
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  
  // Clean FS, for testing
  //SPIFFS.format();
  
  // Read configuration from FS json
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
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
            Serial.println("Parsed JSON");
        }
        else {
            Serial.println("failed to load json config");
        }
        configFile.close();
        }
     }
  }
  else {
    Serial.println("failed to mount FS");
  }
  //end read
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  //reset settings - for testing
  //wifiManager.resetSettings();
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "Garage Door Manager"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Garage Door Manager", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  
  //if you get here you have connected to the WiFi
  Serial.println("Wifi connected");
  
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  
  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  // Start the mDNS responder for garage-door.local
  if (!MDNS.begin("garage-door")) {             
    Serial.println("Error setting up MDNS responder!");
  }

  MDNS.addService("http", "tcp", 80);
  
  // Server handlers for taking action
  server.on("/", handleRootPath);
  server.on("/doorToggle", doorToggle);
  
  // Not needed, but could be useful in automation situations
  server.on("/doorOpen", doorToggle);
  server.on("/doorClose", doorToggle);

  server.begin();
  Serial.println("Webserver listening.");
}

//callback notifying us of the need to save config
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void handleRootPath() {
  // Nothing exciting, just don't live at the root in case things scan or access.
  server.send(200, "text/html", "Door toggle lives at <a href=\"/doorToggle\">/doorToggle</a>");
  Serial.println("Handled /");
}

void doorToggle() {
  /*
  Garage doors operate on an NC circuit. This action opens the circuit, which
  simulates a physical button press in the wired circuit. Upon the call, the
  circuit is opened for one second.
  */

  // TODO: This mode loses scope when inside the webserver handler and must be reset
  pinMode(Relay, OUTPUT);
  Serial.println("Handling /doorToggle");
  Serial.println("Opening the circuit");
  digitalWrite(Relay, LOW);
  delay(1000);
  Serial.println("Closing the circuit");
  digitalWrite(Relay, HIGH);
  server.send(200, "text/html", "Garage door has been toggled.");
  Serial.println("Handled /doorToggle");
}

void loop() {
  server.handleClient();
}
