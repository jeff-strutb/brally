#!/bin/bash
# Run every built test suite and report a trustworthy pass/fail count.
#
# WHY THIS IS NOT A ONE-LINE grep: the obvious check -- grep -qi fail -- reports
# EVERY passing suite as a failure, because a passing suite prints "0 failures".
# That bug shipped twice in this project's ad-hoc scripts and both times made a
# fully green tree look half-broken. Match the COUNT, never the word.
#
# A suite passes iff it exits 0 AND its last line reports zero failures.
# Anything whose last line cannot be parsed is reported as UNKNOWN rather than
# silently counted either way.
cd "$(dirname "$0")/.." || exit 1

# STALENESS GATE -- a green report from stale binaries is worse than no report.
#
# This runner once printed "104 suites passed, 0 failed" while build.sh was
# FAILING. The link died partway, every test binary on disk was from the last
# good build, and the suite validated code that no longer existed. It was used
# to "confirm" a source change that had never been compiled, and then to
# "prove" a mutation harmless -- the mutation was never built either. Two
# conclusions in a row, both false, both from a green suite.
#
# build/.build-ok is touched ONLY as build.sh's last line, so `set -e` means it
# cannot exist for a build that failed anywhere. If any source is newer than
# the stamp, the binaries do not contain that source and this refuses to
# report. It deliberately does NOT rebuild -- it just declines to pretend.
if [ ! -f build/.build-ok ]; then
  echo "STALE: build/.build-ok is missing -- the last ./build.sh did not"
  echo "       complete. Refusing to report a pass count about old binaries."
  exit 2
fi
stale=$(find port tools -type f \( -name '*.c' -o -name '*.h' -o -name '*.m' \) \
        -newer build/.build-ok 2>/dev/null | head -20)
if [ -n "$stale" ]; then
  echo "STALE: these sources are newer than the last successful build --"
  echo "       the test binaries do NOT contain them. Run ./build.sh first."
  echo "$stale" | sed 's/^/         /'
  exit 2
fi

pass=0; fail=0; unknown=0; skip=0
for t in build/test_*; do
  [ -f "$t" ] && [ -x "$t" ] || continue     # skips .dSYM directories
  out=$("$t" 2>&1); rc=$?
  last=$(echo "$out" | tail -1)
  n=$(echo "$last" | grep -oE '[0-9]+ (failures|failed)' | grep -oE '^[0-9]+')
  if echo "$last" | grep -q '^SKIP '; then
    skip=$((skip+1)); echo "  $last"
  elif [ $rc -ne 0 ]; then
    fail=$((fail+1)); echo "  FAIL(rc=$rc) $(basename "$t"): $last"
  elif [ -z "$n" ]; then
    if echo "$last" | grep -qiE 'all .*(pass|passed|held)'; then
      pass=$((pass+1))
    else
      unknown=$((unknown+1)); echo "  UNKNOWN $(basename "$t"): $last"
    fi
  elif [ "$n" -ne 0 ]; then
    fail=$((fail+1)); echo "  FAIL $(basename "$t"): $last"
  else
    pass=$((pass+1))
  fi
done
echo "suites passed: $pass   failed: $fail   skipped: $skip   unparseable: $unknown"
[ $skip -gt 0 ] && echo "  ($skip suite(s) need extracted assets; see README)"
[ $fail -eq 0 ] && [ $unknown -eq 0 ]
