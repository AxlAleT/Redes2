// client.c
// Cliente TCP con login, chat y envío de archivos de texto.
// Comandos:
//  - Escribe mensajes y Enter para enviarlos.
//  - 'salir' para terminar.
//  - '/enviar <ruta>' para mandar archivo al servidor.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 4096

static volatile int running = 1;

typedef struct { int sock; } io_ctx;

static void rstrip_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}
static ssize_t read_line(int fd, char *buf, size_t maxlen) {
    size_t i = 0;
    while (i < maxlen - 1) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) { if (i == 0) return 0; break; }
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}
static long long file_size(const char *path) {
    struct stat st; return (stat(path, &st) == 0) ? (long long)st.st_size : -1;
}
static const char *basename_simple(const char *p) {
    const char *b = strrchr(p, '/');
#ifdef _WIN32
    const char *b2 = strrchr(p, '\\');
    if (!b || (b2 && b2 > b)) b = b2;
#endif
    return b ? b + 1 : p;
}
static int send_file(int sock, const char *path) {
    long long sz = file_size(path);
    if (sz < 0) { fprintf(stderr, "No se puede leer tamaño de %s\n", path); return -1; }
    const char *name = basename_simple(path);

    char header[512];
    snprintf(header, sizeof(header), "FILE %s %lld\n", name, sz);
    if (send(sock, header, strlen(header), 0) < 0) return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return -1; }
    char buf[BUFFER_SIZE];
    long long remaining = sz;
    while (remaining > 0) {
        ssize_t r = read(fd, buf, (remaining > (long long)sizeof(buf)) ? sizeof(buf) : (size_t)remaining);
        if (r < 0) { perror("read"); close(fd); return -1; }
        if (r == 0) break;
        ssize_t sent = 0;
        while (sent < r) {
            ssize_t w = send(sock, buf + sent, (size_t)(r - sent), 0);
            if (w < 0) { if (errno == EINTR) continue; perror("send"); close(fd); return -1; }
            sent += w;
        }
        remaining -= r;
    }
    close(fd);

    char line[BUFFER_SIZE];
    ssize_t n = read_line(sock, line, sizeof(line));
    if (n <= 0) { fprintf(stderr, "Conexión cerrada esperando FILE_OK\n"); return -1; }
    fputs(line, stdout);
    return 0;
}
static void *reader_thread(void *arg) {
    io_ctx *ctx = (io_ctx*)arg;
    char line[BUFFER_SIZE];
    while (running) {
        ssize_t n = read_line(ctx->sock, line, sizeof(line));
        if (n == 0) { printf("Conexión cerrada por el servidor.\n"); running = 0; break; }
        if (n < 0) { perror("recv"); running = 0; break; }
        if (strncasecmp(line, "BYE", 3) == 0) { printf("Servidor solicitó terminar.\n"); running = 0; break; }
        fputs(line, stdout);
    }
    return NULL;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    char user[256], pass[256];
    printf("Usuario: "); fflush(stdout);
    if (!fgets(user, sizeof(user), stdin)) return 1;
    rstrip_newline(user);
    printf("Password: "); fflush(stdout);
    if (!fgets(pass, sizeof(pass), stdin)) return 1;
    rstrip_newline(pass);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv_addr; memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) { perror("inet_pton"); close(sock); return 1; }
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) { perror("connect"); close(sock); return 1; }

    char auth[BUFFER_SIZE];
    snprintf(auth, sizeof(auth), "AUTH %s %s\n", user, pass);
    if (send(sock, auth, strlen(auth), 0) < 0) { perror("send AUTH"); close(sock); return 1; }

    char line[BUFFER_SIZE]; ssize_t n = read_line(sock, line, sizeof(line));
    if (n <= 0) { fprintf(stderr, "Sin respuesta de autenticación.\n"); close(sock); return 1; }
    rstrip_newline(line);
    if (strcmp(line, "AUTH_OK") != 0) { fprintf(stderr, "Login fallido.\n"); close(sock); return 1; }

    printf("Login OK. Escribe mensajes. Usa '/enviar <ruta>' para enviar archivo. 'salir' para terminar.\n");

    io_ctx ctx = { .sock = sock };
    pthread_t th;
    if (pthread_create(&th, NULL, reader_thread, &ctx) != 0) { perror("pthread_create"); close(sock); return 1; }

    char input[BUFFER_SIZE];
    while (running && fgets(input, sizeof(input), stdin)) {
        rstrip_newline(input);
        if (input[0] == '\0') continue;

        if (strncmp(input, "/enviar ", 8) == 0) {
            const char *path = input + 8;
            if (*path == '\0') { printf("Uso: /enviar <ruta>\n"); continue; }
            if (send_file(sock, path) != 0) printf("Error enviando archivo.\n");
            continue;
        }

        if (strncasecmp(input, "salir", 5) == 0 && (input[5] == '\0' || isspace((unsigned char)input[5]))) {
            const char *bye = "salir\n";
            if (send(sock, bye, strlen(bye), 0) < 0) perror("send salir");
            break;
        }

        char msg[BUFFER_SIZE + 2];
        snprintf(msg, sizeof(msg), "%s\n", input);
        if (send(sock, msg, strlen(msg), 0) < 0) { perror("send"); break; }
    }

    running = 0;
    shutdown(sock, SHUT_RDWR);
    pthread_join(th, NULL);
    close(sock);
    return 0;
}
