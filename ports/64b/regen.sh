#!/bin/sh
# Regenerate the core's shared declarations after a globals/record change,
# then build. Order matters: unify writes the globals and alias headers,
# funcs adds the function aliases, funcproto the true prototypes.
set -e
cd "$(dirname "$0")/../.."
PY=.venv/bin/python
# addresses from the original's relocations, not header comments
$PY ports/64b/tools/vafix.py | head -1
$PY ports/64b/tools/unify.py --no-remove | tail -2
# two declarations unify spells with a bare typedef before it is defined
sed -i '' 's/^extern Dim g_tab\[7\];/extern struct Dim g_tab[7];/; s/^extern FlagObj \*g_29D8;/extern struct FlagObj *g_29D8;/' ports/64b/include/br_coretypes.h
$PY ports/64b/tools/dupdefs.py | tail -1
$PY ports/64b/tools/funcs.py
$PY ports/64b/tools/funcproto.py
ports/64b/build64.sh
