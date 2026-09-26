# Changelog

All notable changes to the KnishIO Client C SDK are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
This SDK has **no package registry**: it is distributed as source, and a release
is a git tag (`CMakeLists.txt` `project(... VERSION ...)` is the version of
record). GitHub Releases are being set up separately.
Conventions for tags, commits, and these entries: `docs/SDK-RELEASE-CONVENTIONS.md`
in the KnishIOClientSDK monorepo.

This file was backfilled on 2026-07-27 from the repository's own tag and commit
history rather than written at release time; where the history does not
substantiate a detail, the entry says so instead of guessing.

## [Unreleased]

### Security

- No behaviour change: `knishio_molecule_check()` already ignores a `signingWallet` meta and
  compares the signer with atoms[0]'s `walletAddress`. The new `SigningWalletForgery` ctest pins
  that with the cross-SDK fixture `tests/fixtures/signing-wallet-forgery.json` (built with JS
  1.2.1): a molecule that claims one wallet's address but carries another wallet's signature, which
  a verifier honouring the meta (and an offline verifier such as knishproof built on one) reports
  as valid, must fail as `KNISHIO_ERROR_SIGNATURE_MISMATCH`.

### Fixed

- A returning user's login is now signed from the ContinuID pointer:
  `knishio_client_request_profile_auth_token()` queries `ContinuId(bundle, USER)` and signs the
  authorization with the USER wallet registered at that position, so validator 0.5.0+ issues a
  proven token and the user keeps read and subscription access to permissioned and private cells.
  A first login (no pointer) is unchanged. A rejected pointer-signed login falls back once to the
  previous unproven login from a fresh AUTH wallet, so one login proposes at most two molecules.
- `knishio_molecule_check()` accepts a U atom with token USER as well as AUTH; any other token is
  still rejected. It rejected every pointer-signed authorization molecule.
- `knishio_auth_token_get_snapshot()` records the bound wallet's token as `wallet.token`, and
  `knishio_auth_token_restore()` rebuilds the wallet with it; a snapshot without it restores an
  AUTH wallet as before. Restore always rebuilt an AUTH wallet, whose key and ML-KEM pair differ
  from a USER-bound session's.
- The profile login released only the atom array of its authorization molecule, leaking the U and
  I atoms and their meta on every login; it now frees them.
- The SDK's ContinuID query (operation `QueryContinuId`) was not in the encrypted-transport bypass
  set, which named only `ContinuId`, so an encryption-enabled client without keys failed it with
  `KNISHIO_ERROR_INVALID_STATE`. The profile login now sends it first, so it is sent in plaintext
  like the rest of the auth bootstrap; the validator's bypass already includes it.
- Every mutation on an encryption-enabled session was reported as failed, and GraphQL errors in
  an encrypted reply were ignored: `knishio_client_execute_graphql()` read success, errors and
  `molecularHash` from the CipherHash envelope, a reply to a query, and only swapped in the
  decrypted reply's text afterwards. This hit token creation, transfers (single and
  multi-recipient), burns, buffer deposits and withdrawals, wallet creation, shadow-wallet claims
  and `knishio_client_propose_molecule()`. It now derives them from the decrypted reply as the
  original operation. A reply that cannot be decrypted fails closed with `CipherHash response could
  not be decrypted`, unless the envelope already carried GraphQL errors.
- `knishio_client_create_token()` released only the atom array of its molecule, leaking the C and I
  atoms and their meta on every call; it now frees them.

## [1.2.1] — 2026-09-23

### Added

- `knishio_molecule_from_json()`, which returned `KNISHIO_ERROR_NOT_IMPLEMENTED`, parses the
  molecule JSON every KnishIO SDK emits and takes every hashed atom field verbatim, so
  `knishio_molecule_check()` can verify another SDK's molecule. The field rules are in
  `knishio/molecule.h`; a molecule with any atom that fails to parse is rejected as a whole.
