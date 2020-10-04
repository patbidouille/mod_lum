 /*
 Projet de sonde luminosité.
 Ajout de commande des volets roulants pour une gestion de la fermeture la nuit du logement.
 
 Basé sur l'exemple Basic ESP8266 MQTT
 
 Il cherche une connexion sur un serveur MQTT puis :
  - Lit la luminusité et réagit en fonction
    * Au dessus du seuil - rien
    * au dessous - ferme les volets, 
  - Il envoie tout cela en MQTT
  
Programmation via OTA possible
 
 FUTUR :
 On pourra définir les paramètres via une instruction MQTT
 
 Exemples :
 MQTT:
 https://github.com/aderusha/IoTWM-ESP8266/blob/master/04_MQTT/MQTTdemo/MQTTdemo.ino
 Witty:
 https://blog.the-jedi.co.uk/2016/01/02/wifi-witty-esp12f-board/
 Module tricapteur:
 http://arduinolearning.com/code/htu21d-bmp180-bh1750fvi-sensor-example.php
 
 Commande et conf MQTT
 - mod_lum/conf = Défini le seuil de luminosité bas
   mosquitto_pub -d -t mod_lum/conf -m "175"
 - mod_lum/cmd  = 
    si mesg == "ON" 	On allume la led
    si mesg == "OFF" 	On éteint la led
 - mod_lum/haut = si mesg == "ON" on monte les volets
 - mod_lum/bas = si mesg == "ON" on baisse les volets
 - mod_lum/cmd = 
    si mesg == "aff" On renvoi la valeur de conf luminosité topic MQTT "mod_lum/conflum"  
    mosquitto_pub -d -t mod_lum/cmd -m "aff"
    si mesg == "tmp" On renvoi le valeur de conf du temps topic MQTT mod_lum/conflum"  
  - mod_lum/conftemps Défini la valeur de temps entre 2 mesures.
 
 
*/
#include <ESP8266WiFi.h>
// includes necessaires au fonctionnement de l'OTA :
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <EEPROM.h>
//#include <PString.h>
#include <ArduinoJson.h> 
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
// Include pour les fonctions dans le même répertoires.
#include "def.h"

// DHT 11 sensor
#define DHTPIN 2
#define DHTTYPE DHT11
 
// Utilisation d’une photo-résistance 
// Et ports pour cmd volet
const int port = A0;    // LDR
// port de commande des volets
#define haut 13		
//#define arret 13
#define bas 15
//#define lbp 14
#define lbp 12
 
// Update these with values suitable for your network.

// Buffer pour convertir en chaine de l'adresse IP de l'appareil
byte mac[] = {0xDE,0xED,0xBA,0xFE,0xFE,0xED};
IPAddress ip(192,168,1,100);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

/* mis dans c_wifi
const char* ssid1 = "Freebox-587BA2_EXT";
const char* ssid2 = "Freebox-587BA2";
const char* password = "Pl_aqpsmdp11";
*/
const char* mqtt_server = "192.168.1.81";
const char* mqttUser = "mod";
const char* mqttPassword = "Plaqpsmdp";
const char* svrtopic = "domoticz/in";
const char* topic_Domoticz_OUT = "domoticz/out"; 
 
// Création objet
WiFiClient modlum;
PubSubClient client(mqtt_server,1883,callback,modlum);
 // DHT sensor
DHT dht(DHTPIN, DHTTYPE, 15);

//StaticJsonDocument<300> root;
//JsonObject& JSONencoder = jsonBuffer.createObject();
//StaticJsonDocument < 300 > jsonBuffer;
//DeserializationError error = deserializeJson (jsonBuffer, createObject());
//DynamicJsonBuffer jsonBuffer( MQTT_MAX_PACKET_SIZE );

// Variables
int valeur = 0;         // temperature
float vin = 0;          // voltage capteur
char msg[100];          // Buffer pour envoyer message mqtt
int value = 0;
unsigned long readTime;
char floatmsg[10];      // Buffer pour les nb avec virgules
char message_buff[100]; // Buffer qui permet de décoder les messages MQTT reçus
long lastMsg = 0;       // Horodatage du dernier message publié sur MQTT
long lastbas = 0;       // compteur entre 2 lecture de capteur
bool debug = true;     // Affiche sur la console si True
bool mess = false;      // true si message reçu
String sujet = "";      // contient le topic
String mesg = "";       // contient le message reçu
int lum = 30;           // Valeur de luminosité par défaut
long lsmg = 60000;       // Valeur de temps entre 2 lectures
bool enbas = false;     // Flag de test volet bas
unsigned int addr = 0;  // addresse de stockage de lum
String idx = "1545";    // Idx du module dans domoticz
 
