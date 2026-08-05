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

[Unreleased]: https://github.com/WishKnish/KnishIO-Client-C/compare/0.9.2...HEAD
[0.9.2]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/0.9.2
[0.9.0]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/0.9.0
[0.8.0]: https://github.com/WishKnish/KnishIO-Client-C/releases/tag/0.8.0