- `knishio_molecule_free_deep()` and `knishio_atom_free_deep()` release a molecule or atom together
  with the atoms and meta it owns, which `knishio_molecule_free()` and `knishio_atom_free()` do not.
  Use them for anything `knishio_molecule_from_json()` or `knishio_atom_from_json()` returns.

### Changed

- `knishio_atom_from_json()` parses every field `knishio_atom_to_json()` emits; it dropped
  `metaType`, `metaId`, `createdAt` and all meta. Release its result with `knishio_atom_free_deep()`.
- `knishio_atom_from_json()` and `knishio_molecule_from_json()` leave their output NULL on every
  failure, a NULL input included.
- `knishio_atom_from_json_string()`, `knishio_atom_from_json_obj()`,
  `knishio_molecule_from_json_obj()` and `knishio_json_parse_atom_array()`
  (`knishio/json/serializers.h`) parse through the same implementation and leave their outputs
  empty on every failure. They returned success with those four atom fields missing
  (`knishio_molecule_from_json_obj()` was a stub), and `knishio_json_parse_atom_array()` silently
  dropped atoms it could not parse; it now fails the whole array.

### Fixed

- The self-test's cross-validation verified nothing: a peer molecule passed if `molecularHash` was
  a string and `atoms` was non-empty, so `crossSdkCompatible: true` in 1.2.0 and earlier said
  nothing about peers' signatures. Each peer molecule now goes through
  `knishio_molecule_from_json()` and `knishio_molecule_check()`.
- `knishio_molecule_check()` rejected every U+I authorization molecule
  (`KNISHIO_ERROR_TRANSFER_MISMATCHED`) because it required every atom to carry atoms[0]'s token;
  the JS reference and the Rust SDK require that of V atoms only. The SDK's own client code never
  calls `knishio_molecule_check()`, so this affected verification only.
- The unit-test runner's exit status reflected only the last suite it ran, and three ctest entries
  ran the same full binary, so two failing molecule assertions (which expected an unsigned and a
  forged molecule to verify) went unnoticed. Each ctest entry now runs one suite, any failure
  fails the run, a `--filter` that matches no suite fails, and the assertions require rejection.

### Security

- `knishio_base64_decode()` read one and two bytes before its input when given an empty string.
  `knishio_molecule_check()` reaches it for an unsigned molecule (no `otsFragment` on any atom),
  and with this release that includes a peer molecule parsed by `knishio_molecule_from_json()`.
  Empty input is now rejected before the padding check; the function returns failure as before.

### Notes

- Atom `createdAt` must be whole seconds, since `knishio_atom_t.created_at` is `time_t` seconds; a
  sub-second value is rejected with `KNISHIO_ERROR_INVALID_JSON` rather than truncated, which would
  change the molecular hash.
- The P and A isotopes are not representable, and `knishio_molecule_check()` does not honour a
  `signingWallet` meta: it compares the signer with atoms[0]'s `walletAddress`.
- The rest of `knishio/json/serializers.h` is not the wire format (for example,
  `knishio_atom_to_json_obj()` writes `createdAt` in seconds); use `knishio_molecule_to_json()` and
  `knishio_atom_to_json()`.
- `CipherHashLive` reports Skipped, not Passed, when `CIPHERHASH_TEST_URL` is unset.
- Evidence: the aarch64 edge-kit rig (stock Ubuntu 22.04, no network). In its round 2 this SDK
  verifies all seven other SDKs' molecules (49/49 checks); in its round 3 it rejects all seven
  peers' molecules whose signature had one character changed, which the 1.2.0 self-test (its
  source built against this library) accepts.

## [1.2.0] — 2026-09-20

### Changed

- `knishio_client_execute_graphql` **fails closed**: `KNISHIO_ERROR_INVALID_STATE` when
  encryption is enabled but the wallet or the validator ML-KEM key is missing, and
  `KNISHIO_ERROR_CRYPTO` when the CipherHash envelope cannot be produced — instead of falling
  through to the plaintext request in either case (`src/client.c`). The bypass set is decided
  first, so the auth bootstrap still goes out in plaintext.

### Notes

