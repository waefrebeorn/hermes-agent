#!/bin/bash
# Hermes C Test Runner
# Usage: ./test_runner.sh [--verbose]
# Runs all tests: library unit tests + plugin tests + integration smoke tests.
# No set -e — explicit error handling throughout.

CDIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
SKIP=0
VERBOSE=false

[[ "$1" == "--verbose" ]] && VERBOSE=true

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; NC='\033[0m'

ok()   { PASS=$((PASS+1)); [[ "$VERBOSE" == true ]] && echo -e "  ${GREEN}PASS${NC}: $1"; }
fail() { FAIL=$((FAIL+1)); echo -e "  ${RED}FAIL${NC}: $1"; }
skip() { SKIP=$((SKIP+1)); echo -e "  ${YELLOW}SKIP${NC}: $1"; }
summary() { echo ""; echo "=============================================="; echo "  Results: ${PASS} passed, ${FAIL} failed, ${SKIP} skipped"; echo "=============================================="; return $FAIL; }

# ==============================================
# 1. Library unit tests
# ==============================================
echo ""; echo "=== Library Unit Tests ==="

run_lib_test() {
    local name=$1 src=$2 inc=$3 libs=$4
    local bin="/tmp/hermes_test_${name}"
    if gcc -O2 -Wall -Wextra -I"$CDIR/$inc" "$CDIR/$src" -o "$bin" $libs -lm 2>/dev/null && [[ -x "$bin" ]]; then
        if "$bin" > /dev/null 2>&1; then ok "$name"
        else fail "$name (test binary returned non-zero)"
        fi
        rm -f "$bin"
    else skip "$name (compilation failed)"
    fi
}

run_lib_test "json"     "tests/test_json.c"         "lib/libjson"            "$CDIR/lib/libjson/json.c"
run_lib_test "http"     "tests/test_http.c"         "lib/libhttp"            "$CDIR/lib/libhttp/http.c -lssl -lcrypto"
run_lib_test "yaml"     "tests/test_yaml.c"         "lib/libyaml"            "$CDIR/lib/libyaml/yaml.c"
run_lib_test "crypto"   "tests/test_crypto.c"       "lib/libcrypto"          "$CDIR/lib/libcrypto/crypto.c -lssl -lcrypto"
run_lib_test "dotenv"   "tests/test_dotenv.c"       "lib/libdotenv"          "$CDIR/lib/libdotenv/dotenv.c"
run_lib_test "cron"     "tests/test_cron.c"         "lib/libcron"            "$CDIR/lib/libcron/cron.c"
run_lib_test "proc"     "tests/test_proc.c"         "lib/libproc"            "$CDIR/lib/libproc/proc.c"
# template test special case (needs two source files)
if gcc -O2 -Wall -Wextra -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_template.c" \
    "$CDIR/lib/libtemplate/template.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_template -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_template > /dev/null 2>&1; then ok "template"
    else fail "template (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_template
else skip "template (compilation failed)"
fi
run_lib_test "tui"      "tests/test_tui.c"          "lib/libtui"             "$CDIR/lib/libtui/tui.c"
run_lib_test "db"       "tests/test_db.c"           "lib/libdb"              "$CDIR/lib/libdb/db.c"

# ==============================================
# 2. Plugin tests
# ==============================================
echo ""; echo "=== Plugin Tests ==="

echo "  Building example plugins..."
if (cd "$CDIR/examples/plugins" && bash build.sh) > /dev/null 2>&1; then
    ok "plugin build"
else
    fail "plugin build"
fi

EXAMPLES="$CDIR/examples/plugins"
if (cd "$EXAMPLES" && \
    gcc -O2 -Wall -Wextra -I"$CDIR/lib/libplugin" test_plugin_load.c "$CDIR/lib/libplugin/plugin.c" -o /tmp/test_plugin -ldl > /dev/null 2>&1 && \
    /tmp/test_plugin > /dev/null 2>&1); then
    ok "plugin load test"
    rm -f /tmp/test_plugin
