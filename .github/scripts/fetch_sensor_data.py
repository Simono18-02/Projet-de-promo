#!/usr/bin/env python3
import os
import json
import requests
from datetime import datetime

# Fichier de sortie pour les données
OUTPUT_FILE = "data.json"

# URL du fichier data.json sur GitHub
GITHUB_REPO = "Simono18-02/Projet-de-promo"
GITHUB_FILE_PATH = "data.json"
GITHUB_API_URL = f"https://api.github.com/repos/{GITHUB_REPO}/contents/{GITHUB_FILE_PATH}"

def fetch_github_data():
    """Récupère les données actuelles depuis GitHub"""
    try:
        response = requests.get(GITHUB_API_URL)
        if response.status_code == 200:
            content_data = response.json()
            # Le contenu est en base64, mais nous n'avons pas besoin de le décoder ici
            # car nous voulons juste voir si le fichier existe
            return True
        else:
            print(f"Erreur lors de la récupération du fichier GitHub: {response.status_code}")
            return False
    except Exception as e:
        print(f"Erreur lors de la connexion à GitHub: {e}")
        return False

def fetch_sensor_data():
    """Récupère les données du capteur depuis l'ESP32 ou utilise les données actuelles"""
    # Récupérer l'adresse IP de l'ESP32 depuis les variables d'environnement
    ESP32_IP = os.environ.get('ESP32_IP', '192.168.1.37')
    ESP32_URL = f"http://{ESP32_IP}/readings"
    
    try:
        # Tenter de récupérer les données depuis l'ESP32
        response = requests.get(ESP32_URL, timeout=5)
        if response.status_code == 200:
            print("Données récupérées avec succès depuis l'ESP32")
            return response.json()
    except requests.exceptions.RequestException as e:
        print(f"Impossible de se connecter à l'ESP32: {e}")
    
    print("Tentative de récupération des données depuis GitHub...")
    
    # Si l'ESP32 n'est pas accessible, essayer de récupérer les données actuelles depuis GitHub
    try:
        if os.path.exists(OUTPUT_FILE):
            with open(OUTPUT_FILE, 'r') as f:
                data = json.load(f)
                if "reading" in data:
                    print("Utilisation des données existantes dans le fichier local")
                    return data["reading"]
    except Exception as e:
        print(f"Erreur lors de la lecture du fichier local: {e}")
    
    return None

def update_data_history(new_data):
    """Met à jour l'historique des données dans le fichier JSON"""
    # Structure de base avec horodatage ISO pour compatibilité
    history = {
        "lastUpdated": datetime.now().isoformat(),
        "reading": new_data,
        "history": []
    }
    
    # Charger l'historique existant s'il existe
    if os.path.exists(OUTPUT_FILE):
        try:
            with open(OUTPUT_FILE, 'r') as f:
                existing_data = json.load(f)
                
            # Si le format est l'ancien format sans historique, convertir
            if "reading" in existing_data and "history" not in existing_data:
                # Ajouter la dernière lecture comme première entrée d'historique
                reading_with_timestamp = existing_data["reading"].copy()
                if "timestamp" not in reading_with_timestamp:
                    reading_with_timestamp["timestamp"] = datetime.now().isoformat()
                history["history"].append(reading_with_timestamp)
            # Si le format comprend déjà un historique, le conserver
            elif "history" in existing_data and isinstance(existing_data["history"], list):
                # Limiter à 50 entrées (incluant la nouvelle)
                history["history"] = existing_data["history"][:49]
                
                # Si on a une lecture précédente, l'ajouter à l'historique
                if "reading" in existing_data:
                    previous_reading = existing_data["reading"].copy()
                    # Utiliser lastUpdated comme timestamp si disponible
                    if "timestamp" not in previous_reading and "lastUpdated" in existing_data:
                        previous_reading["timestamp"] = existing_data["lastUpdated"]
                    elif "timestamp" not in previous_reading:
                        previous_reading["timestamp"] = datetime.now().isoformat()
                    
                    # Insérer au début de l'historique
                    history["history"].insert(0, previous_reading)
        except (json.JSONDecodeError, IOError) as e:
            print(f"Erreur lors de la lecture du fichier existant: {e}")
    
    # Écrire les données mises à jour
    with open(OUTPUT_FILE, 'w') as f:
        json.dump(history, f, indent=2)
    
    print(f"Données mises à jour dans {OUTPUT_FILE}")

def main():
    print("Vérification de l'existence du fichier sur GitHub...")
    github_file_exists = fetch_github_data()
    
    if not github_file_exists:
        print("Le fichier data.json n'existe pas encore sur GitHub.")
        
    print("Récupération des données du capteur...")
    sensor_data = fetch_sensor_data()
    
    if sensor_data:
        print(f"Données récupérées: {sensor_data}")
        update_data_history(sensor_data)
    else:
        print("Impossible de récupérer les données")

if __name__ == "__main__":
    main()
