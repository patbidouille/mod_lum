/*
 Projet de sonde luminosité.
 Ajout de commande des volets roulants pour une gestion de la fermeture la nuit du logement.
 
 Basé sur l'exemple Basic ESP8266 MQTT
 
 Il cherche une connexion sur un serveur MQTT puis :
  - Lit la luminusité et réagit en fonction
    * Au dessus du seuil - rien
    * au dessous - ferme les volets, 
  - Il envoie tout cela en MQTT
 
 FUTUR :
 On pourra définir les paramètres via une instruction MQTT
 
 Exemples :
 MQTT:
 https://github.com/aderusha/IoTWM-ESP8266/blob/master/04_MQTT/MQTTdemo/MQTTdemo.ino
 Witty:
 https://blog.the-jedi.co.uk/2016/01/02/wifi-witty-esp12f-board/
 Module tricapteur:
 http://arduinolearning.com/code/htu21d-bmp180-bh1750fvi-sensor-example.php
 
 
*/
 
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// DHT 11 sensor
#define DHTPIN 2
#define DHTTYPE DHT11
 
// Utilisation d’une photo-résistance 
// Et ports pour cmd volet
const int port = A0;    // LDR
#define haut 12		// port de commande des volets
//#define arret 13
#define bas 15
//#define lbp 14
#define lbp 13
 
// Update these with values suitable for your network.
// Buffer pour convertir en chaine de l'adresse IP de l'appareil
char buffer[20];                
/*const char* ssid1 = "FREEBOX_xxx_EXT";
const char* ssid2 = "FREEBOX_xxx_P2";
const char* password = "xxxx";
const char* mqtt_server = "192.168.0.x";
*/
byte mac[] = {0xDE,0xED,0xBA,0xFE,0xFE,0xED};
IPAddress ip(192,168,0,100);
IPAddress gateway(192,168,0,254);
IPAddress subnet(255,255,255,0);

const char* ssid1 = "FREEBOX_CHRISTINE_P2_EXT";
const char* ssid2 = "FREEBOX_CHRISTINE_P2";
const char* password = "F4CAE5467377";
const char* mqtt_server = "192.168.0.3";
const char* mqttUser = "mod";
const char* mqttPassword = "Plaqpsmdp";

 
// Création objet
WiFiClient espClient;
PubSubClient client(espClient);
 // DHT sensor
DHT dht(DHTPIN, DHTTYPE, 15);

// Variables
int valeur = 0;         // temperature
float vin = 0;          // voltage capteur
char msg[50];           // message mqtt
int value = 0;
unsigned long readTime;
//Buffer qui permet de décoder les messages MQTT reçus
char message_buff[100];
long lastMsg = 0;       // Horodatage du dernier message publié sur MQTT
long lastbas = 0;       // compteur entre 2 lecture de capteur
bool debug = true;      // Affiche sur la console si True
bool mess = false;      // true si message reçu
String sujet = "";      // contient le topic
String mesg = "";       // contient le message reçu
long lum = 30;          // Valeur de luminosité par défaut
bool enbas = false;     // Flag de test volet bas
 
//========================================
void setup() {
  pinMode(haut, OUTPUT);     // Initialize le mvmt haut
//  pinMode(arret, OUTPUT);     // Initialize le mvmt arret
  pinMode(bas, OUTPUT);     // Initialize le mvmt bas
  pinMode(lbp, OUTPUT);     // Initialize le BP
//  digitalWrite(bas,HIGH);
  
  Serial.begin(115200);
 
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();                      // initialize temperature sensor
  client.subscribe("mod_lum");
}
  
//========================================
/* Connection au wifi
- test le premier ssid
- sinon passe au seconds
*/

void setup_wifi() {
 
  int cpt = 0;
  boolean ssid = true;
  delay(10);
  int ss = 1;
 
  // We start by connecting to a WiFi network
  while (WiFi.status() != WL_CONNECTED) {
    if (ssid) {
      WiFi.begin(ssid1, password);
    }
    else {
      ss = 2;
      WiFi.begin(ssid2, password);
    }
    if (debug) {
        Serial.println();
        Serial.print("Connecting to ");
        if (ss == 1) {
          Serial.println(ssid1);
        } else {
          Serial.println(ssid2);
        }
    }
 
    int counter = 0;
    while ((WiFi.status() != WL_CONNECTED) && (cpt <= 20)) {
      delay(500);
      Serial.print(".");
      counter++;
      cpt=cpt+1;
    }
 
    if (cpt >= 20) {
      if (ssid) {
        ssid=false;
      } else {
        ssid=true;
      }
    }
  }

  // on définit l'ip
  // WiFi.config(ip, gateway, subnet);
   
  // SINON On récupère et on prépare le buffer contenant l'adresse IP attibué à l'ESP-01
  IPAddress ip = WiFi.localIP();   
/*  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  ipStr.toCharArray(buffer, 20);
*/

  if ( debug ) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
 
    Serial.print("Connecting to ");
    Serial.println(mqtt_server);
  }
 
}
 
