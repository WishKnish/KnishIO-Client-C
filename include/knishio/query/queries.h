#ifndef KNISHIO_QUERIES_H
#define KNISHIO_QUERIES_H

/**
 * @file queries.h
 * @brief Complete GraphQL query operations for KnishIO C SDK
 * 
 * Implements all query operations from JavaScript SDK for full compatibility:
 * - QueryBalance - Balance queries with token filtering
 * - QueryWalletList - Multiple wallet queries
 * - QueryWalletBundle - Bundle-specific wallet info  
 * - QueryToken - Token metadata and information
 * - QueryAtom - Individual atom queries
 * - QueryBatch - Batch information queries
 * - QueryBatchHistory - Historical batch data
 * - QueryContinuId - Identity and ContinuID queries
 * - QueryMetaType - Metadata type queries
 * - QueryMetaTypeViaAtom - Meta queries via atom references
 * - QueryPolicy - Policy and rule queries
 * - QueryActiveSession - Active session information
 * - QueryUserActivity - User activity and history
 */

#include "knishio/graphql.h"
#include "knishio/response/responses.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_graphql_client knishio_graphql_client_t;

/* Query parameter structures */

/**
 * @brief Parameters for balance query
 * Matches JavaScript SDK QueryBalance
 */
typedef struct {
    const char* address;             /**< Wallet address */
    const char* bundle_hash;         /**< Bundle hash */
    const char* type;                /**< Wallet type */
    const char* token;               /**< Token slug */
    const char* position;            /**< Wallet position */
} knishio_query_balance_params_t;

/**
 * @brief Parameters for wallet list query
 * Matches JavaScript SDK QueryWalletList
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash */
    const char* token_slug;          /**< Token slug */
} knishio_query_wallet_list_params_t;

/**
 * @brief Parameters for wallet bundle query
 * Matches JavaScript SDK QueryWalletBundle
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash */
    const char* token_slug;          /**< Token slug */
    const char* slug;                /**< Wallet slug */
} knishio_query_wallet_bundle_params_t;

/**
 * @brief Parameters for token query
 * Matches JavaScript SDK QueryToken
 */
typedef struct {
    const char* slug;                /**< Single token slug */
    const char** slugs;              /**< Array of token slugs */
    size_t slug_count;               /**< Number of slugs */
    int limit;                       /**< Query limit */
    const char* order;               /**< Order parameter */
} knishio_query_token_params_t;

/**
 * @brief Parameters for atom query
 * Matches JavaScript SDK QueryAtom
 */
typedef struct {
    const char** molecular_hashes;   /**< Array of molecular hashes */
    size_t molecular_hash_count;     /**< Number of molecular hashes */
    const char** bundle_hashes;      /**< Array of bundle hashes */
    size_t bundle_hash_count;        /**< Number of bundle hashes */
    const char** positions;          /**< Array of positions */
    size_t position_count;           /**< Number of positions */
    const char** wallet_addresses;   /**< Array of wallet addresses */
    size_t wallet_address_count;     /**< Number of wallet addresses */
    const char** isotopes;           /**< Array of isotopes */
    size_t isotope_count;            /**< Number of isotopes */
    const char** token_slugs;        /**< Array of token slugs */
    size_t token_slug_count;         /**< Number of token slugs */
    const char** cell_slugs;         /**< Array of cell slugs */
    size_t cell_slug_count;          /**< Number of cell slugs */
    const char** batch_ids;          /**< Array of batch IDs */
    size_t batch_id_count;           /**< Number of batch IDs */
    const char** values;             /**< Array of values */
    size_t value_count;              /**< Number of values */
    const char** meta_types;         /**< Array of meta types */
    size_t meta_type_count;          /**< Number of meta types */
    const char** meta_ids;           /**< Array of meta IDs */
    size_t meta_id_count;            /**< Number of meta IDs */
    const char** indexes;            /**< Array of atom indexes */
    size_t index_count;              /**< Number of indexes */
    const char* filter;              /**< Filter object (JSON) */
    bool latest;                     /**< Latest flag */
    int limit;                       /**< Query limit (default: 15) */
    int offset;                      /**< Query offset (default: 1) */
} knishio_query_atom_params_t;

