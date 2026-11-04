#!/bin/sh
# linuxcc.sh: clang aimed at 64-bit Linux (x86-64), against the Steam Runtime 3
# ("sniper", Debian 11, glibc 2.31) SDK sysroot in tools/toolchains/linux/sysroot
# (tools/toolchains/linux/setup.sh), linking with LLVM's ld.lld. What it builds
# runs on the Steam Deck and on any x86-64 Linux with glibc 2.31 or later.
# A Linux build of the game:
#   CC=ports/brally/tools/linuxcc.sh HOST=linux RENDER=vulkan ports/brally/link64.sh
# As a C++ driver (the link): ports/brally/tools/linuxcc.sh --driver-mode=g++
D=$(cd "$(dirname "$0")/../../.." && pwd)
SR=${LINUX_SYSROOT:-$D/tools/toolchains/linux/sysroot}
[ -d "$SR/usr/include" ] || { echo "linuxcc.sh: no sysroot at $SR (tools/toolchains/linux/setup.sh)" >&2; exit 1; }
LLD=${LD_LLD:-$(command -v ld.lld || echo /opt/homebrew/opt/lld/bin/ld.lld)}
[ -x "$LLD" ] || { echo "linuxcc.sh: no ld.lld (brew install lld)" >&2; exit 1; }
# -march=x86-64-v2 and -fno-math-errno as wincc.sh: floorf, lrint and sqrtf
# compile to one instruction each; both are exact
exec clang --target=x86_64-linux-gnu --sysroot="$SR" --ld-path="$LLD" -march=x86-64-v2 -fno-math-errno "$@"
