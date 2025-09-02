// server.c
// Servidor TCP con login (CSV), múltiples clientes (fork + pthread), chat simple y recepción de archivos.
// - Imprime en consola los mensajes recibidos (usuario, IP:puerto y contenido).
// - Guarda archivos enviados por el cliente en ./uploads/.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define CSV_PATH "users.csv"
#define UPLOAD_DIR "uploads"

static void rstrip_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}
static void trim(char *s) {
    char *p = s, *q = s + strlen(s) - 1;
    while (*p && isspace((unsigned char)*p)) p++;
    while (q >= p && isspace((unsigned char)*q)) *q-- = '\0';
    if (p != s) memmove(s, p, (size_t)(q - p + 2));
}
static ssize_t read_n(int fd, void *buf, size_t n) {
    size_t total = 0; char *p = (char*)buf;
    while (total < n) {
        ssize_t r = recv(fd, p + total, n - total, 0);
        if (r == 0) return 0;
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        total += (size_t)r;
    }
    return (ssize_t)total;
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
static bool check_credentials(const char *user, const char *pass) {
    FILE *f = fopen(CSV_PATH, "r");
    if (!f) { perror("No se pudo abrir users.csv"); return false; }
    char line[1024]; bool ok = false;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char *p = strchr(line, ','); if (!p) continue;
        *p = '\0';
        char *csv_user = line; char *csv_pass = p + 1;
        rstrip_newline(csv_pass); trim(csv_user); trim(csv_pass);
        if (strcmp(csv_user, user) == 0 && strcmp(csv_pass, pass) == 0) { ok = true; break; }
    }
    fclose(f); return ok;
}
static void sanitize_filename(const char *in, char *out, size_t outsz) {
    const char *base = strrchr(in, '/');
#ifdef _WIN32
    const char *base2 = strrchr(in, '\\');
    if (!base || (base2 && base2 > base)) base = base2;
#endif
    base = base ? base + 1 : in;
    size_t j = 0;
    for (size_t i = 0; base[i] && j + 1 < outsz; i++) {
        unsigned char c = (unsigned char)base[i];
        out[j++] = (isalnum(c) || c == '.' || c == '_' || c == '-') ? (char)c : '_';
    }
    out[j] = '\0';
    if (j == 0) strncpy(out, "archivo.txt", outsz);
}
static int ensure_upload_dir(void) {
    struct stat st;
    if (stat(UPLOAD_DIR, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
    return (mkdir(UPLOAD_DIR, 0755) == -1 && errno != EEXIST) ? -1 : 0;
}
static int receive_file(int sock, const char *name, long long size, const char *who, const char *ipport) {
    if (size < 0) return -1;
    if (ensure_upload_dir() != 0) return -1;

    char safe[256]; sanitize_filename(name, safe, sizeof(safe));
    char path[512]; snprintf(path, sizeof(path), "%s/%s", UPLOAD_DIR, safe);
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) return -1;

    char buf[BUFFER_SIZE];
    long long remaining = size;
    while (remaining > 0) {
        size_t chunk = (remaining > (long long)sizeof(buf)) ? sizeof(buf) : (size_t)remaining;
        ssize_t r = read_n(sock, buf, chunk);
        if (r <= 0) { close(fd); return -1; }
        if (write(fd, buf, (size_t)r) != r) { close(fd); return -1; }
        remaining -= r;
    }
    close(fd);
    printf("[ARCHIVO] %s@%s -> %s (%lld bytes)\n", who, ipport, path, size);
    fflush(stdout);
    return 0;
}

typedef struct {
    int sock;
    struct sockaddr_in addr;
    char user[256];
} client_ctx;

static void now_str(char *out, size_t n) {
    time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%d %H:%M:%S", &tm);
}

static void *client_thread(void *arg) {
    client_ctx *ctx = (client_ctx*)arg;
    int sock = ctx->sock;
    char line[BUFFER_SIZE];

    char ipstr[64]; inet_ntop(AF_INET, &ctx->addr.sin_addr, ipstr, sizeof(ipstr));
    int cport = ntohs(ctx->addr.sin_port);
    char ipport[96]; snprintf(ipport, sizeof(ipport), "%s:%d", ipstr, cport);

    // --- Autenticación ---
    ssize_t n = read_line(sock, line, sizeof(line));
    if (n <= 0) goto done;
    rstrip_newline(line);

    if (strncasecmp(line, "AUTH ", 5) == 0) {
        char user[256], pass[256];
        if (sscanf(line + 5, "%255s %255s", user, pass) != 2 || !check_credentials(user, pass)) {
            send(sock, "AUTH_FAIL\n", 10, 0);
            goto done;
        }
        strncpy(ctx->user, user, sizeof(ctx->user)-1);
        ctx->user[sizeof(ctx->user)-1] = '\0';
        send(sock, "AUTH_OK\n", 8, 0);
        printf("[LOGIN] %s conectado desde %s\n", ctx->user, ipport);
        fflush(stdout);
    } else {
        send(sock, "AUTH_FAIL\n", 10, 0);
        goto done;
    }

    // --- Bucle de chat/comandos ---
    while (1) {
        n = read_line(sock, line, sizeof(line));
        if (n == 0) { // desconexión
            printf("[DESCONECTADO] %s @ %s\n", ctx->user[0] ? ctx->user : "?", ipport);
            fflush(stdout);
            break;
        } else if (n < 0) {
            perror("recv");
            break;
        }
        rstrip_newline(line);
        if (line[0] == '\0') continue;

        // salir
        if (strncasecmp(line, "salir", 5) == 0 && (line[5] == '\0' || isspace((unsigned char)line[5]))) {
            send(sock, "BYE\n", 4, 0);
            printf("[SALIR] %s @ %s cerró sesión\n", ctx->user, ipport);
            fflush(stdout);
            break;
        }

        // FILE <nombre> <bytes>
        if (strncasecmp(line, "FILE ", 5) == 0) {
            char fname[256]; long long fsz = -1;
            if (sscanf(line + 5, "%255s %lld", fname, &fsz) != 2 || fsz < 0) {
                send(sock, "FILE_ERR header\n", 16, 0);
                continue;
            }
            if (receive_file(sock, fname, fsz, ctx->user[0] ? ctx->user : "?", ipport) == 0) {
                char okmsg[512];
                snprintf(okmsg, sizeof(okmsg), "FILE_OK %s %lld\n", fname, fsz);
                send(sock, okmsg, strlen(okmsg), 0);
            } else {
                send(sock, "FILE_ERR io\n", 12, 0);
            }
            continue;
        }

        // Mensaje normal: imprimir en servidor y responder
        char when[32]; now_str(when, sizeof(when));
        printf("[%s] %s @ %s: %s\n", when, ctx->user[0] ? ctx->user : "?", ipport, line);
        fflush(stdout);

        // Respuesta (puedes personalizarla; por simplicidad, eco con prefijo)
        char reply[BUFFER_SIZE + 64];
        snprintf(reply, sizeof(reply), "SERVIDOR: %s\n", line);
        send(sock, reply, strlen(reply), 0);
    }

done:
    close(sock);
    free(ctx);
    return NULL;
}

int main(void) {
    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address; socklen_t addrlen = sizeof(address);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind"); close(server_fd); exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 16) < 0) {
        perror("listen"); close(server_fd); exit(EXIT_FAILURE);
    }

    printf("Servidor esperando conexiones en el puerto %d...\n", PORT);
    printf("Usuarios CSV: %s (formato: usuario,contraseña)\n", CSV_PATH);

    while (1) {
        struct sockaddr_in cliaddr; socklen_t clilen = sizeof(cliaddr);
        int new_sock = accept(server_fd, (struct sockaddr*)&cliaddr, &clilen);
        if (new_sock < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork"); close(new_sock); continue;
        } else if (pid == 0) {
            // Hijo
            close(server_fd);

            client_ctx *ctx = (client_ctx*)calloc(1, sizeof(client_ctx));
            ctx->sock = new_sock; ctx->addr = cliaddr; ctx->user[0] = '\0';

            pthread_t th;
            if (pthread_create(&th, NULL, client_thread, ctx) != 0) {
                perror("pthread_create"); close(new_sock); free(ctx); exit(EXIT_FAILURE);
            }
            pthread_join(th, NULL);
            exit(EXIT_SUCCESS);
        } else {
            // Padre
            close(new_sock);
        }
    }

    close(server_fd);
    return 0;
}
