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
float calibration_factor = -1175.31;  // à remplacer par ta valeur trouvée
float masse_corrige;
String um;
String msg_topic;
String Classe_EMT;
String Tar;

// Exemple : ~ 1000 / lecture brute obtenue avec 1 kg étalon

//const char *mqtt_broker = "broker.hivemq.com"; 
const char *mqtt_broker = "147.94.219.128"; // Identifiant du broker (Adresse IP)
const char *topic = "masse corrigée";     // Nom du topic sur lequel les données seront envoyées.
const char *topic_2 = "masse brut";       // Nom du topic sur lequel les données seront envoyées.
const char *topic_3 = "classe_EMT";       // Nom du topic sur lequel les données seront envoyés.  
const char *topic_4 = "Tar";               // Nom du topic pour commande tare
const char *mqtt_username = "";            // Identifiant dans le cas d'une liaison sécurisée
const char *mqtt_password = "";            // Mdp dans le cas d'une liaison sécurisée
const int mqtt_port = 1883;                 // Port : 1883 non sécurisé, 8883 sécurisé
WiFiClient espClient; 
PubSubClient client(espClient); 

// Paramètres EDUROAM (A COMPLETER)
#define EAP_IDENTITY "anis.remili@etu.univ-amu.fr"
#define EAP_PASSWORD "Anis83600." //mot de passe EDUROAM
#define EAP_USERNAME "anis.remili@etu.univ-amu.fr" 
const char* ssid = "eduroam"; // eduroam SSID

// Fonction réception du message MQTT 
void callback(char *topic, byte *payload, unsigned int length) { 
  Serial.print("Message reçu sur le topic : "); 
  Serial.println(topic); 
  
  String msg;
  for (int i = 0; i < length; i++) { 
    msg += (char) payload[i]; 
  } 
  msg.trim();
  
  Serial.print("Contenu : ");
  Serial.println(msg);
  Serial.println("-----------------------"); 

  // Si le topic est "Tar" et que le message est "TARE", on effectue la tare
  if (String(topic) == String(topic_4)) {  // topic_4 est "Tar"
    if (msg.equalsIgnoreCase("TARE")) {
      Serial.println("Commande TARE reçue : remise à zéro...");
      balance.tare();
      Serial.println("Balance remise à zéro.");
      Tar = "Balance remise à zéro";
      client.publish(topic_4, Tar.c_str()); // envoie un accusé de réception
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
    Serial.printf("La chaîne de mesure %s se connecte au broker MQTT\n", client_id.c_str()); 
 
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) { 
      Serial.println("La chaîne de mesure est connectée au broker."); 
      client.subscribe(topic_4);  // S'abonne au topic "Tar" pour recevoir les commandes tare
    } else { 
      Serial.print("La chaîne de mesure n'a pas réussi à se connecter ... "); 
      Serial.println(client.state()); 
      delay(2000); 
    } 
  } 
} 

void loop() {
  float masse = balance.get_units(10); // Moyenne sur 10 mesures
  if (masse <= 0.09 && masse >= -0.09){
    masse_corrige = 0;
  }
  if (masse <= 1.49 && masse >= 0.5){
    masse_corrige = masse;
    um = "± 0.04 g";
    Classe_EMT = "Classe F2 : EMT = 0.3";
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(um);
  }
  if (masse <= 3.49 && masse >= 1.5){
    masse_corrige = masse;
    um = "± 0.04 g"; 
    Classe_EMT = "Classe F2 : EMT = 0.4";
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(" ± 0.04 g");
  }
  if (masse <= 7.49 && masse >= 3.5){
    masse_corrige = masse;
    um = "± 0.05 g";
    Classe_EMT = "Classe M1 : EMT = 1.5";
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(" ± 0.05 g");
  }
  if (masse <= 14.99 && masse >= 7.5){
    masse_corrige = masse;
    um = "± 0.07 g";
    Classe_EMT = "Classe M1 : EMT = 2";
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(" ± 0.07 g");
  }
  if (masse <= 34.99 && masse >= 15){
    masse_corrige = masse;
    um = "± 0.08 g";
    Classe_EMT = "Classe M2 : EMT = 6";
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(" ± 0.08 g");
  }
  if (masse <= 74.99 && masse >= 35){
    masse_corrige = masse;
    um = "± 0.09 g";
    Classe_EMT = "Classe M2 : EMT = 10";
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(" ± 0.09 g");
  }
  if (masse <= 149.99 && masse >= 75){
    masse_corrige = masse;
    um = "± 0.15 g";
    Classe_EMT = "Classe M2 : EMT = 15";
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(" ± 0.15 g");
  }
  if (masse <= 250 && masse >= 150){
    masse_corrige = masse;
    um = "± 0.29 g";
    Classe_EMT = "Classe M2 : EMT = 30";
    Serial.print("Masse corrigée : ");
    Serial.print(masse_corrige, 2); // 2 décimales
    Serial.println(" ± 0.29 g");
  }
  
  Serial.print("Masse mesurée : ");
  Serial.print(masse, 2); // 2 décimales
  Serial.println(" g");

  msg_topic = String(masse_corrige) + um;
  client.publish(topic, msg_topic.c_str());   // Publication masse corrigée
  client.publish(topic_2, String(masse).c_str()); // Publication masse brute
  client.publish(topic_3, Classe_EMT.c_str());    // Publication classe EMT
  client.publish(topic_4, Tar.c_str());            // Publication message tare (accusé)

  client.loop(); // Gère les messages MQTT
  
  balance.power_down();			        // met le CAN en veille
  delay(1000);
  balance.power_up();
}
