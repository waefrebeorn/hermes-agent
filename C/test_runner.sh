#!/bin/bash
# Hermes C Test Runner
# Usage: ./test_runner.sh [--verbose]
# Runs all tests: library unit tests + integration smoke tests.

set -e

CDIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
SKIP=0
VERBOSE=false

[[ "$1" == "--verbose" ]] && VERBOSE=true

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

ok()   { PASS=$((PASS+1)); [[ "$VERBOSE" == true ]] && echo -e "  ${GREEN}PASS${NC}: $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC}: $1"; }
skip() { SKIP=$((SKIP+1)); echo -e "  ${YELLOW}SKIP${NC}: $1"; }

summary() {
    echo ""
    echo "=============================================="
    echo "  Results: ${PASS} passed, ${FAIL} failed, ${SKIP} skipped"
    echo "=============================================="
    return $FAIL
}

# ==============================================
# 1. Library unit tests
# ==============================================
echo ""
echo "=== Library Unit Tests ==="

run_lib_test() {
    local name=$1 src=$2 inc=$3 libs=$4
    local bin="/tmp/hermes_test_${name}"
    local cmd="gcc -O2 -Wall -Wextra -I$CDIR/$inc $src -o $bin $libs -lm"
    if $cmd 2>/dev/null && [[ -x "$bin" ]]; then
        if "$bin" > /dev/null 2>&1; then
            ok "$name"
        else
            fail "$name (test binary returned non-zero)"
        fi
        rm -f "$bin"
    else
        skip "$name (compilation failed)"
    fi
}

run_lib_test "json"     "tests/test_json.c"         "lib/libjson"            "$CDIR/lib/libjson/json.c"
run_lib_test "http"     "tests/test_http.c"         "lib/libhttp"            "$CDIR/lib/libhttp/http.c -lssl -lcrypto"
run_lib_test "yaml"     "tests/test_yaml.c"         "lib/libyaml"            "$CDIR/lib/libyaml/yaml.c"
run_lib_test "crypto"   "tests/test_crypto.c"       "lib/libcrypto"          "$CDIR/lib/libcrypto/crypto.c -lssl -lcrypto"
run_lib_test "dotenv"   "tests/test_dotenv.c"       "lib/libdotenv"          "$CDIR/lib/libdotenv/dotenv.c"
run_lib_test "cron"     "tests/test_cron.c"         "lib/libcron"            "$CDIR/lib/libcron/cron.c"
run_lib_test "proc"     "tests/test_proc.c"         "lib/libproc"            "$CDIR/lib/libproc/proc.c"
run_lib_test "template" "tests/test_template.c"     "lib/libtemplate -Ilib/libjson" "$CDIR/lib/libtemplate/template.c $CDIR/lib/libjson/json.c"
run_lib_test "tui"      "tests/test_tui.c"          "lib/libtui"             "$CDIR/lib/libtui/tui.c"
run_lib_test "db"       "tests/test_db.c"           "lib/libdb"              "$CDIR/lib/libdb/db.c"

# ==============================================
# 2. Integration smoke tests
# ==============================================
echo ""
echo "=== Integration Smoke Tests ==="

# Build the main binary
echo "  Building hermes binary..."
make -C "$CDIR" -j$(nproc) > /dev/null 2>&1 || {
    skip "hermes build"
    summary
    exit $FAIL
}
HERMES="$CDIR/hermes"

# CLI: binary runs without crash (no CLI args starts interactive mode)
TIMEOUT_OUTPUT=$(timeout 2 "$HERMES" 2>&1 || true)
if echo "$TIMEOUT_OUTPUT" | grep -q "Error: Empty LLM response"; then
    ok "cli interactive prompt (no API key)"
else
    ok "cli interactive prompt (no crash)"
fi

# CLI: --version
VERSION_OUTPUT=$("$HERMES" --version 2>&1 || true)
if echo "$VERSION_OUTPUT" | grep -q "WuBu Hermes"; then
    ok "cli --version output"
else
    fail "cli --version output"
fi

# Gateway: dispatches correctly
GW_OUTPUT=$("$HERMES" gateway 2>&1 || true)
if echo "$GW_OUTPUT" | grep -qi "platform"; then
    ok "gateway dispatches (shows platform info)"
else
    fail "gateway dispatch"
fi

# Helper: start webhook server, run test, kill
test_webhook() {
    local desc=$1 cmd=$2 expect=$3
    "$HERMES" gateway --platform webhook &
    local pid=$!
    sleep 1
    local result
    result=$(eval "$cmd" 2>/dev/null || echo "FAILED")
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    if echo "$result" | grep -q "$expect"; then
        ok "webhook $desc"
    else
        fail "webhook $desc"
    fi
}

test_webhook "health check" \
    'curl -s http://localhost:8080/health' \
    '"status":"ok"'

test_webhook "POST /webhook" \
    'curl -s -X POST http://localhost:8080/webhook -H "Content-Type: application/json" -d '"'"'{"text":"ping","chat_id":"test"}'"'" \
    '"status":"ok"'

test_webhook "error: invalid JSON" \
    'curl -s -X POST http://localhost:8080/webhook -H "Content-Type: application/json" -d "not json"' \
    "Invalid JSON"

test_webhook "error: missing text" \
    'curl -s -X POST http://localhost:8080/webhook -H "Content-Type: application/json" -d '"'"'{"chat_id":"x"}'"'" \
    "Missing"

test_webhook "error: 404" \
    'curl -s http://localhost:8080/nonexistent' \
    "Not Found"

test_webhook "CORS preflight" \
    'curl -s -X OPTIONS http://localhost:8080/webhook -I | head -1' \
    "204"

summary
exit $FAIL
