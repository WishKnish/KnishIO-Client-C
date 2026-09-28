/*
 * Consumer smoke program for an installed KnishIO C SDK package.
 *
 * packaging/check-release-package.sh builds it against a relocated release package three
 * ways (plain -I/-L, pkg-config, find_package) and requires its output to equal the bundle
 * hash computed independently in Python. Nothing in the SDK build references this file.
 *
 * It derives the canonical 2048-hex secret from the seed and prints that secret's bundle
 * hash, the same identity derivation every KnishIO SDK performs.
 */
#include <stdio.h>
#include <knishio/knishio.h>
#include <knishio/wallet.h>
#include <knishio/utils/memory.h>

int main(int argc, char **argv) {
    const char *seed = argc > 1 ? argv[1] : "knishio-consumer-smoke";
    char *secret = NULL;
    char *bundle = NULL;
    int rc = 1;

    if (knishio_init() != KNISHIO_SUCCESS) {
        return 1;
    }
    if (knishio_generate_secret(seed, KNISHIO_SECRET_LENGTH, &secret)
        && knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)) {
        printf("%s\n", bundle);
        rc = 0;
    }
    knishio_free(bundle);
    knishio_free(secret);
    knishio_cleanup();
    return rc;
}
