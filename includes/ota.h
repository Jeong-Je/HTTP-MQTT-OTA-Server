#ifndef OTA_H
#define OTA_H

void send_check_response(int client_sock, const char* client_version);

int compare_version(const char* v1, const char* v2);

void get_latest_version(char* version);

#endif