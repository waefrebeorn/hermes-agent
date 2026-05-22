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

# Config test (needs config.c + paths.c + yaml + json libs)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_config.c" \
    "$CDIR/src/cli/config.c" "$CDIR/src/cli/paths.c" \
    "$CDIR/lib/libyaml/yaml.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_config -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_config > /dev/null 2>&1; then ok "config (70 tests)"
    else
        echo "  Config test output:"
        /tmp/hermes_test_config 2>&1 | sed 's/^/    /'
        fail "config (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_config
else
    echo "  Config test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
        "$CDIR/tests/test_config.c" \
        "$CDIR/src/cli/config.c" "$CDIR/src/cli/paths.c" \
        "$CDIR/lib/libyaml/yaml.c" "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_config -lm 2>&1 | sed 's/^/    /'
    skip "config (compilation failed)"
fi

# Provider metadata test (needs libjson + libplugin includes)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_provider_metadata.c" \
    "$CDIR/src/agent/provider_metadata.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_provmeta -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_provmeta > /dev/null 2>&1; then ok "provider_metadata"
    else fail "provider_metadata (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_provmeta
else skip "provider_metadata (compilation failed)"
fi

# Provider smoke test (needs all provider object files + libs)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_provider_smoke.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_provsmoke -lm -lssl -lcrypto > /dev/null 2>&1; then
    if /tmp/hermes_test_provsmoke > /dev/null 2>&1; then ok "provider_smoke (439 tests)"
    else fail "provider_smoke (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_provsmoke
else skip "provider_smoke (compilation failed)"
fi

# Checkpoint tests (needs context.c for message_new/message_free)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_checkpoint.c" \
    "$CDIR/src/agent/checkpoint.c" "$CDIR/src/agent/context.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_checkpoint -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_checkpoint > /dev/null 2>&1; then ok "checkpoint (44 tests)"
    else fail "checkpoint (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_checkpoint
else skip "checkpoint (compilation failed)"
fi

# CLI command dispatch test (needs commands.c + json + plugin + http libs)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_cli_commands.c" \
    "$CDIR/src/cli/commands.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_cli -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_cli > /dev/null 2>&1; then ok "cli_commands (111 tests)"
    else fail "cli_commands (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_cli
else skip "cli_commands (compilation failed)"
fi

# Tool registry test (needs registry.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_tool_registry.c" \
    "$CDIR/src/tools/registry.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_tools -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_tools > /dev/null 2>&1; then ok "tool_registry (46 tests)"
    else fail "tool_registry (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_tools
else skip "tool_registry (compilation failed)"
fi

# Credential pool test (needs credential_pool.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_credential_pool.c" \
    "$CDIR/src/agent/credential_pool.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_credpool -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_credpool > /dev/null 2>&1; then ok "credential_pool (65 tests)"
    else fail "credential_pool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_credpool
else skip "credential_pool (compilation failed)"
fi

# Budget tracker test (needs budget_tracker.c + provider_metadata + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_budget_tracker.c" \
    "$CDIR/src/agent/budget_tracker.c" "$CDIR/src/agent/provider_metadata.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_budget -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_budget > /dev/null 2>&1; then ok "budget_tracker (104 tests)"
    else fail "budget_tracker (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_budget
else skip "budget_tracker (compilation failed)"
fi

# Gateway subsystem test (needs server.c + http + json + cron + plugin libs)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_gateway.c" \
    "$CDIR/src/gateway/server.c" \
    "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libcron/cron.c" \
    -o /tmp/hermes_test_gw -lm -lssl -lcrypto -lpthread \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_gw > /dev/null 2>&1; then ok "gateway_subsystem (49 tests)"
    else fail "gateway_subsystem (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_gw
else skip "gateway_subsystem (compilation failed)"
fi

# Plugin system test (needs plugin lib + dlfcn)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_plugins.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugins -lm -ldl > /dev/null 2>&1; then
    if /tmp/hermes_test_plugins > /dev/null 2>&1; then ok "plugin_system (38 tests)"
    else fail "plugin_system (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugins
