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


// RGB FET
#define redPIN    15 //12
#define greenPIN  13 //15
#define bluePIN   12 //13

// W FET
#define w1PIN     14
#define w2PIN     4

// onbaord green LED D1
#define LEDPIN    5
// onbaord red LED D2
#define LED2PIN   1

// note
// TX GPIO2 @Serial1 (Serial ONE)
// RX GPIO3 @Serial


#define LEDoff digitalWrite(LEDPIN,HIGH)
#define LEDon digitalWrite(LEDPIN,LOW)

#define LED2off digitalWrite(LED2PIN,HIGH)
#define LED2on digitalWrite(LED2PIN,LOW)

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
  String str = "<root><specVersion><major>1</major><minor>0</minor></specVersion><URLBase>http://" + ipString + ":80/</URLBase><device><deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType><friendlyName>Philips hue (" + ipString + ")</friendlyName><manufacturer>Royal Philips Electronics</manufacturer><manufacturerURL>http://www.philips.com</manufacturerURL><modelDescription>Philips hue Personal Wireless Lighting</modelDescription><modelName>Philips hue bridge 2012</modelName><modelNumber>929000226503</modelNumber><modelURL>http://www.meethue.com</modelURL><serialNumber>00178817122c</serialNumber><UDN>uuid:2f402f80-da50-11e1-9b23-00178817122c</UDN><presentationURL>index.html</presentationURL><iconList><icon><mimetype>image/png</mimetype><height>48</height><width>48</width><depth>24</depth><url>hue_logo_0.png</url></icon><icon><mimetype>image/png</mimetype><height>120</height><width>120</width><depth>24</depth><url>hue_logo_3.png</url></icon></iconList></device></root>";
  server.send(200, "text/plain", str);
  Serial.println(str);
}


void setup()
{

  pinMode(LEDPIN, OUTPUT);
  pinMode(LED2PIN, OUTPUT);

  pinMode(redPIN, OUTPUT);
  pinMode(greenPIN, OUTPUT);
  pinMode(bluePIN, OUTPUT);
  pinMode(w1PIN, OUTPUT);
  pinMode(w2PIN, OUTPUT);

  // Setup console
  Serial1.begin(115200);
  delay(10);
  Serial1.println();
  Serial1.println();

  //client.set_callback(callback);

  LED2off;
  LEDon;

  wifiManager.setTimeout(3600);
  wifiManager.autoConnect();

  macString = String(WiFi.macAddress());
  ipString = StringIPaddress(WiFi.localIP());
  netmaskString = StringIPaddress(WiFi.subnetMask());
  gatewayString = StringIPaddress(WiFi.gatewayIP());

  LEDoff;

  Serial1.println("");

  Serial1.println("WiFi connected");
  Serial1.println("IP address: ");
  Serial1.println(WiFi.localIP());

  Serial.printf("Starting SSDP...\n");
  SSDP.begin();
  SSDP.setSchemaURL((char*)"description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName((char*)"H801 hue clone");
  SSDP.setSerialNumber((char*)"001788102201");
  SSDP.setURL((char*)"index.html");
  SSDP.setModelName((char*)"Philips hue bridge 2012");
  SSDP.setModelNumber((char*)"929000226503");
  SSDP.setModelURL((char*)"http://www.meethue.com");
  SSDP.setManufacturer((char*)"Royal Philips Electronics");
  SSDP.setManufacturerURL((char*)"http://www.philips.com");
  Serial.println("SSDP Started");

  Serial1.println("");

  server.on("/description.xml", HTTP_GET, handleDescription);
  server.begin();
  Serial1.println("Webserver started");

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



//
// LED code
//

void change_LED()
{
  int diff_red = abs(RED - RED_A);
  if (diff_red > 0) {
    led_delay_red = time_at_colour / abs(RED - RED_A);
  } else {
    led_delay_red = time_at_colour / 1023;
  }

  int diff_green = abs(GREEN - GREEN_A);
  if (diff_green > 0) {
    led_delay_green = time_at_colour / abs(GREEN - GREEN_A);
  } else {
    led_delay_green = time_at_colour / 1023;
  }

  int diff_blue = abs(BLUE - BLUE_A);
  if (diff_blue > 0) {
    led_delay_blue = time_at_colour / abs(BLUE - BLUE_A);
  } else {
    led_delay_blue = time_at_colour / 1023;
  }

  int diff_w1 = abs(W1 - W1_A);
  if (diff_w1 > 0) {
    led_delay_w1 = time_at_colour / abs(W1 - W1_A);
  } else {
    led_delay_w1 = time_at_colour / 1023;
  }

  int diff_w2 = abs(W2 - W2_A);
  if (diff_w2 > 0) {
    led_delay_w2 = time_at_colour / abs(W2 - W2_A);
  } else {
    led_delay_w2 = time_at_colour / 1023;
  }
}

void LED_RED()
{
  if (RED != RED_A) {
    if (RED_A > RED) RED_A = RED_A - 1;
    if (RED_A < RED) RED_A++;
    analogWrite(redPIN, RED_A);
  }
}

void LED_GREEN()
{
  if (GREEN != GREEN_A) {
    if (GREEN_A > GREEN) GREEN_A = GREEN_A - 1;
    if (GREEN_A < GREEN) GREEN_A++;
    analogWrite(greenPIN, GREEN_A);
  }
}

void LED_BLUE()
{
  if (BLUE != BLUE_A) {
    if (BLUE_A > BLUE) BLUE_A = BLUE_A - 1;
    if (BLUE_A < BLUE) BLUE_A++;
    analogWrite(bluePIN, BLUE_A);
  }
}

void LED_W1()
{
  if (W1 != W1_A) {
    if (W1_A > W1) W1_A = W1_A - 1;
    if (W1_A < W1) W1_A++;
    analogWrite(w1PIN, W1_A);
  }
}

void LED_W2()
{
  if (W2 != W2_A) {
    if (W2_A > W2) W2_A = W2_A - 1;
    if (W2_A < W2) W2_A++;
    analogWrite(w2PIN, W2_A);
  }
}

int convertToInt(char upper, char lower)
{
  int uVal = (int)upper;
  int lVal = (int)lower;
  uVal = uVal > 64 ? uVal - 55 : uVal - 48;
  uVal = uVal << 4;
  lVal = lVal > 64 ? lVal - 55 : lVal - 48;
  return uVal + lVal;
}
