#ifndef KNISHIO_RESPONSE_PROPOSE_MOLECULE_H
#define KNISHIO_RESPONSE_PROPOSE_MOLECULE_H

/**
 * @file response_propose_molecule.h
 * @brief Response for proposing new molecules
 * 
 * Handles responses from molecule proposals, including status checking,
 * payload extraction, and error handling compatible with the JavaScript
 * SDK's ResponseProposeMolecule.
 */

#include "response.h"
#include "../molecule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_response_propose_molecule knishio_response_propose_molecule_t;

/* Molecule status enum is defined in knishio/molecule.h */

/**
 * @brief Propose molecule response structure
 */
struct knishio_response_propose_molecule {
    knishio_response_t base;                /**< Base response */
    knishio_molecule_t *client_molecule;    /**< Original client molecule */
    knishio_molecule_t *server_molecule;    /**< Server-returned molecule */
    knishio_json_t *parsed_payload;         /**< Parsed payload JSON */
    knishio_molecule_status_t status;       /**< Proposal status */
    char *reason;                           /**< Rejection reason */
    bool payload_cached;                    /**< Whether payload is cached */
    bool server_molecule_cached;            /**< Whether server molecule is cached */
};

/**
 * @brief Create propose molecule response
 * @param query Original mutation query
 * @param json JSON response data
 * @param client_molecule Original molecule that was proposed
 * @return Propose molecule response or NULL on error
 */
knishio_response_propose_molecule_t* knishio_response_propose_molecule_create(knishio_query_t *query,
                                                                              knishio_json_t *json,
                                                                              knishio_molecule_t *client_molecule);

/**
 * @brief Free propose molecule response
 * @param response Propose molecule response to free
 */
void knishio_response_propose_molecule_free(knishio_response_propose_molecule_t *response);

/**
 * @brief Get client molecule (original proposal)
 * @param response Propose molecule response
 * @return Client molecule
 */
knishio_molecule_t* knishio_response_propose_molecule_get_client_molecule(knishio_response_propose_molecule_t *response);

/**
 * @brief Get server molecule (result)
 * @param response Propose molecule response
 * @return Server molecule or NULL if not available
 */
knishio_molecule_t* knishio_response_propose_molecule_get_server_molecule(knishio_response_propose_molecule_t *response);

/**
 * @brief Get proposal status
 * @param response Propose molecule response
 * @return Molecule status
 */
knishio_molecule_status_t knishio_response_propose_molecule_get_status(knishio_response_propose_molecule_t *response);

/**
 * @brief Get status as string
 * @param response Propose molecule response
 * @return Status string or NULL
 */
const char* knishio_response_propose_molecule_get_status_string(knishio_response_propose_molecule_t *response);

/**
 * @brief Check if molecule was accepted
 * @param response Propose molecule response
 * @return True if accepted, false otherwise
 */
bool knishio_response_propose_molecule_success(knishio_response_propose_molecule_t *response);

/**
 * @brief Get rejection reason
 * @param response Propose molecule response
 * @return Rejection reason or NULL if not rejected
 */
const char* knishio_response_propose_molecule_get_reason(knishio_response_propose_molecule_t *response);

/**
 * @brief Get parsed payload
 * @param response Propose molecule response
 * @return Payload JSON or NULL
 */
knishio_json_t* knishio_response_propose_molecule_get_payload(knishio_response_propose_molecule_t *response);

/**
 * @brief Get molecular hash from response
 * @param response Propose molecule response
 * @return Molecular hash or NULL if not available
 */
const char* knishio_response_propose_molecule_get_molecular_hash(knishio_response_propose_molecule_t *response);

/**
 * @brief Get creation timestamp
 * @param response Propose molecule response
 * @return Creation timestamp or NULL if not available
 */
const char* knishio_response_propose_molecule_get_created_at(knishio_response_propose_molecule_t *response);

/* Status checking functions */

/**
 * @brief Check if molecule is pending
 * @param response Propose molecule response
 * @return True if pending, false otherwise
 */
bool knishio_response_propose_molecule_is_pending(knishio_response_propose_molecule_t *response);

/**
 * @brief Check if molecule was rejected
 * @param response Propose molecule response
 * @return True if rejected, false otherwise
 */
bool knishio_response_propose_molecule_is_rejected(knishio_response_propose_molecule_t *response);

/**
 * @brief Check if molecule was accepted
 * @param response Propose molecule response
 * @return True if accepted, false otherwise
 */
bool knishio_response_propose_molecule_is_accepted(knishio_response_propose_molecule_t *response);

/* Payload extraction helpers */

/**
 * @brief Get payload string value
 * @param response Propose molecule response
 * @param key Payload key
 * @return String value or NULL if not found
 */
const char* knishio_response_propose_molecule_get_payload_string(knishio_response_propose_molecule_t *response,
                                                                 const char *key);

/**
 * @brief Get payload number value
 * @param response Propose molecule response
 * @param key Payload key
 * @param value Output number value
 * @return True if found and is number, false otherwise
 */
bool knishio_response_propose_molecule_get_payload_number(knishio_response_propose_molecule_t *response,
                                                          const char *key,
                                                          double *value);

/**
 * @brief Get payload boolean value
 * @param response Propose molecule response
 * @param key Payload key
 * @param value Output boolean value
 * @return True if found and is boolean, false otherwise
 */
bool knishio_response_propose_molecule_get_payload_bool(knishio_response_propose_molecule_t *response,
                                                        const char *key,
                                                        bool *value);

/**
 * @brief Get payload object value
 * @param response Propose molecule response
 * @param key Payload key
 * @return JSON object or NULL if not found
 */
knishio_json_t* knishio_response_propose_molecule_get_payload_object(knishio_response_propose_molecule_t *response,
                                                                     const char *key);

/* Utility functions */

/**
 * @brief Convert status enum to string
 * @param status Molecule status
 * @return Status string
 */
const char* knishio_molecule_status_to_string(knishio_molecule_status_t status);

/**
 * @brief Parse status string to enum
 * @param status_str Status string
 * @return Molecule status enum
 */
knishio_molecule_status_t knishio_molecule_status_from_string(const char *status_str);

/**
 * @brief Check if response has valid proposal data
 * @param response Propose molecule response
 * @return True if has valid proposal data, false otherwise
 */
bool knishio_response_propose_molecule_has_data(knishio_response_propose_molecule_t *response);

/**
 * @brief Parse payload from JSON string or object
 * @param payload_json Payload JSON (can be string or object)
 * @return Parsed JSON object or NULL on error
 */
knishio_json_t* knishio_response_propose_molecule_parse_payload(knishio_json_t *payload_json);

/* Conversion functions */

/**
 * @brief Convert to base response
 * @param propose_response Propose molecule response
 * @return Base response
 */
knishio_response_t* knishio_response_propose_molecule_to_base(knishio_response_propose_molecule_t *propose_response);

/**
 * @brief Convert from base response
 * @param base_response Base response
 * @return Propose molecule response or NULL if not a propose molecule response
 */
knishio_response_propose_molecule_t* knishio_response_propose_molecule_from_base(knishio_response_t *base_response);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_RESPONSE_PROPOSE_MOLECULE_H */