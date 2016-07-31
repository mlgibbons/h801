//
// Alternative firmware for H801 5 channel LED dimmer
// based on https://github.com/mertenats/open-home-automation/blob/master/ha_mqtt_rgb_light/ha_mqtt_rgb_light.ino
//
#include <string>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>   // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>        // WiFi Configuration Magic
#include <PubSubClient.h>       // MQTT client

WiFiManager wifiManager;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

const char* mqtt_server = "192.168.1.89";

// MQTT topics
// state, brightness, rgb
const char* MQTT_UP = "active";
char* MQTT_LIGHT_RGB_STATE_TOPIC = "XXXXXXXX/rgb/light/status";
char* MQTT_LIGHT_RGB_COMMAND_TOPIC = "XXXXXXXX/rgb/light/switch";
char* MQTT_LIGHT_RGB_BRIGHTNESS_STATE_TOPIC = "XXXXXXXX/rgb/brightness/status";
char* MQTT_LIGHT_RGB_BRIGHTNESS_COMMAND_TOPIC = "XXXXXXXX/rgb/brightness/set";
char* MQTT_LIGHT_RGB_RGB_STATE_TOPIC = "XXXXXXXX/rgb/rgb/status";
char* MQTT_LIGHT_RGB_RGB_COMMAND_TOPIC = "XXXXXXXX/rgb/rgb/set";

char* MQTT_LIGHT_W1_STATE_TOPIC = "XXXXXXXX/w1/light/status";
char* MQTT_LIGHT_W1_COMMAND_TOPIC = "XXXXXXXX/w1/light/switch";
char* MQTT_LIGHT_W1_BRIGHTNESS_STATE_TOPIC = "XXXXXXXX/w1/brightness/status";
char* MQTT_LIGHT_W1_BRIGHTNESS_COMMAND_TOPIC = "XXXXXXXX/w1/brightness/set";

char* MQTT_LIGHT_W2_STATE_TOPIC = "XXXXXXXX/w2/light/status";
char* MQTT_LIGHT_W2_COMMAND_TOPIC = "XXXXXXXX/w2/light/switch";
char* MQTT_LIGHT_W2_BRIGHTNESS_STATE_TOPIC = "XXXXXXXX/w2/brightness/status";
char* MQTT_LIGHT_W2_BRIGHTNESS_COMMAND_TOPIC = "XXXXXXXX/w2/brightness/set";


char* chip_id = "00000000";

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

#define LEDPIN    2
#define LED2PIN   2

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

void setup()
{

  pinMode(LEDPIN, OUTPUT);
  pinMode(LED2PIN, OUTPUT);

  pinMode(RGB_LIGHT_RED_PIN, OUTPUT);
  pinMode(RGB_LIGHT_GREEN_PIN, OUTPUT);
  pinMode(RGB_LIGHT_BLUE_PIN, OUTPUT);
  setColor(0, 0, 0);
  pinMode(W1_PIN, OUTPUT);
  setW1(0);
  pinMode(W2_PIN, OUTPUT);
  setW2(0);

  // Setup console
  Serial1.begin(115200);
  delay(10);
  Serial1.println();
  Serial1.println();

  wifiManager.setTimeout(3600);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.autoConnect();

  mqtt_server = custom_mqtt_server.getValue();

  Serial1.println("");

  Serial1.println("WiFi connected");
  Serial1.println("IP address: ");
  Serial1.println(WiFi.localIP());

  Serial1.println("");

  // init the MQTT connection
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  sprintf(chip_id, "%08X", ESP.getChipId());

  // replace chip ID in channel names
  memcpy(MQTT_LIGHT_RGB_STATE_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_RGB_COMMAND_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_RGB_BRIGHTNESS_STATE_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_RGB_BRIGHTNESS_COMMAND_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_RGB_RGB_STATE_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_RGB_RGB_COMMAND_TOPIC, chip_id, 8);

  memcpy(MQTT_LIGHT_W1_STATE_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_W1_COMMAND_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_W1_BRIGHTNESS_STATE_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_W1_BRIGHTNESS_COMMAND_TOPIC, chip_id, 8);

  memcpy(MQTT_LIGHT_W2_STATE_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_W2_COMMAND_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_W2_BRIGHTNESS_STATE_TOPIC, chip_id, 8);
  memcpy(MQTT_LIGHT_W2_BRIGHTNESS_COMMAND_TOPIC, chip_id, 8);

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
    client.publish(MQTT_LIGHT_RGB_STATE_TOPIC, LIGHT_ON);
  } else {
    client.publish(MQTT_LIGHT_RGB_STATE_TOPIC, LIGHT_OFF);
  }
}

void publishW1State() {
  if (m_w1_state) {
    client.publish(MQTT_LIGHT_W1_STATE_TOPIC, LIGHT_ON);
  } else {
    client.publish(MQTT_LIGHT_W1_STATE_TOPIC, LIGHT_OFF);
  }
}

void publishW2State() {
  if (m_w2_state) {
    client.publish(MQTT_LIGHT_W2_STATE_TOPIC, LIGHT_ON);
  } else {
    client.publish(MQTT_LIGHT_W2_STATE_TOPIC, LIGHT_OFF);
  }
}

// function called to publish the colors of the led (xx(x),xx(x),xx(x))
void publishRGBColor() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", m_rgb_red, m_rgb_green, m_rgb_blue);
  client.publish(MQTT_LIGHT_RGB_RGB_STATE_TOPIC, m_msg_buffer);
}

// function called to publish the brightness of the led (0-100)
void publishRGBBrightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_rgb_brightness);
  client.publish(MQTT_LIGHT_RGB_BRIGHTNESS_STATE_TOPIC, m_msg_buffer);
}

void publishW1Brightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_w1_brightness);
  client.publish(MQTT_LIGHT_W1_BRIGHTNESS_STATE_TOPIC, m_msg_buffer);
}

void publishW2Brightness() {
  snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", m_w2_brightness);
  client.publish(MQTT_LIGHT_W2_BRIGHTNESS_STATE_TOPIC, m_msg_buffer);
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  
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
  } else if (String(MQTT_LIGHT_RGB_RGB_COMMAND_TOPIC).equals(p_topic)) {
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
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(chip_id)) {
      Serial.println("connected");

      client.publish(MQTT_UP, chip_id);

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
      client.subscribe(MQTT_LIGHT_RGB_COMMAND_TOPIC);
      client.subscribe(MQTT_LIGHT_RGB_BRIGHTNESS_COMMAND_TOPIC);
      client.subscribe(MQTT_LIGHT_RGB_RGB_COMMAND_TOPIC);

      client.subscribe(MQTT_LIGHT_W1_COMMAND_TOPIC);
      client.subscribe(MQTT_LIGHT_W1_BRIGHTNESS_COMMAND_TOPIC);

      client.subscribe(MQTT_LIGHT_W2_COMMAND_TOPIC);
      client.subscribe(MQTT_LIGHT_W2_BRIGHTNESS_COMMAND_TOPIC);

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
