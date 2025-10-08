/**
 * @file response_types.c
 * @brief Response type registry implementation
 * 
 * Implements the response type system for automatic response creation
 * and type identification.
 */

#include "knishio/response/response_types.h"
#include "knishio/response/responses.h"
#include "knishio/utils/memory.h"
#include "knishio/utils/string.h"
#include <string.h>
#include <stdlib.h>

/* Response data key constants */
const char* const KNISHIO_DATA_KEY_BALANCE = "data.Balance";
const char* const KNISHIO_DATA_KEY_WALLET = "data.Wallet";
const char* const KNISHIO_DATA_KEY_WALLET_BUNDLE = "data.WalletBundle";
const char* const KNISHIO_DATA_KEY_PROPOSE_MOLECULE = "data.ProposeMolecule";
const char* const KNISHIO_DATA_KEY_ATOM = "data.Atom";
const char* const KNISHIO_DATA_KEY_ACTIVE_SESSION = "data.ActiveSession";
const char* const KNISHIO_DATA_KEY_CONTINU_ID = "data.ContinuId";
const char* const KNISHIO_DATA_KEY_META_TYPE = "data.MetaType";
const char* const KNISHIO_DATA_KEY_CREATE_META = "data.CreateMeta";
const char* const KNISHIO_DATA_KEY_REQUEST_TOKENS = "data.RequestTokens";
const char* const KNISHIO_DATA_KEY_CREATE_IDENTIFIER = "data.CreateIdentifier";
const char* const KNISHIO_DATA_KEY_POLICY = "data.Policy";

/* Response type registry */
static knishio_response_type_info_t response_registry[KNISHIO_RESPONSE_TYPE_COUNT];
static bool registry_initialized = false;

/* Type names */
static const char* response_type_names[KNISHIO_RESPONSE_TYPE_COUNT] = {
    "Base",
    "Balance",
    "WalletList",
    "WalletBundle",
    "ProposeMolecule",
    "Atom",
    "CreateToken",
    "TransferTokens",
    "ActiveSession",
    "QueryActiveSession",
    "ContinuId",
    "MetaType",
    "MetaTypeViaAtom",
    "CreateMeta",
    "MetaBatch",
    "CreateWallet",
    "RequestTokens",
    "RequestAuthorization",
    "RequestAuthorizationGuest",
    "AuthorizationGuest",
    "CreateIdentifier",
    "LinkIdentifier",
    "ClaimShadowWallet",
    "CreateRule",
    "Policy",
    "QueryUserActivity"
};

/**
 * @brief Get response type from response object
 */
knishio_response_type_t knishio_response_get_type(knishio_response_t *response) {
    // For now, we'll implement basic type detection
    // In a full implementation, we'd store the type in the response structure
    (void)response;
    return KNISHIO_RESPONSE_TYPE_BASE;
}

/**
 * @brief Get response type name
 */
const char* knishio_response_type_name(knishio_response_type_t type) {
    if (type >= 0 && type < KNISHIO_RESPONSE_TYPE_COUNT) {
        return response_type_names[type];
    }
    return "Unknown";
}

/**
 * @brief Get response type by name
 */
knishio_response_type_t knishio_response_type_from_name(const char *name) {
    if (!name) {
        return KNISHIO_RESPONSE_TYPE_BASE;
    }

    for (int i = 0; i < KNISHIO_RESPONSE_TYPE_COUNT; i++) {
        if (strcmp(response_type_names[i], name) == 0) {
            return (knishio_response_type_t)i;
        }
    }

    return KNISHIO_RESPONSE_TYPE_BASE;
}

/**
 * @brief Register response factory
 */
bool knishio_response_register_type(knishio_response_type_t type,
                                    const char *name,
                                    const char *data_key,
                                    knishio_response_factory_fn factory) {
    if (type < 0 || type >= KNISHIO_RESPONSE_TYPE_COUNT) {
        return false;
    }

    knishio_response_type_info_t *info = &response_registry[type];
    info->type = type;
    info->name = name;
    info->data_key = data_key;
    info->factory = factory;

    return true;
}

/**
 * @brief Create response using factory
 */
knishio_response_t* knishio_response_create_by_type(knishio_response_type_t type,
                                                    knishio_query_t *query,
                                                    knishio_json_t *json) {
    if (type < 0 || type >= KNISHIO_RESPONSE_TYPE_COUNT) {
        return NULL;
    }

    knishio_response_type_info_t *info = &response_registry[type];
    if (info->factory) {
        return info->factory(query, json);
    }

    // Fallback to base response
    return knishio_response_create(query, json, info->data_key);
}

/**
 * @brief Create response by name
 */
knishio_response_t* knishio_response_create_by_name(const char *type_name,
                                                    knishio_query_t *query,
                                                    knishio_json_t *json) {
    knishio_response_type_t type = knishio_response_type_from_name(type_name);
    return knishio_response_create_by_type(type, query, json);
}

/**
 * @brief Auto-detect response type from JSON structure
 */