- Registered as `knishio_tests --filter=encrypted_transport` (`tests/test_encrypted_transport.c`).
- Both live CipherHash cases (`tests/cipherhash_live_test.c`) passed against `testnet.knish.io`
  on 2026-09-20 at ML-KEM-1024 and ML-KEM-768, including the validator refusing a plaintext query
  from an `encrypt=true` session.

## [1.1.0] — 2026-09-12

### Added

- **Secret-storage envelope encryption layer** (`knishio_envelope_seal`, `knishio_envelope_open`, `knishio_envelope_seal_json`, `knishio_envelope_open_json` in `include/knishio/storage/envelope.h`):
  - Custody-agnostic AES-256-GCM envelope encryption with PBKDF2-HMAC-SHA256 key derivation (100,000 iterations, 16-byte random salt, 12-byte random IV, 16-byte tag appended to ciphertext, standard padded base64 encoding).
  - Versioned `knishio_encrypted_payload_t` and `knishio_secret_metadata_t` with camelCase JSON framing adhering to the cross-SDK contract (`bundleHash`, `createdAt`, `hardwareBacked`, `providerType`, optional `label` omitted when NULL).
  - Pluggable `knishio_storage_backend_t` interface with in-memory (`knishio_memory_storage_backend_create`) and atomic file-backed (`knishio_file_storage_backend_create`) implementations (0600 permissions, atomic tempfile rename).
  - `knishio_aes_gcm_encrypt_iv` and `knishio_aes_gcm_decrypt_iv` in `include/knishio/crypto/aes_gcm.h` exposing explicit-IV encryption/decryption.
  - Test suite in `tests/test_secret_storage.c` (CTest `test_secret_storage`) verifying cross-SDK decryption of frozen vector `vectors.secret_storage_envelope.tests[0]`, camelCase metadata contract, wrong-passphrase and corrupted-payload rejection, and storage backend operations.

- **Secret Recovery (`knishio_envelope_recover_secret` & `KNISHIO_RECOVERY_KEY_PREFIX`)**:
  - Added `#define KNISHIO_RECOVERY_KEY_PREFIX "knishio:recovery:"` in `include/knishio/storage/types.h`.
  - Added `recovery_passphrase` and `allow_unrecoverable` to `knishio_storage_options_t`.
  - Secondary recovery envelope: when `options->recovery_passphrase` is provided to `knishio_envelope_store_secret`, a secondary software AES-256-GCM envelope is sealed and stored under `knishio:recovery:<bundleHash>`.
  - Secret recovery: `knishio_envelope_recover_secret` opens the recovery envelope under `recovery_passphrase`, re-encrypts the master secret under `new_passphrase`, stores the new primary envelope, and zeroizes plaintext memory without leaking it to the caller.
  - Fail-closed semantics: wrong recovery passphrase or missing recovery envelope fails closed with an error code.
  - Secret deletion: `knishio_envelope_delete_secret` and `knishio_storage_backend_delete_secret` remove both `knishio:secret:<bundleHash>` and `knishio:recovery:<bundleHash>`.
  - Secret listing: `knishio_envelope_list_secrets` filters out all `knishio:recovery:` keys.
- **Secret storage provider vtable** (`include/knishio/storage/provider.h`): `knishio_secret_storage_provider_t` (`provider_type`, `is_hardware_backed`, `store_secret`, `retrieve_secret`, `delete_secret`, `has_secret`, `list_secrets`, `recover_secret`, `free_provider`) with `knishio_aes_gcm_secret_storage_provider_create(backend, default_passphrase, &out)` (`providerType` `aes-gcm`, software custody, delegates to `knishio_envelope_*`) and `knishio_secret_storage_provider_free`.
- **Client secret storage integration** (`include/knishio/client_ops.h`): `knishio_client_set_secret_storage(client, provider, bundle_hash, options)`, `knishio_client_get_secret_storage`, `knishio_client_retrieve_secret`. With a provider attached, `knishio_client_set_secret` stores the secret in the provider and drops the cleartext; `knishio_client_get_source_wallet` and `knishio_client_get_source_wallet_continuid` unwrap it just-in-time and zeroize it after use, so a client given only a bundle hash can sign without ever holding the cleartext. The provider and backend are not owned by the client — free them after `knishio_client_destroy`.

