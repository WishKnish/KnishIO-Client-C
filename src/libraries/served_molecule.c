/**
 * @file served_molecule.c
 * @brief Verify a molecule exactly as a validator's GraphQL API serves it
 *
 * The served shape differs from the client JSON that knishio_molecule_from_json() reads:
 * bundleHash instead of bundle, tokenSlug instead of token, meta as a metasJson string, and
 * createdAt as an RFC 3339 string (or a millisecond string or number). Atom createdAt values
 * keep their milliseconds here (knishio_atom_t.created_at_ms), so molecules signed at any
 * millisecond verify. Mirrors KnishIO-Client-JS CheckMolecule.fromServerData() and
 * CheckMolecule.verifyFromServerData().
 */

#include "knishio/knishio.h"
#include "knishio/atom.h"
#include "knishio/meta.h"
#include "knishio/molecule.h"
#include "knishio/error/context.h"
#include "knishio/utils/memory.h"
#include "../atom_internal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Atom versions retried, in order, when a molecule is served without atom versions. */
static const char *const UNSERVED_ATOM_VERSIONS[] = { "4", "3" };

typedef struct {
    knishio_error_t code;
    char message[256];
} build_error_t;

static knishio_error_t fail(build_error_t *why, knishio_error_t code, const char *message) {
    why->code = code;
    snprintf(why->message, sizeof(why->message), "%s", message);
    return code;
}

/* ------------------------------------------------------------------------------------------ */
/* createdAt normalization                                                                     */
/* ------------------------------------------------------------------------------------------ */

/* Howard Hinnant's days_from_civil: days since 1970-01-01 for a proleptic Gregorian date. */
static long long days_from_civil(long long y, unsigned m, unsigned d) {
    y -= (m <= 2) ? 1 : 0;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - (era * 400));
    const unsigned doy = (((153 * (m > 2 ? m - 3 : m + 9)) + 2) / 5) + d - 1;
    const unsigned doe = (yoe * 365) + (yoe / 4) - (yoe / 100) + doy;
    return (era * 146097) + (long long)doe - 719468;
}

static bool read_digits(const char *s, size_t n, unsigned *out) {
    unsigned v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        v = (v * 10) + (unsigned)(s[i] - '0');
    }
    *out = v;
    return true;
}

/* ^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})$ -> epoch milliseconds, the
 * fraction truncated to 3 digits and the offset applied. False for anything else, including
 * out-of-range fields. */
static bool rfc3339_to_ms(const char *s, long long *ms) {
    const size_t len = strlen(s);
    unsigned year = 0;
    unsigned month = 0;
    unsigned day = 0;
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
    if (len < 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':'
        || !read_digits(s, 4, &year) || !read_digits(s + 5, 2, &month)
        || !read_digits(s + 8, 2, &day) || !read_digits(s + 11, 2, &hour)
        || !read_digits(s + 14, 2, &minute) || !read_digits(s + 17, 2, &second)) {
        return false;
    }

    size_t pos = 19;
    unsigned fraction_ms = 0;
    if (s[pos] == '.') {
        pos++;
        const size_t start = pos;
        while (pos < len && s[pos] >= '0' && s[pos] <= '9') {
            if (pos - start < 3) {
                fraction_ms = (fraction_ms * 10) + (unsigned)(s[pos] - '0');
            }
            pos++;
        }
        if (pos == start) {
            return false;
        }
        for (size_t digits = pos - start; digits < 3; digits++) {
            fraction_ms *= 10;
        }
    }

    long long offset_seconds = 0;
    if (pos + 1 == len && s[pos] == 'Z') {
        offset_seconds = 0;
    } else if (pos + 6 == len && (s[pos] == '+' || s[pos] == '-') && s[pos + 3] == ':') {
        unsigned offset_hour = 0;
        unsigned offset_minute = 0;
        if (!read_digits(s + pos + 1, 2, &offset_hour) || !read_digits(s + pos + 4, 2, &offset_minute)
            || offset_hour > 23 || offset_minute > 59) {
            return false;
        }
        offset_seconds = ((long long)offset_hour * 3600LL) + ((long long)offset_minute * 60LL);
        if (s[pos] == '-') {
            offset_seconds = -offset_seconds;
        }
    } else {
        return false;
    }

    static const unsigned month_days[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month < 1 || month > 12 || day < 1 || day > month_days[month - 1]
        || (month == 2 && day == 29 && !leap) || hour > 23 || minute > 59 || second > 59) {
        return false;
    }

    const long long seconds = (days_from_civil(year, month, day) * 86400LL)
        + ((long long)hour * 3600LL) + ((long long)minute * 60LL) + (long long)second - offset_seconds;
    *ms = (seconds * 1000LL) + fraction_ms;
    return true;
}

