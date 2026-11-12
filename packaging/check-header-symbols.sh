#!/usr/bin/env bash
# check-header-symbols.sh <include-dir> <static-library>
#
# Every function an installed header declares must be defined by the library. Collects each
# `knishio_<name>(` in the headers under <include-dir> (comments removed; names defined as
# macros or as static inline functions in a header, function-pointer types and the
# debug-only #ifdef KNISHIO_DEBUG blocks are skipped) and fails, listing the
# missing names, when `nm -g <static-library>` has no defined symbol of that name.
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <include-dir> <static-library>" >&2
    exit 2
fi
INCLUDE_DIR=$1
LIB=$2
[ -d "$INCLUDE_DIR" ] || { echo "FAIL: no include directory $INCLUDE_DIR" >&2; exit 1; }
[ -f "$LIB" ] || { echo "FAIL: no library $LIB" >&2; exit 1; }

W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

# All header text with /* */ and // comments removed.
find "$INCLUDE_DIR" -type f -name '*.h' -print0 | LC_ALL=C sort -z | xargs -0 cat \
    | perl -0777 -pe 's{/\*.*?\*/}{ }gs; s{//[^\n]*}{}g; s{#ifdef\s+KNISHIO_DEBUG\b.*?#endif}{}gs' >"$W/headers.txt"

# Names that need no symbol: function-like macros and static inline definitions.
perl -ne 'print "$1\n" while /#\s*define\s+(knishio_\w+)\s*\(/g' "$W/headers.txt" >"$W/exempt.txt"
perl -0777 -ne 'print "$1\n" while /static\s+inline\b[^;{(]*?\b(knishio_\w+)\s*\(/g' "$W/headers.txt" >>"$W/exempt.txt"

perl -ne 'print "$1\n" while /\b(knishio_\w+)\s*\((?!\s*\*)/g' "$W/headers.txt" | LC_ALL=C sort -u >"$W/declared.txt"
LC_ALL=C sort -u "$W/exempt.txt" -o "$W/exempt.txt"
LC_ALL=C comm -23 "$W/declared.txt" "$W/exempt.txt" >"$W/needed.txt"

# Defined external symbols (macOS prefixes C names with an underscore).
nm -g "$LIB" 2>/dev/null | awk 'NF >= 3 && $2 ~ /^[TDBSC]$/ { print $3 }' | sed 's/^_//' \
    | LC_ALL=C sort -u >"$W/defined.txt"

LC_ALL=C comm -23 "$W/needed.txt" "$W/defined.txt" >"$W/missing.txt"
needed=$(wc -l <"$W/needed.txt" | tr -d ' ')
if [ -s "$W/missing.txt" ]; then
    echo "FAIL: $(wc -l <"$W/missing.txt" | tr -d ' ') of $needed functions declared in $INCLUDE_DIR are not defined in $(basename "$LIB"):"
    sed 's/^/  /' "$W/missing.txt"
    exit 1
fi
echo "PASS: all $needed functions declared in $INCLUDE_DIR are defined in $(basename "$LIB")"