### Changed

- **Client secret held per client, not per process**: the process-global secret state is gone; each `knishio_client_t` owns its secret, bundle hash and storage options, and `knishio_client_destroy` zeroizes them with `knishio_secure_free`.

### Fixed

- **BREAKING:** `knishio_client_get_bundle` now returns the canonical 64-hex bundle hash (`SHAKE256(secret, 256)`, matching every other SDK); 1.0.0 returned 128 hex characters (`knishio_shake256_hash(secret, 512, …)`), which could not serve as a cross-SDK storage key.

## [1.0.0] — 2026-09-10

### Changed

- **ML-KEM-1024 is the default parameter set.** `knishio_client_config_t.mlkem_parameter_set`
  and `knishio_wallet_set_mlkem_param()` / `knishio_wallet_set_default_mlkem_param()` select
  `1024` (default) or `768` (opt-in step-back); a wallet's advertised `pubkey` is 1568 raw bytes
  (2092 base64 chars) instead of 1184 (1580). Encapsulation is unchanged and still rejects a
  recipient key whose decoded length is not a FIPS 203 ML-KEM public key.
- `KNISHIO_MAX_META_VALUE_LENGTH` raised from 2048 to 4096 so a 2092-char base64 ML-KEM-1024
  public key fits in atom meta (`walletPubkey` on the U-atom would otherwise be rejected).

### Added

- **A wallet now decrypts records addressed to its own ML-KEM-768 identity even when configured
  at ML-KEM-1024**, by deriving that identity on demand. The 64-byte (d‖z) seed is
  parameter-set-independent, so `src/crypto/cipher_hash.c` derives the sibling keypair, decapsulates
  with it, and releases it through `knishio_secure_zero()` immediately — it is never cached on the
  wallet. `knishio_cipher_hash_decrypt()` dispatches on the decoded ciphertext length, and its
  `CipherHash` map lookup tries the configured identity's `hashShare` first and then the sibling
  identity's, so an envelope addressed by a pre-bump sender is still found. Reading pre-bump 768
  records needs no configuration change. Encapsulation and the advertised public key are unchanged
  and remain single-set.
- `knishio_cipher_hash_decrypt_envelope()` — decrypt one `{cipherText, encryptedMessage}` envelope
  with a wallet's own identity at either parameter set.
- `tests/mlkem_backcompat.c` (CTest `MlkemBackCompat`) asserts, from a default (1024) build: the
  frozen cross-SDK ML-KEM-768 envelope decrypts directly; the advertised key is still 1568 bytes;
  a ciphertext matching neither set still returns `KNISHIO_ERROR_INVALID_ARGS`; a 768-addressed
  `CipherHash` map is found; a pre-bump session snapshot restores as 768; and the frozen pre-bump
  768 auth molecule (`vectors.legacyMlkem768AuthMolecule`) still recomputes to its recorded
  molecular hash with a 1184-byte `walletPubkey`. The C leg of that fixture covers
  molecular-hash validation rather than full `knishio_molecule_check()`, because the SDK has no
  molecule-from-JSON deserializer (`knishio_molecule_from_json()` returns
  `KNISHIO_ERROR_NOT_IMPLEMENTED`); building one is a feature, not a release change.

### Fixed

- **The auth-token session snapshot now records the wallet's ML-KEM parameter set, and
  `knishio_auth_token_restore()` honours it.** `wallet.mlKemParameterSet` joins `position` and
  `characters` in the snapshot JSON and in `knishio_auth_token_snapshot_t`. Restore resolves the
  set in three tiers: explicit field, else the decoded length of `pubkey` (1184 → 768, 1568 →
  1024), else 768. A session persisted by an 0.9.x build therefore restores as ML-KEM-768 instead
  of silently becoming 1024 and advertising a key the validator never recorded for that token.
