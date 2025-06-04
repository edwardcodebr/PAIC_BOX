#!/bin/bash

# Navega para o diretório onde o script storage.py está localizado
# Assumindo que start.sh e storage.py estão no mesmo diretório
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
cd "$SCRIPT_DIR"

DATA_DIR_NAME="dtn_boat_storage"
DATA_DIR_PATH="$HOME/$DATA_DIR_NAME" # Usa o mesmo caminho do script Python

echo "Verificando/Criando diretório de armazenamento: $DATA_DIR_PATH"
if [ ! -d "$DATA_DIR_PATH" ]; then
    mkdir -p "$DATA_DIR_PATH"
    if [ $? -eq 0 ]; then
        echo "Diretório $DATA_DIR_PATH criado."
    else
        echo "ERRO: Falha ao criar diretório $DATA_DIR_PATH. Verifique permissões."
        exit 1
    fi
else
    echo "Diretório $DATA_DIR_PATH já existe."
fi

echo "Iniciando o servidor de armazenamento DTN (storage.py)..."
echo "Log será direcionado para stdout/stderr."
echo "Para rodar em background e persistir após logout, use 'nohup' ou 'systemd'."

# Executa o script Python.
# Certifique-se de que python3 e flask estão instalados.
# (ex: sudo apt update && sudo apt install python3 python3-pip && pip3 install Flask)
python3 storage.py

# Se quiser que ele continue rodando mesmo depois de fechar o terminal:
# nohup python3 storage.py > storage_server.log 2>&1 &
# echo "Servidor iniciado em background. Log em storage_server.log. PID: $!"

echo "Servidor storage.py encerrado."