//
// Alternative firmware for H801 5 channel LED dimmer
// based on https://github.com/mertenats/open-home-automation/blob/master/ha_mqtt_rgb_light/ha_mqtt_rgb_light.ino
//
#include <string>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>   // Local WebServer used to serve the configuration portal
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>        // WiFi Configuration Magic
#include <PubSubClient.h>       // MQTT client
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define DEVELOPMENT

WiFiManager wifiManager;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

bool greenLEDState;
bool redLEDState;

const char* mqtt_server = "192.168.0.57";

// Password for update server
const char* username = "admin";
const char* password = "charlie";

// MQTT topics
// state, brightness, rgb
const char* MQTT_TOPIC_PREFIX = "devices";

#define MAX_TOPIC_SIZE 64
char MQTT_UP_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_RGB_STATE_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_RGB_COMMAND_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_RGB_BRIGHTNESS_STATE_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_RGB_BRIGHTNESS_COMMAND_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_RGB_COLOUR_STATE_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_RGB_COLOUR_COMMAND_TOPIC[MAX_TOPIC_SIZE];

char MQTT_LIGHT_W1_STATE_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_W1_COMMAND_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_W1_BRIGHTNESS_STATE_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_W1_BRIGHTNESS_COMMAND_TOPIC[MAX_TOPIC_SIZE];

char MQTT_LIGHT_W2_STATE_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_W2_COMMAND_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_W2_BRIGHTNESS_STATE_TOPIC[MAX_TOPIC_SIZE];
char MQTT_LIGHT_W2_BRIGHTNESS_COMMAND_TOPIC[MAX_TOPIC_SIZE];

char* chip_id = "00000000";
char* myhostname = "H801-00000000";

char deviceId[32] = "H801";

// buffer used to send/receive data with MQTT
const uint8_t MSG_BUFFER_SIZE = 20;
char m_msg_buffer[MSG_BUFFER_SIZE];

// Light
// the payload that represents enabled/disabled state, by default
const char* LIGHT_ON = "ON";
const char* LIGHT_OFF = "OFF";

#define RGB_LIGHT_RED_PIN    15
#define RGB_LIGHT_GREEN_PIN  13
#define RGB_LIGHT_BLUE_PIN   12
#define W1_PIN               14
#define W2_PIN               4

#define GREEN_PIN    1
#define RED_PIN      5

// store the state of the rgb light (colors, brightness, ...)
boolean m_rgb_state = false;
uint8_t m_rgb_brightness = 255;
uint8_t m_rgb_red = 255;
uint8_t m_rgb_green = 255;
uint8_t m_rgb_blue = 255;

boolean m_w1_state = false;
uint8_t m_w1_brightness = 255;

boolean m_w2_state = false;
uint8_t m_w2_brightness = 255;

// Useful buffer and function for logging
#define LOG_MSG_BUFFER_SIZE 128
char logMsgBuffer[LOG_MSG_BUFFER_SIZE];

void logMsg(const char* msg) {
    Serial1.println(msg);
}

// Wrapper around debug LEDs
void setRedLEDOn(bool onState) {
    digitalWrite(RED_PIN, onState ? 0 : 1);
    redLEDState = onState;
}

void setGreenLEDOn(bool onState) {
    digitalWrite(GREEN_PIN, onState ? 0 : 1);
    greenLEDState = onState;
}

void flashDebugLED(int pin, int numTimes, bool currentOnState) 
{    
    for (int i=0; i<numTimes; i++)  {
        digitalWrite(pin, currentOnState ? 1 : 0);  
        delay(100);
        digitalWrite(pin, currentOnState ? 0: 1);
        delay(100);
    }
}

void flashGreenLED(int numTimes) {
    flashDebugLED(GREEN_PIN, numTimes, greenLEDState);
}

void flashRedLED(int numTimes) {
    flashDebugLED(RED_PIN, numTimes, redLEDState);
}

