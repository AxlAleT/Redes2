import argparse
import socket
import sys


def send_message(host: str, port: int, message: str) -> str:
    with socket.create_connection((host, port), timeout=5) as sock:
        sock.sendall(message.encode('utf-8'))
        data = sock.recv(65536)
        return data.decode('utf-8')


def interactive(host: str, port: int):
    print(f"Cliente conectado a {host}:{port}. Escribe mensajes, 'LIST' para listar, 'CLOSE' para cerrar.")
    with socket.create_connection((host, port), timeout=5) as sock:
        while True:
            try:
                line = input('> ').strip()
            except EOFError:
                line = 'CLOSE'
            if not line:
                continue
            sock.sendall(line.encode('utf-8'))
            data = sock.recv(65536)
            if not data:
                print("Conexión cerrada por el servidor.")
                break
            print(data.decode('utf-8').rstrip())
            if line.upper() == 'CLOSE':
                break


def main():
    parser = argparse.ArgumentParser(description="Cliente TCP simple para el servidor de logs")
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8080)
    parser.add_argument('--message', '-m', help='Enviar un único mensaje y salir (en lugar de modo interactivo).')
    args = parser.parse_args()

    try:
        if args.message:
            print(send_message(args.host, args.port, args.message))
        else:
            interactive(args.host, args.port)
    except (ConnectionRefusedError, socket.timeout) as e:
        print(f"No se pudo conectar al servidor: {e}")
        sys.exit(2)


if __name__ == '__main__':
    main()