/**
 * @brief Parameters for batch query
 * Matches JavaScript SDK QueryBatch
 */
typedef struct {
    const char* batch_id;            /**< Batch ID */
    const char* molecule_hash;       /**< Molecular hash */
    const char* height;              /**< Block height */
    int limit;                       /**< Query limit */
} knishio_query_batch_params_t;

/**
 * @brief Parameters for batch history query
 * Matches JavaScript SDK QueryBatchHistory
 */
typedef struct {
    const char* batch_id;            /**< Batch ID */
    const char* molecule_hash;       /**< Molecular hash */
    const char* from_date;           /**< From date */
    const char* to_date;             /**< To date */
    int limit;                       /**< Query limit */
} knishio_query_batch_history_params_t;

/**
 * @brief Parameters for ContinuID query
 * Matches JavaScript SDK QueryContinuId
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash */
    const char* type;                /**< Identity type */
    const char* key;                 /**< Identity key */
    const char* value;               /**< Identity value */
} knishio_query_continuid_params_t;

/**
 * @brief Parameters for meta type query
 * Matches JavaScript SDK QueryMetaType
 */
typedef struct {
    const char* meta_type;           /**< Meta type */
    const char* meta_id;             /**< Meta ID */
    const char* key;                 /**< Meta key */
    const char* value;               /**< Meta value */
    int limit;                       /**< Query limit */
} knishio_query_meta_type_params_t;

/**
 * @brief Parameters for meta type via atom query
 * Matches JavaScript SDK QueryMetaTypeViaAtom
 */
typedef struct {
    const char* meta_type;           /**< Meta type */
    const char* meta_id;             /**< Meta ID */
    const char* molecular_hash;      /**< Molecular hash */
    const char* position;            /**< Position */
    int limit;                       /**< Query limit */
} knishio_query_meta_type_via_atom_params_t;

/**
 * @brief Parameters for policy query
 * Matches JavaScript SDK QueryPolicy
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash */
    const char* policy_slug;         /**< Policy slug */
    const char* instance_hash;       /**< Instance hash */
    bool active_only;                /**< Active policies only */
} knishio_query_policy_params_t;

/**
 * @brief Parameters for active session query
 * Matches JavaScript SDK QueryActiveSession
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash */
    const char* meta_type;           /**< Meta type */
    const char* meta_id;             /**< Meta ID */
} knishio_query_active_session_params_t;

/**
 * @brief Parameters for user activity query
 * Matches JavaScript SDK QueryUserActivity
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash */
    const char* meta_type;           /**< Meta type */
    const char* meta_id;             /**< Meta ID */
    const char* ip_address;          /**< IP address */
    const char* browser;             /**< Browser */
    const char* os_cpu;              /**< OS and CPU */
    const char* resolution;          /**< Screen resolution */
    const char* time_zone;           /**< Time zone */
    const char* count_by;            /**< Count by parameter */
    const char* interval;            /**< Interval parameter */
} knishio_query_user_activity_params_t;

/* Query execution functions */

/**
 * @brief Query balance information
 * Equivalent to JavaScript: client.queryBalance({ ... })
 */
knishio_error_t knishio_query_balance(
    knishio_graphql_client_t* client,
    const knishio_query_balance_params_t* params,
    knishio_response_balance_t** response
);

/**
 * @brief Query wallet list
 * Equivalent to JavaScript: client.queryWalletList({ ... })
 */
knishio_error_t knishio_query_wallet_list(
    knishio_graphql_client_t* client,
    const knishio_query_wallet_list_params_t* params,
    knishio_response_wallet_list_t** response
);

/**
 * @brief Query wallet bundle
 * Equivalent to JavaScript: client.queryWalletBundle({ ... })
 */