- **`knishio_auth_token_get_snapshot()` emitted only the nested `wallet` object**, dropping
  `token`, `expiresAt`, `pubkey` and `encrypt`, so `knishio_auth_token_restore()` rejected the
  SDK's own snapshots. `knishio_json_builder_start_object()` discards the object under
  construction, so the streaming builder cannot nest; the snapshot is now built with cJSON
  directly. (`knishio_auth_token_get_auth_data()` has the same nesting defect and is unchanged
  here — it is a separate wire surface.)
- `knishio_client_set_cipher_context()` now keeps an owned copy of the AUTH wallet rather than a
  bare public/private ML-KEM key pair, which is what lets the transport derive the wallet's other
  identity for a legacy inbound envelope.

### Removed

- The ML-KEM-768 alias layer: `include/knishio/crypto/mlkem768.h`, its three
  `knishio_mlkem768_*_t` typedefs (`keypair`, `ciphertext`, `shared_secret`) and its thirteen
  `knishio_mlkem768_*` wrappers. Use the parameter-set-neutral `knishio/crypto/mlkem.h` API
  (`knishio_mlkem_keypair_from_seed()`, `knishio_mlkem_encapsulate()`,
  `knishio_mlkem_decapsulate()`, …), which dispatches on key/ciphertext length. No aliases
  retained. `src/crypto/mlkem768.c` is renamed `src/crypto/mlkem.c`.
- `knishio_cipher_hash_decrypt()`'s `(my_pubkey_b64, my_privkey, my_privkey_len)` parameters,
  replaced by the `const knishio_wallet_t*` the dual-identity path requires.

### Notes

- `0.9.5`–`0.9.9` were never released; the ML-KEM-1024 cutover is a breaking API change and takes
  the 1.0.0 line.
- Nothing on the wire changed and no hashed bytes changed: the parameter set is recoverable from
  FIPS 203's disjoint key/ciphertext lengths, so no migration is required.

## [0.9.4] — 2026-08-17

### Security

- **ML-KEM-768 encapsulation now draws its randomness from the platform CSPRNG.** It
  previously used the mlkem-native *test* RNG stub (`external/mlkem-native/test/notrandombytes/notrandombytes.c`,
  a SURF PRNG seeded with a compile-time constant — the digits of π), which was compiled
  into the shipped library and supplied the only `randombytes()` in the link. Because ML-KEM
  derandomizes K-PKE (`r = G(m ‖ H(ek))`), the 32-byte message `m` is the sole entropy in an
  encapsulation — so every process replayed the same sequence of ciphertexts and shared
  secrets, making the AES-256-GCM key of any *sent* message recoverable by an attacker.
  `src/crypto/mlkem768.c` now generates `m` with `knishio_secure_random` and calls the
  derandomized `crypto_kem_enc_derand`; `MLK_CONFIG_NO_RANDOMIZED_API` compiles out the
  randomized path (and the stub) entirely, and `notrandombytes.c` is removed from the build.
  Decapsulation, key generation, and cross-SDK parity are byte-unchanged (self-test 11/11,
  Cross-SDK ✅, 6 frozen molecular hashes intact). A new cross-process determinism test
  (`tests/mlkem_encaps_entropy.c`, CTest `MlkemEncapsEntropy`) fails on the old behavior and
  passes on the fix.

## [0.9.3] — 2026-08-05

### Security

- **The SDK now verifies one-time signatures. It previously did not.**
  `src/libraries/check_molecule.c` — the complete `CheckMolecule.js` port, with OTS,
  ContinuID, batch-ID and every isotope check — had been excluded from the build because it
  and `src/molecule.c` both defined `knishio_molecule_check` and macOS `ld64` rejects the
  duplicate. What shipped was a ~110-line subset carrying the comment *"Skip complex OTS
  verification for now"*. **Any molecule whose molecular hash was internally consistent was
  accepted, forged signature and all** — the hash does not cover `otsFragment`, so
  overwriting a signature fragment produced a molecule the SDK called valid.
  The complete verifier is now compiled and is the sole definition; the subset is deleted.
  Observed directly: a molecule with a forged OTS fragment was accepted before and is now
  rejected with `KNISHIO_ERROR_SIGNATURE_MISMATCH`.
