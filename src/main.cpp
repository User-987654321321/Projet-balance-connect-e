#include <Arduino.h>
#include "HX711.h"
#include "PubSubClient.h"
#include "WiFi.h"
#include "esp_wpa2.h"

// Connexions HX711

#define DT  16   // Broche DT (data)

#define SCK 17   // Broche SCK (clock)


HX711 balance;


// Facteur trouvé à l’étalonnage

float calibration_factor = -1175.31;  //  à remplacer par ta valeur trouvée
// Exemple : ~ 1000 / lecture brute obtenue avec 1 kg étalon
float masse_corrige;
String msg_topic;
String erreur;


//const char *mqtt_broker = "broker.hivemq.com"; 
const char *mqtt_broker = "147.94.219.93"; // Identifiant du broker (Adresse IP)
const char *topic = "masse corrigée"; // Nom du topic sur lequel les données seront envoyés.
const char *topic_2 = "masse brut"; // Nom du topic sur lequel les données seront envoyés.
const char *topic_3 = "erreur"; // Nom du topic sur lequel les données seront envoyés. 
const char *topic_4 = "tare"; // Nom du topic sur lequel les données seront envoyés. 
const char *mqtt_username = ""; // Identifiant dans le cas d'une liaison sécurisée
const char *mqtt_password = ""; // Mdp dans le cas d'une liaison sécurisée
const int mqtt_port = 1883; // Port : 1883 dans le cas d'une liaison non sécurisée et 8883 dans le cas d'une liaison cryptée
WiFiClient espClient; 
PubSubClient client(espClient); 

// Paramètres EDUROAM (A COMPLETER)
#define EAP_IDENTITY "anis.remili@etu.univ-amu.fr"
#define EAP_PASSWORD "Anis83600." //mot de passe EDUROAM
#define EAP_USERNAME "anis.remili@etu.univ-amu.fr" 
const char* ssid = "eduroam"; // eduroam SSID

// Fonction réception du message MQTT 
void callback(char *topic, byte *payload, unsigned int length) { 
  String msg;
  for (int i = 0; i < length; i++) { 
    msg += (char)payload[i];
  } 

  Serial.println("-----------------------");
  Serial.print("Topic reçu : "); 
  Serial.println(topic); 
  Serial.print("Message reçu : "); 
  Serial.println(msg); 
  Serial.println("-----------------------");

  String topicStr = String(topic);
  msg.trim();  // Nettoie les espaces blancs, sauts de ligne éventuels

  if (topicStr.equalsIgnoreCase("tare") && msg.equalsIgnoreCase("tare")) {
    Serial.println("Tare demandée depuis Node-RED !");
    unsigned long start = millis();
    balance.tare();
    unsigned long end = millis();
    Serial.print("Tare effectué en ");
    Serial.print(end - start);
    Serial.println(" ms");
    client.publish("Tar/confirmation", "Tare OK");
  }
}



void reconnect() {
  // Boucle jusqu'à ce que la connexion soit rétablie
  while (!client.connected()) {
    Serial.print("Connexion au broker MQTT... ");
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connecté !");
      client.subscribe("tare");        // On se réabonne aux topics
      client.subscribe("masse corrigée");
      client.subscribe("masse brut");
      client.subscribe("classe_EMT");
    } else {
      Serial.print("Échec, rc=");
      Serial.print(client.state());
      Serial.println(" nouvelle tentative dans 5s");
      delay(5000);
    }
  }
}


void setup() {

  Serial.begin(115200);

  balance.begin(DT, SCK);


  balance.set_scale(calibration_factor); // Applique le facteur d’échelle

  balance.tare(); // Mise à zéro au démarrage


  Serial.println("Balance prête !");

  // Connexion au réseau EDUROAM 

  WiFi.disconnect(true);
  WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD); 
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println("");
  Serial.println(F("L'ESP32 est connecté au WiFi !"));
  
// Connexion au broker MQTT  
  
  client.setServer(mqtt_broker, mqtt_port); 
  client.setCallback(callback); 

  while (!client.connected()) { 
    String client_id = "esp32-client-"; 
    client_id += String(WiFi.macAddress()); 
    Serial.printf("La chaîne de mesure %s se connecte au broker MQTT", client_id.c_str()); 
 
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) { 
      Serial.println("La chaîne de mesure est connectée au broker."); 
    } else { 
      Serial.print("La chaîne de mesure n'a pas réussi à se connecter ... "); 
      Serial.print(client.state()); 
      delay(2000); 
    } 
  } 
  client.subscribe(topic); // S'abonne au topic pour recevoir des messages
  client.subscribe(topic_2); // S'abonne au topic pour recevoir des messages
  client.subscribe(topic_3); // S'abonne au topic pour recevoir des messages
  client.subscribe(topic_4); // S'abonne au topic pour recevoir des messages
} 

unsigned long previousMillis = 0;
const long interval = 1000; // intervalle de mesure en ms

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();  // Gère les messages MQTT (pour lire la valeur de masses sur le moniteur série de platformIO), toujours en premier : vérifie et traite les messages MQTT

  // Mesure toutes les 1s sans bloquer
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    float masse = balance.get_units(10); // Moyenne sur 10 mesures (prend environ 1 seconde)
    if (masse <= 0.09 && masse >= -0.09){
      masse_corrige = masse;
    }
    if (masse <= 1.49 && masse >= 0.5){
      masse_corrige = masse + 0.11004; // On applique l'erreur por 1g
      erreur = "0.11004";
    }
    if (masse <= 3.49 && masse >= 1.5){
      masse_corrige = masse + 0.15997; // On applique l'erreur por 2g
      erreur = "0.15997";
    }
    if (masse <= 7.49 && masse >= 3.5){
      masse_corrige = masse + 0.32008; // On applique l'erreur por 5g
      erreur = "0.32008";
    }
    if (masse <= 14.99 && masse >= 7.5){
      masse_corrige = masse + 0.5999; // On applique l'erreur por 10g
      erreur = "0.5999";
    }
    if (masse <= 34.99 && masse >= 15){
      masse_corrige = masse + 1.1701; // On applique l'erreur por 20g
      erreur = "1.1701";
    }
    if (masse <= 74.99 && masse >= 35){
      masse_corrige = masse + 2.9601; // On applique l'erreur por 50g
      erreur = "2.9601";
    }
    if (masse <= 149.99 && masse >= 75){
      masse_corrige = masse + 5.7897; // On applique l'erreur por 100g
      erreur = "5.7897";
    }
    if (masse <= 250 && masse >= 150){
      masse_corrige = masse + 11.2596; // On applique l'erreur por 200g
      erreur = "11.2596";
    }
    
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(" ± 0.27 g");
    Serial.print("erreur : ");
    Serial.println(erreur);

    Serial.print("Masse mesurée : ");
    Serial.print(masse, 2); // 2 décimales
    Serial.println(" g");

    msg_topic = String(masse_corrige) + " ± 0.27";
    client.publish(topic, msg_topic.c_str()); // Publication de la masse corrigée sur le topic 1 (envoi d'une chaîne de caractères)

    client.publish(topic_2, String(masse).c_str()); // Publication de la masse sur le topic 2 (envoi d'une chaîne de caractères)

    client.publish(topic_3, erreur.c_str()); // Publication de l'erreur appliqué sur la masse sur le topic 3 (envoi d'une chaîne de caractères)

  }
}
