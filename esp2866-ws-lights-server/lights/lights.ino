#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <WiFiClient.h>

#include <WebSocketsServer.h>

// Version number for checking if there are new code releases and notifying the user
String version = "1.0.0";

//Configure Default Settings for AP logon
String APid = "LightsConnectAP";
String APpw = "transpeak";

//Fixed settings for WIFI
WiFiClient espClient;

char config_name[40] = "lights";             //WIFI config: Bonjour name of device

boolean loadDataSuccess = false;
boolean saveItNow = false;          //If true will store positions to SPIFFS
bool shouldSaveConfig = false;      //Used for WIFI Manager callback to save parameters
boolean initLoop = true;            //To enable actions first time the loop is run

int d2Pin = D2;
int d3Pin = D3;
int currentBrightnessD2 = 0;
int currentBrightnessD3 = 0;
int powerStateD2 = 0;
int powerStateD3 = 0;

WiFiServer server(80);              // TCP server at port 80 will respond to HTTP requests
WebSocketsServer webSocket = WebSocketsServer(81);  // WebSockets will respond on port 81


/****************************************************************************************
   Loading configuration that has been saved on SPIFFS.
   Returns false if not successful
*/
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }
  json.printTo(Serial);
  Serial.println();

  //Store variables locally
  currentBrightnessD2 = long(json["currentBrightnessD2"]);
  currentBrightnessD3 = long(json["currentBrightnessD3"]);
  powerStateD2 = long(json["powerStateD2"]);
  powerStateD3 = long(json["powerStateD3"]);
  strcpy(config_name, json["config_name"]);

  return true;
}

/**
   Save configuration data to a JSON file
   on SPIFFS
*/
bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["config_name"] = config_name;
  json["currentBrightnessD2"] = currentBrightnessD2;
  json["powerStateD2"] = powerStateD2;
  json["currentBrightnessD3"] = currentBrightnessD3;
  json["powerStateD3"] = powerStateD3;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);

  Serial.println("Saved JSON to SPIFFS");
  json.printTo(Serial);
  Serial.println();
  return true;
}

/****************************************************************************************
*/
void processMsg(String res, uint8_t clientnum) {
  /*
     Below are actions based on inbound payload
  */
  if (res == "(update-d2)") {
    //Send position details to client
    webSocket.sendTXT(clientnum, "{ \"lightState\": { \"d2\":" + String(currentBrightnessD2) + " } }");
  } else if (res == "(update-d3)") {
    //Send position details to client
    webSocket.sendTXT(clientnum, "{ \"lightState\": { \"d3\":" + String(currentBrightnessD3) + " } }");
  } else if (res == "(on-d2)") {
    if (powerStateD2 != 1) {
      // Turn ON function will set last known brightness
      powerStateD2 = 1;
      setBrightness(d2Pin, currentBrightnessD2);
      webSocket.broadcastTXT("{ \"lightState\": { \"d2\": " + String(currentBrightnessD2) + " } }");
    }
  } else if (res == "(on-d3)") {
    if (powerStateD3 != 1) {
      // Turn ON function will set last known brightness
      powerStateD3 = 1;
      setBrightness(d3Pin, currentBrightnessD3);
      webSocket.broadcastTXT("{ \"lightState\": { \"d3\": " + String(currentBrightnessD3) + " } }");
    }
  } else if (res == "(off-d2)") {
    if (powerStateD2 != 0) {
      // Turn OFF function
      powerStateD2 = 0;
      setBrightness(d2Pin, 0);
      webSocket.broadcastTXT("{ \"lightState\": { \"d2\": " + String(0) + " } }");
    }
  } else if (res == "(off-d3)") {
    if (powerStateD3 != 0) {
      // Turn OFF function
      powerStateD3 = 0;
      setBrightness(d3Pin, 0);
      webSocket.broadcastTXT("{ \"lightState\": { \"d3\": " + String(0) + "} }");
    }
  } else if (res == "(ping)") {
    //Do nothing
  } else if (res.substring(0, 4) == "(d2)" ){
    currentBrightnessD2 = res.substring(4).toInt();
    setBrightness(d2Pin, currentBrightnessD2);
    webSocket.broadcastTXT("{ \"lightState\": { \"d2\": " + String(currentBrightnessD2) + "} }");
  } else if (res.substring(0, 4) == "(d3)" ){
    currentBrightnessD3 = res.substring(4).toInt();
    setBrightness(d3Pin, currentBrightnessD3);
    webSocket.broadcastTXT("{ \"lightState\": { \"d3\": " + String(currentBrightnessD3) + "} }");
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);

      String res = (char*)payload;

      //Send to common MQTT and websocket function
      processMsg(res, num);
      break;
  }
}

