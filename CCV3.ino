#include <base64.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Wire.h>
#include <ArduinoJson.h> 
#include "Adafruit_CCS811.h"

 
// Configuration WiFi
const char* ssid = "Freebox-4AA130";
const char* password = "subrogari5-gignendo&-novisti-evulsero*#";

// Configuration GitHub
const char* githubApiUrl = "https://api.github.com/repos/Simono18-02/Projet-de-promo/contents/data.json";
const char* githubToken = "tokenici";
const char* githubUsername = "Simono18-02";

WebServer server(80);
Adafruit_CCS811 ccs;
HTTPClient http;

// Variables globales pour stocker les valeurs actuelles
int co2Value = 0;
int tvocValue = 0;
bool sensorReady = false;
unsigned long lastUploadTime = 0;
const unsigned long uploadInterval = 60000 ;  // 1 minute en millisecondes

void setup() {
  Serial.begin(115200);
  
  // Initialiser LittleFS
  if(!LittleFS.begin(true)) {
    Serial.println("Erreur lors de l'initialisation de LittleFS");
  } else {
    Serial.println("LittleFS initialisé avec succès");
  }
  
  // Initialiser le capteur CCS811
  Wire.begin(21, 22);
  Serial.println("Initialisation du capteur CCS811...");
  if(!ccs.begin()) {
    Serial.println("Échec de démarrage du CCS811! Vérifiez le câblage.");
  } else {
    Serial.println("CCS811 démarré avec succès");
    sensorReady = true;
  }
  
  // Connexion WiFi
  Serial.println("Connexion au WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connecté");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());
  
  // Route pour la page d'accueil
  server.on("/", HTTP_GET, []() {
    if (LittleFS.exists("/index.html")) {
      File file = LittleFS.open("/index.html", "r");
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.send(404, "text/plain", "Fichier index.html non trouvé");
    }
  });
  
  // Route pour obtenir les données du capteur
  server.on("/readings", HTTP_GET, []() {
    String json = "{\"co2\":" + String(co2Value) + ",\"tvoc\":" + String(tvocValue) + "}";
    server.send(200, "application/json", json);
  });
  
  // Route pour déclencher manuellement un envoi vers GitHub
  server.on("/upload", HTTP_GET, []() {
    if (uploadDataToGitHub()) {
      server.send(200, "text/plain", "Données envoyées avec succès à GitHub");
    } else {
      server.send(500, "text/plain", "Erreur lors de l'envoi des données à GitHub");
    }
  });
  
  server.begin();
  Serial.println("Serveur HTTP démarré");
}

void loop() {
  server.handleClient();
  
  // Lecture régulière du capteur
  if (sensorReady && ccs.available()) {
    if(!ccs.readData()) {
      co2Value = ccs.geteCO2();
      tvocValue = ccs.getTVOC();
      Serial.print("CO2: ");
      Serial.print(co2Value);
      Serial.print(" ppm, TVOC: ");
      Serial.print(tvocValue);
      Serial.println(" ppb");
    } else {
      Serial.println("Erreur de lecture du capteur");
    }
  }
  
  // Envoi périodique des données vers GitHub
  unsigned long currentTime = millis();
  if (currentTime - lastUploadTime >= uploadInterval) {
    if (uploadDataToGitHub()) {
      lastUploadTime = currentTime;
    }
  }
}

bool uploadDataToGitHub() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi non connecté, impossible d'envoyer les données");
    return false;
  }
  
  // Étape 1: Obtenir le contenu actuel et le SHA
  http.begin(githubApiUrl);
  http.addHeader("Authorization", "token " + String(githubToken));
  http.addHeader("User-Agent", githubUsername);
  
  int httpCode = http.GET();
  String sha = "";
  
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    sha = doc["sha"].as<String>();
  } else if (httpCode != 404) {
    // Si le fichier n'existe pas (404), on continue sans SHA
    // Pour toute autre erreur, on abandonne
    Serial.println("Erreur lors de la récupération du fichier: " + String(httpCode));
    http.end();
    return false;
  }
  
  http.end();
  
  // Étape 2: Préparer les nouvelles données
  DynamicJsonDocument dataDoc(1024);
  JsonObject reading = dataDoc.createNestedObject("reading");
  reading["timestamp"] = String(millis()); // Idéalement, utilisez un timestamp réel
  reading["co2"] = co2Value;
  reading["tvoc"] = tvocValue;
  
  String jsonOutput;
  serializeJson(dataDoc, jsonOutput);
  
  // Encodage Base64 du contenu JSON
  String base64Content = base64::encode(jsonOutput);
  
  // Étape 3: Envoyer les nouvelles données
  http.begin(githubApiUrl);
  http.addHeader("Authorization", "token " + String(githubToken));
  http.addHeader("User-Agent", githubUsername);
  http.addHeader("Content-Type", "application/json");
  
  // Préparation du payload pour l'API GitHub
  String requestBody = "{";
  requestBody += "\"message\":\"Update sensor data\",";
  requestBody += "\"content\":\"" + base64Content + "\"";
  
  // Ajouter le SHA si on met à jour un fichier existant
  if (sha != "") {
    requestBody += ",\"sha\":\"" + sha + "\"";
  }
  
  requestBody += "}";
  
  httpCode = http.PUT(requestBody);
  
  if (httpCode == 200 || httpCode == 201) {
    Serial.println("Données envoyées avec succès à GitHub");
    http.end();
    return true;
  } else {
    Serial.println("Erreur lors de l'envoi des données: " + String(httpCode));
    Serial.println(http.getString());
    http.end();
    return false;
  }
}

// Fonction d'encodage Base64
String base64Encode(String input) {
  const char* ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int inputLength = input.length();
  String encoded = "";
  
  // Traitement par blocs de 3 octets
  for (int i = 0; i < inputLength; i += 3) {
    // Convertir trois octets en quatre caractères base64
    uint32_t temp = 0;
    
    for (int j = 0; j < 3; j++) {
      if (i + j < inputLength) {
        temp = (temp << 8) | (uint8_t)input[i + j];
      } else {
        temp = temp << 8;
      }
    }
    
    // Extraire les 4 index de 6 bits
    for (int j = 3; j >= 0; j--) {
      if (i + j > inputLength) {
        encoded += '=';
      } else {
        encoded += ALPHABET[(temp >> (6 * j)) & 0x3F];
      }
    }
  }
  
  return encoded;
}