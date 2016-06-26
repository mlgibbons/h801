//
// Alternative firmware for H801 5 channel LED dimmer
// http://www.open-homeautomation.com
// based on https://eryk.io/2015/10/esp8266-based-wifi-rgb-controller-h801/
// HUE code inspired by https://github.com/probonopd/ESP8266HueEmulator
//
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>   // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>        // WiFi Configuration Magic
#include <ESP8266SSDP.h>
#include <ArduinoJson.h>

void LED_RED();
void LED_GREEN();
void LED_BLUE();
void LED_W1();
void LED_W2();
void change_LED();
int convertToInt(char upper, char lower);

#define PWM_VALUE 63
int gamma_table[PWM_VALUE + 1] = {
  0, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 8, 9, 10,
  11, 12, 13, 15, 17, 19, 21, 23, 26, 29, 32, 36, 40, 44, 49, 55,
  61, 68, 76, 85, 94, 105, 117, 131, 146, 162, 181, 202, 225, 250,
  279, 311, 346, 386, 430, 479, 534, 595, 663, 739, 824, 918, 1023
};


// RGB for Witty Cloud
#define redPIN    15
#define greenPIN  12
#define bluePIN   13

// W FET
#define w1PIN     16
#define w2PIN     14

// onbaord green LED D1
#define LEDPIN    2
// onbaord red LED D2
#define LED2PIN   2

// note
// TX GPIO2 @Serial1 (Serial ONE)
// RX GPIO3 @Serial


#define LEDoff digitalWrite(LEDPIN,HIGH)
#define LEDon digitalWrite(LEDPIN,LOW)

#define LED2off digitalWrite(LED2PIN,HIGH)
#define LED2on digitalWrite(LED2PIN,LOW)

#define SERIAL "001788102201"

int led_delay_red = 0;
int led_delay_green = 0;
int led_delay_blue = 0;
int led_delay_w1 = 0;
int led_delay_w2 = 0;
#define time_at_colour 1300
unsigned long TIME_LED_RED = 0;
unsigned long TIME_LED_GREEN = 0;
unsigned long TIME_LED_BLUE = 0;
unsigned long TIME_LED_W1 = 0;
unsigned long TIME_LED_W2 = 0;
int RED, GREEN, BLUE, W1, W2;
int RED_A = 0;
int GREEN_A = 0;
int BLUE_A = 0;
int W1_A = 0;
int W2_A = 0;

byte mac[6]; // MAC address
String macString;
String ipString;
String netmaskString;
String gatewayString;

WiFiManager wifiManager;
ESP8266WebServer server(80);



void handleDescription() {
  String str = "<root> \
    <specVersion><major>1</major><minor>0</minor></specVersion>\
    <URLBase>http://" + ipString + ":80/</URLBase>\
    <device>\
      <deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\
      <friendlyName>Philips hue (" + ipString + ")</friendlyName>\
      <manufacturer>Royal Philips Electronics</manufacturer>\
      <manufacturerURL>http://www.philips.com</manufacturerURL>\
      <modelDescription>Philips hue Personal Wireless Lighting</modelDescription>\
      <modelName>Philips hue bridge 2012</modelName>\
      <modelNumber>929000226503</modelNumber>\
      <modelURL>http://www.meethue.com</modelURL>\
      <serialNumber>SERIAL</serialNumber>\
      <UDN>uuid:2f402f80-da50-11e1-9b23-00178817122c</UDN>\
      <presentationURL>index.html</presentationURL>\
      <iconList>\
         <icon><mimetype>image/png</mimetype><height>48</height><width>48</width><depth>24</depth><url>hue_logo_0.png</url></icon>\
        <icon><mimetype>image/png</mimetype><height>120</height><width>120</width><depth>24</depth><url>hue_logo_3.png</url></icon>\
       </iconList>\
      </device></root>";
  server.send(200, "text/xml", str);
  Serial.println(str);
}

void handleApiPost() {

  Serial.println("URI: " + server.uri());

  boolean isAuthorized = true; // FIXME: Instead, we should persist (save) the username and put it on the whitelist
  String client = "Test";
  Serial.println("CLIENT: ");
  Serial.println(client);

  String str = "[{\"success\":{\"username\": \"1028d66426293e821ecfd9ef1a0731df\"}}]";
  server.send(200, "text/json", str);
  Serial.println(str);
}

void handleOther() {

  Serial.println("URI: " + server.uri());

  if (server.uri().startsWith("/api/")) {
    String json = apiFull();
    Serial.println(json);
    server.send(200, "text/json", json);
  } else {
    server.send(500, "text/plain", "Unknown request");
  }
}

