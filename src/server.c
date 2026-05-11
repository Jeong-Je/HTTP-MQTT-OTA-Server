#include "server.h"
#include "logger.h"
#include "utils.h"
#include "ota.h"
#include "file.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

/* ---------------- 요청 처리 ---------------- */
void* handle_client(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    int len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    if (len <= 0) {
        close(client_sock);
        return NULL;
    }

    buffer[len] = '\0';

    /* 로그 출력 */
    print_log(client_sock, buffer);

    /* ---------------- ROUTING ---------------- */
    /* ---------------- ROUTING ---------------- */
    if (strstr(buffer, "POST /ota/check")) {
        send_check_response(client_sock, buffer);
    }
    else if (strstr(buffer, "GET /ota/down/")) {

        char path[128] = {0};
        extract_path(buffer, path);

        send_file(client_sock, path);
    }
    else if (strstr(buffer, "GET /ota/key/public.pem")) {

        /* 🔐 public key 다운로드 */
        send_file(client_sock, "keys/public.pem");
    }
    else {
        const char* not_found =
            "HTTP/1.1 404 Not Found\r\n\r\n";

        send(client_sock, not_found, strlen(not_found), 0);
    }

    close(client_sock);
    return NULL;
}