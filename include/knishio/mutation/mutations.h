#ifndef KNISHIO_MUTATIONS_H
#define KNISHIO_MUTATIONS_H

/**
 * @file mutations.h
 * @brief Complete GraphQL mutation operations for KnishIO C SDK
 * 
 * Implements all mutation operations from JavaScript SDK for full compatibility:
 * - MutationProposeMolecule - Core molecule proposals
 * - MutationCreateToken - Token creation operations
 * - MutationTransferTokens - Token transfer operations
 * - MutationRequestTokens - Token request operations
 * - MutationCreateWallet - Wallet creation
 * - MutationClaimShadowWallet - Shadow wallet claiming
 * - MutationCreateMeta - Metadata creation
 * - MutationCreateIdentifier - Identifier creation
 * - MutationLinkIdentifier - Identifier linking
 * - MutationCreateRule - Rule and policy creation
 * - MutationRequestAuthorization - Profile auth
 * - MutationRequestAuthorizationGuest - Guest auth
 * - MutationActiveSession - Session management
 * - MutationDepositBufferToken - Buffer token deposits
 * - MutationWithdrawBufferToken - Buffer token withdrawals
 */

#include "knishio/graphql.h"
#include "knishio/response/responses.h"
#include "knishio/molecule.h"
#include "knishio/wallet.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct knishio_graphql_client knishio_graphql_client_t;
typedef struct knishio_molecule knishio_molecule_t;
typedef struct knishio_wallet knishio_wallet_t;

/* Mutation parameter structures */

/**
 * @brief Parameters for molecule proposal
 * Matches JavaScript SDK MutationProposeMolecule
 */
typedef struct {
    knishio_molecule_t* molecule;    /**< Molecule to propose */
} knishio_mutation_propose_molecule_params_t;

/**
 * @brief Parameters for token creation
 * Matches JavaScript SDK MutationCreateToken
 */
typedef struct {
    knishio_wallet_t* recipient_wallet; /**< Recipient wallet */
    const char* token_slug;          /**< Token identifier */
    const char* amount;              /**< Token amount */
    const char* meta_type;           /**< Metadata type */
    const char* meta_id;             /**< Metadata ID */
    const char* meta_json;           /**< Additional metadata (JSON) */
    const char* fungibility;         /**< Token fungibility */
    const char* supply;              /**< Token supply */
    const char* decimals;            /**< Token decimals */
    const char* icon;                /**< Token icon URL */
} knishio_mutation_create_token_params_t;

/**
 * @brief Parameters for token transfer
 * Matches JavaScript SDK MutationTransferTokens
 */
typedef struct {
    knishio_wallet_t* source_wallet; /**< Source wallet */
    knishio_wallet_t* recipient_wallet; /**< Recipient wallet */
    const char* token_slug;          /**< Token to transfer */
    const char* amount;              /**< Amount to transfer */
    const char* meta_type;           /**< Metadata type */
    const char* meta_id;             /**< Metadata ID */
    const char* meta_json;           /**< Additional metadata (JSON) */
} knishio_mutation_transfer_tokens_params_t;

/**
 * @brief Parameters for token request
 * Matches JavaScript SDK MutationRequestTokens
 */
typedef struct {
    knishio_wallet_t* wallet;        /**< Requesting wallet */
    const char* token_slug;          /**< Token to request */
    const char* amount;              /**< Amount to request */
    const char* meta_type;           /**< Metadata type */
    const char* meta_id;             /**< Metadata ID */
    const char* meta_json;           /**< Additional metadata (JSON) */
    const char* batch_id;            /**< Batch ID (optional) */
} knishio_mutation_request_tokens_params_t;

/**
 * @brief Parameters for wallet creation
 * Matches JavaScript SDK MutationCreateWallet
 */
typedef struct {
    const char* bundle_hash;         /**< Bundle hash */
    const char* token_slug;          /**< Token for wallet */
    const char* batchId;             /**< Batch ID (optional) */
} knishio_mutation_create_wallet_params_t;

/**
 * @brief Parameters for shadow wallet claiming
 * Matches JavaScript SDK MutationClaimShadowWallet
 */
typedef struct {
    knishio_wallet_t* wallet;        /**< Wallet to claim */
    const char* shadow_hash;         /**< Shadow wallet hash */
    const char* meta_type;           /**< Metadata type */
    const char* meta_id;             /**< Metadata ID */
    const char* meta_json;           /**< Additional metadata (JSON) */
} knishio_mutation_claim_shadow_wallet_params_t;

/**
 * @brief Parameters for metadata creation
 * Matches JavaScript SDK MutationCreateMeta
 */
typedef struct {
    knishio_wallet_t* wallet;        /**< Source wallet */
    const char* meta_type;           /**< Metadata type */
    const char* meta_id;             /**< Metadata ID */
    const char* meta_json;           /**< Metadata content (JSON) */
} knishio_mutation_create_meta_params_t;