//========================================
// Déclenche les actions à la réception d'un message
// D'après http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio
void callback(char* topic, byte* payload, unsigned int length) {
 
  // pinMode(lbp,OUTPUT);
  int i = 0;
  if ( debug ) {
    Serial.println("Message recu =>  topic: " + String(topic));
    Serial.print(" | longueur: " + String(length,DEC));
  }
  sujet = String(topic);
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
 
  String msgString = String(message_buff);
  mesg = msgString;
  if ( debug ) {
    Serial.println(" Payload: " + msgString);
  }
  mess = true;
 
}
 
//========================================
/* tente une reconnection au seveur mqtt */
void reconnect() {
  // Loop until we're reconnected
  int counter = 0;
  int compt = 0;
  boolean noconnect = true;
  // tant qu'il ne trouve pas un serveur affiche rond
  while ((!client.connected()) && (noconnect == true)) {
    counter++;
    delay(500);
 
    if (debug ) {
      Serial.print("Attempting MQTT connection...");
    }
    // Attempt to connect
    if (client.connect("ESP8266Client", mqttUser, mqttPassword )) {
      if (debug ) {
        Serial.println("connected");
      }
      // Once connected, publish an announcement...
      client.publish("mod_lum", "hello world");
      // ... and resubscribe
      client.subscribe("#");
      noconnect=true;
    } else {
      if ( debug ) {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
      }  
      // Wait 5 seconds before retrying
      if (compt <= 3) {
        delay(3000);
        counter=0;
        compt++;
      } else {
        noconnect=false;
      }
    }
 
  }
  delay(1500);
}
 
//========================================
/* Baiise le volet
 * essai en 2 fois séparé par 120sec
 */
 void baisse_volet() {
  long tmp = millis();
  
  int cmpt = 1;
  while ( cmpt < 3 ) {
    digitalWrite(bas,HIGH);
    delay(1000);
    digitalWrite(bas,LOW);
    delay(8000);
    client.publish("mod_lum", "On baisse les volets");
    if (debug) { 
      Serial.println("cmpt ");
      Serial.println(cmpt);
    }
    //cmpt = cmpt + 1;
    ++cmpt;
    if (debug) { 
      Serial.println("cmpt++ ");
      Serial.print(cmpt); 
    }
  }
 }
 

//========================================
void loop() {
  // test de connection, sinon reconnecte
  //int counter = 0;
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
    //Serial.println("Lecture du capteur");  
 
  // affiche message reçu en MQTT
  if ( mess ) {
   // pinMode(lbp,OUTPUT);
    if ( sujet == "mod_lum/conf" ) {
      lum = mesg.toInt();
    }
    if ( sujet == "mod_lum/cmd" ) {
      if ( mesg == "ON" ) {
        digitalWrite(lbp,HIGH);  
      } 
      if (mesg == "OFF") {
        digitalWrite(lbp,LOW);  
      }
    }
    if ( sujet == "mod_lum/haut" ) {
      digitalWrite(haut,HIGH);
      delay(5000);
      digitalWrite(haut,LOW);
    } 
    if ( sujet == "mod_lum/bas" ) {
      if ( mesg == "ON" ) {
        digitalWrite(bas,HIGH);
        delay(5000);
        digitalWrite(bas,LOW);
      }
    } 
   // pinMode(lbp,INPUT);
    mess = false;
  }
 
 
  // renvoie le niveau de la ldr tous les 10 sec
  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
//    value = value + 1;
//    snprintf (msg, 50, "Envoi numéro #%ld", value);
    //Serial.print("Publish message: ");
    //Serial.println(msg);
//    client.publish("mod_lum", msg);
 
  // Lit l’entrée analogique A0 
    valeur = analogRead(port);
    
  // convertit l’entrée en volt 
    vin = (valeur * 3.3) / 1024.0; 
    
    snprintf (msg, 50, "Luminosité %ld", valeur);
    client.publish("mod_lum", msg);
    snprintf (msg, 50, "voltage %ld", vin);
    client.publish("mod_lum", msg);
    
 
 
    if (debug) {
      //Serial.print("valeur = ");
      //Serial.println(valeur);   
      //Serial.print("volt = ");
      //Serial.println(vin); 
      if (enbas) {
        Serial.println("état volet = true");
      } else {
        Serial.println("état volet = false");
      }
    }
    
    if (( valeur < lum ) && (!enbas)) {
        baisse_volet();
        enbas = true;
    }
    if (( valeur > lum ) && (enbas)) {
      // renvoie les niveaux des capteurs tous les 180 sec (3min)
      // et traite les infos des capteurs pour lever/abaisser les volets
      long now2 = millis();
      if (lastbas == 0) {
        lastbas=now2;
      }
      if (now2 - lastbas > 8000) {
        lastbas = 0;
        enbas = false;
      }
    }
  }

} 
// fin loop
 

