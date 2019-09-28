#include <Arduino.h>
#include "Adafruit_SSD1306.h"
#include "Adafruit_GFX.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
// New libraries for NTP Client Implementation
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ssid = "**************";
const char* password = "*******************";
const char* mqtt_server = "*****************";

// Initialize pin variables
int relay = D6;
int termPin = A0;               // Analogový pin, ke kterému je termistor připojen
// Initialize timers
unsigned int loopBenchmark = 0;
unsigned int thermo_timer = 0;
unsigned int zakmit_timer = 0;
unsigned int wifiConnection_timer = 0;
unsigned int publish_timer = 0;
unsigned int mqtt_delay = 0;
unsigned int ntp_delay = 0;
// Initialize flags
bool buttonFlag = false;
// Initialize status 
bool relayStatus;
bool wifiConnection = false;
bool mqttConnection = false;
bool initsInitialiazed = false;
bool receivedTimeUpdate = false;
// Initialize normal variables
int loops = 0;
int pocetZakmitu = 0;
float waterTemp;
String str;
// MQTT Variables
long lastMsg = 0;
char msg[50];
int value = 0;
// Thermistor Variables
int termNom = 10000;  // Referenční odpor termistoru
int refTep = 25;      // Teplota pro referenční odpor
int beta = 3380;      // Beta faktor    
int rezistor = 10033; // hodnota odporu v sérii
/* Alternative Thermistor Settings  
int termNom = 10000;  // Referenční odpor termistoru
int refTep = 25;      // Teplota pro referenční odpor
int beta = 3950;      // Beta faktor
int rezistor = 10033; // hodnota odporu v sérii
*/
// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

Adafruit_SSD1306 display(-1);
WiFiClient espClient;
PubSubClient client(espClient);
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

#define LOGO16_GLCD_HEIGHT 32
#define LOGO16_GLCD_WIDTH  128 

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if((char)payload[0] == 'O' && (char)payload[1] == 'N'){
    buttonFlag = true;
  }
  else if((char)payload[0] == 'O' && (char)payload[1] == 'F' && (char)payload[2] == 'F'){
    buttonFlag = true;
  }
}