- **`check_ots` had two memory defects**, invisible because the code had never run:
  `knishio_molecule_normalize_hash` was called with one pointer as both input and output
  (leaking the enumerated array), and every atom's OTS fragment was `strcat`ed into a fixed
  `malloc(8192)` with no bounds check.

### Added

- **`wotsRoundtrip` self-test**, driven by the canonical `wots_roundtrip` vector. Asserts
  the two-pass OTS address derivation directly, independent of any molecule. C and C++ were
  the only SDKs without this coverage; C++ remains without it.
- **B (buffer) isotope.** `KNISHIO_ISOTOPE_B` added to `knishio_isotope_t`, with
  `knishio_molecule_init_deposit_buffer` and `knishio_molecule_init_withdraw_buffer`
  emitting the canonical three-atom molecules — `V(-balance) → B(+amount) → V(+remainder)`
  for a deposit, `B(-balance) → V(+amount) → B(+remainder)` for a withdraw. The full source
  balance is always debited so a partial operation still conserves. C was the last SDK
  without buffer support.
- **Cross-isotope conservation in the verifier.** `knishio_molecule_check` now recognises
  molecules mixing V with B or F, skips the V-only sum (which cannot balance for them), and
  validates the combined V+B / V+F sum instead, requiring each B/F atom to carry
  `metaType: "walletBundle"` and a non-empty `metaId`, and rejecting negative F values.
- **`bufferFamily` self-test**, driven by `canonical-patent-vectors.json`: four positive
  vectors plus four negative vectors that build a valid molecule, tamper one field, re-sign
  and require rejection. The canonical vectors are vendored at
  `tests/fixtures/canonical-patent-vectors.json`.
- `KNISHIO_ISOTOPE_COUNT` sentinel and a `_Static_assert` binding `isotope_strings[]` to
  the enum, so adding an isotope without its string is now a compile error.

### Fixed

- **Fusion (F) molecules produced a different molecular hash from every other SDK.**
  `KNISHIO_ISOTOPE_F` had been added to the enum but never to `isotope_strings[]`, so
  `knishio_isotope_to_string()` returned `NULL` for it, the JSON serializer silently
  omitted the `isotope` field, and — `isotope` being a hashed property — every F-bearing
  molecule this SDK built hashed differently and would have been rejected cross-SDK.
  Affects `knishio_client_*` paths that create F atoms (`src/operations/token.c`).
- A second copy of the same defect: a hand-rolled isotope→string chain in
  `src/libraries/check_molecule.c` missing `L`, `S` and `F`, replaced with a call to
  `knishio_isotope_to_string()`.
- **`knishio_generate_wallet_key` rejected every secret that was not exactly 2048 hex
  characters.** Nothing in the derivation requires that — the secret is consumed as a
  BigInt — and JavaScript imposes no such limit. Since `generateSecret` emits 1024 hex
  chars in JS, TypeScript and Rust, **C could not derive a wallet from any secret those
  SDKs produce.** Now accepts any non-empty hex secret; the canonical `wots_roundtrip`
  vector pins the result, so the relaxation is verified against five other SDKs rather than
  merely permitted.
- **`check_molecular_hash` reimplemented molecular hashing by hand** — bubble-sorting atoms
  and concatenating hashable values into a fixed buffer instead of calling
  `knishio_molecule_generate_hash`. This third implementation disagreed with the canonical
  one and rejected *every* molecule. It shipped undetected only because the file containing
  it was not compiled. It now delegates.
- The two copies of `check_cross_isotope_conservation` had diverged; the stricter
  (`long double`, `1e-9` tolerance, `errno`/ERANGE handling, C++ parity) copy was kept.
- `config.h`, `cmake_install.cmake` and `KnishIOClientConfigVersion.cmake` were tracked in
  git despite being listed in `.gitignore` — they are build outputs, and the committed
  copies were stale (claiming version 0.6.4 and referencing a drive path that no longer
  exists). Now untracked; CMake generates them into the build directory.