knishio_response_type_t knishio_response_detect_type(knishio_json_t *json) {
    if (!json) {
        return KNISHIO_RESPONSE_TYPE_BASE;
    }

    // Check for GraphQL data structure patterns
    knishio_json_t *data = knishio_json_object_get(json, "data");
    if (!data) {
        return KNISHIO_RESPONSE_TYPE_BASE;
    }

    // Check for specific response types by their data keys
    if (knishio_json_object_has(data, "Balance")) {
        return KNISHIO_RESPONSE_TYPE_BALANCE;
    }
    if (knishio_json_object_has(data, "Wallet")) {
        return KNISHIO_RESPONSE_TYPE_WALLET_LIST;
    }
    if (knishio_json_object_has(data, "WalletBundle")) {
        return KNISHIO_RESPONSE_TYPE_WALLET_BUNDLE;
    }
    if (knishio_json_object_has(data, "ProposeMolecule")) {
        return KNISHIO_RESPONSE_TYPE_PROPOSE_MOLECULE;
    }
    if (knishio_json_object_has(data, "Atom")) {
        return KNISHIO_RESPONSE_TYPE_ATOM;
    }
    if (knishio_json_object_has(data, "ActiveSession")) {
        return KNISHIO_RESPONSE_TYPE_ACTIVE_SESSION;
    }
    if (knishio_json_object_has(data, "ContinuId")) {
        return KNISHIO_RESPONSE_TYPE_CONTINU_ID;
    }
    if (knishio_json_object_has(data, "MetaType")) {
        return KNISHIO_RESPONSE_TYPE_META_TYPE;
    }
    if (knishio_json_object_has(data, "CreateMeta")) {
        return KNISHIO_RESPONSE_TYPE_CREATE_META;
    }
    if (knishio_json_object_has(data, "RequestTokens")) {
        return KNISHIO_RESPONSE_TYPE_REQUEST_TOKENS;
    }
    if (knishio_json_object_has(data, "CreateIdentifier")) {
        return KNISHIO_RESPONSE_TYPE_CREATE_IDENTIFIER;
    }
    if (knishio_json_object_has(data, "Policy")) {
        return KNISHIO_RESPONSE_TYPE_POLICY;
    }

    return KNISHIO_RESPONSE_TYPE_BASE;
}

/**
 * @brief Create response with auto-detection
 */
knishio_response_t* knishio_response_create_auto(knishio_query_t *query,
                                                 knishio_json_t *json) {
    knishio_response_type_t type = knishio_response_detect_type(json);
    return knishio_response_create_by_type(type, query, json);
}

/**
 * @brief Initialize response type system
 */
bool knishio_response_types_init(void) {
    if (registry_initialized) {
        return true;
    }

    // Initialize all registry entries
    memset(response_registry, 0, sizeof(response_registry));

    // Register built-in response types
    knishio_response_register_type(KNISHIO_RESPONSE_TYPE_BASE,
                                   "Base", NULL, NULL);
    
    knishio_response_register_type(KNISHIO_RESPONSE_TYPE_BALANCE,
                                   "Balance", KNISHIO_DATA_KEY_BALANCE,
                                   knishio_response_balance_factory);
    
    knishio_response_register_type(KNISHIO_RESPONSE_TYPE_WALLET_LIST,
                                   "WalletList", KNISHIO_DATA_KEY_WALLET,
                                   knishio_response_wallet_list_factory);
    
    knishio_response_register_type(KNISHIO_RESPONSE_TYPE_PROPOSE_MOLECULE,
                                   "ProposeMolecule", KNISHIO_DATA_KEY_PROPOSE_MOLECULE,
                                   knishio_response_propose_molecule_factory);
    
    knishio_response_register_type(KNISHIO_RESPONSE_TYPE_ATOM,
                                   "Atom", KNISHIO_DATA_KEY_ATOM,
                                   knishio_response_atom_factory);
    
    knishio_response_register_type(KNISHIO_RESPONSE_TYPE_CREATE_TOKEN,
                                   "CreateToken", KNISHIO_DATA_KEY_PROPOSE_MOLECULE,
                                   knishio_response_create_token_factory);
    
    knishio_response_register_type(KNISHIO_RESPONSE_TYPE_TRANSFER_TOKENS,
                                   "TransferTokens", KNISHIO_DATA_KEY_PROPOSE_MOLECULE,
                                   knishio_response_transfer_tokens_factory);
    
    knishio_response_register_type(KNISHIO_RESPONSE_TYPE_CONTINU_ID,
                                   "ContinuId", KNISHIO_DATA_KEY_CONTINU_ID,
                                   knishio_response_continu_id_factory);

    registry_initialized = true;
    return true;
}

/**
 * @brief Cleanup response type system
 */
void knishio_response_types_cleanup(void) {
    if (!registry_initialized) {
        return;
    }

    // Clear registry
    memset(response_registry, 0, sizeof(response_registry));
    registry_initialized = false;
}