void ICACHE_RAM_ATTR interruptButtonFlag(){
  // Timer zabraňující chytání zákmitů
  if(zakmit_timer +700 < millis()){
    zakmit_timer = millis();
    buttonFlag = true;
  }
}
void getWifiInfo(){
  Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
  Serial.println("");
}
bool establishWifiConnection(){
  wifiConnection_timer = millis();
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if(WiFi.status() == WL_CONNECTED){
      getWifiInfo();
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

    if (client.connect(clientId.c_str(), "***********", "*******************")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      String statusToPublish = (relayStatus)? "true": "false";
      client.publish("garden/pool/watchdog/relay/state", statusToPublish.c_str());
      // ... and resubscribe
      client.subscribe("garden/pool/watchdog/relay/set");
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

float thermistorReadout(){
  float napeti;
  //změření napětí na termistoru
  napeti = analogRead(termPin);

  // Konverze změřené hodnoty na odpor termistoru
  napeti = 1023 / napeti - 1;
  napeti = rezistor / napeti;

  //Výpočet teploty podle vztahu pro beta faktor
  float teplota;
  teplota = napeti / termNom;         // (R/Ro)
  teplota = log(teplota);             // ln(R/Ro)
  teplota /= beta;                    // 1/B * ln(R/Ro)
  teplota += 1.0 / (refTep + 273.15); // + (1/To)
  teplota = 1.0 / teplota;            // Převrácená hodnota
  teplota -= 273.15;                  // Převod z Kelvinů na stupně Celsia

  //Serial.print("Teplota je: ");
  //Serial.print(teplota);
  //Serial.println(" *C");
  return teplota;
}

void setup() {
  Serial.begin(9600);         // Initialize serial for communication 
  //Serial.setDebugOutput(false); // Turn off WiFi debug messages for now 
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.display(); // Vykreslení loga knihovny na displeji aneb první známka funkčnosti :D
  delay(1000); // Pozdržení programu, aby logo nezmizelo moc rychle
  pinMode(relay,OUTPUT);
  pinMode(termPin,INPUT);
  pinMode(D7, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(int(D7)), interruptButtonFlag,FALLING);
  digitalWrite(relay,LOW);  // Při spuštění je relé defaultně vypnuté
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
  if(loopBenchmark +1000 < millis()){ // Benchmark rychlosti programu
    //Serial.print("Počet loopu za sekundu:");
    //Serial.println(loops);
    loops = 0;
    loopBenchmark = millis();
  }
    if(!wifiConnection){ // Kontrola připojení Wifi. Pokud není připojení aktivní, dojde ke kontrole aktuálního stavu
      if(WiFi.status() == WL_CONNECTED){
        wifiConnection = true; // Stav Wifi připojení se změní na funkční
        getWifiInfo(); // Funkce vypíše informace o Wifi připojení
        mqtt_delay = millis(); // Při připojení dojde k pozdržení pokusu o připojení k MQTT serveru, aby nedošlo k chybě při příliš brzkém pokusu o připojení
      }
    }else // Pokud je Wifi aktivní dojde k případnému připojení k MQTT serveru
    {
      if(!mqttConnection && (mqtt_delay + 5000 < millis())){   
      mqttConnection = establishMQTTConnection();     // Funkce se pokusí o připojení na MQTT Server
      mqtt_delay = millis();   // Pokud nedošlo k úspěšnému připojení, je další pokus iniciován po 5 sekundách
      }
    }
    
    if(wifiConnection && !initsInitialiazed){
      // Initialize a NTPClient to get time
      timeClient.begin();
      // Set offset time in seconds to adjust for your timezone, for example:
      // GMT +1 = 3600
      // GMT +8 = 28800
      // GMT -1 = -3600
      // GMT 0 = 0
      timeClient.setTimeOffset(7200);
    }

    client.loop(); // Udržování funkčnosti MQTT
    if(mqttConnection){   // Informace o stavu zařízení je odesílaná na MQTT Server každých 5 sekund
      if(publish_timer + 5000 < millis()){
        client.publish("garden/pool/watchdog/status","online");
        publish_timer = millis();
      }
    }
  // Smaže obsah na displeji
  display.clearDisplay(); 
  // Nastaví velikost textu
  display.setTextSize(1);
  // Nastaví barvu textu
  display.setTextColor(WHITE);
  // Vypíše text na displej (nezobrazí ho)
  display.setTextSize(4);
  display.setCursor(0,5);
  display.print((relayStatus)?"ON":"OFF");    //Stav relé (filtrace)
  display.setTextSize(1);
  display.setCursor(80,7);
  display.print((wifiConnection && mqttConnection)? "Online": "Offline");
  display.setCursor(84,24);
  display.print("10:23");
  
  //display.print("Filtrace: ");
  // DEBUG DISPLEJ
        //display.setCursor(0,0);
        //display.print("Wifi:");
        //display.print((wifiConnection)?"Connected":"Disconnected");
        //display.setCursor(0,10);
        //display.print("MQTT:");
        //display.print((mqttConnection)?"Connected":"Disconnected");
  
  
  if(thermo_timer + 30000 < millis()){
     waterTemp = thermistorReadout();
      str = String(waterTemp);
     client.publish("garden/pool/watchdog/water/temperature",str.c_str()); // Publikace teploty vody na MQTT
    thermo_timer = millis();
  }
  
  // Zajištění přepnutí při aktivovaném buttonFlagu s publikací informace na MQTT
  if(buttonFlag){
    if(relayStatus){ // Pokud je relé aktivované dojde k deaktivaci
        digitalWrite(relay,LOW);  
        relayStatus = false;      
        client.publish("garden/pool/watchdog/relay/state","false"); // Publikace informace o změně na MQTT
    }else {
      digitalWrite(relay,HIGH);    
      relayStatus = true;
      client.publish("garden/pool/watchdog/relay/state","true");
    }
    buttonFlag = false;   // Deaktivace flagu
  }

  if(millis() > ntp_delay){
    ntp_delay = millis();
  }

  if(receivedTimeUpdate){
    int splitT = formattedDate.indexOf("T");
    dayStamp = formattedDate.substring(0, splitT);
    Serial.print("DATE: ");
    Serial.println(dayStamp);
    // Extract time
    timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
    Serial.print("HOUR: ");
    Serial.println(timeStamp);
  }

  
  // Inicializuje zobrazení na displeji
  display.display();
  //delay(10);
}