knishio_error_t knishio_query_wallet_bundle(
    knishio_graphql_client_t* client,
    const knishio_query_wallet_bundle_params_t* params,
    knishio_response_wallet_bundle_t** response
);

/**
 * @brief Query token information
 * Equivalent to JavaScript: client.queryToken({ ... })
 */
knishio_error_t knishio_query_token(
    knishio_graphql_client_t* client,
    const knishio_query_token_params_t* params,
    knishio_response_token_t** response
);

/**
 * @brief Query atom information
 * Equivalent to JavaScript: client.queryAtom({ ... })
 */
knishio_error_t knishio_query_atom(
    knishio_graphql_client_t* client,
    const knishio_query_atom_params_t* params,
    knishio_response_atom_t** response
);

/**
 * @brief Query batch information
 * Equivalent to JavaScript: client.queryBatch({ ... })
 */
knishio_error_t knishio_query_batch(
    knishio_graphql_client_t* client,
    const knishio_query_batch_params_t* params,
    knishio_response_batch_t** response
);

/**
 * @brief Query batch history
 * Equivalent to JavaScript: client.queryBatchHistory({ ... })
 */
knishio_error_t knishio_query_batch_history(
    knishio_graphql_client_t* client,
    const knishio_query_batch_history_params_t* params,
    knishio_response_batch_history_t** response
);

/**
 * @brief Query ContinuID information
 * Equivalent to JavaScript: client.queryContinuId({ ... })
 */
knishio_error_t knishio_query_continuid(
    knishio_graphql_client_t* client,
    const knishio_query_continuid_params_t* params,
    knishio_response_continu_id_t** response
);

/**
 * @brief Query meta type information
 * Equivalent to JavaScript: client.queryMetaType({ ... })
 */
knishio_error_t knishio_query_meta_type(
    knishio_graphql_client_t* client,
    const knishio_query_meta_type_params_t* params,
    knishio_response_meta_type_t** response
);

/**
 * @brief Query meta type via atom
 * Equivalent to JavaScript: client.queryMetaTypeViaAtom({ ... })
 */
knishio_error_t knishio_query_meta_type_via_atom(
    knishio_graphql_client_t* client,
    const knishio_query_meta_type_via_atom_params_t* params,
    knishio_response_meta_type_t** response
);

/**
 * @brief Query policy information
 * Equivalent to JavaScript: client.queryPolicy({ ... })
 */
knishio_error_t knishio_query_policy(
    knishio_graphql_client_t* client,
    const knishio_query_policy_params_t* params,
    knishio_response_policy_t** response
);

/**
 * @brief Query active session information
 * Equivalent to JavaScript: client.queryActiveSession({ ... })
 */
knishio_error_t knishio_query_active_session(
    knishio_graphql_client_t* client,
    const knishio_query_active_session_params_t* params,
    knishio_response_active_session_t** response
);

/**
 * @brief Query user activity
 * Equivalent to JavaScript: client.queryUserActivity({ ... })
 */
knishio_error_t knishio_query_user_activity(
    knishio_graphql_client_t* client,
    const knishio_query_user_activity_params_t* params,
    knishio_response_query_user_activity_t** response
);

/* Utility functions for building queries */

/**
 * @brief Build balance query GraphQL string
 */
knishio_error_t knishio_build_balance_query(
    const knishio_query_balance_params_t* params,
    char** query_string,
    char** variables_json
);

/**
 * @brief Build wallet list query GraphQL string
 */
knishio_error_t knishio_build_wallet_list_query(
    const knishio_query_wallet_list_params_t* params,
    char** query_string,
    char** variables_json
);

/**
 * @brief Build token query GraphQL string
 */
knishio_error_t knishio_build_token_query(
    const knishio_query_token_params_t* params,
    char** query_string,
    char** variables_json
);

/**
 * @brief Build atom query GraphQL string
 */
knishio_error_t knishio_build_atom_query(
    const knishio_query_atom_params_t* params,
    char** query_string,
    char** variables_json
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_QUERIES_H */