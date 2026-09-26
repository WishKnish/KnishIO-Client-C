#ifndef KNISHIO_GRAPHQL_INTERNAL_H
#define KNISHIO_GRAPHQL_INTERNAL_H

/**
 * @file graphql_internal.h
 * @brief Internal GraphQL response parsing shared between the GraphQL and client modules
 *
 * Not part of the public API.
 */

#include "knishio/graphql.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief (Re)derive a response from a GraphQL reply body
 *
 * Replaces resp->data with a copy of `body`, clears resp->errors and resp->molecular_hash, sets
 * resp->success from resp->status_code (2xx), then extracts the reply's GraphQL errors (which make
 * it a failure) and, for a mutation, data.ProposeMolecule.molecularHash. The client calls it a
 * second time with the decrypted reply of a CipherHash request, whose envelope carries neither.
 *
 * @param resp Response to update (status_code already set)
 * @param body Reply body JSON, or NULL for an empty reply
 * @param is_mutation Whether the operation that produced the reply is a mutation
 */
void knishio_graphql_response_apply_body(knishio_graphql_response_t* resp, const char* body,
                                         bool is_mutation);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_GRAPHQL_INTERNAL_H */