//========================================
void setup() {
  pinMode(haut, OUTPUT);     // Initialize le mvmt haut
//  pinMode(arret, OUTPUT);     // Initialize le mvmt arret
  pinMode(bas, OUTPUT);     // Initialize le mvmt bas
  pinMode(lbp, OUTPUT);     // Initialize la LED verte 
//  digitalWrite(bas,HIGH);
  
  Serial.begin(115200);
  setupwifi(debug);
  ArduinoOTA.setHostname("mod_lum"); // on donne une petit nom a notre module
  ArduinoOTA.begin(); // initialisation de l'OTA
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();                      // initialize temperature sensor
  client.subscribe(topic_Domoticz_OUT);

  EEPROM.begin(512);
  
  lum=eeGetInt(100);
  if ((lum == 0)|| (lsmg == -1))  {
    lum=60;
  }
  lsmg=eeGetInt(50);
  if ((lsmg == 0) || (lsmg == -1)) {
    lsmg=10000;
  }

  if (debug) {
    Serial.print("luminosité enregistrée ");
    Serial.println(lum);
    Serial.print("temps enregistré ");
    Serial.println(lsmg);
  }
}
// FIN du setup


//========================================
void loop() {
   // a chaque iteration, on verifie si une mise a jour nous est envoyee
   // si tel est cas, la lib ArduinoOTA se charge de gerer la suite :)
  ArduinoOTA.handle(); 
  
//  String idx;
  //char volt[8];
  String volt;
  
  // test de connection, sinon reconnecte
  //int counter = 0;
  if (!client.connected()) {
    reconnect();
  }
  
  // doit être appelée régulièrement pour permettre au client de traiter les messages entrants pour envoyer des données de publication et de rafraîchir la connexion
  client.loop();
 
  // affiche message reçu en MQTT
  // if domoticz message
  if ( mess ) {    
    if ( sujet == topic_Domoticz_OUT) {
      String recept; 
      const char* cmd;
      const char* command;
      Receptionmessage(debug, recept, cmd, command);
      if ( recept == "1545" ) {      
        if ( strcmp(command, "bas") == 0) {
          if ( strcmp(cmd, "ON") == 0 ) {  // On baisse
            digitalWrite(bas,HIGH);
            delay(5000);
            digitalWrite(bas,LOW);
          }
        }
        if ( strcmp(command, "haut") == 0) {
          if ( strcmp(cmd, "ON") == 0 ) {  // On baisse
            digitalWrite(haut,HIGH);
            delay(5000);
            digitalWrite(haut,LOW);
          }
        }   
      }  // if ( idx == 1 ) {
      recept="";               
    } // if domoticz message
  
// avec mod_lum
      
   // pinMode(lbp,OUTPUT);
    if ( sujet == "mod_lum/conf" ) {
      lum = mesg.toInt();
 //     sauverInt(100, lum);
      eeWriteInt(100, lum);
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
      if ( mesg == "ON" ) {
        digitalWrite(haut,HIGH);
        delay(5000);
        digitalWrite(haut,LOW);
      }
    } 
    if ( sujet == "mod_lum/bas" ) {
      if ( mesg == "ON" ) {
        digitalWrite(bas,HIGH);
        delay(5000);
        digitalWrite(bas,LOW);
      }
    } 
    if ( sujet == "mod_lum/cmd" ) {
      if ( mesg == "aff" ) {
        digitalWrite(lbp,HIGH);
        delay(5000);
        digitalWrite(lbp,LOW);
        snprintf (msg, 80, "Valeur de conf luminosité", String(lum).c_str());
        client.publish("mod_lum/conflum", String(lum).c_str());  
        if (debug) {
          Serial.print("conf lum = ");
          Serial.println(lum);   
        }
      }
      if ( mesg == "tmp" ) {
        digitalWrite(lbp,HIGH);
        delay(5000);
        digitalWrite(lbp,LOW);
        snprintf (msg, 80, "Valeur de conf du temps", String(lsmg).c_str());
        client.publish("mod_lum/conflum", String(lsmg).c_str());  
        if (debug) {
          Serial.print("conf lsmg = ");
          Serial.println(lsmg);   
        }
      }
    }
    if ( sujet == "mod_lum/conftemps" ) {
      lsmg = mesg.toInt();
      eeWriteInt(50, lsmg);
      if (debug) {
        Serial.print("temps récupéré ");
        Serial.println(lsmg);
      }
    }
     
   // pinMode(lbp,INPUT);
    mess = false;
  }
 
 
  // renvoie le niveau de la ldr tous les 10 sec
  long now = millis();
  if (now - lastMsg > lsmg) {
    lastMsg = now;
    // Lit l’entrée analogique A0 
    
//    StaticJsonDocument<300> root;
//    root["idx"] = 1545;
//    root["name"] = "Luminosité";
//    root['nvalue'] = "0";
//    root["svalue"] = analogRead(port);
//    serializeJson(root, msg);
    int idx = 3404;
    String name = "Luminosité"; 
    String svalue = String(analogRead(port),DEC);
    Emetmessage(idx, name, svalue);
    
    client.publish(svrtopic,msg,false);
    if (debug) {
//      serializeJson(root, Serial);
      //Serial.print("root=" );Serial.println(root.*);
      Serial.print("msg=" );Serial.println(msg);
      //Serial.print("cmd=" );Serial.println(cmd);
    }

    char guillemet = '"';
    valeur = analogRead(port);
    String val = String(valeur);
    // convertit l’entrée en volt 
    vin = (valeur * 3.3) / 1024.0;
    volt=String(vin);
    //dtostrf(vin, 6, 2, volt); 
    
//    JSONencoder["idx"] = 1545;
//    JSONencoder["nvalue"] = 0;
//    JSONencoder["svalue"] = val;
    //JSONencoder["HUM"] = h;
    //char JSONmessageBuffer[100];
    //JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
//    client.publish(svrtopic,JSONmessageBuffer,false);

    //    value = value + 1;
//    snprintf (msg, 50, "Envoi numéro #%ld", value);
    //Serial.print("Publish message: ");
    //Serial.println(msg);
//    client.publish("mod_lum", msg);

    /*
    snprintf (msg, 50, "Luminosité %ld", valeur);
    client.publish("mod_lum", String(valeur).c_str());
    client.publish("mod_lum/lum", msg);
//    dtostrf(vin, 6, 2, floatmsg);
//    PString(floatmsg, 10, vin);
    snprintf (msg, 50, "voltage %ld", String(vin).c_str());
    client.publish("mod_lum/volt", String(vin).c_str());
    */
    if (debug) {
 //     Serial.print("VALEUR: ");
 //     Serial.println(valeur);
      Serial.print("JSON: ");
      Serial.println(msg);
    
    //  Serial.print("string voltage ");
    //  Serial.println(String(vin).c_str());
    }
 
 
    if (debug) {
      Serial.print("valeur = ");
      Serial.println(valeur);   
      Serial.print("volt = ");
      Serial.println(vin); 
//      if (enbas) {
//        Serial.println("état volet = true");
//      } else {
//        Serial.println("état volet = false");
//      }
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
 

//========================================
void eeWriteInt(int pos, int val) {
    byte* p = (byte*) &val;
    EEPROM.write(pos, *p);
    EEPROM.write(pos + 1, *(p + 1));
    EEPROM.write(pos + 2, *(p + 2));
    EEPROM.write(pos + 3, *(p + 3));
    EEPROM.commit();
}

//========================================
int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}
  

//========================================
//on veut sauvegarder par exemple le nombre décimal 55084, en binaire : 1101 0111 0010 1100

//fonction d'écriture d'un type int en mémoire EEPROM
void sauverInt(int adresse, int val) 
{   
    //découpage de la variable val qui contient la valeur à sauvegarder en mémoire
    unsigned char faible = val & 0x00FF; //récupère les 8 bits de droite (poids faible) -> 0010 1100 
    //calcul : 1101 0111 0010 1100 & 0000 0000 1111 1111 = 0010 1100

    unsigned char fort = (val >> 8) & 0x00FF;  //décale puis récupère les 8 bits de gauche (poids fort) -> 1101 0111
    //calcul : 1101 0111 0010 1100 >> 8 = 0000 0000 1101 0111 puis le même & qu’avant

    //puis on enregistre les deux variables obtenues en mémoire
    EEPROM.write(adresse, fort) ; //on écrit les bits de poids fort en premier
    EEPROM.write(adresse+1, faible) ; //puis on écrit les bits de poids faible à la case suivante
    EEPROM.commit();  

  
}
 
//========================================
//lecture de la variable de type int enregistrée précédemment par la fonction que l'on a créée

int lireInt(int adresse)
{
    int val = 0 ; //variable de type int, vide, qui va contenir le résultat de la lecture

    unsigned char fort = EEPROM.read(adresse);     //récupère les 8 bits de gauche (poids fort) -> 1101 0111
    unsigned char faible = EEPROM.read(adresse+1); //récupère les 8 bits de droite (poids faible) -> 0010 1100

    //assemblage des deux variable précédentes
    val = fort ;         // val vaut alors 0000 0000 1101 0111
    val = val << 8 ;     // val vaut maintenant 1101 0111 0000 0000 (décalage)
    val = val | faible ; // utilisation du masque
    // calcul : 1101 0111 0000 0000 | 0010 1100 = 1101 0111 0010 1100

    return val ; //on n’oublie pas de retourner la valeur lue !
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
 
