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
 
 Commande et conf MQTT
 - mod_lum/conf = Défini le seuil de luminosité bas
 - mod_lum/cmd  = 
    si mesg == "ON" 	On allume la led
    si mesg == "OFF" 	On éteint la led
 - mod_lum/haut = si mesg == "ON" on monte les volets
 - mod_lum/bas = si mesg == "ON" on baisse les volets
 - mod_lum/cmd = 
    si mesg == "aff" On renvoi la valeur de conf luminosité topic MQTT "mod_lum/conflum"  
    si mesg == "tmp" On renvoi le valeur de conf du temps topic MQTT mod_lum/conflum"  
  - mod_lum/conftemps Défini la valeur de temps entre 2 mesures.
 
 
*/
 
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <EEPROM.h>
//#include <PString.h>

// DHT 11 sensor
#define DHTPIN 2
#define DHTTYPE DHT11
 
// Utilisation d’une photo-résistance 
// Et ports pour cmd volet
const int port = A0;    // LDR
#define haut 13		// port de commande des volets
//#define arret 13
#define bas 15
//#define lbp 14
#define lbp 12
 
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

const char* ssid1 = "FREEBOX_..._EXT";
const char* ssid2 = "FREEBOX_...._P2";
const char* password = "....";
const char* mqtt_server = "192.168.0.x";
const char* mqttUser = "mod";
const char* mqttPassword = "...";
const char* svrtopic = "domoticz/in";
const char* topic_Domoticz_OUT = "domoticz/out"; 
 
// Création objet
WiFiClient espClient;
PubSubClient client(espClient);
 // DHT sensor
DHT dht(DHTPIN, DHTTYPE, 15);

StaticJsonBuffer<300> JSONbuffer;
JsonObject& JSONencoder = JSONbuffer.createObject();
DynamicJsonBuffer jsonBuffer( MQTT_MAX_PACKET_SIZE );
String messageReceived="";

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
 
//========================================
void setup() {
  pinMode(haut, OUTPUT);     // Initialize le mvmt haut
//  pinMode(arret, OUTPUT);     // Initialize le mvmt arret
  pinMode(bas, OUTPUT);     // Initialize le mvmt bas
  pinMode(lbp, OUTPUT);     // Initialize la LED verte 
//  digitalWrite(bas,HIGH);
  
  Serial.begin(115200);
 
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();                      // initialize temperature sensor
  client.subscribe("#");

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
    messageReceived+=((char)payload[i]);
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
      digitalWrite(lbp,HIGH);
      delay(10000);
      digitalWrite(lbp,LOW);
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
      
  /*
  Sending a Switch Command

    {"command": "switchlight", "idx": 2450, "switchcmd": "On" }
    {"command": "switchlight", "idx": 2450, "switchcmd": "Set Level", "level": 100 }
    {"command": "setcolbrightnessvalue", "idx": 2450, "hue": 274, "brightness": 40, "iswhite": false }
    {"command": "setcolbrightnessvalue", "idx": 2450, "hex": "RRGGBB", "brightness": 100, "iswhite": false }
    {"command": "setcolbrightnessvalue", "idx": 2450, "color": {"m":3,"t":0,"r":0,"g":0,"b":50,"cw":0,"ww":0}, "brightness": 40}
    
    "command": "haut", "idx": 1, "cmd": "ON"
    "command": "bas", "idx": 1, "cmd": "ON"
   */    
      
    // if domoticz message
    if ( sujet == topic_Domoticz_OUT) {
        JsonObject& root = jsonBuffer.parseObject(messageReceived);
        if (!root.success()) {
           digitalWrite(lbp,HIGH);
           if (debug) {
             Serial.println("parsing Domoticz/out JSON Received Message failed");
           }
        }  else {
           const char* idxChar = root["idx"];
           String idx = String( idxChar);
        
        if ( idx == 1 ) {      
           const char* cmd = root["nvalue"];
           const char* command = root["command"];
           if ( strcmp(command, "bas") == 0 {
             if ( strcmp(cmd, "ON") == 0 ) {  // On baisse
                digitalWrite(bas,HIGH);
                delay(5000);
                digitalWrite(bas,LOW);
             }
           }
           if ( strcmp(command, "haut") == 0 {
             if ( strcmp(cmd, "ON") == 0 ) {  // On baisse
                digitalWrite(haut,HIGH);
                delay(5000);
                digitalWrite(haut,LOW);
             }
           }   
        }  // if ( idx == 1 ) {
                   
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
//    value = value + 1;
//    snprintf (msg, 50, "Envoi numéro #%ld", value);
    //Serial.print("Publish message: ");
    //Serial.println(msg);
//    client.publish("mod_lum", msg);
 
  // Lit l’entrée analogique A0 
    valeur = analogRead(port);
    
  // convertit l’entrée en volt 
    vin = (valeur * 3.3) / 1024.0; 
    
    JSONencoder["idx"] = 1;
    JSONencoder["nvalue"] = 0;
    JSONencoder["svalue"] = valeur+";"+volt;
    //JSONencoder["HUM"] = h;
    char JSONmessageBuffer[100];
    JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
    myESP.publish(svrtopic,JSONmessageBuffer,false);
    
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
      Serial.print("JSON: ");
      Serial.println(JSONmessageBuffer);
    
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
 

