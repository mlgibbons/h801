//
// Alternative firmware for H801 5 channel LED dimmer
// based on https://eryk.io/2015/10/esp8266-based-wifi-rgb-controller-h801/
//
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>   // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>        // WiFi Configuration Magic

WiFiManager wifiManager;

void LED_RED();
void LED_GREEN();
void LED_BLUE();
void LED_W1();
void LED_W2();
void change_LED();
int convertToInt(char upper,char lower);

#define PWM_VALUE 63
int gamma_table[PWM_VALUE+1] = {
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

// Start WiFi Server
WiFiServer server(80);

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

  LEDoff;
 
  Serial1.println("");
  
  Serial1.println("WiFi connected");
  Serial1.println("IP address: ");
  Serial1.println(WiFi.localIP());
  
  Serial1.println("");
  
  server.begin();
  Serial1.println("Webserver started");
  
}

void loop()
{
  // Check if a client has connected
  WiFiClient client = server.available();
  if (client) {
    
      // Read the first line of the request
      String req = client.readStringUntil('\r');
      Serial1.println(req);
      client.flush();
    
      // Prepare the response. Start with the common header:
      String s = "HTTP/1.1 200 OK\r\n";
      s += "Content-Type: text/html\r\n\r\n";
      s += "<!DOCTYPE HTML>\r\n<html>\r\n";
      
      if((req.indexOf("/rgb/") != -1)) {
         
        // get RGB value
        int pos = req.length();
        int ind1 = req.indexOf("/rgb/") + 5;
        String teststring = req.substring(ind1, pos);
        int ind2 = teststring.indexOf("/");
        String hexRGB = req.substring(ind1, ind2+ind1);
      
        // convert HEX to RGB
        hexRGB.toUpperCase();
        char c[6];
        hexRGB.toCharArray(c,7);
        long r = convertToInt(c[0],c[1]); //red
        long g = convertToInt(c[2],c[3]); //green
        long b = convertToInt(c[4],c[5]); //blue 
      
        // set value of RGB controller
        int red = map(r,0,255,0,PWM_VALUE); 
        red = constrain(red,0,PWM_VALUE);
        int green = map(g,0,255,0,PWM_VALUE);
        green = constrain(green, 0, PWM_VALUE);
        int blue = map(b,0,255,0,PWM_VALUE);
        blue = constrain(blue,0,PWM_VALUE);
      
        RED = gamma_table[red];
        GREEN = gamma_table[green];
        BLUE = gamma_table[blue];
        W1 = 0;
        W2 = 0;
        
        change_LED();
        
        // confirm to client
        s += "RGB value changed to ";
        s += hexRGB;
      
      }else if((req.indexOf("/w1/") != -1)) { 
         
        // get RGB value
        int pos = req.length();
        int ind1 = req.indexOf("/w1/") + 4;
        String teststring = req.substring(ind1, pos);
        int ind2 = teststring.indexOf("/");
        String hexW1 = req.substring(ind1, ind2+ind1);
      
        // convert HEX to W1
        hexW1.toUpperCase();
        char c[1];
        hexW1.toCharArray(c,2);
        long w = convertToInt(c[0],c[1]); //w1
        int w1 = map(w,0,255,0,1023); 
        w1 = constrain(w1,0,1023);
      
        RED = 0;
        GREEN = 0;
        BLUE = 0;
        W1 = w1;
        W2 = 0;
        
        change_LED();
        
        // confirm to client
        s += "W1 value changed to ";
        s += hexW1;
      
      }else if((req.indexOf("/w2/") != -1)) { 
         
        // get RGB value
        int pos = req.length();
        int ind1 = req.indexOf("/w2/") + 4;
        String teststring = req.substring(ind1, pos);
        int ind2 = teststring.indexOf("/");
        String hexW2 = req.substring(ind1, ind2+ind1);
      
        // convert HEX to W2
        hexW2.toUpperCase();
        char c[1];
        hexW2.toCharArray(c,2);
        long w = convertToInt(c[0],c[1]); //w2
        int w2 = map(w,0,255,0,1023); 
        w2 = constrain(w2,0,1023);
      
        RED = 0;
        GREEN = 0;
        BLUE = 0;
        W1 = 0;
        W2 = w2;
        
        change_LED();
        
        // confirm to client
        s += "W2 value changed to ";
        s += hexW2;      
      }
      
      s += "<br>"; // Go to the next line.
      s += "Request OK!";
      
      s += "</html>\n";
     
      client.flush();
    
      // Send the response to the client
      client.print(s);
      //delay(1);
      Serial1.println("Client disonnected");
    
      // The client will actually be disconnected 
      // when the function returns and 'client' object is detroyed
      
  }
  
  if(millis() - TIME_LED_RED >= led_delay_red){
    TIME_LED_RED = millis();
    LED_RED();
  }
  
  if(millis() - TIME_LED_GREEN >= led_delay_green){
    TIME_LED_GREEN = millis();
    LED_GREEN();
  }
  
  if(millis() - TIME_LED_BLUE >= led_delay_blue){
    TIME_LED_BLUE = millis();
    LED_BLUE();
  }
  
  if(millis() - TIME_LED_W1 >= led_delay_w1){
    TIME_LED_W1 = millis();
    LED_W1();
  }
  
  if(millis() - TIME_LED_W2 >= led_delay_w2){
    TIME_LED_W2 = millis();
    LED_W2();
  }
  
}

