// servidor.c  (espera conexiones y responde)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // Crear socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configuración del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Enlazar socket
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Escuchar conexiones
    listen(server_fd, 5);
    printf("Servidor esperando conexiones en el puerto %d...\n", PORT);

    // Aceptar conexión
    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        perror("Error al aceptar conexión");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Recibir mensaje del cliente
    read(client_fd, buffer, BUFFER_SIZE);
    printf("Cliente dice: %s\n", buffer);

    // Responder al cliente
    char *respuesta = "Hola cliente, tu mensaje fue recibido correctamente.\n";
    send(client_fd, respuesta, strlen(respuesta), 0);

    // Cerrar sockets
    close(client_fd);
    close(server_fd);

    return 0;
}
