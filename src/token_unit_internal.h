#ifndef KNISHIO_TOKEN_UNIT_INTERNAL_H
#define KNISHIO_TOKEN_UNIT_INTERNAL_H

/**
 * @file token_unit_internal.h
 * @brief Reading token units from a reply: shared by the reply wallet parser and the client
 *        operations that must fail on a unit whose metas are not JSON
 *
 * Not part of the public API.
 */

#include <stdbool.h>

#include "knishio/error/context.h"
#include "knishio/json/parser.h"
#include "knishio/wallet.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Whether text is one JSON value as JSON.parse accepts it: nothing but whitespace may
 *        follow the value ("{}x" and "nullx" are not JSON).
 */
bool knishio_metas_text_is_json(const char *text);

/**
 * @brief Build a client wallet from one reply wallet row (Balance or Wallet list entry), as
 *        knishio_response_wallet_list_to_client_wallet() does, reporting why it failed
 *
 * A unit whose metas is a non-empty string that is not JSON fails the row, as JS
 * TokenUnit.createFromGraphQL throws there. On failure *out is NULL and nothing is leaked.
 *
 * @param wallet_data JSON wallet row
 * @param secret Secret for wallet reconstruction (can be NULL; unused)
 * @param out Receives the wallet (free with knishio_wallet_free)
 * @return KNISHIO_SUCCESS, KNISHIO_ERROR_INVALID_ARGS (NULL wallet_data or out),
 *         KNISHIO_ERROR_JSON_PARSE (unit metas not JSON) or KNISHIO_ERROR_MEMORY
 */
knishio_error_t knishio_wallet_from_reply_json(knishio_json_t *wallet_data, const char *secret,
                                               knishio_wallet_t **out);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_TOKEN_UNIT_INTERNAL_H */