void change_LED()
{
  int diff_red = abs(RED-RED_A);
  if(diff_red > 0){
    led_delay_red = time_at_colour / abs(RED-RED_A); 
  }else{
    led_delay_red = time_at_colour / 1023; 
  }
  
  int diff_green = abs(GREEN-GREEN_A);
  if(diff_green > 0){
    led_delay_green = time_at_colour / abs(GREEN-GREEN_A);
  }else{
    led_delay_green = time_at_colour / 1023; 
  }
  
  int diff_blue = abs(BLUE-BLUE_A);
  if(diff_blue > 0){
    led_delay_blue = time_at_colour / abs(BLUE-BLUE_A); 
  }else{
    led_delay_blue = time_at_colour / 1023; 
  }
  
  int diff_w1 = abs(W1-W1_A);
  if(diff_w1 > 0){
    led_delay_w1 = time_at_colour / abs(W1-W1_A); 
  }else{
    led_delay_w1 = time_at_colour / 1023; 
  }
  
  int diff_w2 = abs(W2-W2_A);
  if(diff_w2 > 0){
    led_delay_w2 = time_at_colour / abs(W2-W2_A); 
  }else{
    led_delay_w2 = time_at_colour / 1023; 
  }
}

void LED_RED()
{
  if(RED != RED_A){
    if(RED_A > RED) RED_A = RED_A - 1;
    if(RED_A < RED) RED_A++;
    analogWrite(redPIN, RED_A);
  }
}

void LED_GREEN()
{
  if(GREEN != GREEN_A){
    if(GREEN_A > GREEN) GREEN_A = GREEN_A - 1;
    if(GREEN_A < GREEN) GREEN_A++;
    analogWrite(greenPIN, GREEN_A);
  }
}
  
void LED_BLUE()
{
  if(BLUE != BLUE_A){
    if(BLUE_A > BLUE) BLUE_A = BLUE_A - 1;
    if(BLUE_A < BLUE) BLUE_A++;
    analogWrite(bluePIN, BLUE_A);
  }
}

void LED_W1()
{
  if(W1 != W1_A){
    if(W1_A > W1) W1_A = W1_A - 1;
    if(W1_A < W1) W1_A++;
    analogWrite(w1PIN, W1_A);
  }
}

void LED_W2()
{
  if(W2 != W2_A){
    if(W2_A > W2) W2_A = W2_A - 1;
    if(W2_A < W2) W2_A++;
    analogWrite(w2PIN, W2_A);
  }
}

int convertToInt(char upper,char lower)
{
  int uVal = (int)upper;
  int lVal = (int)lower;
  uVal = uVal >64 ? uVal - 55 : uVal - 48;
  uVal = uVal << 4;
  lVal = lVal >64 ? lVal - 55 : lVal - 48;
  return uVal + lVal;
}
