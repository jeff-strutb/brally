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
