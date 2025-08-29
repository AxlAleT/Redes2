// Axel Alejandro Torres Ruiz
// Victor Manuel Olivares Cruz
// Jonathan Yael Aguilar Cruz
// Grupo: 6CV4
// Cliente que envía datos a una impresora en red (puerto 9100)

#include <stdio.h>      // Para funciones de entrada/salida (printf, perror, etc.)
#include <stdlib.h>     // Para funciones estándar (exit, EXIT_FAILURE, etc.)
#include <string.h>     // Para funciones de manejo de cadenas (strlen, memset, etc.)
#include <unistd.h>     // Para funciones de sistema (close, read, write)
#include <arpa/inet.h>  // Para estructuras y funciones de sockets (inet_pton, sockaddr_in)

#define PRINTER_PORT 9100   // Puerto estándar para comunicación con impresoras (RAW printing)
#define BUFFER_SIZE 1024    // Tamaño máximo del buffer para leer datos del archivo

int main(int argc, char *argv[]) {
    int sockfd;                      // Descriptor del socket
    struct sockaddr_in printer_addr; // Estructura para guardar dirección IP y puerto de la impresora
    char buffer[BUFFER_SIZE];        // Buffer para almacenar los datos a enviar
    char *ip_addr = "127.0.0.1";     // Dirección IP por defecto (localhost)
    FILE *file;                      // Puntero al archivo que contiene los datos a enviar

    // Si el usuario proporciona una IP como argumento en la línea de comandos, se usa esa en vez de la predeterminada
    if (argc > 1) {
        ip_addr = argv[1];
    }

    // Abrir el archivo README.md en modo lectura
    file = fopen("README.md", "r");
    if (file == NULL) {
        perror("Error al abrir archivo"); // Mensaje si el archivo no existe o no se puede abrir
        exit(EXIT_FAILURE);
    }

    // Leer el contenido del archivo (solo la primera línea en este caso)
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        perror("Error al leer archivo");  // Mensaje si no se pudo leer nada
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file); // Cerrar el archivo después de leerlo

    // Crear un socket TCP (SOCK_STREAM)
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Inicializar la estructura con ceros
    memset(&printer_addr, 0, sizeof(printer_addr));
    printer_addr.sin_family = AF_INET;               // IPv4
    printer_addr.sin_port = htons(PRINTER_PORT);     // Convertir el puerto a formato de red

    // Convertir la dirección IP en formato texto a formato binario
    if (inet_pton(AF_INET, ip_addr, &printer_addr.sin_addr) <= 0) {
        perror("Dirección IP inválida");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Conectarse a la impresora (o servidor que emule impresora en el puerto 9100)
    if (connect(sockfd, (struct sockaddr*)&printer_addr, sizeof(printer_addr)) < 0) {
        perror("Error al conectar con la impresora");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Conexión establecida con la impresora (%s).\n", ip_addr);

    // Enviar datos leídos del archivo
    if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
        perror("Error al enviar datos");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Datos enviados a la impresora: %s\n", buffer);

    // Cerrar el socket (fin de la comunicación)
    close(sockfd);

    return 0; // Fin del programa
}