typedef enum {
    CREATED_AT_ABSENT,
    CREATED_AT_MS,
    CREATED_AT_INVALID
} created_at_kind_t;

/* JS toSignedTimestamp, narrowed to what knishio_atom_t can hold: null, "", a non-integer
 * number or a non-string value is absent; an integer or a digit string is milliseconds; an
 * RFC 3339 string becomes epoch milliseconds; any other string (or a negative or oversized
 * value) is invalid. */
static created_at_kind_t normalize_created_at(const cJSON *item, long long *ms) {
    if (!item || cJSON_IsNull(item)) {
        return CREATED_AT_ABSENT;
    }
    if (cJSON_IsNumber(item)) {
        const double v = item->valuedouble;
        /* Number.isSafeInteger */
        if (v != (double)(long long)v || v > 9007199254740991.0 || v < -9007199254740991.0) {
            return CREATED_AT_ABSENT;
        }
        if (v < 0) {
            return CREATED_AT_INVALID;
        }
        *ms = (long long)v;
        return CREATED_AT_MS;
    }
    if (!cJSON_IsString(item)) {
        return CREATED_AT_ABSENT;
    }
    const char *text = item->valuestring;
    if (text[0] == '\0') {
        return CREATED_AT_ABSENT;
    }
    if (knishio_cjson_millis(text, ms)) {
        return CREATED_AT_MS;
    }
    if (rfc3339_to_ms(text, ms) && *ms >= 0) {
        return CREATED_AT_MS;
    }
    return CREATED_AT_INVALID;
}

/* ------------------------------------------------------------------------------------------ */
/* Served -> molecule                                                                          */
/* ------------------------------------------------------------------------------------------ */

/* JS `x || null` on a served string field: an empty or absent string is NULL. */
static bool served_string(const cJSON *json, const char *key, const char **out) {
    if (!knishio_cjson_optional_string(json, key, out)) {
        return false;
    }
    if (*out && (*out)[0] == '\0') {
        *out = NULL;
    }
    return true;
}

/* JS String(value) for a scalar: a string as is, a number as its shortest decimal string,
 * a boolean as "true"/"false". */
static bool scalar_string(const cJSON *item, char *buf, size_t size, const char **out) {
    if (cJSON_IsString(item)) {
        *out = item->valuestring;
        return true;
    }
    if (cJSON_IsNumber(item)) {
        if (!knishio_json_number_to_string(item->valuedouble, buf, size)) {
            return false;
        }
        *out = buf;
        return true;
    }
    if (cJSON_IsBool(item)) {
        *out = cJSON_IsTrue(item) ? "true" : "false";
        return true;
    }
    return false;
}

static knishio_error_t add_served_meta(knishio_atom_t *atom, const char *key, const cJSON *value,
                                       build_error_t *why) {
    char number[64];
    const char *text = NULL;
    const bool is_null = !value || cJSON_IsNull(value);
    if (!is_null && !scalar_string(value, number, sizeof(number), &text)) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: unsupported meta value");
    }

    knishio_meta_t *meta = NULL;
    knishio_error_t err = knishio_meta_create(&meta, key, is_null ? "" : text);
    if (err != KNISHIO_SUCCESS) {
        return fail(why, err, "invalid served molecule: meta entry");
    }
    if (is_null) {
        /* A null meta value is kept: the legacy hash skips it, the Version4 view hashes it. */
        knishio_free(meta->value);
        meta->value = NULL;
    }
    if ((err = knishio_atom_add_meta(atom, meta)) != KNISHIO_SUCCESS) {
        knishio_meta_free(meta);
        return fail(why, err, "invalid served molecule: meta entry");
    }
    return KNISHIO_SUCCESS;
}