/**
  Turn of power to coils whenever the blind
  is not moving
*/
void stopPowerToCoils() {
  digitalWrite(D1, LOW);
  //digitalWrite(D2, HIGH);
  //digitalWrite(D3, HIGH);
  digitalWrite(D4, LOW);
  digitalWrite(D6, LOW);
  digitalWrite(D7, LOW);
  digitalWrite(D8, LOW);
}

/*
   Callback from WIFI Manager for saving configuration
*/
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setLight(int pin, int percentage) {
  analogWrite(pin, 200 - percentage);
}

void setBrightness(int pin, int newbrightness) {

  setLight(pin, newbrightness);

}


void setup(void)
{
  Serial.begin(115200);
  delay(100);
  Serial.print("Starting now\n");

  pinMode(d2Pin, OUTPUT);
  pinMode(d3Pin, OUTPUT);
  analogWriteRange(200);            //This should set PWM range not 1023 but 100 as is %
  setLight(d2Pin, 0);
  setLight(d3Pin, 0);

  //Set the WIFI hostname
  WiFi.hostname(config_name);

  //Define customer parameters for WIFI Manager
  WiFiManagerParameter custom_config_name("Name", "Bonjour name", config_name, 40);

  //Setup WIFI Manager
  WiFiManager wifiManager;

  //reset settings - for testing
  //clean FS, for testing
  //SPIFFS.format();
  //wifiManager.resetSettings();

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  wifiManager.addParameter(&custom_config_name);
  wifiManager.autoConnect(APid.c_str(), APpw.c_str());

  //Load config upon start
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  /* Save the config back from WIFI Manager.
      This is only called after configuration
      when in AP mode
  */
  if (shouldSaveConfig) {
    //read updated parameters
    strcpy(config_name, custom_config_name.getValue());
    //Save the data
    saveConfig();
  }

  /*
     Try to load FS data configuration every time when
     booting up. If loading does not work, set the default
     positions
  */
  loadDataSuccess = loadConfig();
  if (!loadDataSuccess) {

  }

  /*
    Setup multi DNS (Bonjour)
  */
  if (!MDNS.begin(config_name)) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  // Start TCP (HTTP) server
  server.begin();
  Serial.println("TCP server started");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

  //Start websocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  //Setup OTA
  {

    // Authentication to avoid unauthorized updates
    //ArduinoOTA.setPassword((const char *)"nidayand");

    ArduinoOTA.setHostname(config_name);

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
  }
}

