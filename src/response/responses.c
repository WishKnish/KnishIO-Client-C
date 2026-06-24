/**
 * @file responses.c
 * @brief Central response system implementation
 * 
 * Implements the complete response system with factory functions
 * and system initialization.
 */

#include "knishio/response/responses.h"
#include "knishio/response/response_types.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/logging.h"
#include <string.h>
#include <stdio.h>

/* Response system statistics */
static knishio_response_stats_t response_stats;
static bool stats_initialized = false;

/* Response key constants */
const char* const KNISHIO_RESPONSE_KEY_BALANCE = "data.Balance";
const char* const KNISHIO_RESPONSE_KEY_WALLET = "data.Wallet";
const char* const KNISHIO_RESPONSE_KEY_WALLET_BUNDLE = "data.WalletBundle";
const char* const KNISHIO_RESPONSE_KEY_PROPOSE_MOLECULE = "data.ProposeMolecule";
const char* const KNISHIO_RESPONSE_KEY_ATOM = "data.Atom";
const char* const KNISHIO_RESPONSE_KEY_ACTIVE_SESSION = "data.ActiveSession";
const char* const KNISHIO_RESPONSE_KEY_QUERY_ACTIVE_SESSION = "data.QueryActiveSession";
const char* const KNISHIO_RESPONSE_KEY_CONTINU_ID = "data.ContinuId";
const char* const KNISHIO_RESPONSE_KEY_META_TYPE = "data.MetaType";
const char* const KNISHIO_RESPONSE_KEY_META_TYPE_VIA_ATOM = "data.MetaTypeViaAtom";
const char* const KNISHIO_RESPONSE_KEY_CREATE_META = "data.CreateMeta";
const char* const KNISHIO_RESPONSE_KEY_META_BATCH = "data.MetaBatch";
const char* const KNISHIO_RESPONSE_KEY_CREATE_WALLET = "data.CreateWallet";
const char* const KNISHIO_RESPONSE_KEY_REQUEST_TOKENS = "data.RequestTokens";
const char* const KNISHIO_RESPONSE_KEY_REQUEST_AUTHORIZATION = "data.RequestAuthorization";
const char* const KNISHIO_RESPONSE_KEY_REQUEST_AUTHORIZATION_GUEST = "data.RequestAuthorizationGuest";
const char* const KNISHIO_RESPONSE_KEY_AUTHORIZATION_GUEST = "data.AuthorizationGuest";
const char* const KNISHIO_RESPONSE_KEY_CREATE_IDENTIFIER = "data.CreateIdentifier";
const char* const KNISHIO_RESPONSE_KEY_LINK_IDENTIFIER = "data.LinkIdentifier";
const char* const KNISHIO_RESPONSE_KEY_CLAIM_SHADOW_WALLET = "data.ClaimShadowWallet";
const char* const KNISHIO_RESPONSE_KEY_CREATE_RULE = "data.CreateRule";
const char* const KNISHIO_RESPONSE_KEY_POLICY = "data.Policy";

/**
 * @brief Initialize statistics
 */
static void init_stats(void) {
    if (!stats_initialized) {
        memset(&response_stats, 0, sizeof(response_stats));
        stats_initialized = true;
    }
}

/**
 * @brief Update statistics
 */
static void update_stats(knishio_response_type_t type, bool created) {
    init_stats();
    
    if (created) {
        response_stats.total_responses_created++;
        response_stats.active_responses++;
        if (type >= 0 && type < KNISHIO_RESPONSE_TYPE_COUNT) {
            response_stats.type_counts[type]++;
        }
    } else {
        response_stats.total_responses_freed++;
        if (response_stats.active_responses > 0) {
            response_stats.active_responses--;
        }
    }
}

/**
 * @brief Complete response system initialization
 */
bool knishio_responses_init(void) {
    init_stats();
    return knishio_response_types_init();
}

/**
 * @brief Complete response system cleanup
 */
void knishio_responses_cleanup(void) {
    knishio_response_types_cleanup();
    stats_initialized = false;
}

/* Factory functions for all response types */

/**
 * @brief Create balance response factory wrapper
 */
knishio_response_t* knishio_response_balance_factory(knishio_query_t *query, knishio_json_t *json) {
    knishio_response_balance_t *response = knishio_response_balance_create(query, json);
    if (response) {
        update_stats(KNISHIO_RESPONSE_TYPE_BALANCE, true);
        return knishio_response_balance_to_base(response);
    }
    
    response_stats.total_errors++;
    return NULL;
}

/**
 * @brief Create wallet list response factory wrapper
 */
knishio_response_t* knishio_response_wallet_list_factory(knishio_query_t *query, knishio_json_t *json) {
    // For now, return base response - full implementation would create specific wallet list response
    knishio_response_t *response = knishio_response_create(query, json, KNISHIO_RESPONSE_KEY_WALLET);
    if (response) {
        update_stats(KNISHIO_RESPONSE_TYPE_WALLET_LIST, true);
    } else {
        response_stats.total_errors++;
    }
    return response;
}

/**
 * @brief Create propose molecule response factory wrapper
 */
