<div style="text-align:center">
  <img src="https://raw.githubusercontent.com/WishKnish/KnishIO-Technical-Whitepaper/master/KnishIO-Logo.png" alt="Knish.IO: Post-Blockchain Platform" />
</div>
<div style="text-align:center">info@wishknish.com | https://wishknish.com</div>

# Knish.IO C Client SDK

This is the official C implementation of the Knish.IO client SDK. Its purpose is to expose libraries for building and signing Knish.IO Molecules, composing Atoms, generating Wallets, and much more with native performance and quantum-resistant security.

## Installation

The SDK can be built using CMake with the following steps:

### Prerequisites

Ensure you have the following dependencies installed:

- **CMake** 3.16 or higher
- **C17 compatible compiler** (GCC 9+, Clang 10+, or MSVC 2019+)
- **liboqs** for quantum-resistant cryptography
- **libwebsockets** for real-time subscriptions
- **libcurl** for HTTP/GraphQL communication
- **cjson** for JSON parsing and generation
- **GMP** for arbitrary precision arithmetic

### Platform-Specific Installation

#### macOS (Homebrew)
```bash
brew install cmake liboqs libwebsockets curl cjson gmp
```

#### Ubuntu/Debian
```bash
sudo apt-get install cmake liboqs-dev libwebsockets-dev libcurl4-openssl-dev libcjson-dev libgmp-dev
```

