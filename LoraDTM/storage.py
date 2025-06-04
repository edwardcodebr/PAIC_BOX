# Salve este como storage.py
from flask import Flask, request, jsonify
import datetime
import os
import csv
import socket

app = Flask(__name__)

# --- Arquivo CSV para TODOS os dados ---
ALL_DATA_CSV_FILE = "all_sensor_data.csv"
ALL_DATA_CSV_HEADER = [
    "NODE_ID", "MSG_COUNT", "RSSI_PLACEHOLDER", "TIMESTAMP_GPS", "LATITUDE", "LONGITUDE",
    "CO2_PPM", "CO_PPM", "TEMPERATURE_C", "HUMIDITY_PERC", "SERVER_TIMESTAMP_UTC"
] # O server_timestamp_utc é adicionado pelo script Python

# --- Arquivo CSV específico para dados climáticos ---
CLIMATIC_CSV_FILE = "dados_climaticos.csv"
CLIMATIC_CSV_HEADER = ["timestamp", "temperatura", "umidade"] # Estes vêm do ESP32

SERVER_PORT = 5000 # Deve corresponder ao LINUX_SERVER_PORT no ESP32

def save_all_data_to_csv(node_id_from_json, csv_data_string):
    """Salva todos os dados recebidos no arquivo CSV principal."""
    server_timestamp = datetime.datetime.utcnow().isoformat() + "Z"
    
    # A string CSV do ESP32 é: NODE_ID,MSG_COUNT,0,TIMESTAMP_GPS,LAT,LON,CO2,CO,TEMP,HUM
    data_row_values = csv_data_string.split(',')
    
    # Adiciona o timestamp do servidor no final da linha de dados
    full_data_row = data_row_values + [server_timestamp]

    file_exists = os.path.isfile(ALL_DATA_CSV_FILE)
    try:
        # Abre em modo de anexação ('a')
        with open(ALL_DATA_CSV_FILE, 'a', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile)
            if not file_exists or os.path.getsize(ALL_DATA_CSV_FILE) == 0:
                writer.writerow(ALL_DATA_CSV_HEADER) # Escreve o cabeçalho se o arquivo for novo/vazio
            writer.writerow(full_data_row)
        print(f"[{datetime.datetime.now()}] Todos os dados de '{node_id_from_json}' salvos em '{ALL_DATA_CSV_FILE}'")
        return True
    except Exception as e:
        print(f"Erro ao salvar todos os dados em CSV para '{node_id_from_json}': {e}")
        return False

def save_climatic_data_to_csv(csv_data_string):
    """Salva apenas timestamp, temperatura e umidade no arquivo dados_climaticos.csv."""
    parts = csv_data_string.split(',')
    
    # ESP32 envia 10 campos: NODE_ID,MSG_COUNT,0,TIMESTAMP_GPS,LAT,LON,CO2,CO,TEMP,HUM
    if len(parts) < 10:
        print(f"Erro (climatic_csv): Dados CSV incompletos. Recebido: '{csv_data_string}' (Campos: {len(parts)})")
        return False

    try:
        # Índices baseados no formato CSV do ESP32:
        timestamp_gps = parts[3] if parts[3].strip().upper() != "N/A" else "" # dateTimeStr
        temp_str = parts[8] if parts[8].strip().upper() != "N/A" else ""      # dhtData.temperature
        hum_str = parts[9] if parts[9].strip().upper() != "N/A" else ""       # dhtData.humidity
        
        # O timestamp_gps já vem formatado do ESP32 (ex: "2023-10-27T12:34:56Z" ou com .%02dZ se modificado no ESP32)
        climatic_row = [timestamp_gps, temp_str, hum_str]

        file_exists = os.path.isfile(CLIMATIC_CSV_FILE)
        # Abre em modo de anexação ('a')
        with open(CLIMATIC_CSV_FILE, 'a', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile)
            if not file_exists or os.path.getsize(CLIMATIC_CSV_FILE) == 0:
                writer.writerow(CLIMATIC_CSV_HEADER) # Escreve o cabeçalho se novo/vazio
            writer.writerow(climatic_row)
        print(f"[{datetime.datetime.now()}] Dados climáticos (Timestamp: {timestamp_gps}) salvos em '{CLIMATIC_CSV_FILE}'")
        return True
    except IndexError:
        print(f"Erro de índice (climatic_csv) ao processar: '{csv_data_string}'")
        return False
    except Exception as e:
        print(f"Erro ao salvar dados climáticos em CSV: {e}")
        return False

