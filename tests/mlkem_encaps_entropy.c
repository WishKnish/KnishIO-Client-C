/**
 * @file mlkem_encaps_entropy.c
 * @brief Prove-the-gate harness for ML-KEM-768 encapsulation randomness.
 *
 * Derives a FIXED ML-KEM-768 keypair from a constant 64-byte seed, performs ONE
 * encapsulation, and prints the resulting ciphertext as hex to stdout.
 *
 * Rationale: ML-KEM derandomizes K-PKE (r = G(m || H(ek))), so the ONLY entropy in an
 * encapsulation is the 32-byte message m. A CSPRNG-backed build therefore prints a
 * DIFFERENT ciphertext on every process launch; a constant-seed RNG (e.g. the
 * mlkem-native test stub) prints the SAME ciphertext on every launch. The keypair is
 * fixed so that any difference is attributable to encapsulation randomness alone.
 *
 * run_encaps_entropy_twice.cmake runs this binary twice as separate processes and
 * asserts the two outputs differ.
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "knishio/crypto/mlkem768.h"

int main(void) {
    /* Fixed 64-byte (d || z) seed -> deterministic keypair -> fixed public key. */
    uint8_t seed[64];
    for (int i = 0; i < 64; i++) {
        seed[i] = (uint8_t)i;
    }

    knishio_mlkem768_keypair_t keypair = {0};
    if (knishio_mlkem768_keypair_from_seed(&keypair, seed, sizeof(seed)) != KNISHIO_SUCCESS) {
        fprintf(stderr, "mlkem_encaps_entropy: keypair_from_seed failed\n");
        return 2;
    }

    knishio_mlkem768_ciphertext_t ct;
    knishio_mlkem768_shared_secret_t ss;
    if (knishio_mlkem768_encapsulate(keypair.public_key, &ct, &ss) != KNISHIO_SUCCESS) {
        fprintf(stderr, "mlkem_encaps_entropy: encapsulate failed\n");
        return 2;
    }

    for (size_t i = 0; i < sizeof(ct.ciphertext); i++) {
        printf("%02x", ct.ciphertext[i]);
    }
    printf("\n");
    return 0;
}
