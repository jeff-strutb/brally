#!/bin/sh
# dedup_globals.sh DIR [DIR...] -- localize globals defined in two objects.
#
# PORT-ONLY. See the block in build.sh that calls this for why it exists:
# filing drift leaves a global defined in two TUs, and only the port's
# whole-tree link exposes it (the matching sweep compiles one file at a time).
# We keep ONE definition global and localize the rest with
# `ld -r -unexported_symbol`, so ld sees exactly one. Behaviour is unchanged:
# the kept object owns the symbol and every external caller binds to it; the
# other objects' own callers use their byte-identical local copy.
#
# WHICH COPY WINS -- priority, lowest number kept:
#   0  a host object (build/host/*): slice3_32/6_71/6_73 are DELIBERATELY
#      rebuilt with -DBR_HOST_LINK as the port's designated implementations,
#      so a host definition beats any core copy.
#   1  a filed module object (build/core/<dir>_<file>.o, a directory prefix):
#      rule 6 makes the named module the canonical home.
#   2  an address-batch slice object (build/core/sliceN_MM.o): the origin a
#      function was filed OUT of; its leftover copy is the one to localize.
set -e

prio() {   # print priority number for an object path
    case "$1" in
        */host/*)              echo 0;;
        */slice[0-9]*_[0-9]*.o) echo 2;;   # bare sliceN_MM.o (has no dir prefix in name)
        *)                     echo 1;;
    esac
}

tmp=$(mktemp)
for dir in "$@"; do
    for o in "$dir"/*.o; do
        [ -f "$o" ] || continue
        # global definitions only: exclude undefined (no 3rd field / 'U').
        nm -g "$o" 2>/dev/null | awk '$3 != "" && $2 != "U" {print $3}' | while read sym; do
            printf '%s\t%s\n' "$sym" "$o"
        done
    done
done > "$tmp"

# symbols defined in 2+ objects
dupsyms=$(awk -F'\t' '{c[$1]++} END{for(s in c) if(c[s]>1) print s}' "$tmp")

for sym in $dupsyms; do
    defs=$(awk -F'\t' -v s="$sym" '$1==s{print $2}' "$tmp")
    # choose the keeper: lowest priority number, ties broken lexically for
    # determinism.
    keep=""; keepp=99
    for o in $defs; do
        p=$(prio "$o")
        if [ "$p" -lt "$keepp" ] || { [ "$p" -eq "$keepp" ] && [ "$o" \< "$keep" ]; }; then
            keep="$o"; keepp="$p"
        fi
    done
    for o in $defs; do
        [ "$o" = "$keep" ] && continue
        ld -r "$o" -unexported_symbol "$sym" -o "$o.tmp" && mv "$o.tmp" "$o"
    done
done
rm -f "$tmp"
