#ifndef KNISHIO_TOKEN_UNIT_H
#define KNISHIO_TOKEN_UNIT_H

/**
 * @file token_unit.h
 * @brief Token unit structure placeholder for response system
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration - full definition is in wallet.h */
typedef struct knishio_token_unit knishio_token_unit_t;

/**
 * @brief Free token unit
 * @param unit Token unit to free
 */
void knishio_token_unit_free(knishio_token_unit_t *unit);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_TOKEN_UNIT_H */