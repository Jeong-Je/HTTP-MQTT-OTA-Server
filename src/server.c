#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <microhttpd.h>

#include "ota.h"
#include "mqtt_handler.h"
#include "crypto.h"

#define POST_BUFFER_SIZE 1024

extern int PORT;

/* ========================= */
/* CONNECTION INFO */
/* ========================= */

struct ConnectionInfo
{
    struct MHD_PostProcessor* post_processor;

    FILE* hex_fp;

    char address[32];
    char version[32];

    char body[8192];
    size_t body_size;
};

/* ========================= */
/* MULTIPART PARSER */
/* ========================= */

static int iterate_post(
    void* coninfo_cls,
    enum MHD_ValueKind kind,
    const char* key,
    const char* filename,
    const char* content_type,
    const char* transfer_encoding,
    const char* data,
    uint64_t off,
    size_t size
)
{
    struct ConnectionInfo* con_info =
        (struct ConnectionInfo*)coninfo_cls;

    /* ========================= */
    /* address */
    /* ========================= */

    if (strcmp(key, "address") == 0)
    {
        snprintf(
            con_info->address,
            sizeof(con_info->address),
            "%.*s",
            (int)size,
            data
        );
    }

    /* ========================= */
    /* version */
    /* ========================= */

    else if (strcmp(key, "version") == 0)
    {
        snprintf(
            con_info->version,
            sizeof(con_info->version),
            "%.*s",
            (int)size,
            data
        );
    }

    /* ========================= */
    /* HEX FILE */
    /* ========================= */

    else if (strcmp(key, "hex_file") == 0)
    {
        char path[256];

        sprintf(
            path,
            "hex/%s/%s.hex",
            con_info->address,
            con_info->version
        );

        if (off == 0)
        {
            printf(
                "[UPLOAD HEX] %s\n",
                path
            );

            con_info->hex_fp =
                fopen(path, "wb");

            if (!con_info->hex_fp)
            {
                perror("fopen hex");

                return MHD_NO;
            }
        }

        if (con_info->hex_fp)
        {
            fwrite(
                data,
                1,
                size,
                con_info->hex_fp
            );
        }
    }

    return MHD_YES;
}

/* ========================= */
/* HTTP HANDLER */
/* ========================= */