@app.route('/upload', methods=['POST'])
def upload_data():
    if not request.is_json:
        print(f"[{datetime.datetime.now()}] Erro: Requisição não é JSON.")
        return jsonify({"error": "Request must be JSON"}), 400

    content = request.get_json()
    node_id = content.get('node_id')
    csv_data_line = content.get('data')

    if not node_id or not csv_data_line:
        print(f"[{datetime.datetime.now()}] Erro: 'node_id' ou 'data' ausente no payload JSON.")
        return jsonify({"error": "Missing 'node_id' or 'data' in JSON payload"}), 400

    print(f"\n[{datetime.datetime.now()}] Servidor: Recebido de Node ID '{node_id}':")
    print(f"  Payload Data (CSV string): '{csv_data_line}'")

    # Salvar todos os dados em all_sensor_data.csv
    success_all_data_csv = save_all_data_to_csv(node_id, csv_data_line)

    # Salvar dados climáticos específicos em dados_climaticos.csv
    success_climatic_csv = save_climatic_data_to_csv(csv_data_line)
    
    if success_all_data_csv and success_climatic_csv:
        print(f"[{datetime.datetime.now()}] Servidor: Dados de '{node_id}' processados e salvos com sucesso.")
        return jsonify({"message": "Data received and stored in CSV files successfully", "node_id": node_id}), 200
    else:
        error_messages = []
        if not success_all_data_csv: error_messages.append(f"Falha ao salvar em '{ALL_DATA_CSV_FILE}'")
        if not success_climatic_csv: error_messages.append(f"Falha ao salvar em '{CLIMATIC_CSV_FILE}'")
        full_error_msg = f"Falha ao armazenar alguns dados CSV: {'; '.join(error_messages)}"
        print(f"[{datetime.datetime.now()}] Servidor: Erro para Node ID '{node_id}': {full_error_msg}")
        return jsonify({"error": full_error_msg, "node_id": node_id}), 500

if __name__ == '__main__':
    print("\n--- Servidor Flask para Armazenamento de Dados CSV do ESP32 ---")
    print(f"Configurado para escutar na porta {SERVER_PORT} em todas as interfaces (0.0.0.0).")
    
    try:
        hostname = socket.gethostname()
        print(f"Hostname desta máquina: {hostname}")
        # Tenta obter o IP que está na mesma sub-rede que o ESP32 AP (ex: 192.168.4.x)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(0.1) 
        try:
            # O ESP32 AP tipicamente é 192.168.4.1. 
            # Conectar a um IP na sub-rede do AP ajuda a descobrir o IP da interface local correta.
            s.connect(("192.168.4.1", 1)) 
            local_ip_for_esp32_network = s.getsockname()[0]
            print(f"IP POTENCIAL desta máquina na rede do ESP32 AP: {local_ip_for_esp32_network}")
            print(f"==> No código do ESP32, defina LINUX_SERVER_HOST como: \"{local_ip_for_esp32_network}\"")
            print(f"==> E LINUX_SERVER_PORT como: {SERVER_PORT}")
        except Exception:
            print("AVISO: Não foi possível determinar automaticamente o IP na sub-rede 192.168.4.x.")
            print("Verifique manualmente o IP da interface de rede (Wi-Fi) desta máquina")
            print("APÓS conectá-la à rede Wi-Fi criada pelo ESP32 ('ESP32_Node_AP').")
            print("   - No Linux, use: ip addr show <interface_wifi>  (ex: ip addr show wlan0)")
            print("   - No Windows, use: ipconfig")
            print("O IP será algo como 192.168.4.X (onde X é diferente de 1).")
            print(f"==> No código do ESP32, defina LINUX_SERVER_HOST para esse IP e LINUX_SERVER_PORT para {SERVER_PORT}.")
        finally:
            s.close()
    except Exception as e:
        print(f"Erro ao tentar obter informações de rede: {e}")
        print("Por favor, configure manualmente o IP do servidor no ESP32.")

    print(f"\nIniciando servidor Flask em http://0.0.0.0:{SERVER_PORT}")
    print("Aguardando dados do ESP32...")
    print("Pressione CTRL+C para parar o servidor.")
    
    # debug=False para um log mais limpo em "produção", True para mais detalhes durante o desenvolvimento
    app.run(host='0.0.0.0', port=SERVER_PORT, debug=True) 