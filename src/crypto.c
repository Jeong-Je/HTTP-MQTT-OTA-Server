#include "crypto.h"

#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

#define BUFFER_SIZE 4096

/* SHA256 */
void calculate_sha256(const char* path, char output[65]) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        strcpy(output, "0");
        return;
    }

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    unsigned char buffer[BUFFER_SIZE];
    int len;

    while ((len = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        SHA256_Update(&ctx, buffer, len);
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(output + (i * 2), "%02x", hash[i]);

    output[64] = '\0';
    fclose(file);
}