### Changed

- **BREAKING — `knishio_client_deposit_buffer_token` and
  `knishio_client_withdraw_buffer_token` changed signature and wire format.** They
  previously built a single `V` atom of `+amount` with `metaType: "buffer"` and ad-hoc
  `buffer_id`/`operation` meta: no `B` isotope, no conservation (one positive atom with
  nothing debited), and an amount rendered via `"%.8f"` as e.g. `"30.00000000"`, which is
  rejected outright because KnishIO amounts are integer strings. They now build via the
  new molecule builders.
  - `amount` is `int`, was `double`.
  - `buffer_id` is removed — it had no analogue in the protocol. The buffer wallet is
    derived internally, as in every other SDK.
  - withdraw takes `recipient_bundle`, which the recipient `V` atom needs for its `metaId`.
- README buffer-token examples corrected — they previously passed string amounts, a
  `tradeRates` argument no version of the function accepted, and `char*` result pointers.
- `nacl` cross-platform test fixtures synced from the shared canonical master.

- **`knishio_molecule_check` returns a distinct error code per failing check** —
  `SIGNATURE_MISMATCH`, `MOLECULAR_HASH_MISMATCH`, `BATCH_ID`, `TRANSFER_UNBALANCED`,
  `META_MISSING`, `ATOMS_MISSING`, `MOLECULAR_HASH_MISSING` — instead of a blanket
  `INVALID_STATE`. Callers distinguishing rejection reasons should switch on the specific
  codes; `INVALID_STATE` still covers the remaining isotope checks.

### Changed — cross-SDK gauntlet reporting integrity

- The self-test now publishes cross-validation **coverage**, not just a verdict:
  `crossValidation.{ran,targetsExpected,targetsValidated}` and `runId` sit alongside
  `crossSdkCompatible` in the results file. The boolean alone could not distinguish
  "validated every peer, all passed" from "validated nothing and so found no failures".
- `crossSdkCompatible` now defaults to **false** and must be earned.
- Cross-validation **fails** instead of reporting "compatible" when the shared results
  directory is missing or holds no peer results. Absence of evidence is not evidence of
  compatibility.
- Round 1 no longer asserts a cross-SDK verdict it cannot have; it records that no
  cross-validation ran.
- A coverage floor is required before a pass: every expected peer must have been validated,
  in addition to no individual check having failed.
  The previous test was `passed_validations == total_validations`, vacuously true at
  `0 == 0`, so validating nothing reported full cross-SDK compatibility. A peer whose
  results file is absent now counts as unvalidated rather than being skipped silently.
- Each peer is now checked for all 7 required molecule types. The validation loop iterates
  the molecule keys that are **present**, so an omitted molecule was indistinguishable from
  a validated one.

Contract for these fields: `sdks/canonical-test-keys.json` in the KnishIOClientSDK
monorepo. Audit: `docs/audits/REPORTING-INTEGRITY-2026-08-05.md`.

### Known issues

- `check_isotope_v` uses `double`/`atof` with a `< 0.01` tolerance; KnishIO amounts are
  i128 integer strings, so values above 2⁵³ lose precision silently.
- `knishio/utils/string.h` declares `knishio_is_hex_string`, `knishio_string_split`,
  `knishio_string_is_base` and `knishio_string_chunk`, none of which are defined anywhere.
  Calling one produces a link error.
- `knishio_molecule_from_json` cannot parse molecules this SDK itself serialises. It has no
  callers, so nothing depends on it.
- No SDK's verifier is exercised against another SDK's molecule anywhere in the cross-SDK
  suite; round-2 validation compares molecular-hash shape only. OTS verification here is
  proven against this SDK's own molecules.


## [0.9.2] — 2026-07-12

Coordinated dependency-security release across all 8 SDKs. Release record:
`docs/sdk-release-0.9.2-execution-2026-07-12.md` (monorepo).

### Security

- `mlkem-native` bumped to v1.2.0, picking up upstream zeroization and an
  x86-64 assembly overread fix.