String apiFull() {
  DynamicJsonBuffer  jsonBuffer;
  JsonObject& base = jsonBuffer.createObject();
  JsonObject& lights = base.createNestedObject("lights");
  addApiLights(lights);
  JsonObject& config = base.createNestedObject("config");
  addApiConfig(config);

  char buffer[1024];
  base.printTo(buffer, sizeof(buffer));
  String res = String(buffer);
  return res;
}


void addApiLights(JsonObject& lights) {
  int i;
  for (i = 1; i <= 1; i++) {
    JsonObject& l = lights.createNestedObject("1");
    JsonObject& state = l.createNestedObject("state");
    state["on"] = false;
    state["bri"] = 0;
    state["hue"] = 0;
    state["sat"] = 0;
    JsonArray& xy = state.createNestedArray("xy");
    xy.add(0);
    xy.add(0);
    state["ct"] = 0;
    state["alert"] = "none";
    state["effect"] = "none";
    state["colormode"] = "hs";
    state["reachable"] = true;
    l["type"] = "Extended color light";
    l["name"] = "RGB";
    l["modelid"] = "H801";
    l["swversion"] = "10000000";
    JsonObject& ps = l.createNestedObject("pointsymbol");
    ps["1"] = "None";
    ps["2"] = "None";
    ps["3"] = "None";
    ps["4"] = "None";
    ps["5"] = "None";
    ps["6"] = "None";
    ps["7"] = "None";
    ps["8"] = "None";
  }
}

void addApiConfig(JsonObject& config) {
  config["name"] = "H801";
  config["mac"] = macString;
  config["dhcp"] = true;
  config["ipaddress"] = ipString;
  config["gateway"] = gatewayString;
  config["netmask"] = netmaskString;
}



//   "lights": {
//    "1": {
//      "state": {
//        "on": false,
//        "bri": 0,
//        "hue": 0,
//        "sat": 0,
//        "xy": [0.0000, 0.0000],
//        "ct": 0,
//        "alert": "none",
//        "effect": "none",
//        "colormode": "hs",
//        "reachable": true
//      },
//      "type": "Extended color light",
//      "name": "Hue Lamp 1",
//      "modelid": "LCT001",
//      "swversion": "65003148",
//      "pointsymbol": {
//        "1": "none",
//        "2": "none",
//        "3": "none",
//        "4": "none",
//        "5": "none",
//        "6": "none",
//        "7": "none",
//        "8": "none"
//      }
//    },

void setup()
{
  // Setup console
  Serial.begin(115200);
  Serial.println("Starting up");

  // Configure GPIOs
  pinMode(LEDPIN, OUTPUT);
  pinMode(LED2PIN, OUTPUT);

  pinMode(redPIN, OUTPUT);
  pinMode(greenPIN, OUTPUT);
  pinMode(bluePIN, OUTPUT);
  pinMode(w1PIN, OUTPUT);
  pinMode(w2PIN, OUTPUT);

  LED2off;
  LEDon;

  wifiManager.setTimeout(3600);
  wifiManager.autoConnect();

  macString = String(WiFi.macAddress());
  ipString = StringIPaddress(WiFi.localIP());
  netmaskString = StringIPaddress(WiFi.subnetMask());
  gatewayString = StringIPaddress(WiFi.gatewayIP());

  LEDoff;

  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting SSDP...");
  SSDP.setSchemaURL((char*)"description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName((char*)"Philips hue clone (H801)");
  SSDP.setSerialNumber((char*)SERIAL);
  SSDP.setURL((char*)"index.html");
  SSDP.setModelName((char*)"Philips hue bridge 2012");
  SSDP.setModelNumber((char*)"929000226503");
  SSDP.setModelURL((char*)"http://www.meethue.com");
  SSDP.setManufacturer((char*)"Royal Philips Electronics");
  SSDP.setManufacturerURL((char*)"http://www.philips.com");
  SSDP.begin();
  Serial.println("SSDP Started");

  Serial.println("");

  server.on("/description.xml", HTTP_GET, handleDescription);
  server.on("/api", HTTP_POST, handleApiPost);
  server.on("/index.txt", HTTP_GET, []() {
    server.send(200, "text/plain", "Hello World!");
  });
  server.onNotFound(handleOther);

  server.begin();
  Serial.println("Webserver started");

}

void loop()
{
  server.handleClient();
}



//
// Helper functions
//
String StringIPaddress(IPAddress myaddr)
{
  String LocalIP = "";
  for (int i = 0; i < 4; i++)
  {
    LocalIP += String(myaddr[i]);
    if (i < 3) LocalIP += ".";
  }
  return LocalIP;
}