/* metasJson: a JSON string holding [{key, value}] or {key: value}; anything else is no meta. */
static knishio_error_t add_served_metas(knishio_atom_t *atom, const cJSON *metas_json,
                                        build_error_t *why) {
    if (!cJSON_IsString(metas_json)) {
        return KNISHIO_SUCCESS;
    }
    cJSON *parsed = cJSON_Parse(metas_json->valuestring);
    if (!parsed) {
        return KNISHIO_SUCCESS;
    }

    knishio_error_t err = KNISHIO_SUCCESS;
    const cJSON *entry = NULL;
    if (cJSON_IsArray(parsed)) {
        cJSON_ArrayForEach(entry, parsed) {
            const cJSON *key = cJSON_GetObjectItemCaseSensitive(entry, "key");
            if (!cJSON_IsObject(entry) || !cJSON_IsString(key)) {
                err = fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: meta entry");
                break;
            }
            err = add_served_meta(atom, key->valuestring,
                                  cJSON_GetObjectItemCaseSensitive(entry, "value"), why);
            if (err != KNISHIO_SUCCESS) {
                break;
            }
        }
    } else if (cJSON_IsObject(parsed)) {
        cJSON_ArrayForEach(entry, parsed) {
            err = add_served_meta(atom, entry->string, entry, why);
            if (err != KNISHIO_SUCCESS) {
                break;
            }
        }
    }
    cJSON_Delete(parsed);
    return err;
}

static knishio_error_t build_served_atom(const cJSON *json, const char *version_override,
                                         knishio_atom_t **out, build_error_t *why) {
    *out = NULL;
    if (!cJSON_IsObject(json)) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: atom is not an object");
    }

    const char *position = NULL;
    const char *wallet_address = NULL;
    const char *isotope_text = NULL;
    const char *token = NULL;
    const char *batch_id = NULL;
    const char *meta_type = NULL;
    const char *meta_id = NULL;
    const char *ots_fragment = NULL;
    if (!served_string(json, "position", &position)
        || !served_string(json, "walletAddress", &wallet_address)
        || !served_string(json, "isotope", &isotope_text)
        || !served_string(json, "tokenSlug", &token)
        || (!token && !served_string(json, "token", &token))
        || !served_string(json, "batchId", &batch_id)
        || !served_string(json, "metaType", &meta_type)
        || !served_string(json, "metaId", &meta_id)
        || !served_string(json, "otsFragment", &ots_fragment)) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: atom field type");
    }

    /* Unknown includes the JS-only P and A isotopes, which knishio_isotope_t cannot represent. */
    const knishio_isotope_t isotope = isotope_text ? knishio_isotope_from_string(isotope_text)
                                                   : KNISHIO_ISOTOPE_UNKNOWN;
    if (isotope == KNISHIO_ISOTOPE_UNKNOWN) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: atom isotope");
    }

    char value_number[64];
    const char *value = NULL;
    const cJSON *value_item = cJSON_GetObjectItemCaseSensitive(json, "value");
    if (value_item && !cJSON_IsNull(value_item)
        && !scalar_string(value_item, value_number, sizeof(value_number), &value)) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: atom value");
    }

    char version_number[64];
    const char *version = version_override;
    if (!version) {
        const cJSON *version_item = cJSON_GetObjectItemCaseSensitive(json, "version");
        if (version_item && !cJSON_IsNull(version_item)
            && !scalar_string(version_item, version_number, sizeof(version_number), &version)) {
            return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: atom version");
        }
    }

    const cJSON *index_item = cJSON_GetObjectItemCaseSensitive(json, "index");
    if (!cJSON_IsNumber(index_item) || !(index_item->valuedouble >= 0.0 && index_item->valuedouble <= (double)INT_MAX)
        || index_item->valuedouble != (double)(int)index_item->valuedouble) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: atom index");
    }

    long long created_ms = 0;
    const created_at_kind_t created =
        normalize_created_at(cJSON_GetObjectItemCaseSensitive(json, "createdAt"), &created_ms);
    if (created == CREATED_AT_INVALID) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid createdAt");
    }

    /* The constructor requires a token; a served atom without one keeps a NULL token. */
    knishio_atom_t *atom = NULL;
    knishio_error_t err = knishio_atom_create_with_meta(&atom,
        position ? position : "", wallet_address ? wallet_address : "", isotope,
        token ? token : "", value, batch_id, meta_type, meta_id, NULL, 0);
    if (err != KNISHIO_SUCCESS) {
        return fail(why, err == KNISHIO_ERROR_MEMORY ? err : KNISHIO_ERROR_INVALID_JSON,
                    "invalid served molecule: atom fields");
    }
    if (!token) {
        knishio_free(atom->token);
        atom->token = NULL;
    }
    atom->index = (int)index_item->valuedouble;
    if (created == CREATED_AT_MS) {
        atom->created_at = (time_t)(created_ms / 1000);
        atom->created_at_ms = created_ms;
    }

    if ((version && (err = knishio_atom_set_version(atom, version)) != KNISHIO_SUCCESS)
        || (ots_fragment && (err = knishio_atom_set_ots_fragment(atom, ots_fragment)) != KNISHIO_SUCCESS)) {
        knishio_atom_free_deep(atom);
        return fail(why, err, "invalid served molecule: atom fields");
    }
    if ((err = add_served_metas(atom, cJSON_GetObjectItemCaseSensitive(json, "metasJson"), why))
        != KNISHIO_SUCCESS) {
        knishio_atom_free_deep(atom);
        return err;
    }

    *out = atom;
    return KNISHIO_SUCCESS;
}

