#!/usr/bin/env python3
"""macgate.py -- the macOS lane's deliverable gate.

This is the peer of tools/image_build.py, NOT a replacement for it. The two
answer different questions and a green result from one says nothing about the
other:

    image_build.py   PLACEMENT.  Lay every claimed match into a copy of the
                     original binary at its claimed address and diff the whole
                     image. Catches overlapping claims, two names at one
                     address, wrong sizes, wrong offsets. But roughly two
                     thirds of that image is the ORIGINAL'S OWN BYTES, so a
                     green image is byte-identical to the original by
                     construction -- it cannot tell you our code executes.

    macgate.py       REACHABILITY.  Compile the same byte-matched source with
                     clang for this machine, link it, run the boot harness, and
                     fail if the run entered a single stub. Nothing is filled
                     in from the original here, because nothing CAN be: 32-bit
                     x86 with Win32/Glide imports will not run on this host at
                     any coverage percentage. Every byte the run touches has to
                     be real compiled code.

Both green means "placed right" and "nothing faked it". Neither means the game
works, and this tool will not claim that.

Exit codes
    0   build clean, run clean, no stub reached
    1   a gate failed (compile error, link error, crash, or a stub reached)
    2   the tool could not run the check at all (missing build.sh, no binary)

Usage
    python3 tools/macgate.py              # build + gate
    python3 tools/macgate.py --no-build   # gate the existing build/brally
    python3 tools/macgate.py --compile-only
                                          # per-TU compile survey, no link/run
"""

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sh(cmd, **kw):
    """Run cmd in ROOT, capturing output. Never raises on non-zero."""
    return subprocess.run(
        cmd, cwd=ROOT, shell=isinstance(cmd, str),
        capture_output=True, text=True, **kw)


def rule(title):
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def compile_survey():
    """Compile every core TU on its own and report which ones fail.

    build.sh runs under `set -e`, so it stops at the FIRST bad TU and the
    remaining failures stay invisible. A survey that keeps going is the
    difference between "one file is broken" and "one file is broken and here
    are the other four behind it".
    """
    import glob
    srcs = sorted(glob.glob(os.path.join(ROOT, "src/core/**/*.c"), recursive=True))
    cflags = ["-std=c99", "-w", "-D_DARWIN_C_SOURCE", "-Iinclude", "-Itests"]
    bad = []
    for s in srcs:
        rel = os.path.relpath(s, ROOT)
        r = sh(["clang"] + cflags + ["-fsyntax-only", rel])
        if r.returncode != 0:
            first = next((ln for ln in r.stderr.splitlines() if "error:" in ln), "")
            bad.append((rel, first.strip()))
    return len(srcs), bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-build", action="store_true",
                    help="gate the existing build/brally instead of rebuilding")
    ap.add_argument("--compile-only", action="store_true",
                    help="per-TU compile survey only; no link, no run")
    args = ap.parse_args()

    if not os.path.exists(os.path.join(ROOT, "build.sh")):
        print("macgate: build.sh not found", file=sys.stderr)
        return 2

    failed = False

    # --- 1. compile ------------------------------------------------------
    if not args.no_build:
        rule("1. COMPILE -- every core TU, independently")
        total, bad = compile_survey()
        if bad:
            failed = True
            print(f"FAIL: {len(bad)} of {total} TUs do not compile for this host\n")
            for rel, err in bad:
                print(f"  {rel}")
                if err:
                    print(f"      {err}")
        else:
            print(f"ok: {total}/{total} core TUs compile")

        if args.compile_only:
            return 1 if failed else 0

        # --- 2. link -----------------------------------------------------
        rule("2. LINK -- build.sh")
        r = sh(["./build.sh"])
        if r.returncode != 0:
            failed = True
            out = (r.stdout + r.stderr).splitlines()
            keep = [ln for ln in out
                    if "error:" in ln or "Undefined" in ln or "ld:" in ln
                    or ln.strip().startswith('"_')
                    or "referenced from" in ln or ln.startswith("      _")]
            print("FAIL: build.sh exited %d" % r.returncode)
            for ln in keep[-25:]:
                print("  " + ln)
            # STOP HERE. An earlier version fell through to stage 3 and ran
            # whatever build/brally happened to be lying around -- which was a
            # week-old binary -- then printed "no stub reached". A gate that
            # reports on a stale artifact after the build failed is worse than
            # no gate: it is a false green, which is the exact failure this
            # project spends its time guarding against elsewhere.
            rule("VERDICT")
            print("  macOS lane: FAILED at LINK.")
            print("  Stage 3 SKIPPED -- not run against a stale binary.")
            return 1
        print("ok: build.sh linked build/brally")
    elif args.compile_only:
        rule("1. COMPILE -- every core TU, independently")
        total, bad = compile_survey()
        for rel, err in bad:
            print(f"  {rel}\n      {err}")
        print(f"{total - len(bad)}/{total} core TUs compile")
        return 1 if bad else 0

    exe = os.path.join(ROOT, "build/brally")
    if not os.path.exists(exe):
        print("\nmacgate: build/brally does not exist -- cannot run the stub gate",
              file=sys.stderr)
        return 2 if args.no_build else 1

    # --- 3. run under the stub gate --------------------------------------
    rule("3. STUB GATE -- did the run stand on a stub?")
    env = dict(os.environ, BR_STUB_GATE="1")
    r = subprocess.run([exe], cwd=ROOT, env=env,
                       capture_output=True, text=True, timeout=120)
    tail = r.stdout.strip().splitlines()
    for ln in tail[-12:]:
        print("  " + ln)
    if r.stderr.strip():
        print("  " + r.stderr.strip())

    if r.returncode != 0:
        failed = True
        print(f"\nFAIL: harness exited {r.returncode}")
    else:
        print("\nok: no stub reached on the boot path")

    # --- verdict ---------------------------------------------------------
    rule("VERDICT")
    if failed:
        print("  macOS lane: FAILED")
        print("\n  Note what this does and does not say. It gates COMPILE, LINK")
        print("  and REACHABILITY for the boot path the harness walks -- not the")
        print("  whole game, and not correctness of any ported function.")
        return 1
    print("  macOS lane: clean -- compiles, links, and reached no stub.")
    print("\n  This is REACHABILITY on the harness's boot path only. It is not")
    print("  a correctness result and not a claim that the game is playable.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
