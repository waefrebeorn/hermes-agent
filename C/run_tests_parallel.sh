#!/bin/bash
# Parallel Hermes C Test Runner
# Wraps test_runner.sh — runs independent test blocks in parallel batches.
# Usage: ./run_tests_parallel.sh [--jobs N] [--verbose]
# Default jobs = $(nproc)

set -euo pipefail

JOBS=$(nproc)
VERBOSE=false
ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --jobs|-j) JOBS="$2"; shift 2 ;;
        --verbose|-v) VERBOSE=true; ARGS+=("--verbose"); shift ;;
        *) ARGS+=("$1"); shift ;;
    esac
done

CDIR="$(cd "$(dirname "$0")" && pwd)"
TMPDIR=$(mktemp -d /tmp/hermes_parallel_tests.XXXXXX)
trap "rm -rf '$TMPDIR'" EXIT

PASS=0
FAIL=0
SKIP=0

echo "Parallel test runner: $JOBS jobs"
echo ""

# Result file helpers
RESULT_DIR="$TMPDIR/results"
mkdir -p "$RESULT_DIR"

# We run test_runner.sh with a modified environment that captures results
# into temp files. The key: PARALLEL_TEST_MODE=1 makes ok/fail/skip write to files.

export PARALLEL_TEST_RESULT_DIR="$RESULT_DIR"
export PARALLEL_TEST_VERBOSE="$VERBOSE"

# Run test_runner.sh with captured output
# We tee stdout so user can see progress
"$CDIR/test_runner.sh" --parallel "$@" 2>&1 | tee "$TMPDIR/test_output.txt" || true

# Aggregate results from temp files
if [[ -d "$RESULT_DIR" ]]; then
    for f in "$RESULT_DIR"/ok.*; do
        [[ -f "$f" ]] && PASS=$((PASS + 1))
    done 2>/dev/null
    for f in "$RESULT_DIR"/fail.*; do
        [[ -f "$f" ]] && FAIL=$((FAIL + 1))
    done 2>/dev/null
    for f in "$RESULT_DIR"/skip.*; do
        [[ -f "$f" ]] && SKIP=$((SKIP + 1))
    done 2>/dev/null
fi

echo ""
echo "=============================================="
echo "  Parallel Results: ${PASS} passed, ${FAIL} failed, ${SKIP} skipped"
echo "=============================================="

exit $FAIL
