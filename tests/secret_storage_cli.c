#include "knishio/storage/envelope.h"
#include "knishio/storage/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: secret_storage_cli <seal|open> [args...]\n");
        return 1;
    }

    if (strcmp(argv[1], "seal") == 0 || strcmp(argv[1], "seal-recovery") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: secret_storage_cli seal <passphrase> <secret> <bundleHash> [label]\n");
            return 1;
        }
        const char *passphrase = argv[2];
        const char *secret = argv[3];
        const char *bundle_hash = argv[4];
        const char *label = (argc >= 6 && strlen(argv[5]) > 0) ? argv[5] : NULL;

        knishio_secret_metadata_t meta;
        memset(&meta, 0, sizeof(meta));
        meta.bundle_hash = (char *)bundle_hash;
        meta.label = (char *)label;
        meta.created_at = 1700000000000LL;
        meta.hardware_backed = false;
        meta.provider_type = "aes-gcm";

        char *json_out = NULL;
        knishio_error_t err = knishio_envelope_seal_json(secret, strlen(secret), passphrase, &meta, &json_out);
        if (err != KNISHIO_SUCCESS) {
            fprintf(stderr, "Seal failed with error: %d\n", err);
            return 1;
        }
        printf("%s\n", json_out);
        free(json_out);
        return 0;
    } else if (strcmp(argv[1], "open") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: secret_storage_cli open <passphrase> <payloadJson>\n");
            return 1;
        }
        const char *passphrase = argv[2];
        const char *payload_json = argv[3];

        char *plain_out = NULL;
        size_t plain_len = 0;
        knishio_error_t err = knishio_envelope_open_json(payload_json, passphrase, &plain_out, &plain_len);
        if (err != KNISHIO_SUCCESS) {
            fprintf(stderr, "Open failed with error: %d\n", err);
            return 1;
        }
        printf("%s\n", plain_out);
        free(plain_out);
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    return 1;
}
