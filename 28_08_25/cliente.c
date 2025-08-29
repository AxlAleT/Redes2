// cliente.c (envia mensaje al servidor)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = "Hola servidor, soy el cliente.";

    // Crear socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configuración del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);  // localhost

    // Conectar con el servidor
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al conectar");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Enviar mensaje
    send(sockfd, buffer, strlen(buffer), 0);

    // Recibir respuesta
    char respuesta[BUFFER_SIZE];
    read(sockfd, respuesta, BUFFER_SIZE);
    printf("Servidor dice: %s\n", respuesta);

    close(sockfd);

    return 0;
}
