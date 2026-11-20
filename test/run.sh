#!/bin/sh
# Host test entry point. Compiles the math modules against test/shim/trig_host.c
# (no Pebble SDK needed) and runs the assert-based test binaries.
set -e
cd "$(dirname "$0")/.."

OUT=/tmp/opencode/sundial-tests
mkdir -p "$OUT"

# no floats in shipped C (plan rule 5)
if grep -rnE '\b(float|double|strtof)\b' src/c; then
  echo "FAIL: floating point found in src/c"
  exit 1
fi

CFLAGS="-std=gnu11 -Wall -Wextra -Werror -Isrc/c -Itest"

fail=0

run_cc() { # run_cc <binary> <test.c> <src...>
  local bin="$1" testc="$2"; shift 2
  cc $CFLAGS "$testc" "$@" -lm -o "$OUT/$bin" || exit 1
  "$OUT/$bin" || fail=1
}

run_cc test_fixmath test/test_fixmath.c src/c/fixmath.c
run_cc test_trig test/test_trig.c test/shim/trig_host.c
run_cc test_solar test/test_solar.c src/c/solar.c src/c/fixmath.c test/shim/trig_host.c
run_cc test_dial test/test_dial.c src/c/dial.c src/c/fixmath.c test/shim/trig_host.c

exit $fail