static knishio_error_t build_served_molecule(const cJSON *root, const char *version_override,
                                             knishio_molecule_t **out, build_error_t *why) {
    *out = NULL;
    if (!cJSON_IsObject(root)) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: not a JSON object");
    }
    const cJSON *atoms = cJSON_GetObjectItemCaseSensitive(root, "atoms");
    if (!cJSON_IsArray(atoms)) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: atoms missing");
    }

    const char *molecular_hash = NULL;
    const char *bundle = NULL;
    const char *cell_slug = NULL;
    if (!knishio_cjson_optional_string(root, "molecularHash", &molecular_hash)
        || !served_string(root, "bundleHash", &bundle)
        || !served_string(root, "cellSlug", &cell_slug)) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid served molecule: field type");
    }
    long long created_ms = 0;
    const created_at_kind_t created =
        normalize_created_at(cJSON_GetObjectItemCaseSensitive(root, "createdAt"), &created_ms);
    if (created == CREATED_AT_INVALID) {
        return fail(why, KNISHIO_ERROR_INVALID_JSON, "invalid createdAt");
    }

    knishio_molecule_t *mol = NULL;
    knishio_error_t err = knishio_molecule_create(&mol, NULL, bundle, NULL, NULL, cell_slug, NULL);
    if (err != KNISHIO_SUCCESS) {
        return fail(why, err, "invalid served molecule: molecule fields");
    }

    const cJSON *atom_json = NULL;
    cJSON_ArrayForEach(atom_json, atoms) {
        knishio_atom_t *atom = NULL;
        if ((err = build_served_atom(atom_json, version_override, &atom, why)) != KNISHIO_SUCCESS) {
            knishio_molecule_free_deep(mol);
            return err;
        }
        /* knishio_molecule_add_atom() overwrites the index with the insertion position, but the
         * index is what orders the atoms for hashing, so the served value is restored. */
        const int index = atom->index;
        if ((err = knishio_molecule_add_atom(mol, atom)) != KNISHIO_SUCCESS) {
            knishio_atom_free_deep(atom);
            knishio_molecule_free_deep(mol);
            return fail(why, err, "invalid served molecule: too many atoms");
        }
        atom->index = index;
    }

    /* Set last: knishio_molecule_add_atom() frees the molecular hash on every call. */
    if (molecular_hash) {
        mol->molecular_hash = knishio_strdup(molecular_hash);
        if (!mol->molecular_hash) {
            knishio_molecule_free_deep(mol);
            return fail(why, KNISHIO_ERROR_MEMORY, "out of memory");
        }
    }
    if (created == CREATED_AT_MS) {
        mol->created_at = (time_t)(created_ms / 1000);
    }

    *out = mol;
    return KNISHIO_SUCCESS;
}