/**
 * @brief Parameters for identifier creation
 * Matches JavaScript SDK MutationCreateIdentifier
 */
typedef struct {
    knishio_wallet_t* wallet;        /**< Source wallet */
    const char* type;                /**< Identifier type */
    const char* contact;             /**< Contact information */
    const char* code;                /**< Verification code */
    const char* meta_json;           /**< Additional metadata (JSON) */
} knishio_mutation_create_identifier_params_t;

/**
 * @brief Parameters for identifier linking
 * Matches JavaScript SDK MutationLinkIdentifier
 */
typedef struct {
    knishio_wallet_t* wallet;        /**< Source wallet */
    const char* type;                /**< Identifier type */
    const char* provider;            /**< Provider name */
    const char* code;                /**< Link code */
    const char* meta_json;           /**< Additional metadata (JSON) */
} knishio_mutation_link_identifier_params_t;

/**
 * @brief Parameters for rule creation
 * Matches JavaScript SDK MutationCreateRule
 */
typedef struct {
    knishio_wallet_t* wallet;        /**< Source wallet */
    const char* policy_slug;         /**< Policy identifier */
    const char* rule_type;           /**< Rule type */
    const char* rule_value;          /**< Rule value */
    const char* meta_json;           /**< Additional metadata (JSON) */
} knishio_mutation_create_rule_params_t;

/**
 * @brief Parameters for authorization request
 * Matches JavaScript SDK MutationRequestAuthorization
 */
typedef struct {
    const char* cell_slug;           /**< Cell identifier */
    const char* encrypted_payload;   /**< Encrypted auth payload */
} knishio_mutation_request_authorization_params_t;

/**
 * @brief Parameters for guest authorization request
 * Matches JavaScript SDK MutationRequestAuthorizationGuest
 */
typedef struct {
    const char* cell_slug;           /**< Cell identifier */
    const char* encrypted_payload;   /**< Encrypted auth payload */
} knishio_mutation_request_authorization_guest_params_t;

/**
 * @brief Parameters for active session management
 * Matches JavaScript SDK MutationActiveSession
 */
typedef struct {
    knishio_wallet_t* wallet;        /**< Session wallet */
    const char* meta_type;           /**< Session metadata type */
    const char* meta_id;             /**< Session metadata ID */
    const char* ip_address;          /**< Client IP address */
    const char* browser;             /**< Browser information */
    const char* os_cpu;              /**< OS and CPU information */
    const char* resolution;          /**< Screen resolution */
    const char* time_zone;           /**< Client timezone */
    const char* meta_json;           /**< Additional session data (JSON) */
} knishio_mutation_active_session_params_t;

/**
 * @brief Parameters for buffer token deposit
 * Matches JavaScript SDK MutationDepositBufferToken
 */
typedef struct {
    knishio_wallet_t* source_wallet; /**< Source wallet */
    const char* token_slug;          /**< Token to deposit */
    const char* amount;              /**< Amount to deposit */
    const char* buffer_slug;         /**< Buffer identifier */
    const char* meta_json;           /**< Additional metadata (JSON) */
} knishio_mutation_deposit_buffer_token_params_t;

/**
 * @brief Parameters for buffer token withdrawal
 * Matches JavaScript SDK MutationWithdrawBufferToken
 */
typedef struct {
    knishio_wallet_t* wallet;        /**< Withdrawal wallet */
    const char* token_slug;          /**< Token to withdraw */
    const char* amount;              /**< Amount to withdraw */
    const char* buffer_slug;         /**< Buffer identifier */
    const char* meta_json;           /**< Additional metadata (JSON) */
} knishio_mutation_withdraw_buffer_token_params_t;

/* Mutation execution functions */

/**
 * @brief Propose molecule to network
 * Equivalent to JavaScript: client.proposeMolecule(molecule)
 */
knishio_error_t knishio_mutation_propose_molecule(
    knishio_graphql_client_t* client,
    const knishio_mutation_propose_molecule_params_t* params,
    knishio_response_propose_molecule_t** response
);

/**
 * @brief Create new token
 * Equivalent to JavaScript: client.createToken({ ... })
 */
knishio_error_t knishio_mutation_create_token(
    knishio_graphql_client_t* client,
    const knishio_mutation_create_token_params_t* params,
    knishio_response_create_token_t** response
);

/**
 * @brief Transfer tokens between wallets
 * Equivalent to JavaScript: client.transferTokens({ ... })
 */
knishio_error_t knishio_mutation_transfer_tokens(
    knishio_graphql_client_t* client,
    const knishio_mutation_transfer_tokens_params_t* params,
    knishio_response_transfer_tokens_t** response
);

/**
 * @brief Request tokens from faucet or issuer
 * Equivalent to JavaScript: client.requestTokens({ ... })
 */
