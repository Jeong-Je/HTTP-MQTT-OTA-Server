#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "config.h"
#include "server.h"
#include "mqtt_handler.h"

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

    listen(server_sock, 5);

    printf("OTA Server running on port %d...\n", PORT);

    pthread_t monitor_tid;

    pthread_create(&monitor_tid, NULL, version_monitor_thread, NULL);

    while (1) 
    {
        int* client_sock = malloc(sizeof(int));

        *client_sock = accept(server_sock, NULL, NULL);

        pthread_t tid;

        pthread_create(&tid, NULL, handle_client, client_sock);

        pthread_detach(tid);
    }

    close(server_sock);

    return 0;
}