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

#include <DHT.h>


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


// DHT sensor
DHT dht(DHTPIN, DHTTYPE, 15);

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
int lum = 120;           // Valeur de luminosité par défaut
long lsmg = 6000;       // Valeur de temps entre 2 lectures
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
 
  if (debug) {
    Serial.print("luminosité enregistrée ");
    Serial.println(lum);
    Serial.print("temps enregistré ");
    Serial.println(lsmg);
  }
}


//========================================
/* Baiise le volet
 * essai en 2 fois séparé par 120sec
 */
 void baisse_volet() {
  long tmp = millis();
  
  int cmpt = 1;
  while ( cmpt <= 3 ) {
    digitalWrite(bas,HIGH);
    delay(1000);
    digitalWrite(bas,LOW);
    delay(8000);

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
  String idx;
  //char volt[8];
  String volt;
  
  // renvoie le niveau de la ldr tous les 10 sec
  long now = millis();
  if (now - lastMsg > lsmg) {
    lastMsg = now;
  // Lit l’entrée analogique A0 
    char guillemet = '"';
    valeur = analogRead(port);
    //valeur = guillemet + valeur + guillemet;
    String val = String(valeur);
    
  // convertit l’entrée en volt 
    vin = (valeur * 3.3) / 1024.0;
    volt=String(vin);
    //dtostrf(vin, 6, 2, volt); 
    
    
    if (debug) {
      Serial.print("valeur = ");
      Serial.println(valeur);   
      Serial.print("volt = ");
      Serial.println(vin); 
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
 