#### Build Instructions
1. Clone and build the SDK:
   ```bash
   git clone https://github.com/WishKnish/KnishIO-Client-C.git
   cd KnishIO-Client-C
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

2. Install system-wide (optional):
   ```bash
   sudo make install
   ```

3. Link to your project:
   ```bash
   # Static linking (recommended - tested working)
   gcc -std=c17 -I./include $(pkg-config --cflags libcjson liboqs libwebsockets) \
       -o myapp myapp.c -L. -L/opt/homebrew/lib -lknishio-client-static \
       $(pkg-config --libs libcjson liboqs libwebsockets) \
       -lcurl -lgmp -lssl -lcrypto -lz -lpthread

   # Dynamic linking (if shared library is available)
   gcc -o myapp myapp.c -lknishio-client -Wl,-rpath,/usr/local/lib
   ```

(**Note:** The SDK requires C17 standard features and uses modern security practices including stack protection and buffer overflow detection.)

## Basic Usage

The purpose of the Knish.IO SDK is to expose various ledger functions to new or existing applications.

There are two ways to take advantage of these functions:

1. The easy way: use the `knishio_client_*` wrapper functions

2. The granular way: build `knishio_atom_t` and `knishio_molecule_t` instances and broadcast GraphQL messages yourself

This document will explain both ways.

## The Easy Way: KnishIO Client Wrapper

1. Include the SDK in your C application:
   ```c
   #include "knishio/knishio.h"
   #include "knishio/client_auth.h"    // For authentication functions
   #include "knishio/fingerprint.h"    // For fingerprint functions (if needed)
   ```

2. Initialize the client with your node URI:
   ```c
   knishio_client_t* client = NULL;
   knishio_client_config_t config = {
       .uri = "https://api.knishio.dev",
       .cell_slug = "my_cell_slug",     // Optional
       .server_sdk_version = 4,         // Optional, defaults to 4
       .logging = false                 // Optional, enables logging
   };
   
   knishio_error_t result = knishio_client_create(&client, &config);
   if (result != KNISHIO_SUCCESS) {
       fprintf(stderr, "Failed to create client: %d\n", result);
       return -1;
   }
   ```

3. Store secret for authentication (C SDK uses different auth model than JS):
   ```c
   // C SDK architectural difference: No async requestAuthToken equivalent
   // Instead, secret is stored and used for subsequent operations
   result = knishio_client_set_secret(client, "myTopSecretCode");
   if (result != KNISHIO_SUCCESS) {
       fprintf(stderr, "Failed to set secret: %d\n", result);
       knishio_client_destroy(client);
       return -1;
   }
   
   // Note: C SDK uses stored secret for all cryptographic operations
   // No separate token request like JavaScript SDK's await client.requestAuthToken()
   ```

   (**Note:** The `secret` parameter can be a salted combination of username + password, a biometric hash, an existing user identifier from an external authentication process, for example)

4. Begin using `client` to trigger commands described below...

### KnishIO Client Functions

- Query metadata for a **Wallet Bundle**. Omit the `bundle_hash` parameter to query your own Wallet Bundle:
  ```c
  const char* bundle_hash = knishio_client_get_bundle(client);
  if (bundle_hash != NULL) {
      printf("Bundle hash: %s\n", bundle_hash);
      // Note: Do not free bundle_hash - it's managed by the client
  }
  ```

- Query metadata for a **Meta Asset**:
  ```c
  char* result = NULL;
  knishio_error_t error = knishio_client_query_meta(
      client,
      "Vehicle",    // metaType
      "CAR123",     // metaId (optional)
      &result
  );
  
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Meta query result: %s\n", result);
      free(result); // Important: free the allocated result
  }
  ```

- Writing new metadata for a **Meta Asset**:
  ```c
  // Create metadata array
  knishio_meta_t* meta_array = malloc(sizeof(knishio_meta_t) * 4);
  meta_array[0] = (knishio_meta_t){"type", "fire"};
  meta_array[1] = (knishio_meta_t){"weakness", "water"};
  meta_array[2] = (knishio_meta_t){"hp", "78"};
  meta_array[3] = (knishio_meta_t){"attack", "84"};
  
  char* molecular_hash = NULL;
  knishio_error_t error = knishio_client_create_meta(
      client,
      "Pokemon",      // metaType
      "Charizard",    // metaId
      meta_array,     // meta array
      4,              // meta count
      &molecular_hash // output molecular hash
  );
  
  if (error == KNISHIO_SUCCESS) {
      printf("Meta created with hash: %s\n", molecular_hash);
      free(molecular_hash);
  }
  
  free(meta_array);
  ```

- Query Wallets associated with a Wallet Bundle:
  ```c
  char* result = NULL;
  knishio_error_t error = knishio_client_query_wallets(
      client,
      "c47e20f99df190e418f0cc5ddfa2791e9ccc4eb297cfa21bd317dc0f98313b1d", // bundle
      "FOO",    // token (optional)
      true,     // unspent
      &result
  );
  
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Wallets: %s\n", result);
      free(result);
  }
  ```

- Declaring new **Wallets**:

  (**Note:** If Tokens are sent to undeclared Wallets, **Shadow Wallets** will be used (placeholder
  Wallets that can receive, but cannot send) to store tokens until they are claimed.)

  ```c
  char* result = NULL;
  knishio_error_t error = knishio_client_create_wallet(
      client,
      "FOO",    // Token Slug for the wallet we are declaring
      &result
  );
  
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Wallet creation result: %s\n", result);
      free(result);
  }
  ```

- Issuing new **Tokens**:
  ```c
  // Create metadata for token
  knishio_meta_t* token_meta = malloc(sizeof(knishio_meta_t) * 4);
  token_meta[0] = (knishio_meta_t){"name", "CrazyCoin"};
  token_meta[1] = (knishio_meta_t){"fungibility", "fungible"};
  token_meta[2] = (knishio_meta_t){"supply", "limited"};
  token_meta[3] = (knishio_meta_t){"decimals", "2"};
  
  char* result = NULL;
  knishio_error_t error = knishio_client_create_token(
      client,
      "CRZY",        // Token slug (ticker symbol)
      "100000000",   // Initial amount to issue
      token_meta,    // Token metadata
      4,             // Metadata count
      NULL,          // units array (optional, for stackable tokens)
      0,             // units count
      NULL,          // batchId (optional, for stackable tokens)
      &result
  );
  
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Token creation result: %s\n", result);
      free(result);
  }
  
  free(token_meta);
  ```

- Transferring **Tokens** to other users:
  ```c
  char* result = NULL;
  knishio_error_t error = knishio_client_transfer_token(
      client,
      "7bf38257401eb3b0f20cabf5e6cf3f14c76760386473b220d95fa1c38642b61d", // Recipient's bundle hash
      "CRZY",    // Token slug
      "100",     // Amount
      NULL,      // units array (optional, for stackable tokens)
      0,         // units count  
      NULL,      // batchId (optional, for stackable tokens)
      &result
  );
  
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Transfer result: %s\n", result);
      free(result);
  }
  ```

- Creating a new **Rule**:
  ```c
  // Create rule definition (JSON string)
  const char* rule_json = "[{\"field\":\"amount\",\"operator\":\"<\",\"value\":1000}]";
  
  char* result = NULL;
  knishio_error_t error = knishio_client_create_rule(
      client,
      "MyMetaType",  // metaType
      "MyMetaId",    // metaId
      rule_json,     // rule definition as JSON
      &result
  );
  
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Rule creation result: %s\n", result);
      free(result);
  }
  ```

- Querying **Atoms**:
  ```c
  char* result = NULL;
  knishio_error_t error = knishio_client_query_atom(
      client,
      "molecular_hash_here",  // molecularHash (optional)
      NULL,                   // bundleHash (optional)
      "V",                   // isotope (optional)
      "CRZY",                // tokenSlug (optional)
      true,                  // latest
      15,                    // limit
      1,                     // offset
      &result
  );
  
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Atom query result: %s\n", result);
      free(result);
  }
  ```

- Working with **Buffer Tokens**:
  ```c
  // Deposit to buffer
  char* deposit_result = NULL;
  knishio_error_t error = knishio_client_deposit_buffer_token(
      client,
      "CRZY",          // tokenSlug
      "100",           // amount
      "{\"OTHER_TOKEN\": \"0.5\"}", // tradeRates as JSON
      &deposit_result
  );
  
  if (error == KNISHIO_SUCCESS && deposit_result != NULL) {
      printf("Deposit result: %s\n", deposit_result);
      free(deposit_result);
  }
  
  // Withdraw from buffer
  char* withdraw_result = NULL;
  error = knishio_client_withdraw_buffer_token(
      client,
      "CRZY",          // tokenSlug
      "50",            // amount
      &withdraw_result
  );
  
  if (error == KNISHIO_SUCCESS && withdraw_result != NULL) {
      printf("Withdraw result: %s\n", withdraw_result);
      free(withdraw_result);
  }
  ```

- Getting client fingerprint:
  ```c
  char* fingerprint = NULL;
  knishio_error_t error = knishio_get_fingerprint(&fingerprint);
  
  if (error == KNISHIO_SUCCESS && fingerprint != NULL) {
      printf("Device fingerprint: %s\n", fingerprint);
      free(fingerprint);
  }
  ```

## Advanced Usage: Working with Molecules

For more granular control, you can work directly with Molecules:

- Create a new Molecule:
  ```c
  knishio_wallet_t* source_wallet = NULL;
  knishio_client_get_source_wallet(client, "USER", &source_wallet);
  
  knishio_molecule_t* molecule = NULL;
  knishio_error_t error = knishio_client_create_molecule(
      client,
      source_wallet,
      "my_cell_slug",  // optional
      &molecule
  );
  
  if (error == KNISHIO_SUCCESS) {
      printf("Molecule created successfully\n");
  }
  ```

- Sign and check a Molecule:
  ```c
  // Sign the molecule
  knishio_error_t error = knishio_molecule_sign(molecule, NULL, NULL);
  if (error != KNISHIO_SUCCESS) {
      fprintf(stderr, "Failed to sign molecule\n");
      knishio_molecule_free(molecule);
      return -1;
  }
  
  // Verify the molecule
  bool is_valid = false;
  error = knishio_molecule_check(molecule, &is_valid);
  if (error != KNISHIO_SUCCESS || !is_valid) {
      fprintf(stderr, "Molecule validation failed\n");
      knishio_molecule_free(molecule);
      return -1;
  }
  ```

- Execute a custom Query or Mutation:
  ```c
  char* result = NULL;
  knishio_error_t error = knishio_client_execute_query(
      client,
      "query { Balance(bundleHash: \"hash\") { amount } }", // GraphQL query
      "{\"bundleHash\": \"my_hash\"}",                     // variables JSON
      &result
  );
  
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Query result: %s\n", result);
      free(result);
  }
  ```

## The Hard Way: DIY Everything

This method involves individually building Atoms and Molecules, triggering the signature and validation processes, and communicating the resulting signed Molecule mutation or Query to a Knish.IO node via GraphQL.

1. Include the relevant headers in your application code:
    ```c
    #include "knishio/knishio.h"
    #include "knishio/molecule.h"
    #include "knishio/wallet.h"
    #include "knishio/atom.h"
    ```

2. Generate a 2048-symbol hexadecimal secret, either randomly, or via hashing login + password + salt, OAuth secret ID, biometric ID, or any other static value.

3. (optional) Initialize a signing wallet:
   ```c
   knishio_wallet_t* wallet = NULL;
   
   // Full equivalent of JavaScript: new Wallet({secret, bundle, token, address, position, batchId, characters})
   // C SDK uses individual parameters instead of object constructor
   bool success = knishio_wallet_create_from_params(
       &wallet,
       "my_secret_phrase_here",  // secret (matches JS secret)
       NULL,                     // bundle (matches JS bundle - NULL for auto-generation)
       "USER",                   // token (matches JS token)
       NULL,                     // address (matches JS address - NULL for auto-generation)
       NULL,                     // position (matches JS position - NULL for auto-generation)
       NULL,                     // batch_id (matches JS batchId - optional)
       "BASE64"                  // characters (matches JS characters)
   );
   
   if (!success || wallet == NULL) {
       fprintf(stderr, "Failed to create wallet\n");
       return -1;
   }
   ```

   **Note:** The C SDK provides full parameter equivalency with the JavaScript SDK but uses individual parameters instead of object constructors due to C language limitations.

   **WARNING 1:** If ContinuID is enabled on the node, you will need to use a specific wallet, and therefore will first need to query the node to retrieve the `position` for that wallet.

   **WARNING 2:** The Knish.IO protocol mandates that all C and M transactions be signed with a `USER` token wallet.

4. Build your molecule:
   ```c
   knishio_molecule_t* molecule = NULL;
   
   // Full equivalent of JavaScript: new Molecule({secret, bundle, sourceWallet, remainderWallet, cellSlug, version})
   // C SDK uses individual parameters instead of object constructor
   knishio_error_t error = knishio_molecule_create(
       &molecule,
       "my_secret_phrase_here", // secret (matches JS secret)
       NULL,                    // bundle (matches JS bundle - NULL for auto-generation)
       source_wallet,           // source_wallet (matches JS sourceWallet)
       remainder_wallet,        // remainder_wallet (matches JS remainderWallet) 
       "my_cell_slug",         // cell_slug (matches JS cellSlug)
       "4"                     // version (matches JS version)
   );
   
   if (error != KNISHIO_SUCCESS) {
       fprintf(stderr, "Failed to create molecule\n");
       return -1;
   }
   ```

   **Note:** The C SDK provides all 6 parameters from the JavaScript SDK constructor but passes them individually rather than as an object literal.

5. Either use one of the shortcut methods provided by the molecule functions (which will build `knishio_atom_t` instances for you), or create `knishio_atom_t` instances yourself.

   DIY example:
    ```c
    // This example records a new Wallet on the ledger
    
    // Create metadata for our new wallet
    knishio_meta_t* wallet_meta = malloc(sizeof(knishio_meta_t) * 5);
    wallet_meta[0] = (knishio_meta_t){"address", new_wallet->address};
    wallet_meta[1] = (knishio_meta_t){"token", new_wallet->token};
    wallet_meta[2] = (knishio_meta_t){"bundle", new_wallet->bundle};
    wallet_meta[3] = (knishio_meta_t){"position", new_wallet->position};
    wallet_meta[4] = (knishio_meta_t){"batchId", new_wallet->batch_id};
    
    // Build the C isotope atom - Full equivalent of JavaScript: new Atom({...})
    knishio_atom_t* wallet_creation_atom = NULL;
    knishio_error_t error = knishio_atom_create_with_meta(
        &wallet_creation_atom,
        source_wallet->position,    // position (matches JS position)
        source_wallet->address,     // wallet_address (matches JS walletAddress)
        KNISHIO_ISOTOPE_C,         // isotope (matches JS isotope: 'C')
        source_wallet->token,       // token (matches JS token)
        NULL,                       // value (matches JS value - NULL for C isotope)
        NULL,                       // batch_id (matches JS batchId - optional)
        "wallet",                   // meta_type (matches JS metaType)
        new_wallet->address,        // meta_id (matches JS metaId)
        wallet_meta,                // meta (matches JS meta array)
        5                           // meta_count (size of meta array)
    );
    
    if (error != KNISHIO_SUCCESS) {
        free(wallet_meta);
        return -1;
    }
    
    // Add the atom to our molecule
    error = knishio_molecule_add_atom(molecule, wallet_creation_atom);
    if (error != KNISHIO_SUCCESS) {
        knishio_atom_free(wallet_creation_atom);
        free(wallet_meta);
        return -1;
    }
    
    free(wallet_meta);
    
    // Adding a ContinuID / remainder atom
    error = knishio_molecule_add_continuid_atom(molecule);
    if (error != KNISHIO_SUCCESS) {
        return -1;
    }
    ```

   Molecule shortcut method example:
    ```c
    // This example commits metadata to some Meta Asset
    // Full equivalent of JavaScript: molecule.initMeta({...})
    
    // Defining metadata keys and values separately (C SDK pattern)
    const char* meta_keys[] = {"foo", "bar"};
    const char* meta_values[] = {"Foo", "Bar"};
    
    knishio_error_t error = knishio_molecule_init_meta(
        molecule,
        "MyMetaType",    // meta_type (matches JS metaType)
        "MetaId123",     // meta_id (matches JS metaId)  
        meta_keys,       // meta_keys array
        meta_values,     // meta_values array
        2                // meta_count (size of arrays)
    );
    
    if (error != KNISHIO_SUCCESS) {
        fprintf(stderr, "Failed to initialize meta\n");
        return -1;
    }
    ```

6. Sign the molecule with the stored user secret:
    ```c
    // Full equivalent of JavaScript: molecule.sign({})
    knishio_error_t error = knishio_molecule_sign(
        molecule,
        NULL,     // bundle (NULL to derive from secret)
        false,    // anonymous (matches JS default)
        false     // compressed (matches JS default)
    );
    
    if (error != KNISHIO_SUCCESS) {
        fprintf(stderr, "Failed to sign molecule\n");
        return -1;
    }
    ```

7. Make sure everything checks out by verifying the molecule:
    ```c
    // Full equivalent of JavaScript: molecule.check() or molecule.check(sourceWallet)
    knishio_error_t error = knishio_molecule_check(
        molecule,
        source_wallet  // sender_wallet (NULL for basic validation, sourceWallet for V isotope)
    );
    
    if (error != KNISHIO_SUCCESS) {
        fprintf(stderr, "Molecule validation failed: %d\n", error);
        return -1;
    }
    
    // Success - molecule is valid
    printf("Molecule validation passed\n");
    ```

8. Broadcast the molecule to a Knish.IO node:
    ```c
    // Propose the molecule to the network
    char* molecular_hash = NULL;
    knishio_error_t error = knishio_client_propose_molecule(
        client,
        molecule,
        &molecular_hash
    );
    
    if (error == KNISHIO_SUCCESS && molecular_hash != NULL) {
        printf("Molecule proposed with hash: %s\n", molecular_hash);
        free(molecular_hash);
    } else {
        fprintf(stderr, "Failed to propose molecule: %d\n", error);
        return -1;
    }
    ```

9. Inspect the response...
    ```c
    // For mutations, check the returned molecular hash
    if (molecular_hash != NULL) {
        printf("Transaction successful: %s\n", molecular_hash);
    }
    
    // For queries, inspect the returned JSON data
    char* query_result = NULL;
    error = knishio_client_execute_query(client, query, variables, &query_result);
    if (error == KNISHIO_SUCCESS && query_result != NULL) {
        printf("Query data: %s\n", query_result);
        free(query_result);
    }
    
    // Error handling
    if (error != KNISHIO_SUCCESS) {
        char error_message[256];
        knishio_error_get_message(error, error_message, sizeof(error_message));
        printf("Error: %s\n", error_message);
    }
    ```

   Response payloads are provided by the following operations:
    1. Balance and ContinuID queries → returns wallet information
    2. Wallet list queries → returns array of wallet data
    3. Molecule proposals, authorization requests, token operations → returns transaction metadata

## Getting Help

Knish.IO is under active development, and our team is ready to assist with integration questions. The best way to seek help is to stop by our [Telegram Support Channel](https://t.me/wishknish). You can also [send us a contact request](https://knish.io/contact) via our website.