knishio_error_t knishio_molecule_from_server_data(const char *served_json,
                                                  knishio_molecule_t **molecule) {
    if (molecule) {
        *molecule = NULL;
    }
    if (!served_json || !molecule) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    cJSON *root = cJSON_Parse(served_json);
    if (!root) {
        return KNISHIO_ERROR_JSON_PARSE;
    }
    build_error_t why = { KNISHIO_SUCCESS, "" };
    const knishio_error_t err = build_served_molecule(root, NULL, molecule, &why);
    cJSON_Delete(root);
    return err;
}

/* ------------------------------------------------------------------------------------------ */
/* Verification                                                                                */
/* ------------------------------------------------------------------------------------------ */

/* One build + knishio_molecule_check(); returns the check's code and fills out->error. */
static knishio_error_t verify_once(const cJSON *root, const char *version_override,
                                   knishio_served_verification_t *out) {
    build_error_t why = { KNISHIO_SUCCESS, "" };
    knishio_molecule_t *mol = NULL;
    knishio_error_t code = build_served_molecule(root, version_override, &mol, &why);
    if (code != KNISHIO_SUCCESS) {
        out->verified = false;
        snprintf(out->error, sizeof(out->error), "%s", why.message);
        return code;
    }

    code = knishio_molecule_check(mol, NULL);
    knishio_molecule_free_deep(mol);
    out->verified = (code == KNISHIO_SUCCESS);
    if (out->verified) {
        out->error[0] = '\0';
    } else {
        const char *message = knishio_error_to_string(code);
        snprintf(out->error, sizeof(out->error), "%s", message ? message : "verification failed");
    }
    return code;
}

knishio_error_t knishio_verify_from_server_data(const char *served_json,
                                                knishio_served_verification_t *out) {
    if (!served_json || !out) {
        return KNISHIO_ERROR_INVALID_ARGS;
    }
    memset(out, 0, sizeof(*out));

    cJSON *root = cJSON_Parse(served_json);
    if (!root) {
        snprintf(out->error, sizeof(out->error), "invalid served molecule: not JSON");
        return KNISHIO_SUCCESS;
    }

    const cJSON *hash = cJSON_GetObjectItemCaseSensitive(root, "molecularHash");
    if (cJSON_IsString(hash) && strlen(hash->valuestring) < sizeof(out->molecular_hash)) {
        snprintf(out->molecular_hash, sizeof(out->molecular_hash), "%s", hash->valuestring);
    }

    const knishio_error_t code = verify_once(root, NULL, out);

    /* A molecule signed with versioned atoms but served without its atom versions only matches
     * its hash when rebuilt with that version, so "4" and then "3" are tried. A served version
     * disables the retry. */
    bool served_version = false;
    const cJSON *atoms = cJSON_GetObjectItemCaseSensitive(root, "atoms");
    const cJSON *atom = NULL;
    if (cJSON_IsArray(atoms)) {
        cJSON_ArrayForEach(atom, atoms) {
            const cJSON *version = cJSON_GetObjectItemCaseSensitive(atom, "version");
            if (version && !cJSON_IsNull(version)) {
                served_version = true;
                break;
            }
        }
    }
    if (code == KNISHIO_ERROR_MOLECULAR_HASH_MISMATCH && !served_version) {
        for (size_t i = 0; i < sizeof(UNSERVED_ATOM_VERSIONS) / sizeof(UNSERVED_ATOM_VERSIONS[0]); i++) {
            if (verify_once(root, UNSERVED_ATOM_VERSIONS[i], out) == KNISHIO_SUCCESS) {
                break;
            }
        }
    }

    cJSON_Delete(root);
    return KNISHIO_SUCCESS;
}