bool checkHTTPRequestPassword() {
    bool rc = false;

    String subPassword = httpServer.arg("password");

    if (strlen(password) == 0) {
        httpServer.send(401, "text/plain", "Operation not allowed: no password set on server");
        rc = false;
    } else if (strcmp(password, subPassword.c_str()) != 0) {
        httpServer.send(401, "text/plain", "Invalid password");
        rc = false;
    } else {
        rc = true;
    }

    return rc;
}

void setup()
{
  // Setup console
  Serial1.begin(57600);
  delay(10);

  logMsg("");
  logMsg("Setting up");

  pinMode(RGB_LIGHT_RED_PIN, OUTPUT);
  pinMode(RGB_LIGHT_GREEN_PIN, OUTPUT);
  pinMode(RGB_LIGHT_BLUE_PIN, OUTPUT);
  setColor(0, 0, 0);
  pinMode(W1_PIN, OUTPUT);
  setW1(0);
  pinMode(W2_PIN, OUTPUT);
  setW2(0);

  pinMode(GREEN_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  setRedLEDOn(false);
  setGreenLEDOn(false);

  sprintf(chip_id, "%08X", ESP.getChipId());
  sprintf(myhostname, "H801-%08X", ESP.getChipId());
  snprintf(deviceId, sizeof(deviceId), "H801-%s", chip_id);

  snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "Chip id=[%s]", chip_id);
  logMsg(logMsgBuffer);

  snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "Hostname=[%s]", myhostname);
  logMsg(logMsgBuffer);

  snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "Device id=[%s]", deviceId);
  logMsg(logMsgBuffer);

  wifiManager.setTimeout(3600);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  wifiManager.addParameter(&custom_mqtt_server);

  WiFiManagerParameter custom_password("password", "password for updates", password, 40);
  wifiManager.addParameter(&custom_password);

  wifiManager.setCustomHeadElement(chip_id);

  bool wifiConnected = false;
  logMsg("Wifi not connected");

  setRedLEDOn(false);           // No wifi connection
  setGreenLEDOn(false);         // No MQTT connection

  while (!wifiConnected) 
  { 
    logMsg("Calling wifiManager.autoConnect()");
    flashRedLED(2);

    wifiConnected = wifiManager.autoConnect();

    if (!wifiConnected) {
        logMsg("Wifi connect failed. Retrying");
        delay(1000);
    }
  }

  snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "Connected IP address=[%s]", WiFi.localIP().toString().c_str());
  logMsg(logMsgBuffer);
  setRedLEDOn(true);

  mqtt_server = custom_mqtt_server.getValue();
  password = custom_password.getValue();

  snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "MQTT server=[%s]", mqtt_server);
  logMsg(logMsgBuffer);
  snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "OTA user=[%s]", username);
  logMsg(logMsgBuffer);
  snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "OTA password=[%s]", password);
  logMsg(logMsgBuffer);

  // init the MQTT connection
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);

  // add prefix and chip ID to channel names
  snprintf(MQTT_UP_TOPIC, sizeof(MQTT_UP_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, "active", deviceId);

  snprintf(MQTT_LIGHT_RGB_STATE_TOPIC, sizeof(MQTT_LIGHT_RGB_STATE_TOPIC),"%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "rgb/light/status");
  snprintf(MQTT_LIGHT_RGB_COMMAND_TOPIC, sizeof(MQTT_LIGHT_RGB_COMMAND_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "rgb/light/command");
  snprintf(MQTT_LIGHT_RGB_BRIGHTNESS_STATE_TOPIC, sizeof(MQTT_LIGHT_RGB_BRIGHTNESS_STATE_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "rgb/brightness/status");
  snprintf(MQTT_LIGHT_RGB_BRIGHTNESS_COMMAND_TOPIC, sizeof(MQTT_LIGHT_RGB_BRIGHTNESS_COMMAND_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "rgb/brightness/command");
  snprintf(MQTT_LIGHT_RGB_COLOUR_STATE_TOPIC, sizeof(MQTT_LIGHT_RGB_COLOUR_STATE_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "rgb/colour/status");
  snprintf(MQTT_LIGHT_RGB_COLOUR_COMMAND_TOPIC, sizeof(MQTT_LIGHT_RGB_COLOUR_COMMAND_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "rgb/colour/command");

  snprintf(MQTT_LIGHT_W1_STATE_TOPIC, sizeof(MQTT_LIGHT_W1_STATE_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "w1/light/status");
  snprintf(MQTT_LIGHT_W1_COMMAND_TOPIC, sizeof(MQTT_LIGHT_W1_COMMAND_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "w1/light/command");
  snprintf(MQTT_LIGHT_W1_BRIGHTNESS_STATE_TOPIC, sizeof(MQTT_LIGHT_W1_BRIGHTNESS_STATE_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "w1/brightness/status");
  snprintf(MQTT_LIGHT_W1_BRIGHTNESS_COMMAND_TOPIC, sizeof(MQTT_LIGHT_W1_BRIGHTNESS_COMMAND_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "w1/brightness/command");

  snprintf(MQTT_LIGHT_W2_STATE_TOPIC, sizeof(MQTT_LIGHT_W2_STATE_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "w2/light/status");
  snprintf(MQTT_LIGHT_W2_COMMAND_TOPIC, sizeof(MQTT_LIGHT_W2_COMMAND_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "w2/light/command");
  snprintf(MQTT_LIGHT_W2_BRIGHTNESS_STATE_TOPIC, sizeof(MQTT_LIGHT_W2_BRIGHTNESS_STATE_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "w2/brightness/status");
  snprintf(MQTT_LIGHT_W2_BRIGHTNESS_COMMAND_TOPIC, sizeof(MQTT_LIGHT_W2_BRIGHTNESS_COMMAND_TOPIC), "%s/%s/%s", MQTT_TOPIC_PREFIX, deviceId, "w2/brightness/command");

  // Setup HTTP server URLs for browser control
  logMsg("Starting HTTP server");

  httpServer.on("/", []() {
      flashRedLED(2);
      httpServer.send(200, "text/html",   "<html>"
                                          "<p>This is an index page</p>"
                                          "<p>Commands available are : </p>"
                                          "<ul>"
                                          " <li>Reset - /reset?password=password</li>"
                                          "   <ul><li>Resets the device</li></ul>"
                                          " <li>ResetWifiMgr - /resetwifimgr?password=password</li>"
                                          "   <ul><li>Resets the Wifi Manager settings and resets the device</li></ul>"
                                          "</ul>"
                                          "</html>");
  });

  httpServer.on("/reset", []() {
      flashRedLED(1);
      logMsg("HTTP req for /reset");

      if (checkHTTPRequestPassword()) {
          const char* msg = "Resetting device ...";
          httpServer.send(200, "text/plain", msg);
          logMsg(msg);
          delay(1000);
          ESP.reset();
      }
  });

  httpServer.on("/resetwifimgr", []() {
      flashRedLED(1);
      logMsg("HTTP req for /resetwifimgr");

      if (checkHTTPRequestPassword()) {
          const char* msg = "Resetting WiFi manager config and resetting device ...";
          httpServer.send(200, "text/plain", msg);
          logMsg(msg);
          delay(1000);

          wifiManager.resetSettings();
          delay(200);
          ESP.reset();
      }
  });


  httpServer.begin();

  logMsg("HTTP server started");

  // do not start the OTA server if no password has been set
  if (strlen(password)!=0) 
  {
    MDNS.begin(myhostname);
    MDNS.addService("http", "tcp", 80);
    httpUpdater.setup(&httpServer, username, password);
    logMsg("OTA started ");
  }
  else {
    logMsg("OTA not started as not password set");
  }

  logMsg("Setup complete");
}

// function called to adapt the brightness and the colors of the led
void setColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue) {
  analogWrite(RGB_LIGHT_RED_PIN, map(p_red, 0, 255, 0, m_rgb_brightness));
  analogWrite(RGB_LIGHT_GREEN_PIN, map(p_green, 0, 255, 0, m_rgb_brightness));
  analogWrite(RGB_LIGHT_BLUE_PIN, map(p_blue, 0, 255, 0, m_rgb_brightness));
}


void setW1(uint8_t brightness) {
  // convert the brightness in % (0-100%) into a digital value (0-255)
  analogWrite(W1_PIN, brightness);
}

void setW2(uint8_t brightness) {
  // convert the brightness in % (0-100%) into a digital value (0-255)
  analogWrite(W2_PIN, brightness);
}

// function called to publish the state of the led (on/off)
void publishRGBState() {
  if (m_rgb_state) {
    mqttClient.publish(MQTT_LIGHT_RGB_STATE_TOPIC, LIGHT_ON, true);
  } else {
    mqttClient.publish(MQTT_LIGHT_RGB_STATE_TOPIC, LIGHT_OFF, true);
  }
}

void publishW1State() {
  if (m_w1_state) {
    mqttClient.publish(MQTT_LIGHT_W1_STATE_TOPIC, LIGHT_ON, true);
  } else {
    mqttClient.publish(MQTT_LIGHT_W1_STATE_TOPIC, LIGHT_OFF, true);
  }
}

void publishW2State() {
  if (m_w2_state) {
    mqttClient.publish(MQTT_LIGHT_W2_STATE_TOPIC, LIGHT_ON, true);
  } else {
    mqttClient.publish(MQTT_LIGHT_W2_STATE_TOPIC, LIGHT_OFF, true);
  }
}

// function called to publish the colors of the led (xx(x),xx(x),xx(x))
void publishRGBColor() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", m_rgb_red, m_rgb_green, m_rgb_blue);
  mqttClient.publish(MQTT_LIGHT_RGB_COLOUR_STATE_TOPIC, m_msg_buffer, true);
}

// function called to publish the brightness of the led (0-100)
void publishRGBBrightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_rgb_brightness);
  mqttClient.publish(MQTT_LIGHT_RGB_BRIGHTNESS_STATE_TOPIC, m_msg_buffer, true);
}

void publishW1Brightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_w1_brightness);
  mqttClient.publish(MQTT_LIGHT_W1_BRIGHTNESS_STATE_TOPIC, m_msg_buffer, true);
}

void publishW2Brightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_w2_brightness);
  mqttClient.publish(MQTT_LIGHT_W2_BRIGHTNESS_STATE_TOPIC, m_msg_buffer, true);
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) 
{
  flashGreenLED(1);

  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }

  snprintf(logMsgBuffer, sizeof(logMsgBuffer), "mqtt rx [%s] on [%s]", payload.c_str(), p_topic);
  logMsg(logMsgBuffer);

  // handle message topic
  if (String(MQTT_LIGHT_RGB_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON))) {
      m_rgb_state = true;
      setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
      publishRGBState();
    } else if (payload.equals(String(LIGHT_OFF))) {
      m_rgb_state = false;
      setColor(0, 0, 0);
      publishRGBState();
    }
  }  else if (String(MQTT_LIGHT_W1_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON))) {
      m_w1_state = true;
      setW1(m_w1_brightness);
      publishW1State();
    } else if (payload.equals(String(LIGHT_OFF))) {
      m_w1_state = false;
      setW1(0);
      publishW1State();
    }
  }  else if (String(MQTT_LIGHT_W2_COMMAND_TOPIC).equals(p_topic)) {
    // test if the payload is equal to "ON" or "OFF"
    if (payload.equals(String(LIGHT_ON))) {
      m_w2_state = true;
      setW2(m_w2_brightness);
      publishW2State();
    } else if (payload.equals(String(LIGHT_OFF))) {
      m_w2_state = false;
      setW2(0);
      publishW2State();
    }
  } else if (String(MQTT_LIGHT_RGB_BRIGHTNESS_COMMAND_TOPIC).equals(p_topic)) {
    uint8_t brightness = payload.toInt();
    if (brightness < 0 || brightness > 255) {
      // do nothing...
      return;
    } else {
      m_rgb_brightness = brightness;
      setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
      publishRGBBrightness();
    }
  } else if (String(MQTT_LIGHT_W1_BRIGHTNESS_COMMAND_TOPIC).equals(p_topic)) {
    uint8_t brightness = payload.toInt();
    if (brightness < 0 || brightness > 255) {
      // do nothing...
      return;
    } else {
      m_w1_brightness = brightness;
      setW1(m_w1_brightness);
      publishW1Brightness();
    }
  } else if (String(MQTT_LIGHT_W2_BRIGHTNESS_COMMAND_TOPIC).equals(p_topic)) {
    uint8_t brightness = payload.toInt();
    if (brightness < 0 || brightness > 255) {
      // do nothing...
      return;
    } else {
      m_w2_brightness = brightness;
      setW2(m_w2_brightness);
      publishW2Brightness();
    }
  } else if (String(MQTT_LIGHT_RGB_COLOUR_COMMAND_TOPIC).equals(p_topic)) {
    // get the position of the first and second commas
    uint8_t firstIndex = payload.indexOf(',');
    uint8_t lastIndex = payload.lastIndexOf(',');

    uint8_t rgb_red = payload.substring(0, firstIndex).toInt();
    if (rgb_red < 0 || rgb_red > 255) {
      return;
    } else {
      m_rgb_red = rgb_red;
    }

    uint8_t rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
    if (rgb_green < 0 || rgb_green > 255) {
      return;
    } else {
      m_rgb_green = rgb_green;
    }

    uint8_t rgb_blue = payload.substring(lastIndex + 1).toInt();
    if (rgb_blue < 0 || rgb_blue > 255) {
      return;
    } else {
      m_rgb_blue = rgb_blue;
    }

    setColor(m_rgb_red, m_rgb_green, m_rgb_blue);
    publishRGBColor();
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    setGreenLEDOn(false);   

    logMsg("Attempting MQTT connection...");
    flashGreenLED(3); 

    // Attempt to connect - set LWT
    if (mqttClient.connect(deviceId, MQTT_UP_TOPIC, 2, true, "0")) {
      logMsg("Connected");
      setGreenLEDOn(true);

      mqttClient.publish(MQTT_UP_TOPIC, "1");

      // Once connected, publish an announcement...
      // publish the initial values
      publishRGBState();
      publishRGBBrightness();
      publishRGBColor();

      publishW1State();
      publishW1Brightness();

      publishW2State();
      publishW2Brightness();

      // ... and resubscribe
      mqttClient.subscribe(MQTT_LIGHT_RGB_COMMAND_TOPIC);
      mqttClient.subscribe(MQTT_LIGHT_RGB_BRIGHTNESS_COMMAND_TOPIC);
      mqttClient.subscribe(MQTT_LIGHT_RGB_COLOUR_COMMAND_TOPIC);

      mqttClient.subscribe(MQTT_LIGHT_W1_COMMAND_TOPIC);
      mqttClient.subscribe(MQTT_LIGHT_W1_BRIGHTNESS_COMMAND_TOPIC);

      mqttClient.subscribe(MQTT_LIGHT_W2_COMMAND_TOPIC);
      mqttClient.subscribe(MQTT_LIGHT_W2_BRIGHTNESS_COMMAND_TOPIC);

    } else {
      snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "Failed to connect to mqtt server - mqtt.connect() rc=[%d]", mqttClient.state());
      logMsg(logMsgBuffer);
      logMsg("Waiting two seconds to reconnect");
      delay(2000);
    }
  }
}

uint16_t i = 0;

void loop()
{
  // process OTA updates
  httpServer.handleClient();
  
  i++;
  if (!mqttClient.connected()) {
      snprintf(logMsgBuffer, LOG_MSG_BUFFER_SIZE, "MQTT not connected state=[%d]", mqttClient.state());
      logMsg(logMsgBuffer);

      reconnect();
  }
  mqttClient.loop();

  // Post the full status to MQTT every 65535 cycles. This is roughly once a minute
  // this isn't exact, but it doesn't have to be. Usually, clients will store the value
  // internally. This is only used if a client starts up again and did not receive
  // previous messages
  delay(1);
  if (i == 0) {
    flashRedLED(1);

    publishRGBState();
    publishRGBBrightness();
    publishRGBColor();
    publishW1State();
    publishW1Brightness();
    publishW2State();
    publishW2Brightness();
  }
}
