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

#include "WittyCloud.h"

void LED_RED();
void LED_GREEN();
void LED_BLUE();
void LED_W1();
void LED_W2();
void change_LED();
int convertToInt(char upper, char lower);

#define PWM_VALUE 63

#define LEDoff digitalWrite(LEDPIN,HIGH)
#define LEDon digitalWrite(LEDPIN,LOW)

#define LED2off digitalWrite(LED2PIN,HIGH)
#define LED2on digitalWrite(LED2PIN,LOW)

#define SERIALNUMBER "001788102201"

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
      <serialNumber>SERIALNUMBER</serialNumber>\
      <UDN>uuid:2f402f80-da50-11e1-9b23-00178817122c</UDN>\
      <presentationURL>index.html</presentationURL>\
      <iconList>\
         <icon><mimetype>image/png</mimetype><height>48</height><width>48</width><depth>24</depth><url>hue_logo_0.png</url></icon>\
        <icon><mimetype>image/png</mimetype><height>120</height><width>120</width><depth>24</depth><url>hue_logo_3.png</url></icon>\
       </iconList>\
      </device></root>";
  server.send(200, "text/xml", str);
}

void handleApiPost() {
  SERIAL.println("URI: " + server.uri());
  String str = "[{\"success\":{\"username\": \"1028d66426293e821ecfd9ef1a0731df\"}}]";
  server.send(200, "text/json", str);
}

void handleOther() {

  String uri = server.uri();

  if (server.method() == HTTP_GET) {
    if (uri.startsWith("/api/")) {
      String json = apiFull();
      SERIAL.println(json);
      server.send(200, "text/json", json);
    } else {
      server.send(500, "text/plain", "Unknown request");
    }
  } else if (server.method() == HTTP_POST) {
    SERIAL.println("Post to "+server.uri());
  } else if (server.method() == HTTP_PUT) {
    if ((uri.endsWith("/state")) && (uri.indexOf("/lights/") > 0)) {
      int p1=uri.lastIndexOf("/lights/")+8;
      int p2=uri.lastIndexOf("/state");
      String lightid = uri.substring(p1,p2);
      SERIAL.println("Put for light id "+lightid);
      SERIAL.print(server.arg(0));
      
    }
  } else {
    SERIAL.println("Unknown method to "+server.uri());
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


void setup()
{
  // Setup console
  SERIAL.begin(115200);
  SERIAL.println("Starting up");

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

  SERIAL.println("");

  SERIAL.println("WiFi connected");
  SERIAL.println("IP address: ");
  SERIAL.println(WiFi.localIP());

  SERIAL.println("Starting SSDP...");
  SSDP.setSchemaURL((char*)"description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName((char*)"Philips hue clone (H801)");
  SSDP.setSerialNumber((char*)"SERIALNUMBER");
  SSDP.setURL((char*)"index.html");
  SSDP.setModelName((char*)"Philips hue bridge 2012");
  SSDP.setModelNumber((char*)"929000226503");
  SSDP.setModelURL((char*)"http://www.meethue.com");
  SSDP.setManufacturer((char*)"Royal Philips Electronics");
  SSDP.setManufacturerURL((char*)"http://www.philips.com");
  SSDP.begin();
  SERIAL.println("SSDP Started");

  SERIAL.println("");

  server.on("/description.xml", HTTP_GET, handleDescription);
  server.on("/api", HTTP_POST, handleApiPost);
  server.on("/index.txt", HTTP_GET, []() {
    server.send(200, "text/plain", "Hello World!");
  });
  server.onNotFound(handleOther);

  server.begin();
  SERIAL.println("Webserver started");

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


