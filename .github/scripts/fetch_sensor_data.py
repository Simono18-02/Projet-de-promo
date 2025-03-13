#!/usr/bin/env python3
import os
import json
import time
import requests
from datetime import datetime

# Récupérer l'adresse IP de l'ESP32 depuis les variables d'environnement
ESP32_IP = os.environ.get('ESP32_IP', 'localhost')
ESP32_URL = f"http://{ESP32_IP}/readings"

# Fichier de sortie pour les données
OUTPUT_FILE = "data.json"

def fetch_sensor_data():
    """Récupère les données du capteur depuis l'ESP32"""
    try:
        response = requests.get(ESP32_URL, timeout=5)
        if response.status_code == 200:
            return response.json()
        else:
            print(f"Erreur lors de la récupération des données: {response.status_code}")
            return None
    except requests.exceptions.RequestException as e:
        print(f"Erreur de connexion à l'ESP32: {e}")
        return None

def update_data_history(new_data):
    """Met à jour l'historique des données dans le fichier JSON"""
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
                
            # Conserver l'historique existant
            if "history" in existing_data:
                # Limiter à 50 entrées
                history["history"] = existing_data["history"][:49]
                
            # Ajouter la dernière lecture à l'historique
            if "reading" in existing_data:
                existing_data["reading"]["timestamp"] = existing_data.get("lastUpdated", datetime.now().isoformat())
                history["history"].insert(0, existing_data["reading"])
        except (json.JSONDecodeError, IOError) as e:
            print(f"Erreur lors de la lecture du fichier existant: {e}")
    
    # Écrire les données mises à jour
    with open(OUTPUT_FILE, 'w') as f:
        json.dump(history, f, indent=2)
    
    print(f"Données mises à jour dans {OUTPUT_FILE}")

def main():
    print("Récupération des données du capteur...")
    sensor_data = fetch_sensor_data()
    
    if sensor_data:
        print(f"Données récupérées: {sensor_data}")
        update_data_history(sensor_data)
    else:
        print("Impossible de récupérer les données, utilisation des données précédentes")

if __name__ == "__main__":
    main()