static int answer_to_connection(
    void* cls,
    struct MHD_Connection* connection,
    const char* url,
    const char* method,
    const char* version,
    const char* upload_data,
    size_t* upload_data_size,
    void** con_cls
)
{
    /* ========================= */
    /* FIRST CONNECTION */
    /* ========================= */

    if (*con_cls == NULL)
    {
        struct ConnectionInfo* con_info =
            calloc(
                1,
                sizeof(struct ConnectionInfo)
            );

        if (strcmp(method, "POST") == 0)
        {
            con_info->post_processor =
                MHD_create_post_processor(
                    connection,
                    POST_BUFFER_SIZE,
                    iterate_post,
                    con_info
                );
        }

        *con_cls = con_info;

        return MHD_YES;
    }

    struct ConnectionInfo* con_info =
        (struct ConnectionInfo*)(*con_cls);

    /* ========================= */
    /* POST */
    /* ========================= */

    if (strcmp(method, "POST") == 0)
    {
        /* ========================= */
        /* RECEIVE BODY */
        /* ========================= */

        if (*upload_data_size != 0)
        {
            /* /ota/check only */

            if (strcmp(url, "/ota/check") == 0)
            {
                if (
                    con_info->body_size +
                    *upload_data_size <
                    sizeof(con_info->body) - 1
                )
                {
                    memcpy(
                        con_info->body +
                        con_info->body_size,
                        upload_data,
                        *upload_data_size
                    );

                    con_info->body_size +=
                        *upload_data_size;

                    con_info->body[
                        con_info->body_size
                    ] = '\0';
                }
            }

            if (con_info->post_processor)
            {
                MHD_post_process(
                    con_info->post_processor,
                    upload_data,
                    *upload_data_size
                );
            }

            *upload_data_size = 0;

            return MHD_YES;
        }

        /* ========================= */
        /* /ota/check */
        /* ========================= */

        if (strcmp(url, "/ota/check") == 0)
        {
            printf("\n");

            printf(
                "====================================\n"
            );

            printf(
                "[OTA CHECK REQUEST]\n"
            );

            printf(
                "%s\n",
                con_info->body
            );

            printf(
                "====================================\n"
            );

            char response_json[8192];

            build_check_response_json(
                con_info->body,
                response_json
            );

            struct MHD_Response* response =
                MHD_create_response_from_buffer(
                    strlen(response_json),
                    (void*)response_json,
                    MHD_RESPMEM_MUST_COPY
                );

            int ret =
                MHD_queue_response(
                    connection,
                    MHD_HTTP_OK,
                    response
                );

            MHD_destroy_response(response);

            return ret;
        }

        /* ========================= */
        /* /upload */
        /* ========================= */

        if (strcmp(url, "/upload") == 0)
        {
            if (con_info->hex_fp)
            {
                fclose(con_info->hex_fp);

                con_info->hex_fp = NULL;
            }

            printf("\n");

            printf(
                "====================================\n"
            );

            printf(
                "[UPLOAD COMPLETE]\n"
            );

            printf(
                "ADDRESS : %s\n",
                con_info->address
            );

            printf(
                "VERSION : %s\n",
                con_info->version
            );

            printf(
                "====================================\n"
            );

            /* ========================= */
            /* GENERATE SIG */
            /* ========================= */

            char hex_path[256];
            char sig_path[256];

            sprintf(
                hex_path,
                "hex/%s/%s.hex",
                con_info->address,
                con_info->version
            );

            sprintf(
                sig_path,
                "sig/%s/%s.sig",
                con_info->address,
                con_info->version
            );

            generate_sig_file(
                hex_path,
                sig_path
            );

            printf(
                "[SIG GENERATED] %s\n",
                sig_path
            );

            /* ========================= */
            /* version.list */
            /* ========================= */

            char version_path[256];

            sprintf(
                version_path,
                "hex/%s/version.list",
                con_info->address
            );

            FILE* fp =
                fopen(version_path, "a");

            if (fp)
            {
                fseek(fp, 0, SEEK_END);

                long size = ftell(fp);

                if (size > 0)
                {
                    fseek(fp, -1, SEEK_END);

                    int last = fgetc(fp);

                    if (last != '\n')
                    {
                        fseek(fp, 0, SEEK_END);

                        fprintf(fp, "\n");
                    }
                }

                fprintf(
                    fp,
                    "%s\n",
                    con_info->version
                );

                fclose(fp);
            }

            /* ========================= */
            /* MQTT */
            /* ========================= */

            publish_update_notification(
                con_info->address,
                con_info->version
            );

            /* ========================= */
            /* RESPONSE */
            /* ========================= */

            const char* json =
                "{"
                "\"result\":\"ok\""
                "}";

            struct MHD_Response* response =
                MHD_create_response_from_buffer(
                    strlen(json),
                    (void*)json,
                    MHD_RESPMEM_PERSISTENT
                );

            int ret =
                MHD_queue_response(
                    connection,
                    MHD_HTTP_OK,
                    response
                );

            MHD_destroy_response(response);

            return ret;
        }
    }

    /* ========================= */
    /* GET */
    /* ========================= */

    if (strcmp(method, "GET") == 0)
    {
        /* ========================= */
        /* OTA FILE DOWNLOAD */
        /* ========================= */

        if (strncmp(url, "/ota/down/", 10) == 0)
        {
            char path[256];

            strcpy(
                path,
                url + 10
            );

            FILE* fp =
                fopen(path, "rb");

            if (!fp)
            {
                return MHD_NO;
            }

            fseek(fp, 0, SEEK_END);

            long size = ftell(fp);

            rewind(fp);

            char* buffer =
                malloc(size);

            fread(
                buffer,
                1,
                size,
                fp
            );

            fclose(fp);

            struct MHD_Response* response =
                MHD_create_response_from_buffer(
                    size,
                    buffer,
                    MHD_RESPMEM_MUST_FREE
                );

            int ret =
                MHD_queue_response(
                    connection,
                    MHD_HTTP_OK,
                    response
                );

            MHD_destroy_response(response);

            return ret;
        }

        /* ========================= */
        /* PUBLIC KEY */
        /* ========================= */

        if (
            strcmp(
                url,
                "/ota/key/public.pem"
            ) == 0
        )
        {
            FILE* fp =
                fopen(
                    "keys/public.pem",
                    "rb"
                );

            if (!fp)
            {
                return MHD_NO;
            }

            fseek(fp, 0, SEEK_END);

            long size = ftell(fp);

            rewind(fp);

            char* buffer =
                malloc(size);

            fread(
                buffer,
                1,
                size,
                fp
            );

            fclose(fp);

            struct MHD_Response* response =
                MHD_create_response_from_buffer(
                    size,
                    buffer,
                    MHD_RESPMEM_MUST_FREE
                );

            int ret =
                MHD_queue_response(
                    connection,
                    MHD_HTTP_OK,
                    response
                );

            MHD_destroy_response(response);

            return ret;
        }
    }

    return MHD_NO;
}

/* ========================= */
/* START SERVER */
/* ========================= */

void start_server()
{
    struct MHD_Daemon* daemon;

    daemon =
        MHD_start_daemon(
            MHD_USE_INTERNAL_POLLING_THREAD,
            PORT,
            NULL,
            NULL,
            answer_to_connection,
            NULL,
            MHD_OPTION_END
        );

    if (!daemon)
    {
        printf(
            "Server start failed\n"
        );

        return;
    }

    printf(
        "OTA Server running on port %d\n",
        PORT
    );

    getchar();

    MHD_stop_daemon(daemon);
}