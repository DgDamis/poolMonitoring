#include <Arduino.h>
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Adam_2.4G";
const char* password = "******";
const char* mqtt_server = "broker.shiftr.io";

// Initialize pin variables
int relay = D6;
int thermistor = A0;
// Initialize timers
unsigned int loopBenchmark = 0;
unsigned int thermo_timer = 0;
unsigned int zakmit_timer = 0;
unsigned int wifiConnection_timer = 0;
unsigned int publish_timer = 0;
unsigned int mqtt_delay = 0;
// Initialize flags
bool buttonFlag = false;
// Initialize status 
bool relayStatus;
bool wifiConnection = false;
bool mqttConnection = false;
// Initialize normal variables
int loops = 0;
int pocetZakmitu = 0;
// MQTT Variables
long lastMsg = 0;
char msg[50];
int value = 0;

Adafruit_SSD1306 display(-1);
WiFiClient espClient;
PubSubClient client(espClient);

#define LOGO16_GLCD_HEIGHT 32
#define LOGO16_GLCD_WIDTH  128 

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}
void printThermistor(){
  Serial.print("Hodnota analogu A0:");
  Serial.println(analogRead(thermistor));
}
void ICACHE_RAM_ATTR interruptButtonFlag(){
  // Timer zabraňující chytání zákmitů
  if(zakmit_timer +700 < millis()){
    zakmit_timer = millis();
    buttonFlag = true;
    //pocetZakmitu++; //Debugovací proměnná
  }
}
bool establishWifiConnection(){
  wifiConnection_timer = millis();
  //unsigned int timeout = millis();
  //unsigned int print_timer = millis();

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  /* 
  while (WiFi.status() != WL_CONNECTED && timeout + 5000 > millis() ) {
      if(print_timer + 100 < millis()){
          Serial.print(".");
          print_timer = millis();
      }
      yield();
  }
  */

  if(WiFi.status() == WL_CONNECTED){
      //Serial.println("");
      //Serial.println("WiFi connected");
      ///Serial.println("IP address: ");
      //Serial.println(WiFi.localIP());
      return true;
  }
  else {
      //Serial.println("");
      //Serial.println("Couldn't establish Wifi connection at the moment.");
      return false;
  }
  
}
bool establishMQTTConnection(){
  if(wifiConnection){
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-WemosPool";

    if (client.connect(clientId.c_str(), "*****", "******")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("status", "alive");
      // ... and resubscribe
      client.subscribe("management");
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again later");
      return false;
  }
  }
return false;
}
void getWifiInfo(){
  Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
  Serial.println("");
}

void setup() {
  Serial.begin(9600);         // Initialize serial for communication 
  //Serial.setDebugOutput(false); // Turn off WiFi debug messages for now 
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.display();
  delay(1000);
   // Clear the buffer.
  //display.clearDisplay(); 
  pinMode(relay,OUTPUT);
  pinMode(thermistor,INPUT);
  pinMode(D7, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(int(D7)), interruptButtonFlag,FALLING);
  digitalWrite(relay,LOW);
  relayStatus = false;
  wifiConnection = establishWifiConnection();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  if(wifiConnection){
    mqttConnection = establishMQTTConnection();
  }
}

void loop(){
  loops++;
  if(loopBenchmark +1000 < millis()){
    Serial.print("Počet loopu za sekundu:");
    Serial.println(loops);
    //Serial.print("Počet zakmitu:");
    //Serial.println(pocetZakmitu);
    loops = 0;
    loopBenchmark = millis();
  }
    if(!wifiConnection){
      if(WiFi.status() == WL_CONNECTED){
        wifiConnection = true;
        getWifiInfo();
        mqtt_delay = millis();
      }
    }else
    {
      if(!mqttConnection && (mqtt_delay + 5000 < millis())){
      mqttConnection = establishMQTTConnection();
      mqtt_delay = millis();
      }
    }
    
    client.loop();
    if(mqttConnection){
      if(publish_timer + 5000 < millis()){
        client.publish("status","alive");
        publish_timer = millis();
      }
    }
  // Smaže obsah na displeji
  display.clearDisplay(); 
  // Nastaví velikost textu
  display.setTextSize(1);
  // Nastaví barvu textu
  display.setTextColor(WHITE);
  // Nastaví pozici kurzoru
  display.setCursor(0,0);
  // Vypíše text na displej (nezobrazí ho)
  display.print("Wifi:");
  display.print((wifiConnection)?"Connected":"Disconnected");
  display.setCursor(0,10);
  display.print("MQTT:");
  display.print((mqttConnection)?"Connected":"Disconnected");
  //display.print(analogRead(thermistor));
  
  if(thermo_timer + 1000 < millis()){
  //printThermistor();
  thermo_timer = millis();
  }
  
  if(buttonFlag){
    if(relayStatus){
        digitalWrite(relay,LOW);
        relayStatus = false;
    }else {
      digitalWrite(relay,HIGH);    
      relayStatus = true;
    }
    buttonFlag = false;
  }
  
  // Inicializuje zobrazení na displeji
  display.display();
  //delay(10);
}