knishio_response_t* knishio_response_propose_molecule_factory(knishio_query_t *query, knishio_json_t *json) {
    // For now, return base response - full implementation would create specific propose molecule response
    knishio_response_t *response = knishio_response_create(query, json, KNISHIO_RESPONSE_KEY_PROPOSE_MOLECULE);
    if (response) {
        update_stats(KNISHIO_RESPONSE_TYPE_PROPOSE_MOLECULE, true);
    } else {
        response_stats.total_errors++;
    }
    return response;
}

/**
 * @brief Create atom response factory wrapper
 */
knishio_response_t* knishio_response_atom_factory(knishio_query_t *query, knishio_json_t *json) {
    // For now, return base response - full implementation would create specific atom response
    knishio_response_t *response = knishio_response_create(query, json, KNISHIO_RESPONSE_KEY_ATOM);
    if (response) {
        update_stats(KNISHIO_RESPONSE_TYPE_ATOM, true);
    } else {
        response_stats.total_errors++;
    }
    return response;
}

/**
 * @brief Create create token response factory wrapper
 */
knishio_response_t* knishio_response_create_token_factory(knishio_query_t *query, knishio_json_t *json) {
    // For now, return base response - full implementation would create specific create token response
    knishio_response_t *response = knishio_response_create(query, json, KNISHIO_RESPONSE_KEY_PROPOSE_MOLECULE);
    if (response) {
        update_stats(KNISHIO_RESPONSE_TYPE_CREATE_TOKEN, true);
    } else {
        response_stats.total_errors++;
    }
    return response;
}

/**
 * @brief Create transfer tokens response factory wrapper
 */
knishio_response_t* knishio_response_transfer_tokens_factory(knishio_query_t *query, knishio_json_t *json) {
    // For now, return base response - full implementation would create specific transfer tokens response
    knishio_response_t *response = knishio_response_create(query, json, KNISHIO_RESPONSE_KEY_PROPOSE_MOLECULE);
    if (response) {
        update_stats(KNISHIO_RESPONSE_TYPE_TRANSFER_TOKENS, true);
    } else {
        response_stats.total_errors++;
    }
    return response;
}

/**
 * @brief Create ContinuID response factory wrapper
 */
knishio_response_t* knishio_response_continu_id_factory(knishio_query_t *query, knishio_json_t *json) {
    // For now, return base response - full implementation would create specific ContinuID response
    knishio_response_t *response = knishio_response_create(query, json, KNISHIO_RESPONSE_KEY_CONTINU_ID);
    if (response) {
        update_stats(KNISHIO_RESPONSE_TYPE_CONTINU_ID, true);
    } else {
        response_stats.total_errors++;
    }
    return response;
}

/* Response validation and utility functions */

/**
 * @brief Validate any response type
 */
bool knishio_response_validate_any(void *response) {
    if (!response) {
        return false;
    }
    
    // Cast to base response for validation
    knishio_response_t *base_response = (knishio_response_t*)response;
    return knishio_response_validate(base_response);
}

/**
 * @brief Get response type name as string
 */
const char* knishio_response_get_type_name(void *response) {
    if (!response) {
        return "null";
    }
    
    knishio_response_t *base_response = (knishio_response_t*)response;
    knishio_response_type_t type = knishio_response_get_type(base_response);
    return knishio_response_type_name(type);
}

/**
 * @brief Check if response is of expected type
 */
bool knishio_response_check_type(void *response, knishio_response_type_t expected_type) {
    if (!response) {
        return false;
    }
    
    knishio_response_t *base_response = (knishio_response_t*)response;
    knishio_response_type_t actual_type = knishio_response_get_type(base_response);
    return actual_type == expected_type;
}

/**
 * @brief Get response debug information
 */
int knishio_response_debug_info(void *response, char *buffer, size_t buffer_size) {
    if (!response || !buffer || buffer_size == 0) {
        return -1;
    }
    
    knishio_response_t *base_response = (knishio_response_t*)response;
    const char *type_name = knishio_response_get_type_name(response);
    bool has_errors = knishio_response_has_errors(base_response);
    const char *error_msg = has_errors ? knishio_response_error_message(base_response) : "none";
    
    return snprintf(buffer, buffer_size,
        "Response Type: %s, Has Errors: %s, Error: %s",
        type_name,
        has_errors ? "yes" : "no",
        error_msg ? error_msg : "none"
    );
}

/**
 * @brief Log response information (placeholder implementation)
 */
void knishio_response_log(void *response, int level, const char *message) {
    // Placeholder implementation - in production, this would use the logging system
    (void)level; // Suppress unused parameter warning
    
    char debug_info[256];
    if (knishio_response_debug_info(response, debug_info, sizeof(debug_info)) > 0) {
        /* Route through the logging layer (disabled by default) instead of raw stdout. */
        KNISHIO_LOG_DEBUG("RESPONSE LOG [%s]: %s", message ? message : "info", debug_info);
    }
}

/* Response system statistics */

/**
 * @brief Get response system statistics
 */
bool knishio_response_get_stats(knishio_response_stats_t *stats) {
    if (!stats) {
        return false;
    }
    
    init_stats();
    memcpy(stats, &response_stats, sizeof(knishio_response_stats_t));
    return true;
}

/**
 * @brief Reset response system statistics
 */
void knishio_response_reset_stats(void) {
    init_stats();
    memset(&response_stats, 0, sizeof(knishio_response_stats_t));
}