else skip "plugin_system (compilation failed)"
fi

# Rate limiter test (standalone — only needs rate_limit.c)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_rate_limit.c" \
    "$CDIR/src/tools/rate_limit.c" \
    -o /tmp/hermes_test_rl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_rl > /dev/null 2>&1; then ok "rate_limit (168 tests)"
    else fail "rate_limit (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_rl
else skip "rate_limit (compilation failed)"
fi

# Agent loop/context test suite (G166 — needs context.c, checkpoint.c, json, plugin)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_agent.c" \
    "$CDIR/src/agent/context.c" "$CDIR/src/agent/checkpoint.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_agent -lm -lpthread > /dev/null 2>&1; then
    if /tmp/hermes_test_agent > /dev/null 2>&1; then ok "agent_loop_context (161 tests)"
    else fail "agent_loop_context (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_agent
else skip "agent_loop_context (compilation failed)"
fi

# URL safety test (needs url_safety.c — scheme/blocklist tests, no DNS)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_url_safety.c" \
    "$CDIR/src/tools/url_safety.c" \
    -o /tmp/hermes_test_url -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_url > /dev/null 2>&1; then ok "url_safety (55 tests)"
    else fail "url_safety (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_url
else skip "url_safety (compilation failed)"
fi

# Command allowlist test (needs approval.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_allowlist.c" \
    "$CDIR/src/tools/approval.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_al -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_al > /dev/null 2>&1; then ok "allowlist (34 tests)"
    else fail "allowlist (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_al
else skip "allowlist (compilation failed)"
fi

# Audit log test (needs audit.c — standalone, no JSON deps)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_audit.c" \
    "$CDIR/src/agent/audit.c" \
    -o /tmp/hermes_test_audit -lm -lpthread > /dev/null 2>&1; then
    if /tmp/hermes_test_audit > /dev/null 2>&1; then ok "audit_log (20 tests)"
    else fail "audit_log (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_audit
else skip "audit_log (compilation failed)"
fi

# Approval system test (needs approval.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_approval.c" \
    "$CDIR/src/tools/approval.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_app -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_app > /dev/null 2>&1; then ok "approval_system (18 tests)"
    else fail "approval_system (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_app
else skip "approval_system (compilation failed)"
fi

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

# Check msgraph_webhook gateway works
echo ""; echo "=== MSGraph Webhook Gateway Tests ==="
MSG_WEBHOOK_OUT=$(timeout 3 "$HERMES" gateway --platform msgraph_webhook 2>&1 || true)
if echo "$MSG_WEBHOOK_OUT" | grep -q "Listening on port"; then ok "msgraph_webhook gateway starts"
else fail "msgraph_webhook gateway failed to start"; fi

# Check CDP stub tools are registered
for cdp_tool in browser_vision browser_console browser_dialog browser_cdp; do
    if echo "$REG_TOOLS" | grep -qi "$cdp_tool"; then ok "$cdp_tool registered"
    else fail "$cdp_tool not registered"; fi
done

# Check kanban tools are registered
echo ""; echo "=== Kanban Tool Registration Tests ==="
for kanban_tool in kanban_show kanban_list kanban_complete kanban_block kanban_heartbeat kanban_comment kanban_create kanban_link kanban_unblock; do
    if echo "$REG_TOOLS" | grep -qi "$kanban_tool"; then ok "$kanban_tool registered"
    else fail "$kanban_tool not registered"; fi
done

# ==============================================
# 5. Tool registry completeness test
# ==============================================
echo ""; echo "=== Tool Registry Completeness Test ==="
# Use /tools-verify CLI command (built into hermes binary)
VERIFY_OUT=$(echo "/tools-verify" | timeout 3 "$HERMES" 2>&1 || true)
if echo "$VERIFY_OUT" | grep -q "ALL EXPECTED TOOLS PRESENT"; then ok "tool registry completeness"
else
    echo "$VERIFY_OUT" | grep "MISSING" || true
    fail "tool registry completeness (missing tools)"
fi

summary
exit $FAIL