knishio_error_t knishio_mutation_request_tokens(
    knishio_graphql_client_t* client,
    const knishio_mutation_request_tokens_params_t* params,
    knishio_response_request_tokens_t** response
);

/**
 * @brief Create new wallet
 * Equivalent to JavaScript: client.createWallet({ ... })
 */
knishio_error_t knishio_mutation_create_wallet(
    knishio_graphql_client_t* client,
    const knishio_mutation_create_wallet_params_t* params,
    knishio_response_create_wallet_t** response
);

/**
 * @brief Claim shadow wallet
 * Equivalent to JavaScript: client.claimShadowWallet({ ... })
 */
knishio_error_t knishio_mutation_claim_shadow_wallet(
    knishio_graphql_client_t* client,
    const knishio_mutation_claim_shadow_wallet_params_t* params,
    knishio_response_claim_shadow_wallet_t** response
);

/**
 * @brief Create metadata entry
 * Equivalent to JavaScript: client.createMeta({ ... })
 */
knishio_error_t knishio_mutation_create_meta(
    knishio_graphql_client_t* client,
    const knishio_mutation_create_meta_params_t* params,
    knishio_response_create_meta_t** response
);

/**
 * @brief Create identifier
 * Equivalent to JavaScript: client.createIdentifier({ ... })
 */
knishio_error_t knishio_mutation_create_identifier(
    knishio_graphql_client_t* client,
    const knishio_mutation_create_identifier_params_t* params,
    knishio_response_create_identifier_t** response
);

/**
 * @brief Link identifier
 * Equivalent to JavaScript: client.linkIdentifier({ ... })
 */
knishio_error_t knishio_mutation_link_identifier(
    knishio_graphql_client_t* client,
    const knishio_mutation_link_identifier_params_t* params,
    knishio_response_link_identifier_t** response
);

/**
 * @brief Create rule
 * Equivalent to JavaScript: client.createRule({ ... })
 */
knishio_error_t knishio_mutation_create_rule(
    knishio_graphql_client_t* client,
    const knishio_mutation_create_rule_params_t* params,
    knishio_response_create_rule_t** response
);

/**
 * @brief Request authorization
 * Equivalent to JavaScript: client.requestAuthorization({ ... })
 */
knishio_error_t knishio_mutation_request_authorization(
    knishio_graphql_client_t* client,
    const knishio_mutation_request_authorization_params_t* params,
    knishio_response_request_authorization_t** response
);

/**
 * @brief Request guest authorization
 * Equivalent to JavaScript: client.requestAuthorizationGuest({ ... })
 */
knishio_error_t knishio_mutation_request_authorization_guest(
    knishio_graphql_client_t* client,
    const knishio_mutation_request_authorization_guest_params_t* params,
    knishio_response_request_authorization_guest_t** response
);

/**
 * @brief Manage active session
 * Equivalent to JavaScript: client.activeSession({ ... })
 */
knishio_error_t knishio_mutation_active_session(
    knishio_graphql_client_t* client,
    const knishio_mutation_active_session_params_t* params,
    knishio_response_active_session_t** response
);

/**
 * @brief Deposit buffer tokens
 * Equivalent to JavaScript: client.depositBufferToken({ ... })
 */
knishio_error_t knishio_mutation_deposit_buffer_token(
    knishio_graphql_client_t* client,
    const knishio_mutation_deposit_buffer_token_params_t* params,
    knishio_response_deposit_buffer_token_t** response
);

/**
 * @brief Withdraw buffer tokens
 * Equivalent to JavaScript: client.withdrawBufferToken({ ... })
 */
knishio_error_t knishio_mutation_withdraw_buffer_token(
    knishio_graphql_client_t* client,
    const knishio_mutation_withdraw_buffer_token_params_t* params,
    knishio_response_withdraw_buffer_token_t** response
);

/* Utility functions for building mutations */

/**
 * @brief Build propose molecule mutation GraphQL string
 */
knishio_error_t knishio_build_propose_molecule_mutation(
    const knishio_mutation_propose_molecule_params_t* params,
    char** mutation_string,
    char** variables_json
);

/**
 * @brief Build create token mutation GraphQL string
 */
knishio_error_t knishio_build_create_token_mutation(
    const knishio_mutation_create_token_params_t* params,
    char** mutation_string,
    char** variables_json
);

/**
 * @brief Build transfer tokens mutation GraphQL string
 */
knishio_error_t knishio_build_transfer_tokens_mutation(
    const knishio_mutation_transfer_tokens_params_t* params,
    char** mutation_string,
    char** variables_json
);

/**
 * @brief Build request tokens mutation GraphQL string
 */
knishio_error_t knishio_build_request_tokens_mutation(
    const knishio_mutation_request_tokens_params_t* params,
    char** mutation_string,
    char** variables_json
);

#ifdef __cplusplus
}
#endif

#endif /* KNISHIO_MUTATIONS_H */