#include "mqtt_handler.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "MQTTClient.h"

#include "crypto.h"
#include "ota.h"

extern char SERVER_IP[64];
extern int PORT;

/* ECU 목록 */
typedef struct {
    char address[16];
    char previous_version[32];
} ECUInfo;

ECUInfo ecus[] = {
    {"1234", ""},
    {"5678", ""}
};

#define ECU_COUNT (sizeof(ecus) / sizeof(ECUInfo))

/* MQTT 발행 */
void publish_update_notification(
    const char* address,
    const char* version
) {

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

    sprintf(
        hex_path,
        "hex/%s/%s.hex",
        address,
        version
    );

    /* SHA256 */
    char checksum[65];

    calculate_sha256(
        hex_path,
        checksum
    );

    /* MQTT payload */
    char payload[2048];

    sprintf(
        payload,

        "{"
        "\"address\":\"%s\","
        "\"version\":\"%s\","

        "\"firmware_url\":"
        "\"http://%s:%d/ota/down/hex/%s/%s.hex\","

        "\"signature_url\":"
        "\"http://%s:%d/ota/down/sig/%s/%s.sig\","

        "\"public_key_url\":"
        "\"http://%s:%d/ota/key/public.pem\","

        "\"checksum\":\"%s\""
        "}",

        address,
        version,

        SERVER_IP,
        PORT,
        address,
        version,

        SERVER_IP,
        PORT,
        address,
        version,

        SERVER_IP,
        PORT,

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

    printf("\n");
    printf("====================================\n");
    printf("[MQTT OTA NOTIFICATION]\n");
    printf("ECU ADDRESS : %s\n", address);
    printf("VERSION     : %s\n", version);
    printf("====================================\n");

    printf("TOPIC: %s\n", MQTT_TOPIC);

    printf("PAYLOAD:\n%s\n", payload);

    MQTTClient_disconnect(client, 1000);

    MQTTClient_destroy(&client);
}

/* 버전 감시 */
void* version_monitor_thread(void* arg) {

    /* 초기 버전 로드 */
    for (int i = 0; i < ECU_COUNT; i++) {

        get_latest_version(
            ecus[i].address,
            ecus[i].previous_version
        );

        printf(
            "[MONITOR] ECU %s current version: %s\n",
            ecus[i].address,
            ecus[i].previous_version
        );
    }

    while (1) {

        sleep(1);

        for (int i = 0; i < ECU_COUNT; i++) {

            char current_version[32] = {0};

            get_latest_version(
                ecus[i].address,
                current_version
            );

            if (strcmp(
                    ecus[i].previous_version,
                    current_version
                ) != 0) {

                printf("\n");

                printf("====================================\n");

                printf("[OTA NEW VERSION DETECTED]\n");

                printf(
                    "ECU : %s\n",
                    ecus[i].address
                );

                printf(
                    "%s -> %s\n",
                    ecus[i].previous_version,
                    current_version
                );

                printf("====================================\n");

                publish_update_notification(
                    ecus[i].address,
                    current_version
                );

                strcpy(
                    ecus[i].previous_version,
                    current_version
                );
            }
        }
    }

    return NULL;
}