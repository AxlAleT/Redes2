// Axel Alejandro Torres Ruiz
// Victor Manuel Olivares Cruz
// Jonathan Yael Aguilar Cruz
// Grupo: 6CV4
// Simulador de impresora en red escuchando en puerto 9100
// Recibe datos de un cliente y los muestra en pantalla

#include <stdio.h>      // Entrada/salida estándar (printf, perror, etc.)
#include <stdlib.h>     // Funciones estándar (exit, EXIT_FAILURE, etc.)
#include <string.h>     // Manejo de cadenas y memoria (memset, etc.)
#include <unistd.h>     // close(), read(), write()
#include <arpa/inet.h>  // Funciones para direcciones IP y sockets
#include <sys/socket.h> // Definiciones de sockets

#define PRINTER_PORT 9100   // Puerto típico de impresoras en red
#define BUFFER_SIZE 1024    // Tamaño del buffer de recepción

int main() {
    int server_fd, client_fd;                     // Descriptores de socket (servidor y cliente)
    struct sockaddr_in server_addr, client_addr;  // Estructuras para direcciones IP y puertos
    socklen_t client_len = sizeof(client_addr);   // Tamaño de la dirección del cliente
    char buffer[BUFFER_SIZE];                     // Buffer donde se guardan los datos recibidos
    int bytes_received;                           // Número de bytes recibidos

    // Crear socket TCP (AF_INET = IPv4, SOCK_STREAM = TCP)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error al crear socket"); // Mensaje si falla
        exit(EXIT_FAILURE);
    }

    // Inicializar estructura de dirección del servidor con ceros
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;              // Familia de direcciones IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;      // Aceptar conexiones en cualquier interfaz local
    server_addr.sin_port = htons(PRINTER_PORT);    // Puerto del servidor (convertido a orden de red)

    // Asociar el socket a la dirección y puerto especificados
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");  // Falla si el puerto está ocupado o sin permisos
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Poner el socket en modo escucha, con cola máxima de 1 cliente
    if (listen(server_fd, 1) < 0) {
        perror("Error en listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Simulador de impresora escuchando en el puerto %d...\n", PRINTER_PORT);

    // Esperar (bloqueante) a que se conecte un cliente
    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("Error en accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Cliente conectado.\n");

    // Bucle de recepción: recibe datos del cliente y los imprime
    while ((bytes_received = recv(client_fd, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[bytes_received] = '\0'; // Se agrega fin de cadena para imprimir correctamente
        printf("[Trabajo recibido]: %s", buffer);
    }

    // Si hubo un error en la recepción
    if (bytes_received < 0) {
        perror("Error al recibir datos");
    }

    // Cerrar conexión con el cliente y el socket del servidor
    close(client_fd);
    close(server_fd);

    printf("\nConexión cerrada.\n");
    return 0;
}
