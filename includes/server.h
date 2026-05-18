#ifndef SERVER_H
#define SERVER_H

void* handle_client(void* arg);

void report(int client_sock, const char* request);

#endif