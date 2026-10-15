#!/bin/sh
# mixcheck.sh: build and run mixcheck.c (the music's routing in mux.c)
set -e
cd "$(dirname "$0")/../../.."
OUT=build/remastered/mixcheck
mkdir -p $OUT
clang -O1 -std=gnu11 -Wall -Iports/brally/platform/host -Iports/remastered/platform \
    ports/remastered/tools/mixcheck.c ports/remastered/platform/mux.c -o $OUT/mixcheck -lm
$OUT/mixcheck
