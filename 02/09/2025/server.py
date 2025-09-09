import socket
import threading
from typing import List, Tuple

import mysql.connector
from mysql.connector import Error

# Dirección y puerto del servidor
HOST = '127.0.0.1'
PORT = 8080

# Configuración de MySQL (según el contexto proporcionado)
MYSQL_HOST = '127.0.0.1'
MYSQL_PORT = 3306
MYSQL_USER = 'root'
MYSQL_PASSWORD = 'change-me-root'
MYSQL_DB = 'appdb'


def get_connection(database: str | None = MYSQL_DB):
    """Devuelve una conexión a MySQL. Si database es None, conecta sin seleccionar BD."""
    return mysql.connector.connect(
        host=MYSQL_HOST,
        port=MYSQL_PORT,
        user=MYSQL_USER,
        password=MYSQL_PASSWORD,
        database=database if database else None,
        autocommit=True,
    )


def create_db():
    """Crea la BD y la tabla si no existen."""
    # Crear BD si no existe
    conn = get_connection(database=None)
    try:
        with conn.cursor() as cur:
            cur.execute(f"CREATE DATABASE IF NOT EXISTS `{MYSQL_DB}` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;")
    finally:
        conn.close()

    # Crear tabla dentro de la BD
    conn = get_connection(database=MYSQL_DB)
    try:
        with conn.cursor() as cur:
            cur.execute(
                """
                CREATE TABLE IF NOT EXISTS logs (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    msg TEXT NOT NULL
                ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
                """
            )
    finally:
        conn.close()

def insert_message(msg: str) -> None:
    """Insertar mensaje en la base de datos MySQL."""
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("INSERT INTO logs (msg) VALUES (%s)", (msg,))
    finally:
        conn.close()

def get_messages() -> List[Tuple[int, str]]:
    """Consultar mensajes en la base de datos MySQL."""
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("SELECT id, msg FROM logs ORDER BY id ASC")
            rows = cur.fetchall()  # list[tuple]
            return [(int(r[0]), str(r[1])) for r in rows]
    finally:
        conn.close()

# Manejar cada cliente
def handle_client(client_socket):
    while True:
        msg = client_socket.recv(4096).decode('utf-8').strip()  # Recibir mensaje del cliente
        if not msg:
            break
        print(f"Mensaje recibido: {msg}")

        try:
            if msg.upper() == "LIST":
                # Si el mensaje es "LIST", enviar los mensajes almacenados
                logs = get_messages()
                response_lines = ["Mensajes en la base de datos:"]
                for log in logs:
                    response_lines.append(f"ID: {log[0]}, Mensaje: {log[1]}")
                response = "\n".join(response_lines) + "\n"
                client_socket.send(response.encode('utf-8'))
            elif msg.upper() == "CLOSE":
                # Si el mensaje es "CLOSE", cerrar la conexión
                client_socket.send("Conexión cerrada.".encode('utf-8'))
                break
            else:
                # Insertar cualquier otro mensaje en la base de datos
                insert_message(msg)
                client_socket.send(f"Mensaje recibido: {msg}".encode('utf-8'))
        except Error as e:
            err = f"Error de base de datos: {e}"
            print(err)
            client_socket.send(err.encode('utf-8'))

    client_socket.close()

# Crear servidor
def start_server():
    create_db()  # Inicializar la base de datos y tabla en MySQL

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((HOST, PORT))
    server_socket.listen(5)
    print(f"Servidor escuchando en {HOST}:{PORT} (MySQL {MYSQL_HOST}:{MYSQL_PORT}/{MYSQL_DB})...")

    while True:
        client_socket, addr = server_socket.accept()
        print(f"Conexión recibida de {addr}")
        
        # Crear un hilo para manejar al cliente
        client_thread = threading.Thread(target=handle_client, args=(client_socket,))
        client_thread.start()

if __name__ == "__main__":
    start_server()