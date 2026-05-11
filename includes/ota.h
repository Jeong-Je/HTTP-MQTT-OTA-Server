#ifndef OTA_H
#define OTA_H

void send_check_response(int client_sock, const char* request);

int compare_version(const char* v1, const char* v2);

void get_latest_version(
    const char* address,
    char* version
);

#endif