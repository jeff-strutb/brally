#!/bin/sh
# newrec.sh RECORD HEADER: put a placeholder for RECORD in HEADER (after its
# includes) so recspec.py emit can write the spec's definition there.
rec="$1"; hdr="$2"
grep -q "typedef struct $rec " "$hdr" && exit 0
line=$(grep -n '^#include' "$hdr" | tail -1 | cut -d: -f1)
[ -z "$line" ] && line=$(grep -n '^#define [A-Z_]*_H$' "$hdr" | head -1 | cut -d: -f1)
sed -i '' "${line}a\\
typedef struct $rec { int x; } $rec;
" "$hdr"
