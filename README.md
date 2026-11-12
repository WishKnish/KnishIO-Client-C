<div style="text-align:center">
  <img src="https://raw.githubusercontent.com/WishKnish/KnishIO-Technical-Whitepaper/master/KnishIO-Logo.png" alt="Knish.IO: Post-Blockchain Platform" />
</div>
<div style="text-align:center">info@wishknish.com | https://wishknish.com</div>

# Knish.IO C Client SDK

This is the official C implementation of the Knish.IO client SDK. It builds and signs Knish.IO Molecules, composes Atoms, derives Wallets, and talks to a Knish.IO node over GraphQL. Signing uses WOTS+ one-time signatures; encrypted transport uses ML-KEM (FIPS 203).

The JavaScript SDK is the reference implementation. Where this SDK does not yet offer a JavaScript feature, it is listed under [Not implemented](#not-implemented-compared-with-the-javascript-sdk).

Every C code block in this README is compiled and linked against an installed copy of the headers and library.

## Installation

### Prerequisites

- **CMake** 3.16 or newer, and **pkg-config**
- A **C17 compiler** (GCC 9+, Clang 10+, or MSVC 2019+)
- **OpenSSL 3** (AES-256-GCM, hashing, the system CSPRNG) — required
- **libcurl** (HTTP/GraphQL transport) — required
- **cJSON** (JSON parsing and generation) — required
- **GMP** (arbitrary-precision arithmetic for key derivation) — required
- **git** with submodule support: ML-KEM-768 and ML-KEM-1024 come from the vendored `external/mlkem-native` submodule, so clone with `--recursive`

Optional, detected by CMake when present and not needed for any documented feature:

- **liboqs** — an extra liboqs code path. ML-KEM does not depend on it.
- **libwebsockets** — the build links it when found, but the SDK has no public subscription API yet (see [Not implemented](#not-implemented-compared-with-the-javascript-sdk)).

#### macOS (Homebrew)
```bash
brew install cmake pkg-config openssl@3 curl cjson gmp
```

#### Ubuntu/Debian
```bash
sudo apt-get install build-essential cmake pkg-config libssl-dev libcurl4-openssl-dev libcjson-dev libgmp-dev
```

### System dependencies

These libraries are resolved from the **host system** (via `pkg-config` / `find_library`) and are **not** vendored or version-pinned by the build. Because they live outside this repo, their security posture is the responsibility of the host: keep them patched with your platform package manager (`brew upgrade`, `apt upgrade`, etc.).

| Library       | Role                                          | Required? | Verified on build host |
|---------------|-----------------------------------------------|-----------|------------------------|
| OpenSSL       | AES-256-GCM (EVP), hashing, CSPRNG            | Yes       | 3.5.9                  |
| libcurl       | HTTP / GraphQL transport                      | Yes       | 8.7.1                  |
| libcjson      | JSON parsing and generation                   | Yes       | 1.7.19                 |
| GMP           | Arbitrary-precision arithmetic (key derivation) | Yes     | 6.3.0                  |
| liboqs        | Optional liboqs code path                     | No        | not installed          |
| libwebsockets | Not used by any public API yet                | No        | not installed          |

"Verified on build host" is the version this SDK was last built against; it is not a ceiling. Newer patch/minor releases are expected to work and should be preferred for security updates. The optional libraries are absent on the reference build host with no impact on cryptographic parity — ML-KEM-768 and ML-KEM-1024 are provided by the vendored `mlkem-native` submodule (see the Vendored crypto pin note below), not liboqs.

### Build, test and install

```bash
# --recursive fetches the pinned external/mlkem-native submodule (required for ML-KEM)
git clone --recursive https://github.com/WishKnish/KnishIO-Client-C.git
cd KnishIO-Client-C
# if you cloned without --recursive, or after pulling: fetch/realign the submodule
git submodule update --init external/mlkem-native

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build --prefix /usr/local   # use sudo if the prefix is not writable
```

CMake options:

| Option | Default | Effect |
|---|---|---|
| `KNISHIO_BUILD_TESTS` | `ON` | Builds the ctest suite. `c_self_test` (the cross-SDK self-test) is always built. |
| `KNISHIO_USE_KCORE` | `OFF` | Runs WOTS+ and ML-KEM through the KnishIO Crypto Core (see below). |
| `KNISHIO_ENABLE_SANITIZERS` | `OFF` | Builds with sanitizers. |
| `KNISHIO_ENABLE_COVERAGE` | `OFF` | Builds with coverage instrumentation. |

The install puts the headers under `<prefix>/include/knishio/`, the shared library (`libknishio-client`) and the static library (`libknishio-client-static.a`) under `<prefix>/lib`, a pkg-config file (`knishio-client.pc`) and a CMake package (`KnishIOClient`).

> **Vendored crypto pin:** `external/mlkem-native` (ML-KEM-768 and ML-KEM-1024, FIPS 203) is pinned to **v1.2.0 (`0ba906cb`)** from [pq-code-package/mlkem-native](https://github.com/pq-code-package/mlkem-native) — the commit CI builds and the cross-SDK family is aligned to. This pin was advanced from the earlier `cf895dd5 (v1.0.0-116)` to pick up the v1.1.0 security fixes: zeroization of secret `pk`/`sk` buffers on keypair-generation failure, zeroization of the intermediate `pkpv` polynomial vector, a bounded x86_64 rejection-sampling buffer overread, and a hardened (`volatile`) value barrier. The bump is output-preserving — the self-test's ML-KEM768 keygen vector still matches byte-for-byte across all SDKs. Keep your working tree at the pin with `git submodule update --init` (a `git status` showing `M external/mlkem-native` means it has drifted); don't bump it ad hoc, so local builds stay identical to what CI tests and ships. Bump only for a genuine upstream security fix, and only after the full self-test still passes byte-identically.

> **Optional kcore backend:** `-DKNISHIO_USE_KCORE=ON` links the [KnishIO Crypto Core](https://github.com/WishKnish/KnishIO-Crypto-Core) (kcore ≥ 0.2.0) statically and runs the WOTS+ chains (signing and verification), the WOTS+ address (v1 and v2 keys) and ML-KEM-1024/768 through it. Extract a `knishio-crypto-core-<version>-<target>` release package and pass its root: `cmake -S . -B build -DKNISHIO_USE_KCORE=ON -DCMAKE_PREFIX_PATH=/path/to/knishio-crypto-core-0.2.0-linux-arm64-gnu`; the configure log prints `kcore: KnishIOCryptoCore <version>`. Output is byte-identical with the default (OFF) build. Only lowercase-hex keys and signatures of the exact length go to kcore; anything else (for example a peer's malformed OTS) takes the SDK's own SHAKE256 loop, so verification verdicts do not depend on the option. The vendored mlkem-native stays compiled for the OFF build.

> **Prebuilt release packages:** the `knishio-client-c-<version>-<platform>.tar.gz` tarballs on the GitHub Releases page are built with kcore (`KNISHIO_USE_KCORE=ON`) and bundle it: `include/kcore.h`, `lib/libkcore.*`, `lib/cmake/KnishIOCryptoCore/` and `lib/pkgconfig/knishio-crypto-core.pc` sit in the same prefix, with kcore's license under `share/licenses/knishio-crypto-core/`, so `find_package(KnishIOClient)` and `pkg-config` resolve kcore without a separate download. Source builds take `-DKNISHIO_USE_KCORE=ON -DCMAKE_PREFIX_PATH=<kcore package>`; OFF is still the default.

### Link to your project

```bash
# Shared library, flags from pkg-config (add <prefix>/lib/pkgconfig to PKG_CONFIG_PATH for a non-default prefix)
cc -std=c17 myapp.c -o myapp $(pkg-config --cflags --libs knishio-client) \
    -Wl,-rpath,"$(pkg-config --variable=libdir knishio-client)"

# Static library: name the archive, then its dependencies
cc -std=c17 myapp.c -o myapp $(pkg-config --cflags knishio-client) \
    "$(pkg-config --variable=libdir knishio-client)/libknishio-client-static.a" \
    $(pkg-config --static --libs knishio-client | sed 's/-lknishio-client//')
```

With CMake:

```cmake
find_package(KnishIOClient REQUIRED)
target_link_libraries(myapp PRIVATE KnishIO::knishio-client)          # shared
# target_link_libraries(myapp PRIVATE KnishIO::knishio-client-static) # static
```

## Basic Usage

There are two ways to use the SDK:

1. The easy way: the `knishio_client_*` functions build, sign and submit molecules for you.
2. The granular way: build `knishio_wallet_t`, `knishio_atom_t` and `knishio_molecule_t` values yourself, sign and check them, then submit them.

### Amounts

Ledger values are integer strings (the validator parses them as 128-bit integers). In C, amounts are `int64_t` wherever the API is integer-typed: `knishio_create_token_params_t.amount`, `knishio_client_deposit_buffer_token`, `knishio_client_withdraw_buffer_token`, `knishio_client_replenish_token`, `knishio_transfer_recipient_t.amount`, and the molecule builders (`knishio_molecule_init_value(s)`, `_init_burn`, `_init_deposit_buffer`, `_init_withdraw_buffer`, `_init_replenish`). `knishio_transfer_params_t.amount` and the `amount` of `knishio_client_burn_tokens` / `knishio_client_query_source_wallet` are `double`: pass whole numbers (exact up to 2^53−1); the fractional part is dropped. Balances come back as strings.

## The Easy Way: KnishIO Client Functions

1. Include the SDK. `knishio/knishio.h` declares the client, wallets, molecules, atoms, and the wallet, token and transfer operations; the other operation groups have their own headers:
   ```c
   #include <knishio/knishio.h>               /* client, wallets, molecules; wallet, token and transfer operations */
   #include <knishio/operations/auth.h>       /* guest and profile auth tokens, active sessions */
   #include <knishio/operations/identity.h>   /* identifiers, shadow-wallet claims */
   #include <knishio/operations/policy.h>     /* policies and rules */
   #include <knishio/operations/query.h>      /* atom and active-session queries */
   #include <knishio/fingerprint.h>           /* guest fingerprint */
   ```

2. Create a client, log in, and run a query. This is a complete program:
   ```c
   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   #include <knishio/knishio.h>

   int main(void) {
       knishio_client_config_t config = {
           .uri = "https://your-node.example/graphql",  /* GraphQL endpoint of a Knish.IO node */
           .cell_slug = "my_cell_slug",                 /* optional */
           .server_sdk_version = 4,                     /* optional */
           .mlkem_parameter_set = 1024,                 /* 1024 (default; 0 means 1024) or 768 */
           .ots_version = 2,                            /* 2 (default; 0 means 2) or 1 */
       };
       knishio_client_t* client = NULL;
       knishio_error_t error = knishio_client_create(&client, &config);
       if (error != KNISHIO_SUCCESS) {
           fprintf(stderr, "client: %s\n", knishio_error_message(error));
           return 1;
       }

       /* A 2048-character hex secret from any stable input (JS generateSecret) */
       char* secret = NULL;
       if (!knishio_generate_secret("username:password:salt", 2048, &secret)) {
           knishio_client_destroy(client);
           return 1;
       }

       /* JS: await client.requestAuthToken({ secret, encrypt: false }).
        * Stores the secret on the client, signs a U-isotope authorization molecule
        * and keeps the token the node returns. */
       error = knishio_client_authenticate(client, secret, false);
       knishio_secure_free(secret, strlen(secret));
       if (error != KNISHIO_SUCCESS) {
           fprintf(stderr, "authenticate: %s\n", knishio_error_message(error));
           knishio_client_destroy(client);
           return 1;
       }

       char* balance = NULL;
       error = knishio_client_query_balance(client, "USER", knishio_client_get_bundle(client), &balance);
       if (error == KNISHIO_SUCCESS && balance != NULL) {
           printf("USER balance: %s\n", balance);
       }
       free(balance);

       knishio_client_destroy(client);
       return 0;
   }
   ```

   (**Note:** the secret can be derived from a salted username + password, a biometric hash, or an identifier from an external authentication process. `knishio_client_set_secret` stores a secret without contacting the node.)

   A guest session needs no secret: the client derives a guest wallet from its process fingerprint (`knishio_get_fingerprint`, random per process) and asks the node for a token:
   ```c
   knishio_request_guest_auth_token_params_t params = { .cell_slug = "my_cell_slug", .encrypt = false };
   knishio_request_guest_auth_token_result_t* result = NULL;
   knishio_error_t error = knishio_client_request_guest_auth_token(client, &params, &result);
   if (error == KNISHIO_SUCCESS && result->success) {
       printf("Guest token: %s\n", result->token);
   }
   knishio_request_guest_auth_token_result_free(result);
   ```

3. Use the client. The examples below assume a `knishio_client_t* client` that has authenticated. Every function returns a `knishio_error_t`; `knishio_error_message(error)` describes it. Strings the SDK returns are freed with `free()`; result structures with their own `*_free` function.

### KnishIO Client Functions

- Query the **Balance** of a token in a Wallet Bundle (pass `NULL` as the bundle to omit it from the query):
  ```c
  char* balance = NULL;
  knishio_error_t error = knishio_client_query_balance(client, "CRZY", knishio_client_get_bundle(client), &balance);
  if (error == KNISHIO_SUCCESS && balance != NULL) {
      printf("Balance: %s\n", balance);
  }
  free(balance);
  ```

- Query metadata for a **Meta Asset**:
  ```c
  char* result = NULL;
  knishio_error_t error = knishio_client_query_meta(
      client,
      "Vehicle",    /* metaType */
      "CAR123",     /* metaId (optional) */
      &result
  );
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Meta query result: %s\n", result);
  }
  free(result);
  ```

- Writing new metadata for a **Meta Asset**:
  ```c
  const char* keys[] = { "type", "weakness", "hp", "attack" };
  const char* values[] = { "fire", "water", "78", "84" };
  knishio_meta_t* meta[4] = { NULL };
  for (size_t i = 0; i < 4; i++) {
      if (knishio_meta_create(&meta[i], keys[i], values[i]) != KNISHIO_SUCCESS) {
          return 1;
      }
  }

  char* molecular_hash = NULL;
  knishio_error_t error = knishio_client_create_meta(
      client,
      "Pokemon",      /* metaType */
      "Charizard",    /* metaId */
      meta,           /* meta pairs */
      4,              /* meta count */
      &molecular_hash
  );
  if (error == KNISHIO_SUCCESS) {
      printf("Meta created with hash: %s\n", molecular_hash);
  }
  free(molecular_hash);
  for (size_t i = 0; i < 4; i++) {
      knishio_meta_free(meta[i]);
  }
  ```

- Query the **Wallets** of a Wallet Bundle (`bundle = NULL` queries your own):
  ```c
  knishio_wallet_list_params_t params = { .bundle = NULL, .token = "FOO", .include_shadow = false };
  knishio_wallet_list_result_t* result = NULL;
  knishio_error_t error = knishio_client_query_wallets(client, &params, &result);
  if (error == KNISHIO_SUCCESS) {
      for (size_t i = 0; i < knishio_wallet_list_result_get_count(result); i++) {
          knishio_wallet_t* wallet = knishio_wallet_list_result_get_wallet(result, i);
          printf("%s %s\n", wallet->token, wallet->address);
      }
  }
  knishio_wallet_list_result_free(result);
  ```

- Declaring new **Wallets**:

  (**Note:** If Tokens are sent to undeclared Wallets, **Shadow Wallets** will be used (placeholder
  Wallets that can receive, but cannot send) to store tokens until they are claimed with
  `knishio_client_claim_shadow_wallet` / `knishio_client_claim_shadow_wallets`.)

  ```c
  knishio_create_wallet_params_t params = { .token = "FOO" };
  knishio_create_wallet_result_t* result = NULL;
  knishio_error_t error = knishio_client_create_wallet(client, &params, &result);
  if (error == KNISHIO_SUCCESS && knishio_create_wallet_result_is_success(result)) {
      printf("Wallet declared in molecule %s\n", result->molecular_hash);
  }
  knishio_create_wallet_result_free(result);
  ```

- Issuing new **Tokens**:
  ```c
  knishio_meta_t* meta[2] = { NULL };
  knishio_meta_create(&meta[0], "supply", "limited");
  knishio_meta_create(&meta[1], "decimals", "0");

  knishio_create_token_params_t params = {
      .token = "CRZY",                         /* token slug (ticker symbol) */
      .name = "CrazyCoin",
      .amount = 100000000,                     /* int64_t initial supply */
      .fungibility = KNISHIO_TOKEN_FUNGIBLE,   /* or _NONFUNGIBLE, _STACKABLE, _REPLENISHABLE */
      .meta = meta,
      .meta_count = 2,
  };
  knishio_create_token_result_t* result = NULL;
  knishio_error_t error = knishio_client_create_token(client, &params, &result);
  if (error == KNISHIO_SUCCESS && knishio_create_token_result_is_success(result)) {
      printf("Token created in molecule %s\n", knishio_create_token_result_get_hash(result));
  }
  knishio_create_token_result_free(result);
  knishio_meta_free(meta[0]);
  knishio_meta_free(meta[1]);
  ```

- Transferring **Tokens** to other users:
  ```c
  knishio_transfer_params_t params = {
      .recipient = "7bf38257401eb3b0f20cabf5e6cf3f14c76760386473b220d95fa1c38642b61d", /* recipient bundle hash */
      .token = "CRZY",
      .amount = 100,     /* whole number; use .units / .unit_count instead for a stackable token */
  };
  knishio_transfer_result_t* result = NULL;
  knishio_error_t error = knishio_client_transfer_tokens(client, &params, &result);
  if (error == KNISHIO_SUCCESS && knishio_transfer_result_is_success(result)) {
      printf("Transfer molecule: %s\n", knishio_transfer_result_get_hash(result));
  }
  knishio_transfer_result_free(result);
  ```

  `knishio_client_transfer_tokens_multi` sends to several recipients in one molecule, and `knishio_client_burn_tokens` destroys tokens.

- Creating a **Policy** and a **Rule** for a Meta Asset (both build and sign R-isotope molecules as the JavaScript SDK does):
  ```c
  knishio_create_policy_params_t policy = {
      .meta_type = "profile",
      .meta_id = "p1",
      .policy_json = "{\"read\":{\"name\":[\"all\"]},\"write\":{\"name\":[\"self\"]}}",
  };
  knishio_create_policy_result_t* policy_result = NULL;
  knishio_error_t error = knishio_client_create_policy(client, &policy, &policy_result);
  if (error == KNISHIO_SUCCESS && policy_result->success) {
      printf("Policy molecule: %s\n", policy_result->molecular_hash);
  }
  knishio_create_policy_result_free(policy_result);

  knishio_create_rule_params_t rule = {
      .meta_type = "profile",
      .meta_id = "p1",
      .rule_json = "[{\"condition\":[{\"key\":\"amount\",\"value\":\"100\",\"comparison\":\">=\"}],"
                   "\"callback\":[{\"action\":\"reject\"}]}]",
      .policy_json = NULL,   /* optional */
  };
  knishio_create_rule_result_t* rule_result = NULL;
  error = knishio_client_create_rule(client, &rule, &rule_result);
  if (error == KNISHIO_SUCCESS && rule_result->success) {
      printf("Rule molecule: %s\n", rule_result->molecular_hash);
  }
  knishio_create_rule_result_free(rule_result);
  ```

- Creating an **Identifier** (the contact is hashed into the atom and is not sent):
  ```c
  knishio_create_identifier_params_t params = { .type = "email", .contact = "alice@example.com", .code = "1234" };
  knishio_create_identifier_result_t* result = NULL;
  knishio_error_t error = knishio_client_create_identifier(client, &params, &result);
  if (error == KNISHIO_SUCCESS && result->success) {
      printf("Identifier molecule: %s\n", result->molecular_hash);
  }
  knishio_create_identifier_result_free(result);
  ```

- Querying **Atoms**:
  ```c
  knishio_query_atom_params_t params = {
      .isotope = "V",
      .token_slug = "CRZY",
      .latest = true,
      .limit = 15,
      .offset = 0,
  };
  knishio_query_atom_result_t* result = NULL;
  knishio_error_t error = knishio_client_query_atom(client, &params, &result);
  if (error == KNISHIO_SUCCESS && result->success) {
      printf("%zu atoms\n", result->atom_count);
  }
  knishio_query_atom_result_free(result);
  ```

- Working with **Buffer Tokens**:

  A deposit emits `V(-balance) -> B(+amount) -> V(+remainder)`; a withdraw emits
  `B(-balance) -> V(+amount) -> B(+remainder)`. The full source balance is always debited,
  with the change routed to a remainder atom, so a partial operation still conserves.
  Amounts are `int64_t`.

  ```c
  knishio_transfer_result_t* deposit_result = NULL;
  knishio_error_t error = knishio_client_deposit_buffer_token(client, "CRZY", 100, &deposit_result);
  if (error == KNISHIO_SUCCESS && deposit_result->success) {
      printf("Deposit molecule: %s\n", deposit_result->molecular_hash);
  }
  knishio_transfer_result_free(deposit_result);

  /* Withdraw from the buffer to a recipient bundle */
  const char* recipient_bundle = "7bf38257401eb3b0f20cabf5e6cf3f14c76760386473b220d95fa1c38642b61d";
  knishio_transfer_result_t* withdraw_result = NULL;
  error = knishio_client_withdraw_buffer_token(client, "CRZY", 50, recipient_bundle, &withdraw_result);
  if (error == KNISHIO_SUCCESS && withdraw_result->success) {
      printf("Withdraw molecule: %s\n", withdraw_result->molecular_hash);
  }
  knishio_transfer_result_free(withdraw_result);
  ```

- Running a custom **Query** or **Mutation**:
  ```c
  char* result = NULL;
  knishio_error_t error = knishio_client_execute_query(
      client,
      "query ($bundleHash: String!, $token: String!) { Balance(bundleHash: $bundleHash, token: $token) { amount } }",
      "{\"bundleHash\":\"7bf38257401eb3b0f20cabf5e6cf3f14c76760386473b220d95fa1c38642b61d\",\"token\":\"USER\"}",
      &result
  );
  if (error == KNISHIO_SUCCESS && result != NULL) {
      printf("Query result: %s\n", result);
  }
  free(result);
  ```
  `knishio_client_execute_mutation` takes the same arguments.

## Advanced Usage: Working with Molecules

For more control, build the molecule through the client and submit it yourself. `knishio_client_get_source_wallet_continuid` returns the signing wallet at the bundle's current ContinuID position (queried from the node), which a `USER`-signed molecule needs. `knishio_molecule_init_meta` adds the M atom and the ContinuID atom, as JavaScript `initMeta` does.

```c
knishio_wallet_t* source_wallet = NULL;
knishio_error_t error = knishio_client_get_source_wallet_continuid(client, "USER", &source_wallet);
if (error != KNISHIO_SUCCESS) {
    return 1;
}

knishio_molecule_t* molecule = NULL;
error = knishio_client_create_molecule(client, source_wallet, knishio_client_get_cell_slug(client), &molecule);
if (error == KNISHIO_SUCCESS) {
    const char* keys[] = { "foo", "bar" };
    const char* values[] = { "Foo", "Bar" };
    error = knishio_molecule_init_meta(molecule, "MyMetaType", "MetaId123", keys, values, 2);
}
if (error == KNISHIO_SUCCESS) {
    /* JS: molecule.sign({}) — bundle from the secret, not anonymous, compressed signature */
    error = knishio_molecule_sign(molecule, NULL, false, true);
}
if (error == KNISHIO_SUCCESS) {
    /* JS: molecule.check(sourceWallet); returns the first failed rule as an error code */
    error = knishio_molecule_check(molecule, source_wallet);
}
char* molecular_hash = NULL;
if (error == KNISHIO_SUCCESS) {
    /* Sends the molecule as given (no local check); see also knishio_client_submit_molecule */
    error = knishio_client_propose_molecule(client, molecule, &molecular_hash);
}
if (error == KNISHIO_SUCCESS) {
    printf("Molecule proposed: %s\n", molecular_hash);
} else {
    fprintf(stderr, "Molecule failed: %s\n", knishio_error_message(error));
}
free(molecular_hash);
knishio_molecule_free_deep(molecule);
knishio_wallet_free(source_wallet);
```

## The Hard Way: DIY Everything

This method builds the wallets and the molecule by hand, signs and verifies it locally, and prints the JSON a node expects inside a `ProposeMolecule` mutation. It needs no node and runs as is:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <knishio/knishio.h>

int main(void) {
    int rc = 1;
    char* secret = NULL;
    char* bundle = NULL;
    char* remainder_position = NULL;
    knishio_wallet_t* source_wallet = NULL;
    knishio_wallet_t* remainder_wallet = NULL;
    knishio_molecule_t* molecule = NULL;
    char* json = NULL;

    /* 1. A 2048-character hex secret from login + password + salt, an OAuth id, a biometric
     *    hash, or any other stable value (JS generateSecret). */
    if (!knishio_generate_secret("username:password:salt", 2048, &secret)
        || !knishio_generate_bundle_hash(secret, NULL, NULL, &bundle)) {
        goto done;
    }

    /* 2. The signing wallet. With ContinuID the position must be the bundle's current one,
     *    read from the node (knishio_client_get_source_wallet_continuid); a bundle that has
     *    never signed starts at KNISHIO_FIXED_POSITION. The protocol requires C and M
     *    molecules to be signed by a USER wallet. */
    if (!knishio_wallet_create_with_ots_version(&source_wallet, secret, "USER",
                                                KNISHIO_FIXED_POSITION, KNISHIO_OTS_V2)) {
        goto done;
    }

    /* 3. The remainder wallet the ContinuID atom moves to: a fresh random position. */
    if (!knishio_generate_position(&remainder_position)
        || !knishio_wallet_create_with_ots_version(&remainder_wallet, secret, "USER",
                                                   remainder_position, KNISHIO_OTS_V2)) {
        goto done;
    }

    /* 4. The molecule (JS new Molecule({ secret, bundle, sourceWallet, remainderWallet,
     *    cellSlug, version })). version NULL gives unversioned atoms; "4" gives Version 4 atoms. */
    if (knishio_molecule_create(&molecule, secret, bundle, source_wallet, remainder_wallet,
                                "my_cell_slug", NULL) != KNISHIO_SUCCESS) {
        goto done;
    }

    /* 5. Atoms through a shortcut method: an M atom with the metadata plus the ContinuID
     *    I atom (JS initMeta). knishio_atom_create / knishio_molecule_add_atom build atoms
     *    one by one instead. */
    const char* keys[] = { "foo", "bar" };
    const char* values[] = { "Foo", "Bar" };
    if (knishio_molecule_init_meta(molecule, "MyMetaType", "MetaId123", keys, values, 2) != KNISHIO_SUCCESS) {
        goto done;
    }

    /* 6. Sign (JS molecule.sign({})): bundle NULL, not anonymous, compressed. */
    knishio_error_t error = knishio_molecule_sign(molecule, NULL, false, true);
    if (error != KNISHIO_SUCCESS) {
        fprintf(stderr, "sign: %s\n", knishio_error_message(error));
        goto done;
    }

    /* 7. Verify everything a node verifies (JS molecule.check(sourceWallet)). */
    error = knishio_molecule_check(molecule, source_wallet);
    if (error != KNISHIO_SUCCESS) {
        fprintf(stderr, "check: %s\n", knishio_error_message(error));
        goto done;
    }

    /* 8. The molecule as JSON, for MutationProposeMolecule, or send it with
     *    knishio_client_propose_molecule. */
    if (knishio_molecule_to_json(molecule, &json) != KNISHIO_SUCCESS) {
        goto done;
    }
    printf("Signed molecule %s with %zu atoms (%zu bytes of JSON)\n",
           molecule->molecular_hash, molecule->atom_count, strlen(json));
    rc = 0;

done:
    free(json);
    knishio_molecule_free_deep(molecule);   /* the molecule and its atoms; the wallets stay with the caller */
    knishio_wallet_free(remainder_wallet);
    knishio_wallet_free(source_wallet);
    free(remainder_position);
    free(bundle);
    if (secret) {
        knishio_secure_free(secret, strlen(secret));
    }
    return rc;
}
```

## Network Freshness

The GraphQL transport (libcurl) issues a fresh network request per query — there
is **no response caching** — so a long-lived client never serves a stale read of
ledger state. No fresh-read knob (e.g. a request policy) is required.

## Secret storage

Master secrets are stored at rest in the cross-SDK AES-256-GCM envelope (PBKDF2-HMAC-SHA256 ×100000, camelCase metadata; frozen in `sdks/shared-test-results/cross-platform-test-vectors.json`). Every SDK decrypts every other SDK's envelope.

| Provider | `providerType` | Custody (`hardwareBacked`) | Where the key lives | Recovery passphrase |
|---|---|---|---|---|
| `knishio_aes_gcm_secret_storage_provider_create` | `aes-gcm` | Software (`false`) | Any `knishio_storage_backend_t` (`knishio_memory_storage_backend_create`, `knishio_file_storage_backend_create` [0600]) | Optional |

`hardwareBacked` is derived by the provider from the platform, never accepted from the caller; software providers always report `false`. A hardware provider refuses to store without a recovery passphrase unless `allow_unrecoverable` is set — the recovery envelope (`knishio:recovery:<bundleHash>`, `providerType: "aes-gcm"`) is *software* custody whose strength is bounded by that passphrase: enforce passphrase entropy or keep it on a second device.

Client integration: `knishio_client_set_secret_storage(client, provider, bundle_hash, options)` stores the secret and unregisters cleartext memory, unwrapping just-in-time on signing; the client does not own the provider or backend (free them with `knishio_secret_storage_provider_free` and `knishio_storage_backend_free` after `knishio_client_destroy`).

```c
knishio_storage_backend_t* backend = NULL;
knishio_secret_storage_provider_t* provider = NULL;
if (knishio_file_storage_backend_create("knishio-secrets.json", &backend) != KNISHIO_SUCCESS
    || knishio_aes_gcm_secret_storage_provider_create(backend, "a long passphrase", &provider) != KNISHIO_SUCCESS) {
    knishio_storage_backend_free(backend);
    return 1;
}

knishio_storage_options_t options;
knishio_storage_options_init(&options);
options.label = "main";

/* The client holds a secret (knishio_client_authenticate or knishio_client_set_secret):
 * it is stored in the provider and the cleartext copy is dropped. */
knishio_error_t error = knishio_client_set_secret_storage(client, provider, NULL, &options);
if (error != KNISHIO_SUCCESS) {
    fprintf(stderr, "secret storage: %s\n", knishio_error_message(error));
}

/* ... sign and submit as usual ... */

knishio_client_destroy(client);
knishio_secret_storage_provider_free(provider);
knishio_storage_backend_free(backend);
```

## Thread safety

- Signing and verification on separate objects (wallets, molecules) are thread-safe. Everything runs on the calling thread (kcore starts no threads of its own), so for multi-core throughput process independent objects on separate threads.
- A `knishio_client_t` must not be shared between threads without external locking; use one client per thread or guard a shared one with a mutex.
- `knishio_wallet_set_default_mlkem_param` and `knishio_set_error_handler` — process-wide setter: call during single-threaded initialization, before any thread uses the SDK. A client's ML-KEM set comes from its own configuration and does not change this default.

## Not implemented (compared with the JavaScript SDK)

These JavaScript SDK features have no C equivalent yet. Use `knishio_client_execute_query` / `knishio_client_execute_mutation` for a GraphQL operation the C client does not wrap.

- **Client operations:** `queryMetaVerified`, `hasQueryField`, `queryEmbeddingStatus`, `registerPeer`, `appendRequest`, `linkIdentifier`, `cancelQuery` / `cancelAllQueries`, `defaultRequestPolicy` (not needed: there is no response cache), and the multi-URI node pool (one `uri` per client).
- **Subscriptions:** no WebSocket `subscribe*` API. `knishio_client_unsubscribe` and `knishio_client_unsubscribe_all` exist but do nothing and return `KNISHIO_SUCCESS`.
- **Molecule builders:** `withParentHashes`, `initPeering`, `initAppendRequest`, `initTokenRequest`, `cellSlugBase`, `fill`, and `continuIdPosition`. (`initIdentifierCreation`, `addPolicyAtom` and `createRule` are available as `knishio_molecule_init_identifier_creation`, `knishio_molecule_add_policy_atom` and `knishio_molecule_create_rule`.)
- **Query and mutation classes:** `QueryToken`, `QueryEmbeddingStatus`, `QueryMetaTypeViaAtom`, `QueryMetaTypeViaMolecule`, `MutationPeering`, `MutationAppendRequest`, `MutationLinkIdentifier`. C exposes functions rather than the JavaScript query, mutation and subscription classes.
- **Secret storage providers:** only the AES-256-GCM software provider (over the memory or file backend). There is no `MemorySecretStorageProvider`, `WebAuthnPrf*` or `NonExtractableKey*` provider.

Known differences in operations that exist:

- `knishio_client_request_tokens` sends a bare `RequestTokens` mutation; JavaScript `requestTokens` builds and signs a T-isotope molecule.
- `knishio_client_request_guest_auth_token` sends `cell_slug` to the node but does not make it the client's cell slug, which JavaScript `requestGuestAuthToken` does (`setCellSlug`). Set the slug in `knishio_client_config_t` instead.

## Getting Help

Knish.IO is under active development, and our team is ready to assist with integration questions. The best way to seek help is to stop by our [Telegram Support Channel](https://t.me/wishknish). You can also [send us a contact request](https://knish.io/contact) via our website.