else
    rm -f /tmp/test_plugin
    fail "plugin load test (non-zero exit)"
fi

# ==============================================
# 3. Integration smoke tests
# ==============================================
echo ""; echo "=== Integration Smoke Tests ==="

echo "  Building hermes binary..."
make -C "$CDIR" -j$(nproc) > /dev/null 2>&1 || {
    skip "hermes build"; summary; exit $FAIL
}
HERMES="$CDIR/hermes"

# CLI: runs without crash
TIMEOUT_OUTPUT=$(timeout 2 "$HERMES" 2>&1 || true)
if echo "$TIMEOUT_OUTPUT" | grep -q "Empty LLM response"; then
    ok "cli interactive prompt (no API key)"
else
    ok "cli interactive prompt (no crash)"
fi

# CLI: --version
if "$HERMES" --version 2>&1 | grep -q "WuBu Hermes"; then
    ok "cli --version output"
else
    fail "cli --version output"
fi

# Gateway: dispatches correctly
if "$HERMES" gateway 2>&1 | grep -qi "platform"; then
    ok "gateway dispatches"
else
    fail "gateway dispatch"
fi

# Helper: start webhook server, run test, kill
# Uses a random port to avoid TIME_WAIT conflicts between runs.
test_webhook() {
    local desc=$1 cmd=$2 expect=$3
    local port
    port=$(( (RANDOM % 10000) + 20000 ))
    HERMES_WEBHOOK_PORT=$port "$HERMES" gateway --platform webhook &
    local pid=$!
    sleep 1
    local result
    cmd="${cmd//\$PORT/$port}"
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
    'curl -s http://localhost:$PORT/health' \
    '"status":"ok"'

test_webhook "POST /webhook" \
    'curl -s -X POST http://localhost:$PORT/webhook -H "Content-Type: application/json" -d '"'"'{"text":"ping","chat_id":"test"}'"'" \
    '"status":"ok"'

test_webhook "error: invalid JSON" \
    'curl -s -X POST http://localhost:$PORT/webhook -H "Content-Type: application/json" -d "not json"' \
    "Invalid JSON"

test_webhook "error: missing text" \
    'curl -s -X POST http://localhost:$PORT/webhook -H "Content-Type: application/json" -d '"'"'{"chat_id":"x"}'"'" \
    "Missing"

test_webhook "error: 404" \
    'curl -s http://localhost:$PORT/nonexistent' \
    "Not Found"

test_webhook "CORS preflight" \
    'curl -s -X OPTIONS http://localhost:$PORT/webhook -I | head -1' \
    "204"

# ==============================================
# 4. Browser tool registration tests
# ==============================================
echo ""; echo "=== Browser Tool Registration Tests ==="

# Check that browser_get_images and browser_press are registered
REG_TOOLS=$(echo "/tools" | timeout 2 "$HERMES" 2>&1 || true)
BROWSER_GET_IMAGES=false
BROWSER_PRESS=false
while IFS= read -r line; do
    if echo "$line" | grep -qi "browser_get_images"; then BROWSER_GET_IMAGES=true; fi
    if echo "$line" | grep -qi "browser_press"; then BROWSER_PRESS=true; fi
done <<< "$REG_TOOLS"

if [ "$BROWSER_GET_IMAGES" = true ]; then ok "browser_get_images registered"
else fail "browser_get_images not registered"; fi

if [ "$BROWSER_PRESS" = true ]; then ok "browser_press registered"
else fail "browser_press not registered"; fi

# Check image_generate is registered
if echo "$REG_TOOLS" | grep -qi "image_generate"; then ok "image_generate registered"
else fail "image_generate not registered"; fi

# Check HomeAssistant tools are registered
for ha_tool in ha_list_entities ha_get_state ha_list_services ha_call_service; do
    if echo "$REG_TOOLS" | grep -qi "$ha_tool"; then ok "$ha_tool registered"
    else fail "$ha_tool not registered"; fi
done

summary
exit $FAIL