### Changed

- Unity test framework bumped to 2.6.1.
- Version constants in `knishio.h` bumped alongside the CMake project version.

### Fixed

- Round-2 self-test results are preserved rather than clobbered, which had made
  cross-SDK validation flaky when rounds ran in parallel.

### Notes

- `0.9.1` was staged in `CMakeLists.txt` on 2026-06-30 (an actionable message for
  the ML-KEM key-size guard) but was never tagged. That change ships in `0.9.2`.

## [0.9.0] — 2026-06-29

Coordinated `0.9.0` across all 8 SDKs, marking the post-quantum ML-KEM transport
milestone. The C SDK was the last to complete the transport. Runbook:
`docs/sdk-release-audit-2026-06-29.md` (monorepo).

### Added

- **ML-KEM768 CipherHash encrypted transport** (PQ Phase E), backed by
  `mlkem-native` vendored as a git submodule at `external/mlkem-native` — clone
  with `--recursive`.
- Live wiring of the whole client: GraphQL transport with TLS and
  `X-Auth-Token`, U-isotope auth molecule → bundle-scoped JWT, ContinuID position
  resolution, and `create_token` round-trips accepted by a live validator.
- Stackable (NFT) support: `TokenUnit` substrate, `split_units`, `tokenUnits`
  emission and response parsing, `query_balance_wallet`, and a multi-recipient
  stackable transfer builder (`transfer_tokens_multi`).
- `tokenCreation`, `walletCreation`, and `shadowWalletClaim` cross-SDK parity
  (8/8 each), plus the `mlkem768` keygen + decrypt vector and a "decrypt their
  message" ML-KEM768 cross-validation.
- A clang-tidy lint gate (`bugprone-*` / `performance-*`) and the repo's first CI
  workflow — build, self-test, unit suites, and lint, all born-enforced.
- Unit test suite resurrected behind `KNISHIO_BUILD_TESTS`.

### Fixed

- **Security:** wallet position and salt generation moved off a time-seeded
  `rand()` onto a CSPRNG.
- Use-after-free in the JSON helper `get_string_path` (SDK-wide), and OTS reuse
  on re-authentication.
- Client transfer rebuilt from a broken single-atom form to the canonical
  3-atom `init_value`; burn rebuilt as a canonical 3-V-atom zero-sum.
- Wire serializer JSON-escapes string values, which had blocked stackable
  `createToken`.
- Legacy `query_balance` modernized (bundle hash + auth + transport);
  `ProposeMolecule` uses the camelCase schema; the `/graphql` path is no longer
  doubled; an insecure-TLS option was added for local development.
- Two GraphQL pointer bugs found by the new lint gate; `_GNU_SOURCE` defined
  globally for Linux glibc.
- Unconditional DEBUG output on production code paths silenced.

### Removed

- Dead `QueryUserActivity` query.

### Notes

- Local version `0.8.1` was staged on 2026-06-16 (the tokenCreation /
  walletCreation / shadowWalletClaim parity work) but was never tagged; it
  reaches consumers here.

## [0.8.0] — 2026-06-15

First tagged release of the C SDK. The manifest had carried an inherited `0.6.4`
since the initial commit; it was set to `0.8.0` to join the coordinated SDK
version line.

### Fixed

- Molecule construction reconciled to the JS reference, bringing the C SDK into
  7-SDK molecular-hash parity.
- Build repaired for release: policy handling aligned to molecule-building, and
  duplicate symbols de-duplicated.

### Added

- README, LICENSE, and examples (from the 2025-10-08 initial import).

[Unreleased]: https://github.com/WishKnish/KnishIO-Client-C/compare/1.2.1...HEAD
[1.2.1]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/1.2.1
[1.2.0]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/1.2.0
[1.1.0]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/1.1.0
[1.0.0]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/1.0.0
[0.9.4]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/0.9.4
[0.9.3]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/0.9.3
[0.9.2]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/0.9.2
[0.9.0]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/0.9.0
[0.8.0]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/0.8.0
