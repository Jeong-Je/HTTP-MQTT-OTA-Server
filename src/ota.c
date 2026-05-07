#include "ota.h"
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "crypto.h"
#include "file.h"

/* ---------------- 유틸 ---------------- */
void get_latest_version(char* version) {
    FILE* fp = fopen("version.list", "r");
    if (!fp) {
        strcpy(version, "0.0");
        return;
    }

    char line[64];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        strcpy(version, line);
    }
    fclose(fp);
}


/* ---------------- CHECK API ---------------- */
void send_check_response(int client_sock, const char* client_version) {

    char latest[32];
    get_latest_version(latest);

    int need_update = compare_version(client_version, latest);

    char response[1024];

    if (!need_update) {
        sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n\r\n"
            "{ \"update\": false }"
        );
    }
    else {
        char hex_path[128];
        sprintf(hex_path, "hex/%s.hex", latest);

        long size = get_file_size(hex_path);

        char checksum[65];
        calculate_sha256(hex_path, checksum);

        sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n\r\n"
            "{"
            "\"update\":true,"
            "\"version\":\"%s\","
            "\"firmware_url\":\"http://%s:%d/ota/down/hex/%s.hex\","
            "\"signature_url\":\"http://%s:%d/ota/down/sig/%s.sig\","
            "\"public_key_url\":\"http://%s:%d/ota/key/public.pem\","
            "\"checksum\":\"%s\","
            "\"size\":%ld"
            "}",
            latest, SERVER_IP, PORT, latest, SERVER_IP, PORT, latest, SERVER_IP, PORT, checksum, size
        );
    }

    send(client_sock, response, strlen(response), 0);
}

int compare_version(const char* v1, const char* v2) {
    int a, b, c, d;

    sscanf(v1, "%d.%d", &a, &b);
    sscanf(v2, "%d.%d", &c, &d);

    if (a != c)
        return a < c;

    return b < d;
}