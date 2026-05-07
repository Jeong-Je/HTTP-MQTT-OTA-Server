#include "mqtt_handler.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "MQTTClient.h"

#include "config.h"
#include "crypto.h"
#include "ota.h"


void publish_update_notification(const char* version) {

    MQTTClient client;
    MQTTClient_connectOptions conn_opts =
        MQTTClient_connectOptions_initializer;

    MQTTClient_create(
        &client,
        MQTT_ADDRESS,
        MQTT_CLIENT_ID,
        MQTTCLIENT_PERSISTENCE_NONE,
        NULL
    );

    int rc;

    rc = MQTTClient_connect(client, &conn_opts);

    if (rc != MQTTCLIENT_SUCCESS) {
        printf("MQTT connect failed: %d\n", rc);
        return;
    }

    /* HEX 경로 */
    char hex_path[128];

    sprintf(hex_path,
            "hex/%s.hex",
            version);

    /* SHA256 계산 */
    char checksum[65];

    calculate_sha256(
        hex_path,
        checksum
    );

    /* MQTT payload */
    char payload[1024];

    sprintf(payload,
        "{"
        "\"version\":\"%s\","
        "\"firmware_url\":\"http://%s:%d/ota/down/hex/%s.hex\","
        "\"signature_url\":\"http://%s:%d/ota/down/sig/%s.sig\","
        "\"public_key_url\":\"http://%s:%d/ota/key/public.pem\","
        "\"checksum\":\"%s\""
        "}",
        version,
        SERVER_IP, PORT, version,
        SERVER_IP, PORT, version,
        SERVER_IP, PORT,
        checksum
    );

    MQTTClient_message pubmsg =
        MQTTClient_message_initializer;

    pubmsg.payload = payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = MQTT_QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;

    MQTTClient_publishMessage(
        client,
        MQTT_TOPIC,
        &pubmsg,
        &token
    );

    MQTTClient_waitForCompletion(
        client,
        token,
        MQTT_TIMEOUT
    );

    printf("\n[MQTT] OTA update notification published\n");
    printf("TOPIC: %s\n", MQTT_TOPIC);
    printf("PAYLOAD:\n%s\n", payload);

    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
}



void* version_monitor_thread(void* arg) {

    char previous_version[32] = {0};

    get_latest_version(previous_version);

    printf("[MONITOR] current version: %s\n",
           previous_version);

    while (1) {

        sleep(1);

        char current_version[32] = {0};

        get_latest_version(current_version);

        if (strcmp(previous_version,
                   current_version) != 0) {

            printf("\n");
            printf("====================================\n");
            printf("[OTA NEW VERSION DETECTED]\n");
            printf("%s -> %s\n",
                   previous_version,
                   current_version);
            printf("====================================\n");

            publish_update_notification(
                current_version
            );

            strcpy(previous_version,
                   current_version);
        }
    }

    return NULL;
}