int tripCounter = 0;
void loop(void)
{
  //OTA client code
  ArduinoOTA.handle();

  //Websocket listner
  webSocket.loop();

  /**
    Storing positioning data and turns off the power to the coils
  */
  if (saveItNow) {
    saveConfig();
    saveItNow = false;

    /*
      If no action is required by the motor make sure to
      turn off all coils to avoid overheating and less energy
      consumption
    */
    stopPowerToCoils();
  }

  /*
     After running setup() the motor might still have
     power on some of the coils. This is making sure that
     power is off the first time loop() has been executed
     to avoid heating the stepper motor draining
     unnecessary current
  */
  if (initLoop) {
    initLoop = false;
    stopPowerToCoils();
  }

  /**
    Serving the webpage/api
  */
  {
    // Check if a client has connected
    WiFiClient webclient = server.available();
    if (!webclient) {
      return;
    }
    Serial.println("New client");

    // Wait for data from client to become available
    /*while(webclient.connected() && !webclient.available()){
      Serial.println("Delay");
      delay(1);
      }*/

    // Read the first line of HTTP request
    String req = webclient.readStringUntil('\r');

    // First line of HTTP request looks like "GET /path HTTP/1.1"
    // Retrieve the "/path" part by finding the spaces
    int addr_start = req.indexOf(' ');
    int addr_end = req.indexOf(' ', addr_start + 1);
    if (addr_start == -1 || addr_end == -1) {
      Serial.print("Invalid request: ");
      Serial.println(req);
      return;
    }
    req = req.substring(addr_start + 1, addr_end);
    Serial.print("Request: ");
    Serial.println(req);
    webclient.flush();

    String s;
    if (req == "/")
    {
      s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html> <html> <head> <meta http-equiv=\"Cache-Control\" content=\"no-cache, no-store, must-revalidate\"/> <meta http-equiv=\"Pragma\" content=\"no-cache\"/> <meta http-equiv=\"Expires\" content=\"0\"/> <title>{NAME}</title> <link rel=\"stylesheet\" href=\"https://unpkg.com/onsenui/css/onsenui.css\"> <link rel=\"stylesheet\" href=\"https://unpkg.com/onsenui/css/onsen-css-components.min.css\"> <script src=\"https://unpkg.com/onsenui/js/onsenui.min.js\"> </script> <script src=\"https://unpkg.com/jquery/dist/jquery.min.js\"> </script> <script> var cversion=\"{VERSION}\"; var wsUri=\"ws://lights.local:81/\"; var repo=\"esp8266blinds\"; window.fn={}; window.fn.open=function(){ var menu=document.getElementById('menu'); menu.open(); }; window.fn.load=function(page){ var content=document.getElementById('content'); var menu=document.getElementById('menu'); content.load(page) .then(menu.close.bind(menu)).then(setActions()); }; var gotoPos=function(percent, pin){ doSend('(' + pin + ')' + percent); }; var setActions=function(){ doSend(\"(update-d2)\"); doSend(\"(update-d3)\"); $.get(\"https://github.com/xurban42/esp8266blinds/releases\", function(data){if (data.length>0 && data[0].tag_name !==cversion){$(\"#cversion\").text(cversion); $(\"#nversion\").text(data[0].tag_name); $(\"#update-card\").show();}}); setTimeout(function(){ $(\"#light-on-d2\").on(\"click\", function(){$(\"#setrange-d2\").val(100); gotoPos(100, 'd2');}); $(\"#light-off-d2\").on(\"click\", function(){$(\"#setrange-d2\").val(0); gotoPos(0, 'd2');}); $(\"#setrange-d2\").on(\"change\", function(){gotoPos($(\"#setrange-d2\").val(), 'd2')}); $(\"#light-on-d3\").on(\"click\", function(){$(\"#setrange-d3\").val(100); gotoPos(100, 'd3');}); $(\"#light-off-d3\").on(\"click\", function(){$(\"#setrange-d3\").val(0); gotoPos(0, 'd3');}); $(\"#setrange-d3\").on(\"change\", function(){gotoPos($(\"#setrange-d3\").val(), 'd3')}); }, 200); }; $(document).ready(function(){setActions();}); var websocket; var timeOut; function retry(){ clearTimeout(timeOut); timeOut=setTimeout(function(){ websocket=null; init(); }, 5000); }; function init(){ons.notification.toast({message: 'Connecting...', timeout: 1000}); try { websocket=new WebSocket(wsUri); websocket.onclose=function (){}; websocket.onerror=function(evt){ ons.notification.toast({message: 'Cannot connect to device', timeout: 2000}); retry(); }; websocket.onopen=function(evt){ ons.notification.toast({message: 'Connected to device', timeout: 2000}); setTimeout(function(){ doSend(\"(update-d2)\"); doSend(\"(update-d3)\"); }, 1000); }; websocket.onclose=function(evt){ ons.notification.toast({message: 'Disconnected. Retrying', timeout: 2000}); retry(); }; websocket.onmessage=function(evt){ try{ var msg=JSON.parse(evt.data); if (typeof msg.lightState.d2 !=='undefined'){ $(\"#setrange-d2\").val(msg.lightState.d2); }; if (typeof msg.lightState.d3 !=='undefined'){ $(\"#setrange-d3\").val(msg.lightState.d3); }; }catch(err){} }; }catch (e){ ons.notification.toast({message: 'Cannot connect to device. Retrying...', timeout: 2000}); retry(); }; }; function doSend(msg){ if (websocket && websocket.readyState==1){ websocket.send(msg); } }; window.addEventListener(\"load\", init, false); window.onbeforeunload=function(){ if (websocket && websocket.readyState==1){ websocket.close(); }; }; </script> </head> <body> <ons-splitter> <ons-splitter-side id=\"menu\" side=\"left\" width=\"220px\" collapse swipeable> <ons-page> <ons-list> <ons-list-item onclick=\"fn.load('home.html')\" tappable> Home </ons-list-item> <ons-list-item onclick=\"fn.load('settings.html')\" tappable> Settings </ons-list-item> <ons-list-item onclick=\"fn.load('about.html')\" tappable> About </ons-list-item> </ons-list> </ons-page> </ons-splitter-side> <ons-splitter-content id=\"content\" page=\"home.html\"> </ons-splitter-content> </ons-splitter> <template id=\"home.html\"> <ons-page> <ons-toolbar> <div class=\"left\"> <ons-toolbar-button onclick=\"fn.open()\"> <ons-icon icon=\"md-menu\"> </ons-icon> </ons-toolbar-button> </div> <div class=\"center\">{NAME}</div> </ons-toolbar> <ons-card> <div class=\"title\">Adjust lights</div> <div class=\"content\"> <p>Move the slider to the wanted position or use the circles to turn on/off lights</p> </div> <ons-row> <ons-col width=\"40px\" style=\"text-align: center; line-height: 31px;\"> </ons-col> <ons-col> </ons-col> <ons-col width=\"40px\" style=\"text-align: center; line-height: 31px;\"> </ons-col> </ons-row> <ons-row> <ons-col width=\"40px\" style=\"text-align: center; line-height: 31px;\"> <ons-icon id=\"light-off-d2\" style=\"color:gray\" icon=\"fa-circle\" size=\"2x\"> </ons-icon> </ons-col> <ons-col> <ons-range id=\"setrange-d2\" style=\"width: 100%;\" value=\"25\"> </ons-range> </ons-col> <ons-col width=\"40px\" style=\"text-align: center; line-height: 31px;\"> <ons-icon id=\"light-on-d2\" style=\"color:orange\" icon=\"fa-circle\" size=\"2x\"> </ons-icon> </ons-col> </ons-row> <ons-row> <ons-col width=\"40px\" style=\"text-align: center; line-height: 31px;\"> <ons-icon id=\"light-off-d3\" style=\"color:gray\" icon=\"fa-circle\" size=\"2x\"> </ons-icon> </ons-col> <ons-col> <ons-range id=\"setrange-d3\" style=\"width: 100%;\" value=\"25\"> </ons-range> </ons-col> <ons-col width=\"40px\" style=\"text-align: center; line-height: 31px;\"> <ons-icon id=\"light-on-d3\" style=\"color:orange\" icon=\"fa-circle\" size=\"2x\"> </ons-icon> </ons-col> </ons-row> </ons-card> <ons-card id=\"update-card\" style=\"display:none\"> <div class=\"title\">Update available</div> <div class=\"content\">You are running <span id=\"cversion\"> </span> and <span id=\"nversion\"> </span> is the latest. Go to <a href=\"https://github.com/xurban42/esp8266blinds/releases\">the repo</a> to download</div> </ons-card> </ons-page> </template> <template id=\"settings.html\"> <ons-page> <ons-toolbar> <div class=\"left\"> <ons-toolbar-button onclick=\"fn.open()\"> <ons-icon icon=\"md-menu\"> </ons-icon> </ons-toolbar-button> </div> <div class=\"center\"> Settings </div> </ons-toolbar> </ons-page> </template> <template id=\"about.html\"> <ons-page> <ons-toolbar> <div class=\"left\"> <ons-toolbar-button onclick=\"fn.open()\"> <ons-icon icon=\"md-menu\"> </ons-icon> </ons-toolbar-button> </div> <div class=\"center\"> About </div> </ons-toolbar> <ons-card> <div class=\"title\">Lights Homebridge (ws/http)</div> <div class=\"content\"> <p> <ul> <li>Licensed unnder <a href=\"https://creativecommons.org/licenses/by/3.0/\">Creative Commons</a> </li> </ul> </p> </div> </ons-card> </ons-page> </template> </body> </html>\r\n\r\n";
      s.replace("{VERSION}", "V" + version);
      s.replace("{NAME}", String(config_name));
      Serial.println("Sending 200");
    } else if (req.substring(0, 8) == "/set/d2/") {
      String newPositionD2 = req.substring(8);
      s = "HTTP/1.1 204 No Content\r\n\r\n\r\n";
      Serial.println("Sending 204");
      currentBrightnessD2 = newPositionD2.toInt();
      setLight(d2Pin, newPositionD2.toInt());
      processMsg("(d2)" + newPositionD2, NULL);
    } else if (req.substring(0, 8) == "/set/d3/") {
      String newPositionD3 = req.substring(8);
      s = "HTTP/1.1 204 No Content\r\n\r\n\r\n";
      Serial.println("Sending 204");
      currentBrightnessD3 = newPositionD3.toInt();
      setLight(d3Pin, newPositionD3.toInt());
      processMsg("(d3)" + newPositionD3, NULL);
    } else {
      s = "HTTP/1.1 404 Not Found\r\n\r\n";
      Serial.println("Sending 404");
    }

    //Print page but as max package is 2048 we need to break it down
    while (s.length() > 2000) {
      String d = s.substring(0, 2000);
      webclient.print(d);
      s.replace(d, "");
    }
    webclient.print(s);

    Serial.println("Done with client");

  }
}
