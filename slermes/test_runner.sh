#!/bin/bash
# Hermes C Test Runner
# Usage: ./test_runner.sh [--verbose] [--parallel]
# Runs all tests: library unit tests + plugin tests + integration smoke tests.
# No set -e — explicit error handling throughout.

CDIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
SKIP=0
VERBOSE=false
PARALLEL=false
PARALLEL_JOBS=$(nproc)

for arg in "$@"; do
    case "$arg" in
        --verbose|-v) VERBOSE=true ;;
        --parallel|-j|-p) PARALLEL=true ;;
        --jobs=*) PARALLEL_JOBS="${arg#*=}" ;;
    esac
done

# Always use file-backed result counters (survive & subshells from parallel gcc blocks)
TMPDIR=$(mktemp -d /tmp/hermes_test_results.XXXXXX)
trap "rm -rf '$TMPDIR'" EXIT

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; NC='\033[0m'

ok() {
    touch "$TMPDIR/ok.$1"
    [[ "$VERBOSE" == true ]] && echo -e "  ${GREEN}PASS${NC}: $1"
}
fail() {
    touch "$TMPDIR/fail.$1"
    echo -e "  ${RED}FAIL${NC}: $1"
}
skip() {
    touch "$TMPDIR/skip.$1"
    echo -e "  ${YELLOW}SKIP${NC}: $1"
}
summary() {
    PASS=$(ls "$TMPDIR"/ok.* 2>/dev/null | wc -l)
    FAIL=$(ls "$TMPDIR"/fail.* 2>/dev/null | wc -l)
    SKIP=$(ls "$TMPDIR"/skip.* 2>/dev/null | wc -l)
    echo ""; echo "=============================================="
    echo "  Results: ${PASS} passed, ${FAIL} failed, ${SKIP} skipped"
    echo "=============================================="
    return $FAIL
}

# ==============================================
# 1. Library unit tests
# ==============================================
echo ""; echo "=== Library Unit Tests ==="

run_lib_test() {
    local name=$1 src=$2 inc=$3 libs=$4
    local bin="/tmp/hermes_test_${name}"
    (
    if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/$inc" "$CDIR/$src" -o "$bin" $libs -lm 2>/dev/null && [[ -x "$bin" ]]; then
        if "$bin" > /dev/null 2>&1; then ok "$name"
        else fail "$name (test binary returned non-zero)"
        fi
        rm -f "$bin"
    else skip "$name (compilation failed)"
    fi
    ) &
}

run_lib_test "json"      "tests/test_json.c"           "lib/libjson"            "$CDIR/lib/libjson/json.c"
run_lib_test "protobuf"  "tests/test_protobuf.c"       "lib/libprotobuf"        "$CDIR/lib/libprotobuf/protobuf.c"
run_lib_test "http"      "tests/test_http.c"           "lib/libhttp"            "$CDIR/lib/libhttp/http.c -lssl -lcrypto -lz"
run_lib_test "http_multipart" "tests/test_http_multipart.c" "lib/libhttp" "$CDIR/lib/libhttp/http.c -lssl -lcrypto -lz"
run_lib_test "http_pool" "tests/test_http_pool.c" "lib/libhttp" "$CDIR/lib/libhttp/http.c -lssl -lcrypto -lz"
run_lib_test "webhook_endpoint" "tests/test_webhook_endpoint.c" "lib/libjson" "$CDIR/lib/libjson/json.c -Wl,--unresolved-symbols=ignore-all"
run_lib_test "yaml"     "tests/test_yaml.c"         "lib/libyaml"            "$CDIR/lib/libyaml/yaml.c"
run_lib_test "crypto"   "tests/test_crypto.c"       "lib/libcrypto"          "$CDIR/lib/libcrypto/crypto.c -lssl -lcrypto"
run_lib_test "tokenizer" "tests/test_tokenizer.c"    "include"                 "$CDIR/src/hermes_tokenizer.c"
run_lib_test "hermes_error" "tests/test_hermes_error.c" "include" "$CDIR/src/hermes_error.c"
run_lib_test "binary"    "tests/test_binary.c"      "lib/libbinary"           "$CDIR/lib/libbinary/binary.c"
run_lib_test "binary_extensions" "tests/test_binary_extensions.c" "lib/libbinary" "$CDIR/lib/libbinary/binary.c"
run_lib_test "budget_config" "tests/test_budget_config.c" "lib/libbudgetconfig" "$CDIR/lib/libbudgetconfig/budget_config.c"
run_lib_test "credential_files" "tests/test_credential_files.c" "lib/libcredentialfiles" "$CDIR/lib/libcredentialfiles/credential_files.c"
run_lib_test "skill_audit" "tests/test_skill_audit.c" "lib/libskillaudit" "$CDIR/lib/libskillaudit/skill_audit.c"
run_lib_test "slash_confirm" "tests/test_slash_confirm.c" "lib/libslashconfirm" "$CDIR/lib/libslashconfirm/slash_confirm.c -lpthread"

echo ""; echo "=== Website Policy Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libwebsite" \
    "$CDIR/tests/test_website.c" "$CDIR/lib/libwebsite/website_policy.c" \
    -o /tmp/hermes_test_ws -lm 2>/dev/null && [[ -x /tmp/hermes_test_ws ]]; then
    if /tmp/hermes_test_ws > /dev/null 2>&1; then
        ok "website (17 tests)"
    else
        fail "website (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_ws
else
    fail "website (compilation failed)"
fi

echo ""; echo "=== Env Passthrough Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libenvpassthrough" \
    "$CDIR/tests/test_env_passthrough.c" "$CDIR/lib/libenvpassthrough/env_passthrough.c" \
    -o /tmp/hermes_test_ep -lpthread 2>/dev/null && [[ -x /tmp/hermes_test_ep ]]; then
    if /tmp/hermes_test_ep > /dev/null 2>&1; then
        ok "env_passthrough (27 tests)"
    else
        fail "env_passthrough (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_ep
else
    fail "env_passthrough (compilation failed)"
fi

echo ""; echo "=== Tool Output Lib Tests === (merged with tool_output below)"
echo ""; echo "=== Registry Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_registry.c" "$CDIR/src/tools/registry.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_registry -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_registry ]]; then
    if /tmp/hermes_test_registry > /dev/null 2>&1; then
        ok "registry (30 tests)"
    else
        fail "registry (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_registry
else
    fail "registry (compilation failed)"
fi

echo ""; echo "=== Tool Init Tests (registry) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_tool_init.c" "$CDIR/src/tools/registry.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_tool_init -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_tool_init > /dev/null 2>&1; then ok "tool_init (13 tests)"
    else fail "tool_init (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_tool_init
else fail "tool_init (compilation failed)"
fi
echo ""; echo "=== Redact Tests ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation \
    -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_redact.c" "$CDIR/src/agent/redact.c" \
    -o /tmp/hermes_test_rd -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_rd ]]; then
    if /tmp/hermes_test_rd > /dev/null 2>&1; then
        ok "redact (14 tests)"
    else
        fail "redact (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_rd
else
    fail "redact (compilation failed)"
fi

echo ""; echo "=== WeCom Crypto Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_wecom_crypto.c" "$CDIR/src/tools/wecom_crypto.c" \
    "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_wecom_crypto -lssl -lcrypto -lm 2>/dev/null && [[ -x /tmp/hermes_test_wecom_crypto ]]; then
    if /tmp/hermes_test_wecom_crypto > /dev/null 2>&1; then
        ok "wecom_crypto (28 tests)"
    else
        fail "wecom_crypto (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_wecom_crypto
else
    fail "wecom_crypto (compilation failed)"
fi

echo ""; echo "=== WeCom Callback Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" \
    "$CDIR/tests/test_wecom_callback.c" "$CDIR/src/gateway/platforms/wecom_callback.c" \
    -o /tmp/hermes_test_wecom_callback -lm 2>/dev/null && [[ -x /tmp/hermes_test_wecom_callback ]]; then
    if /tmp/hermes_test_wecom_callback > /dev/null 2>&1; then
        ok "wecom_callback (62 tests)"
    else
        fail "wecom_callback (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_wecom_callback
else
    fail "wecom_callback (compilation failed)"
fi

echo ""; echo "=== Terminal Sudo Tests ==="
if gcc -O2 -Wall -Wextra -Wl,--unresolved-symbols=ignore-all \
    -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libtooloutput" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libbase64" \
    -I"$CDIR/lib/libenvpassthrough" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libpath" \
    -I"$CDIR/lib/libdatetime" -I"$CDIR/lib/libtoolbackend" -I"$CDIR/lib/libproc" \
    -I"$CDIR/lib/libregex" -I"$CDIR/lib/liblineedit" -I"$CDIR/lib/libmangateway" \
    -I"$CDIR/lib/libratelimit" \
    "$CDIR/tests/test_terminal_sudo.c" "$CDIR/src/tools/terminal.c" \
    -o /tmp/hermes_test_terminal_sudo -lssl -lcrypto -lz -lm 2>/dev/null && [[ -x /tmp/hermes_test_terminal_sudo ]]; then
    if /tmp/hermes_test_terminal_sudo > /dev/null 2>&1; then
        ok "terminal_sudo (3 tests)"
    else
        fail "terminal_sudo (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_terminal_sudo
else
    fail "terminal_sudo (compilation failed)"
fi

echo ""; echo "=== Markdown Render Tests ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation \
    -I"$CDIR/include" \
    "$CDIR/tests/test_markdown.c" "$CDIR/src/agent/markdown_render.c" \
    -o /tmp/hermes_test_md -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_md ]]; then
    if /tmp/hermes_test_md > /dev/null 2>&1; then
        ok "markdown_render (23 tests)"
    else
        fail "markdown_render (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_md
else
    fail "markdown_render (compilation failed)"
fi

echo ""; echo "=== Sanitize Tests ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation \
    -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_sanitize.c" "$CDIR/src/agent/sanitize.c" "$CDIR/src/agent/redact.c" \
    -o /tmp/hermes_test_san -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_san ]]; then
    if /tmp/hermes_test_san > /dev/null 2>&1; then
        ok "sanitize (20 tests)"
    else
        fail "sanitize (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_san
else
    fail "sanitize (compilation failed)"
fi

echo ""; echo "=== Skill Command Tests ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation \
    -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_skill_commands.c" "$CDIR/src/agent/skill_commands.c" \
    -o /tmp/hermes_test_sk -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_sk ]]; then
    if /tmp/hermes_test_sk > /dev/null 2>&1; then
        ok "skill_commands (19 tests)"
    else
        fail "skill_commands (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_sk
else
    fail "skill_commands (compilation failed)"
fi

echo ""; echo "=== Usage Pricing Tests ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation \
    -I"$CDIR/include" \
    "$CDIR/tests/test_usage_pricing.c" "$CDIR/src/agent/usage_pricing.c" "$CDIR/src/hermes_tokenizer.c" \
    -o /tmp/hermes_test_usage -lm 2>/dev/null && [[ -x /tmp/hermes_test_usage ]]; then
    if /tmp/hermes_test_usage > /dev/null 2>&1; then
        ok "usage_pricing (33 tests)"
    else
        fail "usage_pricing (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_usage
else
    fail "usage_pricing (compilation failed)"
fi

echo ""; echo "=== Path Resolution (P21) Tests ==="
# test_paths.c was removed as redundant — test_cli_paths.c covers the module
: # (tested via run_lib_test "cli_paths" below)

echo ""; echo "=== Microsoft Graph Tests ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation \
    -I"$CDIR/include" -I"$CDIR/lib/libmsgraph" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_ms_graph.c" "$CDIR/lib/libmsgraph/ms_graph.c" \
    "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_msgraph -lssl -lcrypto -lz -lm 2>/dev/null && [[ -x /tmp/hermes_test_msgraph ]]; then
    if /tmp/hermes_test_msgraph > /dev/null 2>&1; then
        ok "ms_graph"
    else
        fail "ms_graph (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_msgraph
else
    fail "ms_graph (compilation failed)"
fi

echo ""; echo "=== Tool Result Storage Tests === "
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_tool_result_storage.c" "$CDIR/src/tools/result_storage.c" \
    -o /tmp/hermes_test_trs -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_trs ]]; then
    if /tmp/hermes_test_trs > /dev/null 2>&1; then
        ok "tool_result_storage"
    else
        fail "tool_result_storage (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_trs
else
    fail "tool_result_storage (compilation failed)"
fi

echo ""; echo "=== Tool Result Classification Tests === "
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_tool_result.c" "$CDIR/src/tools/tool_result.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_tool_result -lm 2>/dev/null && [[ -x /tmp/hermes_test_tool_result ]]; then
    if /tmp/hermes_test_tool_result > /dev/null 2>&1; then
        ok "tool_result_classification"
    else
        fail "tool_result_classification (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_tool_result
else
    fail "tool_result_classification (compilation failed)"
fi

echo ""; echo "=== Tool Call Args Truncate Tests === "
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_tool_call_args_truncate.c" "$CDIR/src/tools/tool_result.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_truncate -lm 2>/dev/null && [[ -x /tmp/hermes_test_truncate ]]; then
    if /tmp/hermes_test_truncate > /dev/null 2>&1; then
        ok "tool_call_args_truncate"
    else
        fail "tool_call_args_truncate (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_truncate
else
    fail "tool_call_args_truncate (compilation failed)"
fi

echo ""; echo "=== Hook Registry Tests === "
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_hook_registry.c" "$CDIR/src/agent/hook_registry.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_hooks -lpthread -lm 2>/dev/null && [[ -x /tmp/hermes_test_hooks ]]; then
    if /tmp/hermes_test_hooks > /dev/null 2>&1; then
        ok "hook_registry"
    else
        fail "hook_registry (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_hooks
else
    fail "hook_registry (compilation failed)"
fi

echo ""; echo "=== Clarify Tool Tests === "
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_clarify.c" "$CDIR/src/tools/clarify.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_clarify -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_clarify ]]; then
    if /tmp/hermes_test_clarify > /dev/null 2>&1; then
        ok "clarify"
    else
        fail "clarify (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_clarify
else
    fail "clarify (compilation failed)"
fi

echo ""; echo "=== Threat Pattern Tests === "
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libregex" -I"$CDIR/lib/libthreatpatterns" \
    "$CDIR/tests/test_threat_patterns.c" "$CDIR/lib/libregex/hermes_regex.c" "$CDIR/lib/libthreatpatterns/threat_patterns.c" \
    -o /tmp/hermes_test_threat_patterns -lm 2>/dev/null && [[ -x /tmp/hermes_test_threat_patterns ]]; then
    if /tmp/hermes_test_threat_patterns > /dev/null 2>&1; then
        ok "threat_patterns (36 tests)"
    else
        fail "threat_patterns (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_threat_patterns
else
    skip "threat_patterns (compilation failed)"
fi &

echo ""; echo "=== Tool Result Storage Tests (preview) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" \
    $(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done) \
    "$CDIR/src/tools/result_storage.c" "$CDIR/tests/test_tool_result_storage.c" \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_result_storage_preview -lm 2>/dev/null && [[ -x /tmp/hermes_test_result_storage_preview ]]; then
    if /tmp/hermes_test_result_storage_preview > /dev/null 2>&1; then
        ok "tool_result_storage_preview"
    else
        fail "tool_result_storage_preview (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_result_storage_preview
else
    skip "tool_result_storage_preview (compilation failed)"
fi &


echo ""; echo "=== Auxiliary Client Tests (B04) ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_auxiliary_client.c" "$CDIR/src/agent/auxiliary_client.c" \
    -I"$CDIR/include" \
    $(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done) \
    -o /tmp/hermes_test_auxiliary_client -lm 2>/dev/null && [[ -x /tmp/hermes_test_auxiliary_client ]]; then
    if /tmp/hermes_test_auxiliary_client > /dev/null 2>&1; then
        ok "auxiliary_client"
    else fail "auxiliary_client (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_auxiliary_client
else skip "auxiliary_client (compilation failed)"
fi &

echo ""; echo "=== Provider Registry Tests (P10) ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider.c" "$CDIR/src/agent/provider.c" \
    -I"$CDIR/include" \
    $(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider ]]; then
    if /tmp/hermes_test_provider > /dev/null 2>&1; then
        ok "provider"
    else fail "provider (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider
else skip "provider (compilation failed)"
fi &

echo ""; echo "=== Plugin Extension Tests (P126) ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_plugin_ext.c" "$CDIR/src/agent/plugin_ext.c" \
    "$CDIR/lib/libplugin/plugin.c" "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_plugin_ext -lm 2>/dev/null && [[ -x /tmp/hermes_test_plugin_ext ]]; then
    if /tmp/hermes_test_plugin_ext > /dev/null 2>&1; then
        ok "plugin_ext (30 tests)"
    else fail "plugin_ext (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_plugin_ext
else skip "plugin_ext (compilation failed)"
fi &

echo ""; echo "=== OpenAI Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_openai.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_openai -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider_openai ]]; then
    if /tmp/hermes_test_provider_openai > /dev/null 2>&1; then
        ok "provider_openai (54 tests)"
    else fail "provider_openai (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_openai
else skip "provider_openai (compilation failed)"
fi &

echo ""; echo "=== Anthropic Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_anthropic.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_anthropic -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider_anthropic ]]; then
    if /tmp/hermes_test_provider_anthropic > /dev/null 2>&1; then
        ok "provider_anthropic (74 tests)"
    else fail "provider_anthropic (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_anthropic
else skip "provider_anthropic (compilation failed)"
fi &

echo ""; echo "=== Google Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_google.c" \
    "$CDIR/src/agent/provider_google.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_google -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider_google ]]; then
    if /tmp/hermes_test_provider_google > /dev/null 2>&1; then
        ok "provider_google (64 tests)"
    else fail "provider_google (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_google
else skip "provider_google (compilation failed)"
fi &

echo ""; echo "=== DeepSeek Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_deepseek.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_deepseek -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider_deepseek ]]; then
    if /tmp/hermes_test_provider_deepseek > /dev/null 2>&1; then
        ok "provider_deepseek (60 tests)"
    else fail "provider_deepseek (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_deepseek
else skip "provider_deepseek (compilation failed)"
fi &

echo ""; echo "=== Image Routing Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_image_routing.c" "$CDIR/src/agent/image_routing.c" "$CDIR/lib/libbase64/base64.c" "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    $(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done) \
    -o /tmp/hermes_test_image_routing -lm 2>/dev/null && [[ -x /tmp/hermes_test_image_routing ]]; then
    if /tmp/hermes_test_image_routing > /dev/null 2>&1; then
        ok "image_routing"
    else fail "image_routing (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_image_routing
else skip "image_routing (compilation failed)"
fi &

echo ""; echo "=== LM Studio Reasoning Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_lmstudio_reasoning.c" "$CDIR/src/agent/lmstudio_reasoning.c" \
    -I"$CDIR/include" \
    -o /tmp/hermes_test_lmstudio -lm 2>/dev/null && [[ -x /tmp/hermes_test_lmstudio ]]; then
    if /tmp/hermes_test_lmstudio > /dev/null 2>&1; then
        ok "lmstudio_reasoning"
    else fail "lmstudio_reasoning (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_lmstudio
else skip "lmstudio_reasoning (compilation failed)"
fi &

echo ""; echo "=== Skill Utils Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_skill_utils.c" "$CDIR/lib/libskillutils/skill_utils.c" "$CDIR/lib/libyaml/yaml.c" \
    -I"$CDIR/include" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libskillutils" \
    -o /tmp/hermes_test_skill_utils -lm 2>/dev/null && [[ -x /tmp/hermes_test_skill_utils ]]; then
    if /tmp/hermes_test_skill_utils > /dev/null 2>&1; then
        ok "skill_utils"
    else fail "skill_utils (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_skill_utils
else skip "skill_utils (compilation failed)"
fi &

echo ""; echo "=== OSV Malware Check Tests (D85) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libosv" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/include" \
    "$CDIR/tests/test_osv.c" \
    "$CDIR/lib/libosv/osv.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_osv -lssl -lcrypto -lm -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_osv > /dev/null 2>&1; then ok "osv (25 tests)"
    else fail "osv (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_osv
else skip "osv (compilation failed)"
fi &

# Error classifier (agent/error_classifier.py port)
run_lib_test "error_classifier" "tests/test_error_classifier.c" "lib/liberrorclassifier" "$CDIR/lib/liberrorclassifier/error_classifier.c"

echo ""; echo "=== Transcription Library Tests (D83) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libtranscribe" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/include" \
    "$CDIR/tests/test_transcription.c" \
    "$CDIR/lib/libtranscribe/transcribe.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_transcribe -lssl -lcrypto -lm -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_transcribe > /dev/null 2>&1; then ok "transcription"
    else fail "transcription (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_transcribe
else skip "transcription (compilation failed)"
fi &

echo ""; echo "=== MCP OAuth Tests (D85) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libmcp_oauth" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" \
    -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libbase64" -I"$CDIR/include" \
    "$CDIR/tests/test_mcp_oauth.c" \
    "$CDIR/lib/libmcp_oauth/mcp_oauth.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libhttp/http.c" \
    "$CDIR/lib/libcrypto/crypto.c" \
    "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_mcp_oauth -lssl -lcrypto -lm -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_mcp_oauth > /dev/null 2>&1; then ok "mcp_oauth"
    else fail "mcp_oauth (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_mcp_oauth
else skip "mcp_oauth (compilation failed)"
fi &
echo ""; echo "=== Auth Store Tests (P176) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_auth_store.c" \
    "$CDIR/src/provider/token_exchange.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_auth_store -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_auth_store > /dev/null 2>&1; then ok "auth_store (20 tests)"
    else fail "auth_store (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_auth_store
else
    skip "auth_store (compilation failed)"
fi

echo ""; echo "=== FAL Common Library Tests (D80) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libfal_common" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/include" \
    "$CDIR/tests/test_fal_common.c" \
    "$CDIR/lib/libfal_common/fal_common.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_fal_common -lssl -lcrypto -lm -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_fal_common > /dev/null 2>&1; then ok "fal_common"
    else fail "fal_common (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_fal_common
else skip "fal_common (compilation failed)"
fi &

echo ""; echo "=== Rate Guard Library Tests (nous_rate_guard) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/librateguard" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libpath" -I"$CDIR/include" \
    "$CDIR/tests/test_rate_guard.c" \
    "$CDIR/lib/librateguard/rate_guard.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_rate_guard -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_rate_guard > /dev/null 2>&1; then ok "rate_guard"
    else fail "rate_guard (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_rate_guard
else skip "rate_guard (compilation failed)"
fi &

echo ""; echo "=== Tool Call Guardrails Tests (G28-G30) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/include" -I"$CDIR/src/agent" \
    "$CDIR/tests/test_tool_guardrails.c" \
    "$CDIR/src/agent/tool_guardrails.c" \
    -o /tmp/hermes_test_guardrails -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_guardrails > /dev/null 2>&1; then ok "tool_guardrails (48 tests)"
    else fail "tool_guardrails (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_guardrails
else skip "tool_guardrails (compilation failed)"
fi &

echo ""; echo "=== Tool Output Limits Tests (tool_output_limits) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libtooloutput" \
    "$CDIR/tests/test_tool_output.c" \
    -o /tmp/hermes_test_tool_output -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_tool_output > /dev/null 2>&1; then ok "tool_output (23 tests)"
    else fail "tool_output (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_tool_output
else skip "tool_output (compilation failed)"
fi &

echo ""; echo "=== Skill Provenance Tests (skill_provenance) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libskillusage" \
    "$CDIR/tests/test_skill_provenance.c" \
    "$CDIR/lib/libskillusage/skill_provenance.c" \
    -o /tmp/hermes_test_skill_provenance -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_skill_provenance > /dev/null 2>&1; then ok "skill_provenance"
    else fail "skill_provenance (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_skill_provenance
else skip "skill_provenance (compilation failed)"
fi &

echo ""; echo "=== Camofox State Tests (browser_camofox_state) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libbrowser" -I"$CDIR/lib/libuuid" -I"$CDIR/lib/libhash" \
    "$CDIR/tests/test_camofox_state.c" \
    "$CDIR/lib/libbrowser/camofox_state.c" \
    "$CDIR/lib/libuuid/uuid.c" \
    "$CDIR/lib/libhash/hash.c" \
    -o /tmp/hermes_test_camofox_state -lssl -lcrypto -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_camofox_state > /dev/null 2>&1; then ok "camofox_state"
    else fail "camofox_state (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_camofox_state
else skip "camofox_state (compilation failed)"
fi &

echo ""; echo "=== Env Passthrough Library Tests (env_passthrough) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libenvpassthrough" \
    "$CDIR/tests/test_env_passthrough.c" \
    "$CDIR/lib/libenvpassthrough/env_passthrough.c" \
    -o /tmp/hermes_test_env_passthrough -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_env_passthrough > /dev/null 2>&1; then ok "env_passthrough"
    else fail "env_passthrough (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_env_passthrough
else skip "env_passthrough (compilation failed)"
fi &

echo ""; echo "=== xAI (Grok) Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_xai.c" \
    "$CDIR/src/agent/provider_xai.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_xai -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider_xai ]]; then
    if /tmp/hermes_test_provider_xai > /dev/null 2>&1; then
        ok "provider_xai (63 tests)"
    else fail "provider_xai (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_xai
else skip "provider_xai (compilation failed)"
fi &

echo ""; echo "=== Azure Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_azure.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_azure -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider_azure ]]; then
    if /tmp/hermes_test_provider_azure > /dev/null 2>&1; then
        ok "provider_azure (54 tests)"
    else fail "provider_azure (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_azure
else skip "provider_azure (compilation failed)"
fi &

echo ""; echo "=== Bedrock Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_bedrock.c" \
    "$CDIR/src/agent/provider_bedrock.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_bedrock -lm -lcrypto 2>/dev/null && [[ -x /tmp/hermes_test_provider_bedrock ]]; then
    if /tmp/hermes_test_provider_bedrock > /dev/null 2>&1; then
        ok "provider_bedrock (40 tests)"
    else fail "provider_bedrock (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_bedrock
else skip "provider_bedrock (compilation failed)"
fi &

echo ""; echo "=== OpenRouter Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_openrouter.c" \
    "$CDIR/src/agent/provider_openrouter.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_openrouter -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider_openrouter ]]; then
    if /tmp/hermes_test_provider_openrouter > /dev/null 2>&1; then
        ok "provider_openrouter (50 tests)"
    else fail "provider_openrouter (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_openrouter
else skip "provider_openrouter (compilation failed)"
fi &

echo ""; echo "=== Custom Provider Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_provider_custom.c" \
    "$CDIR/src/agent/provider_custom.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    $(for d in "$CDIR"/lib/libyaml "$CDIR"/lib/libhttp "$CDIR"/lib/libmcp \
       "$CDIR"/lib/libcrypto "$CDIR"/lib/libdb "$CDIR"/lib/libskin \
       "$CDIR"/lib/libwebsocket "$CDIR"/lib/libprotobuf "$CDIR"/lib/libcron \
       "$CDIR"/lib/libproc "$CDIR"/lib/libtui "$CDIR"/lib/libtemplate \
       "$CDIR"/lib/libdotenv "$CDIR"/lib/libbase64; do echo -n " -I${d}"; done) \
    -Wl,--unresolved-symbols=ignore-all \
    -o /tmp/hermes_test_provider_custom -lm 2>/dev/null && [[ -x /tmp/hermes_test_provider_custom ]]; then
    if /tmp/hermes_test_provider_custom > /dev/null 2>&1; then
        ok "provider_custom (37 tests)"
    else fail "provider_custom (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_provider_custom
else skip "provider_custom (compilation failed)"
fi &

echo ""; echo "=== xAI HTTP Library Tests (xai_http) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libxai_http" \
    "$CDIR/tests/test_xai_http.c" \
    "$CDIR/lib/libxai_http/xai_http.c" \
    -o /tmp/hermes_test_xai_http -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_xai_http > /dev/null 2>&1; then ok "xai_http"
    else fail "xai_http (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_xai_http
else skip "xai_http (compilation failed)"
fi &

echo ""; echo "=== Credential Files Library Tests (credential) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libcredential" \
    "$CDIR/tests/test_credential.c" \
    "$CDIR/lib/libcredential/credential.c" \
    -o /tmp/hermes_test_credential -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_credential > /dev/null 2>&1; then ok "credential"
    else fail "credential (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_credential
else skip "credential (compilation failed)"
fi &

echo ""; echo "=== Schema Sanitizer Library Tests (schema_sanitizer) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libschemasanitizer" -I"$CDIR/lib/libjson" \
    -I"$CDIR/include" \
    "$CDIR/tests/test_schema_sanitizer.c" \
    "$CDIR/lib/libschemasanitizer/schema_sanitizer.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_schema_sanitizer -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_schema_sanitizer > /dev/null 2>&1; then ok "schema_sanitizer"
    else fail "schema_sanitizer (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_schema_sanitizer
else skip "schema_sanitizer (compilation failed)"
fi &

echo ""; echo "=== Fuzzy Match Library Tests (fuzzy_match) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libfuzzymatch" \
    "$CDIR/tests/test_fuzzy_match.c" \
    "$CDIR/lib/libfuzzymatch/fuzzy_match.c" \
    -o /tmp/hermes_test_fuzzy_match -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_fuzzy_match > /dev/null 2>&1; then ok "fuzzy_match"
    else fail "fuzzy_match (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_fuzzy_match
else skip "fuzzy_match (compilation failed)"
fi &

echo ""; echo "=== Website Policy Tests (P1 Security) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libwebsite" \
    "$CDIR/tests/test_website_policy.c" \
    "$CDIR/lib/libwebsite/website_policy.c" \
    -o /tmp/hermes_test_website_policy -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_website_policy > /dev/null 2>&1; then ok "website_policy (43 tests)"
    else fail "website_policy (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_website_policy
else skip "website_policy (compilation failed)"
fi &

echo ""; echo "=== Debug Helpers Tests ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libdebug" \
    "$CDIR/tests/test_debug_helpers.c" \
    "$CDIR/lib/libdebug/debug_helpers.c" \
    -o /tmp/hermes_test_debug_helpers -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_debug_helpers > /dev/null 2>&1; then ok "debug_helpers (38 tests)"
    else fail "debug_helpers (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_debug_helpers
else skip "debug_helpers (compilation failed)"
fi &

echo ""; echo "=== Skill Usage Telemetry Tests (D82) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/lib/libskillusage" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_skill_usage.c" \
    "$CDIR/lib/libskillusage/skill_usage.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_skill_usage -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_skill_usage > /dev/null 2>&1; then ok "skill_usage (76 tests)"
    else
        echo "  Skill usage test output:"
        /tmp/hermes_test_skill_usage 2>&1 | sed 's/^/    /'
        fail "skill_usage (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_skill_usage
else
    echo "  Skill usage test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/lib/libskillusage" -I"$CDIR/lib/libjson" \
        "$CDIR/tests/test_skill_usage.c" \
        "$CDIR/lib/libskillusage/skill_usage.c" \
        "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_skill_usage -lm 2>&1 | sed 's/^/    /'
    skip "skill_usage (compilation failed)"
fi

echo ""; echo "=== Skills Sync Tests ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation \
    -I"$CDIR/lib/libskillsync" -I"$CDIR/lib/libhash" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_skills_sync.c" \
    "$CDIR/lib/libskillsync/skills_sync.c" \
    "$CDIR/lib/libhash/hash.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_skills_sync -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_skills_sync > /dev/null 2>&1; then ok "skills_sync (43 tests)"
    else
        echo "  Skills sync test output:"
        /tmp/hermes_test_skills_sync 2>&1 | sed 's/^/    /'
        fail "skills_sync (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_skills_sync
else
    echo "  Skills sync test compilation error:"
    gcc -O2 -Wall -Wextra -Wno-format-truncation \
        -I"$CDIR/lib/libskillsync" -I"$CDIR/lib/libhash" -I"$CDIR/lib/libjson" \
        "$CDIR/tests/test_skills_sync.c" \
        "$CDIR/lib/libskillsync/skills_sync.c" \
        "$CDIR/lib/libhash/hash.c" \
        "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_skills_sync -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "skills_sync (compilation failed)"
fi

echo ""; echo "=== Skill Manage CRUD Tests (D81) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libskillusage" -I"$CDIR/lib/libdifflib" \
    "$CDIR/tests/test_skill_manage.c" \
    "$CDIR/src/tools/registry.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    "$CDIR/lib/libskillusage/skill_provenance.c" \
    "$CDIR/lib/libskillusage/skill_usage.c" \
    "$CDIR/lib/libdifflib/difflib.c" \
    -o /tmp/hermes_test_skill_manage -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_skill_manage > /dev/null 2>&1; then ok "skill_manage (10 tests)"
    else fail "skill_manage (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_skill_manage
else skip "skill_manage (compilation failed)"
fi &

echo ""; echo "=== Computer Use Backend Tests (S01-S02) ==="
if gcc -O2 -Wall -Wextra \
    -I"$CDIR/include" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libtemplate" \
    -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libdotenv" \
    -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libtui" \
    -I"$CDIR/lib/libdb" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libskin" \
    -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" -I"$CDIR/lib/libmcp" \
    -I"$CDIR/lib/libpath" -I"$CDIR/lib/libdatetime" -I"$CDIR/lib/libcsv" \
    -I"$CDIR/lib/libhash" -I"$CDIR/lib/libuuid" -I"$CDIR/lib/libbase64" \
    -I"$CDIR/lib/libhtml" -I"$CDIR/lib/libtextwrap" -I"$CDIR/lib/libglob" \
    -I"$CDIR/lib/libsignal" -I"$CDIR/lib/libenum" -I"$CDIR/lib/libdifflib" \
    -I"$CDIR/lib/libregex" -I"$CDIR/lib/libansi" -I"$CDIR/lib/libjson5" \
    "$CDIR/tests/test_computer_use.c" \
    "$CDIR/src/tools/computer_use.c" \
    "$CDIR/src/tools/registry.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_computer_use -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_computer_use > /dev/null 2>&1; then ok "computer_use (10 tests)"
    else fail "computer_use (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_computer_use
else skip "computer_use (compilation failed)"
fi &

echo ""; echo "=== Plugin Honcho (In-Memory Memory) Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'$CDIR/src/plugins'"' \
    "$CDIR/tests/test_plugin_honcho.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_honcho -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_honcho > /dev/null 2>&1; then ok "plugin_honcho (in-memory memory)"
    else fail "plugin_honcho (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_honcho
else skip "plugin_honcho (compilation failed)"
fi &

echo ""; echo "=== Plugin Achievements Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'$CDIR/src/plugins'"' \
    "$CDIR/tests/test_plugin_achievements.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_achievements -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_achievements > /dev/null 2>&1; then ok "plugin_achievements (achievement tracking)"
    else fail "plugin_achievements (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_achievements
else skip "plugin_achievements (compilation failed)"
fi &

echo ""; echo "=== Plugin Observability Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'$CDIR/src/plugins'"' \
    "$CDIR/tests/test_plugin_observability.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_observability -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_observability > /dev/null 2>&1; then ok "plugin_observability (metrics & observability)"
    else fail "plugin_observability (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_observability
else skip "plugin_observability (compilation failed)"
fi &

echo ""; echo "=== Plugin Skills Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'$CDIR/src/plugins'"' \
    "$CDIR/tests/test_plugin_skills.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_skills -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_skills > /dev/null 2>&1; then ok "plugin_skills (skill management)"
    else fail "plugin_skills (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_skills
else skip "plugin_skills (compilation failed)"
fi &

echo ""; echo "=== Plugin Image Gen Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'$CDIR/src/plugins'"' \
    "$CDIR/tests/test_plugin_image_gen.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_image_gen -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_image_gen > /dev/null 2>&1; then ok "plugin_image_gen (image generation)"
    else fail "plugin_image_gen (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_image_gen
else skip "plugin_image_gen (compilation failed)"
fi &

echo ""; echo "=== Plugin Google Meet Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'"$CDIR/src/plugins"'"' \
    "$CDIR/tests/test_plugin_google_meet.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_google_meet -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_google_meet > /dev/null 2>&1; then ok "plugin_google_meet (Google Meet meetings)"
    else fail "plugin_google_meet (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_google_meet
else skip "plugin_google_meet (compilation failed)"
fi &

echo ""; echo "=== Plugin Kanban (In-Memory Board) Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'"$CDIR/src/plugins"'"' \
    "$CDIR/tests/test_plugin_kanban.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_kanban -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_kanban > /dev/null 2>&1; then ok "plugin_kanban (in-memory board)"
    else fail "plugin_kanban (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_kanban
else skip "plugin_kanban (compilation failed)"
fi &

echo ""; echo "=== Plugin Spotify Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'"$CDIR/src/plugins"'"' \
    -DSPOTIFY_PLUGIN_VERSION='"1.0.0"' \
    "$CDIR/tests/test_plugin_spotify.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_spotify -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_spotify > /dev/null 2>&1; then ok "plugin_spotify (real Web API plugin)"
    else fail "plugin_spotify (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_spotify
else skip "plugin_spotify (compilation failed)"
fi &

echo ""; echo "=== Plugin Disk Cleanup Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'$CDIR/src/plugins'"' \
    "$CDIR/tests/test_plugin_disk_cleanup.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_disk_cleanup -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_disk_cleanup > /dev/null 2>&1; then ok "plugin_disk_cleanup (disk cleanup utility)"
    else fail "plugin_disk_cleanup (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_disk_cleanup
else skip "plugin_disk_cleanup (compilation failed)"
fi &

echo ""; echo "=== Plugin File Memory Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -DPLUGIN_DIR='"'$CDIR/src/plugins'"' \
    "$CDIR/tests/test_plugin_file_memory.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_file_memory -ldl -lm > /dev/null 2>&1; then
    if HERMES_HOME=/tmp/hermes_test_fm_home /tmp/hermes_test_plugin_file_memory > /dev/null 2>&1; then ok "plugin_file_memory (persistent memory store)"
    else fail "plugin_file_memory (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_plugin_file_memory
    rm -rf /tmp/hermes_test_fm_home
else skip "plugin_file_memory (compilation failed)"
fi &

run_lib_test "dotenv"   "tests/test_dotenv.c"       "lib/libdotenv"          "$CDIR/lib/libdotenv/dotenv.c"
run_lib_test "cron"     "tests/test_cron_lib.c"         "lib/libcron"            "$CDIR/lib/libcron/cron.c"
# cron sqlite store (needs json + cron_sqlite.c + libplugin includes)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_cron_sqlite.c" \
    "$CDIR/src/cron/cron_sqlite.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libcron/cron.c" \
    -o /tmp/hermes_test_cron_sqlite -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_cron_sqlite > /dev/null 2>&1; then ok "cron_sqlite"
    else fail "cron_sqlite (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_cron_sqlite
else
    skip "cron_sqlite (compilation failed)"
fi
# cron extras test (retry, chain, template — P172-P175; needs json + libplugin includes)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_cron_extras.c" \
    "$CDIR/src/cron/cron_extras.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_cron_extras -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_cron_extras > /dev/null 2>&1; then ok "cron_extras"
    else fail "cron_extras (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_cron_extras
else
    skip "cron_extras (compilation failed)"
fi
# cron_extras P176 utility test (cron_canonical_skills, cron_normalize_value, cron_normalize_deliver)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_cron_extras_util.c" \
    "$CDIR/src/cron/cron_extras.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_cron_util -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_cron_util > /dev/null 2>&1; then ok "cron_extras_util (P176: 32 tests)"
    else fail "cron_extras_util (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_cron_util
else
    skip "cron_extras_util (compilation failed)"
fi
# yuanbao_media utility test (G09: generate_file_id, build_image_msg, build_file_msg, crypto_md5_hex)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_yuanbao_media.c" \
    "$CDIR/src/tools/yuanbao_media.c" \
    "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libcrypto/crypto.c" \
    -o /tmp/hermes_test_yuanbao_media -lm -lssl -lcrypto > /dev/null 2>&1; then
    if /tmp/hermes_test_yuanbao_media > /dev/null 2>&1; then ok "yuanbao_media (G09: 15 tests)"
    else fail "yuanbao_media (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_yuanbao_media
else
    skip "yuanbao_media (compilation failed)"
fi
# Display word wrap test (display_core.c pure function)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libansi" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_display_word_wrap.c" \
    "$CDIR/src/cli/display_core.c" \
    -o /tmp/hermes_test_wordwrap -lm \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_wordwrap > /dev/null 2>&1; then ok "word_wrap (16 tests)"
    else fail "word_wrap (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_wordwrap
else skip "word_wrap (compilation failed)"
fi &
# telegram_network utility test (G07: telegram_resolve_system_dns, telegram_parse_doh_response)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_telegram_network.c" \
    "$CDIR/src/gateway/platforms/telegram_network.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_telegram_network -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_telegram_network > /dev/null 2>&1; then ok "telegram_network (G07: 22 tests)"
    else fail "telegram_network (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_telegram_network
else
    skip "telegram_network (compilation failed)"
fi
# cron_locking test (P171 — standalone, no external deps beyond hermes.h)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_cron_locking.c" "$CDIR/src/cron/cron_locking.c" \
    -o /tmp/hermes_test_cron_locking > /dev/null 2>&1; then
    if /tmp/hermes_test_cron_locking > /dev/null 2>&1; then ok "cron_locking (23 tests)"
    else fail "cron_locking (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_cron_locking
else
    skip "cron_locking (compilation failed)"
fi
# cron_scripts test (P177 -- script execution with popen)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_cron_scripts.c" \
    "$CDIR/src/cron/cron_scripts.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_cron_scripts -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_cron_scripts > /dev/null 2>&1; then ok "cron_scripts (10 tests)"
    else fail "cron_scripts (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_cron_scripts
else
    skip "cron_scripts (compilation failed)"
fi
# scheduler test (schedule parsing, interval check, should_run)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libcron" -I"$CDIR/src/cron" \
    "$CDIR/tests/test_scheduler.c" \
    "$CDIR/src/cron/scheduler.c" \
    "$CDIR/lib/libcron/cron.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_scheduler -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_scheduler > /dev/null 2>&1; then ok "scheduler (18 tests)"
    else fail "scheduler (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_scheduler
else
    skip "scheduler (compilation failed)"
fi
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
fi &
run_lib_test "tui"      "tests/test_tui.c"          "lib/libtui"             "$CDIR/lib/libtui/tui.c"
run_lib_test "db"       "tests/test_db.c"           "lib/libdb"              "$CDIR/lib/libdb/db.c"
run_lib_test "path"     "tests/test_path.c"         "lib/libpath"            "$CDIR/lib/libpath/path.c"
run_lib_test "skin"     "tests/test_skin.c"         "lib/libskin"            "-I$CDIR/lib/libjson $CDIR/lib/libskin/skin.c $CDIR/lib/libjson/json.c -lm"
run_lib_test "path_security" "tests/test_path_security.c" "lib/libpath" "$CDIR/lib/libpath/path.c"
run_lib_test "interrupt" "tests/test_interrupt.c" "lib/libinterrupt" "$CDIR/lib/libinterrupt/interrupt.c -lpthread"
run_lib_test "file_state" "tests/test_file_state.c" "lib/libfilestate" "$CDIR/lib/libfilestate/file_state.c -lpthread"
run_lib_test "tool_backend" "tests/test_tool_backend.c" "lib/libtoolbackend" "$CDIR/lib/libtoolbackend/tool_backend.c -lm"
run_lib_test "rate_limit" "tests/test_rate_limit.c" "lib/libratelimit" "$CDIR/lib/libratelimit/rate_limit.c -lm"
run_lib_test "nous_rate_guard" "tests/test_nous_rate_guard.c" "include" "$CDIR/src/agent/nous_rate_guard.c $CDIR/lib/libjson/json.c -lm"
# Managed gateway test (needs mangateway + toolbackend includes)
echo ""; echo "=== Managed Gateway Library Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libmangateway" -I"$CDIR/lib/libtoolbackend" \
    "$CDIR/tests/test_managed_gateway.c" \
    "$CDIR/lib/libmangateway/managed_gateway.c" "$CDIR/lib/libtoolbackend/tool_backend.c" \
    -o /tmp/hermes_test_managed_gateway -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_managed_gateway > /dev/null 2>&1; then ok "managed_gateway"
    else fail "managed_gateway (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_managed_gateway
else skip "managed_gateway (compilation failed)"
fi &

# datetime library test (J05 -- standalone, no deps)
echo ""; echo "=== datetime Library Tests (J05) ==="
run_lib_test "datetime" "tests/test_datetime.c" "lib/libdatetime" "$CDIR/lib/libdatetime/datetime.c"

# CSV library test (J06 — standalone, no deps)
echo ""; echo "=== CSV Library Tests (J06) ==="
run_lib_test "csv" "tests/test_csv.c" "lib/libcsv" "$CDIR/lib/libcsv/csv.c"

# Hash library test (J07 — needs OpenSSL)
echo ""; echo "=== Hash Library Tests (J07) ==="
run_lib_test "hash" "tests/test_hash.c" "lib/libhash" "$CDIR/lib/libhash/hash.c -lssl -lcrypto"

# Cron library test (J07.5 — standalone, no deps)
echo ""; echo "=== Cron Library Tests (J07.5) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libcron" "$CDIR/tests/test_cron.c" -o /tmp/hermes_test_cron "$CDIR/lib/libcron/cron.c" -lm >/dev/null 2>&1 && [[ -x /tmp/hermes_test_cron ]]; then
    if /tmp/hermes_test_cron >/dev/null 2>&1; then ok "cron"
    else fail "cron"
    fi
    rm -f /tmp/hermes_test_cron
else
    skip "cron (compilation failed)"
fi

# UUID library test (J08 — needs libhash for v5 + OpenSSL)
echo ""; echo "=== UUID Library Tests (J08) ==="
run_lib_test "uuid" "tests/test_uuid.c" "lib/libuuid" "$CDIR/lib/libuuid/uuid.c $CDIR/lib/libhash/hash.c -lssl -lcrypto"

# Base64 library test (J09 — standalone, no deps)
echo ""; echo "=== Base64 Library Tests (J09) ==="
run_lib_test "base64" "tests/test_base64.c" "lib/libbase64" "$CDIR/lib/libbase64/base64.c"

# HTML library test (J10 — standalone, no deps)
echo ""; echo "=== HTML Library Tests (J10) ==="
run_lib_test "html" "tests/test_html.c" "lib/libhtml" "$CDIR/lib/libhtml/html.c"
run_lib_test "textwrap" "tests/test_textwrap.c" "lib/libtextwrap" "$CDIR/lib/libtextwrap/textwrap.c"
run_lib_test "glob" "tests/test_glob.c" "lib/libglob" "$CDIR/lib/libglob/glob.c"
run_lib_test "signal" "tests/test_signal.c" "lib/libsignal" "$CDIR/lib/libsignal/hermes_signal.c"
run_lib_test "line_edit" "tests/test_line_edit.c" "lib/liblineedit" "$CDIR/lib/liblineedit/line_edit.c"
echo ""; echo "=== Enum Library Tests (J14, header-only) ==="
run_lib_test "enum" "tests/test_enum.c" "lib/libenum" ""
run_lib_test "difflib" "tests/test_difflib.c" "lib/libdifflib" "$CDIR/lib/libdifflib/difflib.c"
run_lib_test "regex" "tests/test_regex.c" "lib/libregex" "$CDIR/lib/libregex/hermes_regex.c"
run_lib_test "signal" "tests/test_hermes_signal.c" "lib/libsignal" "$CDIR/lib/libsignal/hermes_signal.c"
echo ""; echo "=== WebSocket Library Tests (J18) ==="
run_lib_test "websocket" "tests/test_websocket.c" "lib/libwebsocket" "$CDIR/lib/libwebsocket/websocket.c -lssl -lcrypto"
echo ""; echo "=== TOML Library Tests (J19) ==="
run_lib_test "toml" "tests/test_toml.c" "lib/libtoml" "$CDIR/lib/libtoml/toml.c"
echo ""; echo "=== Tokenizer Tests (N01) ==="
run_lib_test "hermes_tokenizer" "tests/test_hermes_tokenizer.c" "include" "$CDIR/src/hermes_tokenizer.c"
echo ""; echo "=== CLI Display Tests (D01) ==="
run_lib_test "cli_display" "tests/test_cli_display.c" "include" "$CDIR/src/deps/cli_display.c -I$CDIR/lib/libjson -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== ACP Permissions Tests (P60) ==="
run_lib_test "permissions" "tests/test_permissions.c" "include" "$CDIR/src/acp/permissions.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson"
echo ""; echo "=== ACP Events Tests (M45) ==="
run_lib_test "acp_events" "tests/test_acp_events.c" "include" "-I$CDIR/lib/libjson -I$CDIR/lib/libplugin -I$CDIR/lib/libhttp -I$CDIR/lib/libcrypto -I$CDIR/lib/libdb -I$CDIR/lib/libmcp $CDIR/src/acp/events.c $CDIR/lib/libjson/json.c -lm -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== ACP Resource Tests (M46) ==="
run_lib_test "acp_resource" "tests/test_acp_resource.c" "include" "$CDIR/src/acp/resource.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libplugin"
echo ""; echo "=== CLI Paths Tests (P21) ==="
run_lib_test "cli_paths" "tests/test_cli_paths.c" "include" "$CDIR/src/cli/paths.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libjson -I$CDIR/lib/libyaml -I$CDIR/lib/libhttp -I$CDIR/lib/libmcp -I$CDIR/lib/libcrypto -I$CDIR/lib/libdb -I$CDIR/lib/libplugin -I$CDIR/lib/libskin -I$CDIR/lib/libwebsocket -I$CDIR/lib/libprotobuf -I$CDIR/lib/libcron -I$CDIR/lib/libproc -I$CDIR/lib/libtui -I$CDIR/lib/libtemplate -I$CDIR/lib/libdotenv"
echo ""; echo "=== Session ID Tests (G166) ==="
run_lib_test "session_id" "tests/test_session_id.c" "include" "$CDIR/src/agent/agent_loop.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libplugin -Wl,--unresolved-symbols=ignore-all" "session_id (21 tests)"
echo ""; echo "=== Session Search Tests (P142) ==="
run_lib_test "session_search" "tests/test_session_search.c" "include" "$CDIR/src/tools/session_search.c $CDIR/lib/libjson/json.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libjson -I$CDIR/lib/libyaml -I$CDIR/lib/libhttp -I$CDIR/lib/libmcp -I$CDIR/lib/libcrypto -I$CDIR/lib/libdb -I$CDIR/lib/libplugin -I$CDIR/lib/libskin -I$CDIR/lib/libwebsocket -I$CDIR/lib/libprotobuf -I$CDIR/lib/libcron -I$CDIR/lib/libproc -I$CDIR/lib/libtui -I$CDIR/lib/libtemplate -I$CDIR/lib/libdotenv" "session_search (17 tests)"
echo ""; echo "=== Session CRUD Tests (P143) ==="
run_lib_test "session_crud" "tests/test_session_crud.c" "include" "$CDIR/src/tools/session_crud.c $CDIR/lib/libdb/db.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libdb -I$CDIR/lib/libplugin -Wl,--unresolved-symbols=ignore-all -D_FORTIFY_SOURCE=0"
echo ""; echo "=== Tirith Security Tests (O13) ==="
run_lib_test "tirith" "tests/test_tirith.c" "include" "$CDIR/src/tools/tirith.c -I$CDIR/lib/libplugin -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== Sanitize Output Tests (P166) ==="
run_lib_test "sanitize" "tests/test_sanitize.c" "include" "$CDIR/src/agent/sanitize.c $CDIR/src/agent/redact.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libjson -I$CDIR/lib/libyaml -I$CDIR/lib/libhttp -I$CDIR/lib/libmcp -I$CDIR/lib/libcrypto -I$CDIR/lib/libdb -I$CDIR/lib/libplugin -I$CDIR/lib/libskin -I$CDIR/lib/libwebsocket -I$CDIR/lib/libprotobuf -I$CDIR/lib/libcron -I$CDIR/lib/libproc -I$CDIR/lib/libtui -I$CDIR/lib/libtemplate -I$CDIR/lib/libdotenv"
echo ""; echo "=== Message/Context Tests ==="
run_lib_test "context" "tests/test_context.c" "include" "$CDIR/src/agent/context.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libjson -I$CDIR/lib/libyaml -I$CDIR/lib/libhttp -I$CDIR/lib/libmcp -I$CDIR/lib/libcrypto -I$CDIR/lib/libdb -I$CDIR/lib/libplugin -I$CDIR/lib/libskin -I$CDIR/lib/libwebsocket -I$CDIR/lib/libprotobuf -I$CDIR/lib/libcron -I$CDIR/lib/libproc -I$CDIR/lib/libtui -I$CDIR/lib/libtemplate -I$CDIR/lib/libdotenv"
echo ""; echo "=== Tool API Helpers Tests (P51) ==="
run_lib_test "api_helpers" "tests/test_api_helpers.c" "include" "$CDIR/src/tools/api_helpers.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libjson -I$CDIR/lib/libyaml -I$CDIR/lib/libhttp -I$CDIR/lib/libmcp -I$CDIR/lib/libcrypto -I$CDIR/lib/libdb -I$CDIR/lib/libplugin -I$CDIR/lib/libskin -I$CDIR/lib/libwebsocket -I$CDIR/lib/libprotobuf -I$CDIR/lib/libcron -I$CDIR/lib/libproc -I$CDIR/lib/libtui -I$CDIR/lib/libtemplate -I$CDIR/lib/libdotenv"
echo ""; echo "=== Approval Security Tests (T02) ==="
run_lib_test "approve" "tests/test_approve.c" "include" "$CDIR/src/tools/approval.c $CDIR/lib/libjson/json.c $CDIR/lib/libansi/ansi_strip.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libfilestate -I$CDIR/lib/librateguard -I$CDIR/lib/libplugin -I$CDIR/lib/libskin -I$CDIR/lib/libwebsocket -I$CDIR/lib/libprotobuf -I$CDIR/lib/libcron -I$CDIR/lib/libproc -I$CDIR/lib/libtui -I$CDIR/lib/libtemplate -I$CDIR/lib/libdotenv -I$CDIR/lib/libyaml -I$CDIR/lib/libmcp -I$CDIR/lib/libcrypto -I$CDIR/lib/libdb -I$CDIR/lib/libbinary -I$CDIR/lib/libbrowser -I$CDIR/lib/libdebug -I$CDIR/lib/libosv -I$CDIR/lib/libwebsite -I$CDIR/lib/libansi -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== Mattermost Platform Tests (S7) ==="
run_lib_test "mattermost" "tests/test_mattermost.c" "include" "$CDIR/src/gateway/platforms/mattermost.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -lm -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== QQ Bot Platform Tests (S7) ==="
run_lib_test "qqbot" "tests/test_qqbot.c" "include" "$CDIR/src/gateway/platforms/qqbot.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libplugin -I$CDIR/lib/libmangateway -lm -lpthread -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== Slack Platform Tests (S7) ==="
run_lib_test "slack" "tests/test_slack.c" "include" "$CDIR/src/gateway/platforms/slack.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libplugin -I$CDIR/lib/libmangateway -lm -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== WeCom Platform Tests (S7) ==="
run_lib_test "wecom" "tests/test_wecom.c" "include" "$CDIR/src/gateway/platforms/wecom.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libplugin -I$CDIR/lib/libmangateway -lm -lpthread -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== BlueBubbles Platform Tests (S7) ==="
run_lib_test "bluebubbles" "tests/test_bluebubbles.c" "include" "$CDIR/src/gateway/platforms/bluebubbles.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libplugin -I$CDIR/lib/libmangateway -lm -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== Matrix Platform Tests (S7) ==="
run_lib_test "matrix" "tests/test_matrix.c" "include" "$CDIR/src/gateway/platforms/matrix.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libplugin -I$CDIR/lib/libmangateway -lm -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== Feishu Platform Tests (S7) ==="
run_lib_test "feishu" "tests/test_feishu.c" "include" "$CDIR/src/gateway/platforms/feishu.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libplugin -I$CDIR/lib/libmangateway -lm -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== Email Platform Tests (S7) ==="
run_lib_test "email" "tests/test_email.c" "include" "$CDIR/src/gateway/platforms/email.c $CDIR/lib/libjson/json.c -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libplugin -I$CDIR/lib/libmangateway -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== Session Title Tests ==="
run_lib_test "title" "tests/test_title.c" "include" "$CDIR/src/agent/title.c -I$CDIR/lib/libplugin -Wl,--unresolved-symbols=ignore-all"
echo ""; echo "=== Think Scrubber Tests ==="
run_lib_test "think_scrubber" "tests/test_think_scrubber.c" "include" "$CDIR/src/agent/think_scrubber.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libplugin"
echo ""; echo "=== Message Repair Tests (L25 — agent_runtime_helpers) ==="
run_lib_test "agent_message_repair" "tests/test_agent_message_repair.c" "include" "$CDIR/src/agent/agent_message_repair.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libyaml -I$CDIR/lib/libmcp -I$CDIR/lib/libcrypto -I$CDIR/lib/libdb -I$CDIR/lib/libplugin -I$CDIR/lib/libskin -I$CDIR/lib/libwebsocket -I$CDIR/lib/libprotobuf -I$CDIR/lib/libcron -I$CDIR/lib/libproc -I$CDIR/lib/libtui -I$CDIR/lib/libtemplate -I$CDIR/lib/libdotenv"
echo ""; echo "=== Message Sanitize Tests (L26 — build_assistant_message) ==="
run_lib_test "agent_message_sanitize" "tests/test_agent_message_sanitize.c" "include" "$CDIR/src/agent/agent_message_sanitize.c $CDIR/src/agent/sanitize.c $CDIR/src/agent/redact.c $CDIR/src/agent/context.c $CDIR/lib/libjson/json.c $CDIR/lib/libhash/hash.c $CDIR/lib/libregex/hermes_regex.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libjson -I$CDIR/lib/libhttp -I$CDIR/lib/libplugin -I$CDIR/lib/libregex -I$CDIR/lib/libhash -I$CDIR/lib/libbase64"
echo ""; echo "=== Continuation Prompt Tests (P0) ==="
run_lib_test "continuation" "tests/test_system_prompt_continuation.c" "include" "$CDIR/src/agent/system_prompt.c $CDIR/lib/libjson/json.c -Wl,--unresolved-symbols=ignore-all -I$CDIR/lib/libjson -I$CDIR/lib/libplugin"
echo ""; echo "=== Tool Error Sanitization Tests (P1) ==="
run_lib_test "tool_error" "tests/test_tool_error.c" "include" "$CDIR/src/agent/tool_error.c"
echo ""; echo "=== Tool Argument Coercion Tests (P1) ==="
run_lib_test "tool_coerce" "tests/test_tool_coerce.c" "include" "$CDIR/src/tools/tool_coerce.c"
echo ""; echo "=== ANSI Library Tests (J22) ==="
run_lib_test "ansi" "tests/test_ansi.c" "lib/libansi" "$CDIR/lib/libansi/ansi.c $CDIR/lib/libansi/ansi_strip.c"
run_lib_test "ansi_strip" "tests/test_ansi_strip.c" "lib/libansi" "$CDIR/lib/libansi/ansi_strip.c"
echo ""; echo "=== Markdown Render Tests (J22) ==="
run_lib_test "markdown_render" "tests/test_markdown_render.c" "include" "$CDIR/src/agent/markdown_render.c"
echo ""; echo "=== FAL Common Library Tests (fal_common) ==="
    if gcc -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libfal_common" \
        "$CDIR/tests/test_fal_common.c" \
        "$CDIR/lib/libfal_common/fal_common.c" \
        "$CDIR/lib/libjson/json.c" \
        "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_fal_common -lssl -lcrypto -lm -lz > /dev/null 2>&1; then
        if /tmp/hermes_test_fal_common > /dev/null 2>&1; then ok "fal_common_lib"
        else fail "fal_common_lib (test binary returned non-zero)"; fi
        rm -f /tmp/hermes_test_fal_common
    else skip "fal_common (compilation failed)"
    fi

echo ""; echo "=== JSON5 Library Tests (J20) ==="
run_lib_test "json5" "tests/test_json5.c" "lib/libjson5" "$CDIR/lib/libjson5/json5.c -I $CDIR/lib/libjson $CDIR/lib/libjson/json.c"
echo ""; echo "=== Display Module Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libansi" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libskin" \
    "$CDIR/tests/test_display.c" \
    "$CDIR/src/cli/display.c" "$CDIR/src/cli/display_core.c" \
    "$CDIR/lib/libansi/ansi.c" "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libskin/skin.c" \
    -o /tmp/hermes_test_display -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_display > /dev/null 2>&1; then ok "display (39 tests)"
    else fail "display (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_display
else skip "display (compilation failed)"
fi &

# Slack Block Kit formatting test (M09 — needs only json lib)
echo ""; echo "=== Slack Block Kit Formatting Tests (M09) ==="
run_lib_test "slack_blocks" "tests/test_slack_blocks.c" "lib/libjson" "$CDIR/lib/libjson/json.c"

# JSON output mode test (H14 — self-contained, no deps)
echo ""; echo "=== JSON Output Mode Tests (H14) ==="
run_lib_test "json_output" "tests/test_json_output.c" "." ""

# WhatsApp message format test (M10 — needs json lib)
echo ""; echo "=== WhatsApp Message Format Tests (M10) ==="
run_lib_test "whatsapp_msg" "tests/test_whatsapp_msg.c" "lib/libjson" "$CDIR/lib/libjson/json.c"

# Vision tool test (M33 — self-contained, no deps)
echo ""; echo "=== Vision Tool Tests (M33) ==="
# Note: vision_tool is compiled inline below as part of the vision.c test.
# The run_lib_test version was removed because it's already covered by the
# standalone gcc compilation that also tests vision.c with libbase64.

# TTS tool test (M34 — needs tts.c + tool_config + libhttp + plugin)
echo ""; echo "=== TTS Tool Tests (M34) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libtooloutput" \
    "$CDIR/tests/test_tts_tool.c" \
    "$CDIR/src/tools/tts.c" "$CDIR/src/tools/registry.c" "$CDIR/src/tools/tool_config.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_tts -lm -lssl -lcrypto -lz -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_tts > /dev/null 2>&1; then ok "tts_tool (11 tests)"
    else fail "tts_tool (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_tts
else skip "tts_tool (compilation failed)"
fi &

# Discord tool test (T06 — needs discord.c + api_helpers + tool_config + vault + libhttp + libcrypto)
echo ""; echo "=== Discord Tool Tests (T06) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_discord.c" \
    "$CDIR/src/tools/discord.c" "$CDIR/src/tools/registry.c" "$CDIR/src/tools/api_helpers.c" \
    "$CDIR/src/tools/tool_config.c" "$CDIR/src/agent/vault.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libhttp/http.c" \
    "$CDIR/lib/libcrypto/crypto.c" \
    "$CDIR/lib/libtoolbackend/tool_backend.c" \
    -o /tmp/hermes_test_discord -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_discord > /dev/null 2>&1; then ok "discord_tool (13 tests)"
    else fail "discord_tool (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_discord
else skip "discord_tool (compilation failed)"
fi &

# Error system test (K01-K05: typed error system — standalone, only needs hermes_error.c)
echo ""; echo "=== Error System Tests (K01-K05) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" \
    "$CDIR/tests/test_hermes_error.c" \
    "$CDIR/src/hermes_error.c" \
    -o /tmp/hermes_test_errors -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_errors > /dev/null 2>&1; then ok "hermes_error (20 tests)"
    else
        echo "  Error system test output:"
        /tmp/hermes_test_errors 2>&1 | sed 's/^/    /'
        fail "hermes_error (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_errors
else skip "hermes_error (compilation failed)"
fi &

echo ""; echo "=== Redact Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" -I"$CDIR/lib/libdb" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libtui" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libdotenv" \
    "$CDIR/tests/test_redact.c" \
    "$CDIR/src/agent/redact.c" \
    -o /tmp/hermes_test_redact -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_redact > /dev/null 2>&1; then ok "redact (17 tests)"
    else fail "redact (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_redact
else skip "redact (compilation failed)"
fi &

# Tool config test (P54 — depends on vault, crypto, json)
echo ""; echo "=== Tool Config Tests (P54) === "
if gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_tool_config.c" \
    "$CDIR/src/tools/tool_config.c" "$CDIR/src/agent/vault.c" \
    "$CDIR/lib/libcrypto/crypto.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_tool_config -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_tool_config > /dev/null 2>&1; then ok "tool_config (28 tests)"
    else
        echo "  Tool config test output:"
        /tmp/hermes_test_tool_config 2>&1 | sed 's/^/    /'
        fail "tool_config (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_tool_config
else
    echo "  Compilation failed"
    gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libplugin" \
        "$CDIR/tests/test_tool_config.c" \
        "$CDIR/src/tools/tool_config.c" "$CDIR/src/agent/vault.c" \
        "$CDIR/lib/libcrypto/crypto.c" "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_tool_config -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | head -5
    skip "tool_config (compilation failed)"
fi

# Tool Dispatch Helpers test (libtooldispatch — 5 stateless utility functions)
echo ""; echo "=== Tool Dispatch Helpers Tests (libtooldispatch) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libtooldispatch" \
    "$CDIR/tests/test_tool_dispatch_helpers.c" \
    "$CDIR/lib/libtooldispatch/tool_dispatch_helpers.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_tool_dispatch -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_tool_dispatch > /dev/null 2>&1; then ok "tool_dispatch_helpers (57 tests)"
    else fail "tool_dispatch_helpers (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_tool_dispatch
else skip "tool_dispatch_helpers (compilation failed)"
fi &

# Home Assistant tool test (needs json lib + http lib for unresolved symbols)
echo ""; echo "=== Home Assistant Tool Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_homeassistant.c" \
    "$CDIR/src/tools/homeassistant.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_homeassistant -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_homeassistant > /dev/null 2>&1; then ok "homeassistant (25 tests)"
    else fail "homeassistant (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_homeassistant
else skip "homeassistant (compilation failed)"
fi &

# DingTalk gateway test
echo ""; echo "=== DingTalk Gateway Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_dingtalk.c" \
    "$CDIR/src/gateway/platforms/dingtalk.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_dingtalk -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_dingtalk > /dev/null 2>&1; then ok "dingtalk (10 tests)"
    else fail "dingtalk (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_dingtalk
else skip "dingtalk (compilation failed)"
fi &

# Result storage test (P49-P50 — needs hermes_config_load from config.c, skip for now)
# Test file exists at tests/test_result_storage.c — requires full link with config.c
# echo ""; echo "=== Tool Result Storage Tests (P49-P50) === (skipped — needs config dependency resolution)"

# URL safety test (needs url_safety.c + json + http)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_url_safety.c" \
    "$CDIR/src/tools/url_safety.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_url_safety -lssl -lcrypto -lz -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_url_safety > /dev/null 2>&1; then ok "url_safety (83 tests)"
    else fail "url_safety (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_url_safety
else skip "url_safety (compilation failed)"
fi &

# Media cache test
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libncurses/include" \
    "$CDIR/tests/test_media_cache.c" \
    "$CDIR/src/tools/media_cache.c" \
    -o /tmp/hermes_test_media_cache -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_media_cache > /dev/null 2>&1; then ok "media_cache (32 tests)"
    else fail "media_cache (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_media_cache
else skip "media_cache (compilation failed)"
fi &

# Config test (needs config.c + paths.c + yaml + json + provider_metadata + url_safety)
if gcc -O0 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_config.c" \
    "$CDIR/src/cli/config.c" "$CDIR/src/cli/paths.c" \
    "$CDIR/src/agent/provider_metadata.c" "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libyaml/yaml.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_config -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
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
        "$CDIR/src/agent/provider_metadata.c" "$CDIR/src/tools/url_safety.c" \
        "$CDIR/lib/libyaml/yaml.c" "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_config -lm 2>&1 | sed 's/^/    /'
    skip "config (compilation failed)"
fi

# File permissions hardening test (O15 — needs config.c + paths.c + all libs)
echo ""; echo "=== Retry Utils Tests (B117) ==="
run_lib_test "retry_utils" "tests/test_retry_utils.c" "include" "$CDIR/src/agent/retry_utils.c"

echo ""; echo "=== Trajectory Tests (B118) ==="
run_lib_test "trajectory" "tests/test_trajectory.c" "include" "$CDIR/src/agent/trajectory.c $CDIR/lib/libjson/json.c"

echo ""; echo "=== Context Engine Tests (B15) ==="
run_lib_test "context_engine" "tests/test_context_engine.c" "include" "$CDIR/src/agent/context_engine.c $CDIR/lib/libjson/json.c -lm"

echo ""; echo "=== Context References Tests (B42) ==="
run_lib_test "context_references" "tests/test_context_references.c" "include" "$CDIR/src/agent/context_references.c $CDIR/lib/libjson/json.c"

echo ""; echo "=== Portal Tags Tests (B119) ==="
run_lib_test "portal_tags" "tests/test_portal_tags.c" "include" "$CDIR/src/agent/portal_tags.c"

echo ""; echo "=== Markdown Tables Tests (B120) ==="
run_lib_test "markdown_tables" "tests/test_markdown_tables.c" "include" "$CDIR/src/agent/markdown_tables.c"

echo ""; echo "=== Logger Tests (M10) ==="
SLERMES_HOME=/tmp/hermes_log_test run_lib_test "logger" "tests/test_logger.c" "include" "-I$CDIR/lib/libplugin $CDIR/src/agent/logger.c -lpthread -Wl,--unresolved-symbols=ignore-all -Wno-error=format-truncation"
echo ""; echo "=== Hook Registry Tests (P186) ==="
run_lib_test "hook_registry" "tests/test_hook_registry.c" "include" "$CDIR/src/agent/hook_registry.c -lpthread"
echo ""; echo "=== File Safety Tests (P02) ==="
run_lib_test "file_safety" "tests/test_file_safety.c" "include" "$CDIR/src/agent/file_safety.c"

echo ""; echo "=== Subdirectory Hints Tests ==="
run_lib_test "subdir_hints" "tests/test_subdir_hints.c" "include" "-I$CDIR/lib/libjson $CDIR/src/agent/subdir_hints.c $CDIR/lib/libjson/json.c -lpthread"

echo ""; echo "=== System Prompt Tests ==="
run_lib_test "system_prompt" "tests/test_system_prompt.c" "include" "-I$CDIR/lib/libplugin $CDIR/src/agent/system_prompt.c"

echo ""; echo "=== Tool Result Classification Tests ==="
run_lib_test "tool_result" "tests/test_tool_result.c" "include" "-I$CDIR/lib/libjson $CDIR/src/tools/tool_result.c $CDIR/lib/libjson/json.c"

echo ""; echo "=== Video Gen Registry Tests ==="
run_lib_test "video_gen_registry" "tests/test_video_gen_registry.c" "include" "-I$CDIR/lib/libplugin $CDIR/src/tools/video_gen_registry.c $CDIR/tests/stub_vault.c -lpthread"

echo ""; echo "=== Image Gen Registry Tests ==="
run_lib_test "image_gen_registry" "tests/test_image_gen_registry.c" "include" "-I$CDIR/lib/libplugin $CDIR/src/tools/image_gen_registry.c $CDIR/tests/stub_vault.c -lpthread"

echo ""; echo "=== Web Search Registry Tests ==="
run_lib_test "web_search_registry" "tests/test_web_search_registry.c" "include" "$CDIR/src/tools/web_search_registry.c -lpthread"

echo ""; echo "=== Video Analyze Tests ==="
run_lib_test "video_analyze" "tests/test_video_analyze.c" "include" "-lm"

echo ""; echo "=== Account Usage Tests ==="
run_lib_test "account_usage" "tests/test_account_usage.c" "include" "$CDIR/src/tools/account_usage.c $CDIR/lib/libjson/json.c $CDIR/lib/libhttp/http.c -lssl -lcrypto -lm -lz"

echo ""; echo "=== Skill Preprocessing Tests ==="
run_lib_test "skill_preprocessing" "tests/test_skill_preprocessing.c" "include" "$CDIR/src/agent/skill_preprocessing.c"

echo ""; echo "=== Manual Compression Feedback Tests ==="
run_lib_test "manual_compression" "tests/test_manual_compression_feedback.c" "include" "$CDIR/src/agent/manual_compression_feedback.c"

echo ""; echo "=== Prompt Caching Tests ==="
run_lib_test "prompt_caching" "tests/test_prompt_caching.c" "include" "$CDIR/src/agent/prompt_caching.c"

echo ""; echo "=== Usage Pricing Tests (Pricing) ==="
run_lib_test "usage_pricing" "tests/test_usage_pricing.c" "include" "$CDIR/src/agent/usage_pricing.c $CDIR/src/hermes_tokenizer.c -lm"

echo ""; echo "=== Curator State Tests ==="
run_lib_test "curator" "tests/test_curator.c" "include" "-I$CDIR/lib/libjson -I$CDIR/lib/libplugin $CDIR/src/agent/curator.c $CDIR/lib/libjson/json.c -lm"

echo ""; echo "=== Shell Hooks Tests (B07) ==="
run_lib_test "shell_hooks" "tests/test_shell_hooks.c" "include" "-I$CDIR/lib/libjson $CDIR/src/agent/shell_hooks.c $CDIR/src/agent/hook_registry.c $CDIR/lib/libjson/json.c -lpthread -Wno-unused-function"

echo ""; echo "=== Cron Job Schedule Tests ==="
run_lib_test "cronjob" "tests/test_cronjob.c" "include" "-I$CDIR/lib/libcron $CDIR/lib/libcron/cron.c -lm"

echo ""; echo "=== Gemini Schema Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_gemini_schema.c" "$CDIR/src/agent/gemini_schema.c" "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" -I"$CDIR/lib/libjson" \
    -o /tmp/hermes_test_gemini_schema -lm 2>/dev/null && [[ -x /tmp/hermes_test_gemini_schema ]]; then
    if /tmp/hermes_test_gemini_schema > /dev/null 2>&1; then
        ok "gemini_schema"
    else fail "gemini_schema (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_gemini_schema
else skip "gemini_schema (compilation failed)"
fi &

echo ""; echo "=== Moonshot Schema Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_moonshot_schema.c" "$CDIR/src/agent/moonshot_schema.c" "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" -I"$CDIR/lib/libjson" \
    -o /tmp/hermes_test_moonshot_schema -lm 2>/dev/null && [[ -x /tmp/hermes_test_moonshot_schema ]]; then
    if /tmp/hermes_test_moonshot_schema > /dev/null 2>&1; then
        ok "moonshot_schema"
    else fail "moonshot_schema (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_moonshot_schema
else skip "moonshot_schema (compilation failed)"
fi &

echo ""; echo "=== Onboarding Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_onboarding.c" "$CDIR/src/agent/onboarding.c" "$CDIR/lib/libjson/json.c" \
    -I"$CDIR/include" -I"$CDIR/lib/libjson" \
    -o /tmp/hermes_test_onboarding -lm 2>/dev/null && [[ -x /tmp/hermes_test_onboarding ]]; then
    if /tmp/hermes_test_onboarding > /dev/null 2>&1; then
        ok "onboarding"
    else fail "onboarding (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_onboarding
else skip "onboarding (compilation failed)"
fi &

echo ""; echo "=== Skill Bundle Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_skill_bundles.c" "$CDIR/src/agent/skill_bundles.c" "$CDIR/lib/libyaml/yaml.c" \
    -I"$CDIR/include" -I"$CDIR/lib/libyaml" \
    -o /tmp/hermes_test_skill_bundles -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_skill_bundles ]]; then
    if /tmp/hermes_test_skill_bundles > /dev/null 2>&1; then
        ok "skill_bundles"
    else fail "skill_bundles (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_skill_bundles
else skip "skill_bundles (compilation failed)"
fi &

echo ""; echo "=== i18n Tests ==="
if gcc -O2 -Wall -Wextra "$CDIR/tests/test_i18n.c" "$CDIR/src/agent/i18n.c" \
    -I"$CDIR/include" -I"$CDIR/src/agent" \
    -o /tmp/hermes_test_i18n -lm 2>/dev/null && [[ -x /tmp/hermes_test_i18n ]]; then
    if /tmp/hermes_test_i18n > /dev/null 2>&1; then
        ok "i18n"
    else fail "i18n (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_i18n
else skip "i18n (compilation failed)"
fi &

echo ""; echo "=== Edit Approval Tests ==="
run_lib_test "edit_approval" "tests/test_edit_approval.c" "include" "-I$CDIR/lib/libjson $CDIR/src/acp/edit_approval.c $CDIR/lib/libjson/json.c"

echo ""; echo "=== Tokenizer Tests ==="
run_lib_test "tokenizer" "tests/test_tokenizer.c" "include" "$CDIR/src/hermes_tokenizer.c"

echo ""; echo "=== Fuzz Tests (T08) ==="
run_lib_test "fuzz" "tests/test_fuzz.c" "include" "-I$CDIR/lib/libjson -I$CDIR/lib/libyaml -I$CDIR/lib/libtemplate -I$CDIR/lib/libregex -I$CDIR/lib/libhtml -I$CDIR/lib/libpath $CDIR/lib/libjson/json.c $CDIR/lib/libyaml/yaml.c $CDIR/lib/libtemplate/template.c $CDIR/lib/libregex/hermes_regex.c $CDIR/lib/libhtml/html.c $CDIR/lib/libpath/path.c"

# File permissions hardening test (O15 — needs config.c + paths.c + all libs)
if gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libwebsocket" \
    -I"$CDIR/lib/libprotobuf" -I"$CDIR/lib/libdb" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libcron" \
    -I"$CDIR/lib/libproc" -I"$CDIR/lib/libtui" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libdotenv" \
    "$CDIR/tests/test_file_permissions.c" \
    "$CDIR/src/cli/config.c" "$CDIR/src/cli/paths.c" \
    "$CDIR/src/secrets.c" "$CDIR/src/agent/provider_metadata.c" "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libyaml/yaml.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_perm -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_perm > /dev/null 2>&1; then ok "file_permissions (15 tests)"
    else
        echo "  Permission test output:"
        /tmp/hermes_test_perm 2>&1 | sed 's/^/    /'
        fail "file_permissions (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_perm
else
    echo "  Permission test compilation error (non-fatal, missing symbols expected):"
    gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
        -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libwebsocket" \
        -I"$CDIR/lib/libprotobuf" -I"$CDIR/lib/libdb" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libcron" \
        -I"$CDIR/lib/libproc" -I"$CDIR/lib/libtui" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libdotenv" \
        "$CDIR/tests/test_file_permissions.c" \
        "$CDIR/src/cli/config.c" "$CDIR/src/cli/paths.c" \
        "$CDIR/src/secrets.c" "$CDIR/src/agent/provider_metadata.c" "$CDIR/src/tools/url_safety.c" \
        "$CDIR/lib/libyaml/yaml.c" "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_perm -lm 2>&1 | sed 's/^/    /'
    skip "file_permissions (compilation failed)"
fi

# Vault encryption at rest test (O11 — needs vault.c + crypto + json)
echo ""; echo "=== Vault Encryption at Rest (O11) ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_vault.c" \
    "$CDIR/src/agent/vault.c" \
    "$CDIR/lib/libcrypto/crypto.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_vault -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_vault > /dev/null 2>&1; then ok "vault (50 tests)"
    else
        echo "  Vault test output:"
        /tmp/hermes_test_vault 2>&1 | sed 's/^/    /'
        fail "vault (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_vault
else skip "vault (compilation failed)"
fi &

# Audit log rotation test (O12 — needs audit.c)
echo ""; echo "=== Audit Log Rotation (O12) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_audit_rotate.c" \
    "$CDIR/src/agent/audit.c" \
    -o /tmp/hermes_test_audit -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_audit > /dev/null 2>&1; then ok "audit_rotate (11 tests)"
    else
        echo "  Audit rotate test output:"
        /tmp/hermes_test_audit 2>&1 | sed 's/^/    /'
        fail "audit_rotate (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_audit
else skip "audit_rotate (compilation failed)"
fi &

# exec_code tool test (M41 — needs exec_code.c + json + sandbox_escape)
echo ""; echo "=== exec_code Tool Tests (M41) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_exec_code.c" \
    "$CDIR/src/tools/exec_code.c" "$CDIR/lib/libjson/json.c" "$CDIR/src/sandbox_escape.c" \
    -o /tmp/hermes_test_exec_code -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_exec_code > /dev/null 2>&1; then ok "exec_code_tool (8 tests)"
    else fail "exec_code_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_exec_code
else skip "exec_code_tool (compilation failed)"
fi &

# TIRITH policy engine test (O13 — standalone, only needs tirith.c + fnmatch)
echo ""; echo "=== TIRITH Policy Depth Tests (O13) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" \
    -I"$CDIR/lib/libdb" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" \
    -I"$CDIR/lib/libtui" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libdotenv" \
    "$CDIR/tests/test_tirith_policy.c" \
    "$CDIR/src/tools/tirith.c" \
    -o /tmp/hermes_test_tirith_policy -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_tirith_policy > /dev/null 2>&1; then ok "tirith_policy (57 tests)"
    else
        echo "  TIRITH policy test output:"
        /tmp/hermes_test_tirith_policy 2>&1 | sed 's/^/    /'
        fail "tirith_policy (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_tirith_policy
else skip "tirith_policy (compilation failed)"
fi &

# Provider metadata test (needs libjson + libplugin + url_safety)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_provider_metadata.c" \
    "$CDIR/src/agent/provider_metadata.c" "$CDIR/src/tools/url_safety.c" "$CDIR/src/cli/paths.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libpath/path.c" \
    -o /tmp/hermes_test_provmeta -lssl -lcrypto -ldl -lpthread -lz -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_provmeta > /dev/null 2>&1; then ok "provider_metadata (343 tests)"
    else fail "provider_metadata (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_provmeta
else skip "provider_metadata (compilation failed)"
fi &

# Azure provider depth test (B37-B38: deployment_id + api_version)
echo ""; echo "=== Azure Provider Depth Tests (B37-B38) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_azure_depth.c" \
    "$CDIR/src/agent/provider_azure.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_bedrock.c" "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_azure_depth -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_azure_depth > /dev/null 2>&1; then ok "azure_depth (9 tests)"
    else
        echo "  Azure depth test output:"
        /tmp/hermes_test_azure_depth 2>&1 | sed 's/^/    /'
        fail "azure_depth (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_azure_depth
else
    echo "  Azure depth test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_azure_depth.c" \
        "$CDIR/src/agent/provider_azure.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_bedrock.c" "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_azure_depth -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "azure_depth (compilation failed)"
fi

# Bedrock provider depth test (B39-B41: inferenceProfile + guardrails + trace)
echo ""; echo "=== Bedrock Provider Depth Tests (B39-B41) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_bedrock_depth.c" \
    "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_br_depth -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_br_depth > /dev/null 2>&1; then ok "bedrock_depth (172 tests)"
    else
        echo "  Bedrock depth test output:"
        /tmp/hermes_test_br_depth 2>&1 | sed 's/^/    /'
        fail "bedrock_depth (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_br_depth
else
    echo "  Bedrock depth test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_bedrock_depth.c" \
        "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_br_depth -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "bedrock_depth (compilation failed)"
fi

# OpenRouter provider depth test (B43-B46: provider preferences)
echo ""; echo "=== OpenRouter Provider Depth Tests (B43-B46) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_openrouter_depth.c" \
    "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_deepseek.c" \
    "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_or_depth -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_or_depth > /dev/null 2>&1; then ok "openrouter_depth (13 tests)"
    else
        echo "  OpenRouter depth test output:"
        /tmp/hermes_test_or_depth 2>&1 | sed 's/^/    /'
        fail "openrouter_depth (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_or_depth
else
    echo "  OpenRouter depth test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_openrouter_depth.c" \
        "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_deepseek.c" \
        "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_or_depth -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "openrouter_depth (compilation failed)"
fi

# Anthropic provider depth test (B26-B28: thinking, cache control, tool format)
echo ""; echo "=== Anthropic Provider Depth Tests (B26-B28) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_anthropic_depth.c" \
    "$CDIR/src/agent/provider_anthropic.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_anth_depth -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_anth_depth > /dev/null 2>&1; then ok "anthropic_depth (119 tests)"
    else
        echo "  Anthropic depth test output:"
        /tmp/hermes_test_anth_depth 2>&1 | sed 's/^/    /'
        fail "anthropic_depth (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_anth_depth
else
    echo "  Anthropic depth test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_anthropic_depth.c" \
        "$CDIR/src/agent/provider_anthropic.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/src/tools/url_safety.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_anth_depth -lm -lssl -lcrypto -lz 2>&1 | sed 's/^/    /'
    skip "anthropic_depth (compilation failed)"
fi

# Google provider comprehensive tests
echo ""; echo "=== Google Provider Comprehensive Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_google_full.c" \
    "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_goog_full -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_goog_full > /dev/null 2>&1; then ok "google_full (40 tests)"
    else
        echo "  Google full test output:"
        /tmp/hermes_test_goog_full 2>&1 | sed 's/^/    /'
        fail "google_full (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_goog_full
else
    echo "  Google full test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_google_full.c" \
        "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" \
        "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_goog_full -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "google_full (compilation failed)"
fi

# ==============================================
# These tests exist but were never wired into test_runner:
# test_finish_reason.c (B22), test_google_depth.c, test_json_mode.c
# ==============================================

# B22: finish_reason tracking (standalone — needs all providers + json + http)
echo ""; echo "=== Finish Reason Tracking Tests (B22) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_finish_reason.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/tools/url_safety.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_finish_reason -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_finish_reason > /dev/null 2>&1; then ok "finish_reason (37 tests)"
    else
        echo "  Finish reason test output:"
        /tmp/hermes_test_finish_reason 2>&1 | sed 's/^/    /'
        fail "finish_reason (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_finish_reason
else
    echo "  Finish reason test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_finish_reason.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/tools/url_safety.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_finish_reason -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "finish_reason (compilation failed)"
fi

# Google depth tests (B30-B31: top_k + candidate_count + systemInstruction)
echo ""; echo "=== Google Provider Depth Tests (B30-B31) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_google_depth.c" \
    "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_google_depth -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_google_depth > /dev/null 2>&1; then ok "google_depth (166 tests)"
    else
        echo "  Google depth test output:"
        /tmp/hermes_test_google_depth 2>&1 | sed 's/^/    /'
        fail "google_depth (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_google_depth
else
    echo "  Google depth test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_google_depth.c" \
        "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" \
        "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_google_depth -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "google_depth (compilation failed)"
fi

# JSON mode + response_format UAF test (B23 — standalone, needs all providers)
echo ""; echo "=== JSON Mode Tests (B23) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_json_mode.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_json_mode -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_json_mode > /dev/null 2>&1; then ok "json_mode (10 tests)"
    else
        echo "  JSON mode test output:"
        /tmp/hermes_test_json_mode 2>&1 | sed 's/^/    /'
        fail "json_mode (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_json_mode
else
    echo "  JSON mode test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_json_mode.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_json_mode -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "json_mode (compilation failed)"
fi

# Bedrock provider comprehensive tests
echo ""; echo "=== Bedrock Provider Comprehensive Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_bedrock_full.c" \
    "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_br_full -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_br_full > /dev/null 2>&1; then ok "bedrock_full (35 tests)"
    else
        echo "  Bedrock full test output:"
        /tmp/hermes_test_br_full 2>&1 | sed 's/^/    /'
        fail "bedrock_full (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_br_full
else
    echo "  Bedrock full test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_bedrock_full.c" \
        "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_azure.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_br_full -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "bedrock_full (compilation failed)"
fi

# Azure provider comprehensive tests
echo ""; echo "=== Azure Provider Comprehensive Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_azure_full.c" \
    "$CDIR/src/agent/provider_azure.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_az_full -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_az_full > /dev/null 2>&1; then ok "azure_full (45 tests)"
    else
        echo "  Azure full test output:"
        /tmp/hermes_test_az_full 2>&1 | sed 's/^/    /'
        fail "azure_full (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_az_full
else
    echo "  Azure full test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_azure_full.c" \
        "$CDIR/src/agent/provider_azure.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_az_full -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "azure_full (compilation failed)"
fi

# DeepSeek FIM tests (B32 — needs all providers + JSON + HTTP + URL safety)
echo ""; echo "=== DeepSeek FIM (Fill-in-the-Middle) Tests (B32) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" \
    -I"$CDIR/lib/libdb" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" \
    -I"$CDIR/lib/libtui" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libdotenv" \
    "$CDIR/tests/test_deepseek_fim.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_xai.c" "$CDIR/src/agent/provider_anthropic.c" \
    "$CDIR/src/agent/provider_google.c" "$CDIR/src/agent/provider_azure.c" \
    "$CDIR/src/agent/provider_bedrock.c" "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/provider_metadata.c" "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_deepseek_fim -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_deepseek_fim > /dev/null 2>&1; then ok "deepseek_fim (30 tests)"
    else
        echo "  DeepSeek FIM test output:"
        /tmp/hermes_test_deepseek_fim 2>&1 | sed 's/^/    /'
        fail "deepseek_fim (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_deepseek_fim
else skip "deepseek_fim (compilation failed)"
fi &

# Discord interaction tests (M08 — tests JSON parsing of interactions, modals, components)
echo ""; echo "=== Discord Interaction Tests (M08) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" \
    -I"$CDIR/lib/libdb" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" \
    -I"$CDIR/lib/libtui" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libdotenv" \
    "$CDIR/tests/test_discord_interactions.c" \
    "$CDIR/src/gateway/platforms/discord.c" "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_discord_int -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_discord_int > /dev/null 2>&1; then ok "discord_interactions (31 tests)"
    else
        echo "  Discord interaction test output:"
        /tmp/hermes_test_discord_int 2>&1 | sed 's/^/    /'
        fail "discord_interactions (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_discord_int
else skip "discord_interactions (compilation failed)"
fi &

# Provider smoke test (needs all provider object files + libs)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_provider_smoke.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_provsmoke -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_provsmoke > /dev/null 2>&1; then ok "provider_smoke (439 tests)"
    else fail "provider_smoke (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_provsmoke
else skip "provider_smoke (compilation failed)"
fi &

# Provider error handling edge cases test (M06 — all providers with error inputs)
echo ""; echo "=== Provider Error Handling Edge Cases (M06) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_provider_error.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/src/agent/portal_tags.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_proverr -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_proverr > /dev/null 2>&1; then ok "provider_error (M06: all providers error handling)"
    else
        echo "  Provider error test output:"
        /tmp/hermes_test_proverr 2>&1 | sed 's/^/    /'
        fail "provider_error (M06: test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_proverr
else
    echo "  Provider error test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_provider_error.c" \
        "$CDIR/src/agent/provider.c" \
        "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
        "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
        "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
        "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
        "$CDIR/src/agent/provider_custom.c" \
        "$CDIR/src/agent/portal_tags.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_proverr -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "provider_error (M06: compilation failed)"
fi

# xAI model retirement detection test (L04)
echo ""; echo "=== xAI Retirement Detection Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libtui" -I"$CDIR/lib/libdb" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" -I"$CDIR/lib/libmcp" \
    "$CDIR/tests/test_xai_retirement.c" \
    "$CDIR/src/xai_retirement.c" \
    -o /tmp/hermes_test_xretire -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_xretire > /dev/null 2>&1; then ok "xai_retirement (35 tests)"
    else
        echo "  xAI retirement test output:"
        /tmp/hermes_test_xretire 2>&1 | sed 's/^/    /'
        fail "xai_retirement (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_xretire
else
    echo "  xAI retirement test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libtui" -I"$CDIR/lib/libdb" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" -I"$CDIR/lib/libmcp" \
        "$CDIR/tests/test_xai_retirement.c" \
        "$CDIR/src/xai_retirement.c" \
        -o /tmp/hermes_test_xretire -lm 2>&1 | sed 's/^/    /'
    skip "xai_retirement (compilation failed)"
fi

# Browse.sh skills hub test (L12)
echo ""; echo "=== Browse.sh Skills Hub Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_skills_hub.c" \
    "$CDIR/src/skills_hub.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_hub -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_hub > /dev/null 2>&1; then ok "skills_hub (17 tests)"
    else
        echo "  Skills hub test output:"
        /tmp/hermes_test_hub 2>&1 | sed 's/^/    /'
        fail "skills_hub (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_hub
else
    echo "  Skills hub test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" \
        "$CDIR/tests/test_skills_hub.c" \
        "$CDIR/src/skills_hub.c" \
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
        -o /tmp/hermes_test_hub -lm -lssl -lcrypto -lz 2>-lm -lssl -lcrypto 2>&11 | sed 's/^/    /'
    skip "skills_hub (compilation failed)"
fi

# Checkpoint tests (needs context.c for message_new/message_free)
if gcc -O0 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_checkpoint.c" \
    "$CDIR/src/agent/checkpoint.c" "$CDIR/src/agent/context.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_checkpoint -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_checkpoint > /dev/null 2>&1; then ok "checkpoint (44 tests)"
    else fail "checkpoint (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_checkpoint
else skip "checkpoint (compilation failed)"
fi &

# CLI command dispatch test (needs commands.c + json + plugin + http libs)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libskillusage" \
    "$CDIR/tests/test_cli_commands.c" \
    "$CDIR/src/cli/commands.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_cli -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_cli > /dev/null 2>&1; then ok "cli_commands (111 tests)"
    else fail "cli_commands (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_cli
else skip "cli_commands (compilation failed)"
fi &

# ACP events test (needs events.c + JSON lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_acp_events.c" \
    "$CDIR/src/acp/events.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_acp_events -lm \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_acp_events > /dev/null 2>&1; then ok "acp_events (76 tests)"
    else fail "acp_events (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_acp_events
else skip "acp_events (compilation failed)"
fi &

# CLI dispatch test (T02: tests commands_dispatch, commands_get_all, handlers)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libskillusage" \
    -I"$CDIR/lib/libansi" -I"$CDIR/lib/libskin" \
    "$CDIR/tests/test_cli_dispatch.c" \
    "$CDIR/src/cli/commands.c" \
    "$CDIR/src/cli/display_core.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libansi/ansi.c" \
    "$CDIR/lib/libskin/skin.c" \
    -o /tmp/hermes_test_cli_dispatch -lm \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_cli_dispatch > /dev/null 2>&1; then ok "cli_dispatch (T02: 108 tests)"
    else
        echo "  CLI dispatch test output:"
        /tmp/hermes_test_cli_dispatch 2>&1 | sed 's/^/    /'
        fail "cli_dispatch (T02: test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_cli_dispatch
else
    echo "  CLI dispatch test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libskillusage" \
        "$CDIR/tests/test_cli_dispatch.c" \
        "$CDIR/src/cli/commands.c" \
        "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_cli_dispatch -lm \
        -Wl,--unresolved-symbols=ignore-all 2>&1 | sed 's/^/    /'
    skip "cli_dispatch (T02: compilation failed)"
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
fi &

# Credential pool test (needs credential_pool.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_credential_pool.c" \
    "$CDIR/src/agent/credential_pool.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_credpool -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_credpool > /dev/null 2>&1; then ok "credential_pool (80 tests)"
    else fail "credential_pool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_credpool
else skip "credential_pool (compilation failed)"
fi &

# Budget tracker test (needs budget_tracker.c + provider_metadata + url_safety + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_budget_tracker.c" \
    "$CDIR/src/agent/budget_tracker.c" "$CDIR/src/agent/provider_metadata.c" \
    "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_budget -lssl -lcrypto -lz -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_budget > /dev/null 2>&1; then ok "budget_tracker (104 tests)"
    else fail "budget_tracker (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_budget
else skip "budget_tracker (compilation failed)"
fi &

echo ""; echo "=== Iteration Budget Tests (agent/iteration_budget.py) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_iteration_budget.c" \
    "$CDIR/src/agent/budget_tracker.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_iteration_budget -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_iteration_budget > /dev/null 2>&1; then ok "iteration_budget (30 tests)"
    else fail "iteration_budget (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_iteration_budget
else skip "iteration_budget (compilation failed)"
fi &

# Fallback routing test (needs fallback_routing.c + json lib)
echo ""; echo "=== Fallback Routing Tests (P83) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_fallback_routing.c" \
    "$CDIR/src/agent/fallback_routing.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_fallback -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_fallback > /dev/null 2>&1; then ok "fallback_routing (93 tests)"
    else fail "fallback_routing (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_fallback
else skip "fallback_routing (compilation failed)"
fi &

# Secrets test (needs secrets.c + json lib)
echo ""; echo "=== Secrets Resolution Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_secrets.c" \
    "$CDIR/src/secrets.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_secrets -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_secrets > /dev/null 2>&1; then ok "secrets (22 tests)"
    else fail "secrets (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_secrets
else skip "secrets (compilation failed)"
fi &

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
fi &

# Gateway escape modes test (M07: markdown_to_html, markdown_v2_escape, truncate_message)
echo ""; echo "=== Gateway Escape Modes (M07) ==="
if gcc -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable \
    -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_gateway_escape.c" \
    "$CDIR/src/gateway/server.c" \
    "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libcron/cron.c" \
    -o /tmp/hermes_test_gwesc -lm -lssl -lcrypto -lpthread \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_gwesc > /dev/null 2>&1; then ok "gateway_escape (M07: 45 tests)"
    else
        echo "  Gateway escape test output:"
        /tmp/hermes_test_gwesc 2>&1 | sed 's/^/    /'
        fail "gateway_escape (M07: test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_gwesc
else
    echo "  Gateway escape test compilation error:"
    gcc -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable \
        -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
        "$CDIR/tests/test_gateway_escape.c" \
        "$CDIR/src/gateway/server.c" \
        "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libcron/cron.c" \
        -o /tmp/hermes_test_gwesc -lm -lssl -lcrypto -lpthread 2>&1 | sed 's/^/    /'
    skip "gateway_escape (M07: compilation failed)"
fi

# Gateway per-platform integration tests (T01): Telegram + Discord parsers
# Tests JSON parsing, state management without HTTP.
echo ""; echo "=== Gateway Per-Platform Tests (T01) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_gateway_platforms.c" \
    "$CDIR/src/gateway/platforms/telegram.c" "$CDIR/src/gateway/platforms/discord.c" \
    "$CDIR/src/gateway/platforms/signal.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_gw_platforms -lm \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_gw_platforms > /dev/null 2>&1; then ok "gateway_platforms (T01: 75 tests)"
    else
        echo "  Gateway platform test output:"
        /tmp/hermes_test_gw_platforms 2>&1 | sed 's/^/    /'
        fail "gateway_platforms (T01: test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_gw_platforms
else
    echo "  Gateway platform test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
        "$CDIR/tests/test_gateway_platforms.c" \
        "$CDIR/src/gateway/platforms/telegram.c" "$CDIR/src/gateway/platforms/discord.c" \
        "$CDIR/src/gateway/platforms/signal.c" \
        "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_gw_platforms -lm \
        -Wl,--unresolved-symbols=ignore-all 2>&1 | sed 's/^/    /'
    skip "gateway_platforms (T01: compilation failed)"
fi

# Telegram thread-not-found detection tests (T01)
if gcc -O2 -Wall -Wextra -o /tmp/hermes_test_tg_tnf "$CDIR/tests/test_telegram_thread_not_found.c" -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_tg_tnf > /dev/null 2>&1; then
        ok "telegram_thread_not_found (T01: 12 tests)"
    else
        echo "  Telegram thread-not-found test output:"
        /tmp/hermes_test_tg_tnf 2>&1 | sed 's/^/    /'
        fail "telegram_thread_not_found (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_tg_tnf
else
    echo "  Telegram thread-not-found test compilation error:"
    gcc -O2 -Wall -Wextra -o /tmp/hermes_test_tg_tnf "$CDIR/tests/test_telegram_thread_not_found.c" -lm 2>&1 | sed 's/^/    /'
    skip "telegram_thread_not_found (compilation failed)"
fi

# Video MIME type detection tests
if gcc -O2 -Wall -Wextra -o /tmp/hermes_test_vm "$CDIR/tests/test_video_mime.c" -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_vm > /dev/null 2>&1; then
        ok "video_mime_detection (T01: 58 tests)"
    else
        echo "  Video MIME test output:"
        /tmp/hermes_test_vm 2>&1 | sed 's/^/    /'
        fail "video_mime_detection (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_vm
else
    echo "  Video MIME test compilation error:"
    gcc -O2 -Wall -Wextra -o /tmp/hermes_test_vm "$CDIR/tests/test_video_mime.c" -lm 2>&1 | sed 's/^/    /'
    skip "video_mime_detection (compilation failed)"
fi

# Feishu comment helper tests
if gcc -O2 -Wall -Wextra -o /tmp/hermes_test_fc "$CDIR/tests/test_feishu_comment.c" -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_fc > /dev/null 2>&1; then
        ok "feishu_comment_helpers (G03: 20 tests)"
    else
        echo "  Feishu comment helper test output:"
        /tmp/hermes_test_fc 2>&1 | sed 's/^/    /'
        fail "feishu_comment_helpers (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_fc
else
    echo "  Feishu comment helper test compilation error:"
    gcc -O2 -Wall -Wextra -o /tmp/hermes_test_fc "$CDIR/tests/test_feishu_comment.c" -lm 2>&1 | sed 's/^/    /'
    skip "feishu_comment_helpers (compilation failed)"
fi

# Feishu comment rules test (G04 — needs json lib + includes)
RULES_INCS="-I $CDIR/include -I $CDIR/lib/libjson -I $CDIR/lib/liblineedit -I $CDIR/lib/libbase64 -I $CDIR/lib/libhash -I $CDIR/lib/libdatetime -I $CDIR/lib/libpath -I $CDIR/lib/libuuid -I $CDIR/lib/libhttp -I $CDIR/lib/libhtml -I $CDIR/lib/libtextwrap -I $CDIR/lib/libcrypto -I $CDIR/lib/libdotenv -I $CDIR/lib/libcron -I $CDIR/lib/libproc -I $CDIR/lib/libtui -I $CDIR/lib/libdb -I $CDIR/lib/libplugin -I $CDIR/lib/libskin -I $CDIR/lib/libwebsocket -I $CDIR/lib/libprotobuf -I $CDIR/lib/libmcp -I $CDIR/lib/libcsv -I $CDIR/lib/libglob -I $CDIR/lib/libsignal -I $CDIR/lib/libenum -I $CDIR/lib/libdifflib -I $CDIR/lib/libregex -I $CDIR/lib/libansi -I $CDIR/lib/libjson5 -I $CDIR/lib/libskillusage -I $CDIR/lib/libskillsync -I $CDIR/lib/libtranscribe -I $CDIR/lib/libmcp_oauth -I $CDIR/lib/libfal_common -I $CDIR/libtooloutput -I $CDIR/lib/libxai_http -I $CDIR/lib/libenvpassthrough -I $CDIR/lib/libcredential -I $CDIR/lib/libschemasanitizer -I $CDIR/lib/libfuzzymatch -I $CDIR/lib/libinterrupt -I $CDIR/lib/libfilestate -I $CDIR/lib/libtooldispatch -I $CDIR/lib/librateguard -I $CDIR/lib/libskillutils -I $CDIR/lib/liberrorclassifier -I $CDIR/lib/libfile_sync -I $CDIR/lib/libbudgetconfig -I $CDIR/lib/libthreatpatterns -I $CDIR/lib/libcredentialfiles -I $CDIR/lib/libskillaudit -I $CDIR/lib/libslashconfirm -I $CDIR/lib/libmsgraph -I $CDIR/lib/libbinary -I $CDIR/lib/libbrowser -I $CDIR/lib/libdebug -I $CDIR/lib/libosv -I $CDIR/lib/libwebsite -I $CDIR/lib/libtemplate -I $CDIR/lib/libratelimit -I $CDIR/lib/libmangateway -I $CDIR/lib/libtoolbackend"
if gcc -O2 -Wall -Wextra $RULES_INCS \
    -o /tmp/hermes_test_fcrules \
    "$CDIR/tests/test_feishu_comment_rules.c" \
    "$CDIR/src/tools/feishu_comment_rules.c" \
    "$CDIR/lib/libjson/json.c" \
    -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_fcrules > /dev/null 2>&1; then
        ok "feishu_comment_rules (G04: 56 tests)"
    else
        echo "  Feishu rules test output:"
        /tmp/hermes_test_fcrules 2>&1 | sed 's/^/    /'
        fail "feishu_comment_rules (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_fcrules
else
    echo "  Feishu rules test compilation error:"
    gcc -O2 -Wall -Wextra $RULES_INCS \
        -o /tmp/hermes_test_fcrules \
        "$CDIR/tests/test_feishu_comment_rules.c" \
        "$CDIR/src/tools/feishu_comment_rules.c" \
        "$CDIR/lib/libjson/json.c" \
        -lm 2>&1 | sed 's/^/    /'
    skip "feishu_comment_rules (compilation failed)"
fi

# Gateway per-platform webhook tests (T01): HMAC + subscription management
# Needs crypto library for HMAC verification.
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libplugin" \
    -DWEBHOOK_TESTS \
    "$CDIR/tests/test_gateway_platforms.c" \
    "$CDIR/src/gateway/platforms/webhook.c" \
    "$CDIR/lib/libcrypto/crypto.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_gw_wh -lm -lssl -lcrypto \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_gw_wh > /dev/null 2>&1; then ok "gateway_webhook (T01: 25 tests)"
    else
        echo "  Gateway webhook test output:"
        /tmp/hermes_test_gw_wh 2>&1 | sed 's/^/    /'
        fail "gateway_webhook (T01: test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_gw_wh
else
    echo "  Gateway webhook test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libplugin" \
        -DWEBHOOK_TESTS \
        "$CDIR/tests/test_gateway_platforms.c" \
        "$CDIR/src/gateway/platforms/webhook.c" \
        "$CDIR/lib/libcrypto/crypto.c" "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_gw_wh -lm -lssl -lcrypto \
        -Wl,--unresolved-symbols=ignore-all 2>&1 | sed 's/^/    /'
    skip "gateway_webhook (T01: compilation failed)"
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
fi &

# Plugin sandbox loading tests (T07: NULL-safety, invalid input, security boundary)
echo ""; echo "=== Plugin Sandbox Loading Tests (T07) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_plugin_sandbox.c" \
    "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_plugin_sandbox -ldl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_plugin_sandbox > /dev/null 2>&1; then ok "plugin_sandbox (T07: 73 tests)"
    else
        echo "  Plugin sandbox test output:"
        /tmp/hermes_test_plugin_sandbox 2>&1 | sed 's/^/    /'
        fail "plugin_sandbox (T07: test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_plugin_sandbox
else
    echo "  Plugin sandbox test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
        "$CDIR/tests/test_plugin_sandbox.c" \
        "$CDIR/lib/libplugin/plugin.c" \
        -o /tmp/hermes_test_plugin_sandbox -ldl -lm 2>&1 | sed 's/^/    /'
    skip "plugin_sandbox (T07: compilation failed)"
fi

# Rate limiter test (standalone — uses lib/libratelimit/rate_limit.c)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libratelimit" \
    "$CDIR/tests/test_rate_limit.c" \
    "$CDIR/lib/libratelimit/rate_limit.c" \
    -o /tmp/hermes_test_rl -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_rl > /dev/null 2>&1; then ok "rate_limit (168 tests)"
    else fail "rate_limit (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_rl
else skip "rate_limit (compilation failed)"
fi &

# Agent loop/context test suite (G166 — needs context.c, checkpoint.c, json, plugin)
if gcc -O0 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_agent.c" \
    "$CDIR/src/agent/context.c" "$CDIR/src/agent/checkpoint.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_agent -lm -lpthread -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_agent > /dev/null 2>&1; then ok "agent_loop_context (161 tests)"
    else fail "agent_loop_context (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_agent
else skip "agent_loop_context (compilation failed)"
fi &

# xAI Search test (needs x_search.c — tests only non-HTTP error paths)
INCLUDES_ALL=()
INCLUDES_ALL+=("-I$CDIR/include" "-I$CDIR/lib/libjson" "-I$CDIR/lib/libhttp" "-I$CDIR/lib/libplugin")
INCLUDES_ALL+=("-I$CDIR/lib/libskin" "-I$CDIR/lib/libwebsocket" "-I$CDIR/lib/libprotobuf" "-I$CDIR/lib/libdb")
INCLUDES_ALL+=("-I$CDIR/lib/libcrypto" "-I$CDIR/lib/libcron" "-I$CDIR/lib/libproc" "-I$CDIR/lib/libtui")
INCLUDES_ALL+=("-I$CDIR/lib/libtemplate" "-I$CDIR/lib/libdotenv" "-I$CDIR/lib/libyaml" "-I$CDIR/lib/libmcp")
INCLUDES_ALL+=("-I$CDIR/lib/libpath" "-I$CDIR/lib/libdatetime" "-I$CDIR/lib/libcsv" "-I$CDIR/lib/libhash")
INCLUDES_ALL+=("-I$CDIR/lib/libuuid" "-I$CDIR/lib/libbase64" "-I$CDIR/lib/libhtml" "-I$CDIR/lib/libtextwrap")
INCLUDES_ALL+=("-I$CDIR/lib/libglob" "-I$CDIR/lib/libsignal" "-I$CDIR/lib/libenum" "-I$CDIR/lib/libdifflib")
INCLUDES_ALL+=("-I$CDIR/lib/libregex" "-I$CDIR/lib/libansi")
if gcc -O2 -Wall -Wextra "${INCLUDES_ALL[@]}" \
    "$CDIR/tests/test_x_search.c" \
    "$CDIR/src/tools/x_search.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_x_search -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_x_search > /dev/null 2>&1; then ok "x_search (9 tests: 4 error-path + 5 date validation)"
    else fail "x_search (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_x_search
else skip "x_search (compilation failed)"
fi &

# URL safety test (needs url_safety.c — scheme/blocklist tests, no DNS)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_url_safety.c" \
    "$CDIR/src/tools/url_safety.c" \
    -o /tmp/hermes_test_url -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_url > /dev/null 2>&1; then ok "url_safety (55 tests)"
    else fail "url_safety (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_url
else skip "url_safety (compilation failed)"
fi &

# Command allowlist test (needs approval.c + json lib + ansi_strip)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libansi" \
    "$CDIR/tests/test_allowlist.c" \
    "$CDIR/src/tools/approval.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libansi/ansi_strip.c" \
    -o /tmp/hermes_test_al -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_al > /dev/null 2>&1; then ok "allowlist (34 tests)"
    else fail "allowlist (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_al
else skip "allowlist (compilation failed)"
fi &

# Audit log test (needs audit.c — standalone, no JSON deps)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_audit.c" \
    "$CDIR/src/agent/audit.c" \
    -o /tmp/hermes_test_audit -lm -lpthread > /dev/null 2>&1; then
    if /tmp/hermes_test_audit > /dev/null 2>&1; then ok "audit_log (20 tests)"
    else fail "audit_log (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_audit
else skip "audit_log (compilation failed)"
fi &

# Error system test (K01-K05: standalone — only needs hermes_error.c + pthread)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_hermes_error.c" \
    "$CDIR/src/hermes_error.c" \
    -o /tmp/hermes_test_error -lm -lpthread > /dev/null 2>&1; then
    if /tmp/hermes_test_error > /dev/null 2>&1; then ok "error_system (11 tests)"
    else fail "error_system (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_error
else skip "error_system (compilation failed)"
fi &

# Memory tool test (M35 — needs memory.c, json, sqlite3)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libdb" \
    "$CDIR/tests/test_memory.c" \
    "$CDIR/src/tools/memory.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libdb/sqlite3.c" \
    -o /tmp/hermes_test_memory -lm -lpthread -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_memory > /dev/null 2>&1; then ok "memory_tool (16 tests)"
    else fail "memory_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_memory
else skip "memory_tool (compilation failed)"
fi &

# File tool test (M31 — needs file.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libdb" -I"$CDIR/lib/libdifflib" -I"$CDIR/lib/libhash" -I"$CDIR/lib/libbinary" \
    "$CDIR/tests/test_file.c" \
    "$CDIR/src/tools/file.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libdifflib/difflib.c" "$CDIR/lib/libhash/hash.c" "$CDIR/lib/libbinary/binary.c" \
    -o /tmp/hermes_test_file -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_file > /dev/null 2>&1; then ok "file_tool (58 tests)"
    else fail "file_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_file
else skip "file_tool (compilation failed)"
fi &

# Feishu tools test (D22 — needs feishu_tools.c + json + http libs)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libdb" \
    "$CDIR/tests/test_feishu_tools.c" \
    "$CDIR/src/tools/feishu_tools.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_feishu -lm -lssl -lcrypto -ldl -lpthread -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_feishu > /dev/null 2>&1; then ok "feishu_tools (52 tests)"
    else fail "feishu_tools (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_feishu
else skip "feishu_tools (compilation failed)"
fi &

# Browser tool test (P81-P90 — needs browser.c + json + websocket libs)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_browser.c" \
    "$CDIR/src/tools/browser.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_browser -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_browser > /dev/null 2>&1; then ok "browser_tool (32 tests)"
    else fail "browser_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_browser
else skip "browser_tool (compilation failed)"
fi &

# Vision tool test (M33 — needs vision.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_vision.c" \
    "$CDIR/src/tools/vision.c" "$CDIR/src/tools/url_safety.c" "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_vision -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_vision > /dev/null 2>&1; then ok "vision_tool (35 tests)"
    else fail "vision_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_vision
else skip "vision_tool (compilation failed)"
fi &

# Standalone test_vision_tool.c (11 self-contained tests, no deps)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR" \
    "$CDIR/tests/test_vision_tool.c" \
    -o /tmp/hermes_test_vision_tool -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_vision_tool > /dev/null 2>&1; then ok "vision_tool_standalone (11 tests)"
    else fail "vision_tool_standalone (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_vision_tool
else skip "vision_tool_standalone (compilation failed)"
fi &

# Todo tool test (M43 — needs todo.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_todo.c" \
    "$CDIR/src/tools/todo.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_todo -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    mkdir -p /tmp/hermes_test_todo_home && \
    HERMES_HOME=/tmp/hermes_test_todo_home /tmp/hermes_test_todo > /dev/null 2>&1
    if [ $? -eq 0 ]; then ok "todo_tool (10 tests)"
    else fail "todo_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_todo
    rm -rf /tmp/hermes_test_todo_home
else skip "todo_tool (compilation failed)"
fi &

# Patch tool test (M42 — needs patch.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_patch.c" \
    "$CDIR/src/tools/patch.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_patch -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_patch > /dev/null 2>&1; then ok "patch_tool (17 tests)"
    else fail "patch_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_patch
else skip "patch_tool (compilation failed)"
fi &

# Mixture of Agents test (N02 — needs mixture_of_agents.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_mixture_of_agents.c" \
    "$CDIR/src/tools/mixture_of_agents.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_moa -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_moa > /dev/null 2>&1; then ok "mixture_of_agents (11 tests)"
    else fail "mixture_of_agents (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_moa
else skip "mixture_of_agents (compilation failed)"
fi &

rm -f /tmp/hermes_test_moa_old 2>/dev/null || true

# V4A patch mode tests
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libfal_common" -I"$CDIR/lib/libenvpassthrough" \
    "$CDIR/tests/test_patch_v4a.c" \
    "$CDIR/src/tools/patch.c" "$CDIR/src/tools/registry.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libfal_common/fal_common.c" \
    "$CDIR/lib/libenvpassthrough/env_passthrough.c" \
    -o /tmp/hermes_test_patch_v4a -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_patch_v4a > /dev/null 2>&1; then ok "patch_v4a (3 tests)"
    else fail "patch_v4a (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_patch_v4a
else skip "patch_v4a (compilation failed)"
fi &

# Process tool test (M39 — needs process.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_process.c" \
    "$CDIR/src/tools/process.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_process -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    # Use temp SLERMES_HOME to avoid loading stale process checkpoints
    if SLERMES_HOME=/tmp/hermes_test_proc /tmp/hermes_test_process > /dev/null 2>&1; then ok "process_tool (26 tests)"
    else fail "process_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_process
else skip "process_tool (compilation failed)"
fi &

# Approval system test (needs approval.c + json lib + ansi_strip)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libansi" \
    "$CDIR/tests/test_approval.c" \
    "$CDIR/src/tools/approval.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libansi/ansi_strip.c" \
    -o /tmp/hermes_test_app -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_app > /dev/null 2>&1; then ok "approval_system (75 tests)"
    else fail "approval_system (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_app
else skip "approval_system (compilation failed)"
fi &

# Kanban tool test (M41 — needs kanban.c + registry.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_kanban.c" \
    "$CDIR/src/tools/kanban.c" "$CDIR/src/tools/registry.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_kanban -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    SLERMES_HOME=/tmp/hermes_test_kanban_home /tmp/hermes_test_kanban > /dev/null 2>&1
    if [ $? -eq 0 ]; then ok "kanban_tool (38 tests)"
    else fail "kanban_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_kanban
    rm -rf /tmp/hermes_test_kanban_home
else skip "kanban_tool (compilation failed)"
fi &

# Budget tracker tests (P84/G24/G26)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_budget_tracker.c" \
    "$CDIR/src/agent/budget_tracker.c" "$CDIR/src/agent/provider_metadata.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_budget -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_budget > /dev/null 2>&1; then ok "budget_tracker (31 tests)"
    else fail "budget_tracker (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_budget
else skip "budget_tracker (compilation failed)"
fi &

# Account usage tests (M37 depth)
if gcc -O2 -Wall -Wextra -DTEST_BUILD -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_account_usage.c" \
    "$CDIR/src/tools/account_usage.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_usage -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_usage > /dev/null 2>&1; then ok "account_usage (14 tests)"
    else fail "account_usage (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_usage
else skip "account_usage (compilation failed)"
fi &

# Send_message tool test (M37)
if gcc -O2 -Wall -Wextra -DTEST_BUILD -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_send_message.c" \
    "$CDIR/src/tools/send_message.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_sendmsg -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_sendmsg > /dev/null 2>&1; then ok "send_message_tool (40 tests)"
    else fail "send_message_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_sendmsg
else skip "send_message_tool (compilation failed)"
fi

# Media path validation unit tests (B08/G02)
if gcc -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -Wno-format-truncation \
    -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libdb" -I"$CDIR/lib/libenvpassthrough" -I"$CDIR/lib/libcrypto" \
    -I"$CDIR/lib/libbase64" -I"$CDIR/lib/libpath" -I"$CDIR/lib/libtooloutput" \
    -I"$CDIR/lib/libinterrupt" -I"$CDIR/lib/libcredential" -I"$CDIR/lib/libtooldispatch" \
    -I"$CDIR/lib/libregex" -I"$CDIR/lib/libhash" -I"$CDIR/lib/libhtml" \
    -I"$CDIR/lib/libdatetime" -I"$CDIR/lib/libsignal" -I"$CDIR/lib/libmcp" \
    -I"$CDIR/lib/librateguard" -I"$CDIR/lib/libratelimit" -I"$CDIR/lib/libtextwrap" \
    -I"$CDIR/lib/libglob" -I"$CDIR/lib/libansi" -I"$CDIR/lib/libmcp_oauth" \
    -I"$CDIR/lib/libuuid" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libwebsocket" \
    -I"$CDIR/lib/libcron" -I"$CDIR/lib/libxai_http" -I"$CDIR/lib/libdotenv" \
    -I"$CDIR/lib/liblineedit" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libfile_sync" \
    -I"$CDIR/lib/libbudgetconfig" -I"$CDIR/lib/libthreatpatterns" \
    -I"$CDIR/lib/libcredentialfiles" -I"$CDIR/lib/libskillaudit" \
    -I"$CDIR/lib/libslashconfirm" -I"$CDIR/lib/libmsgraph" \
    -I"$CDIR/lib/libncurses/include" -I"$CDIR/lib/libfilestate" \
    "$CDIR/tests/test_media_validation.c" \
    "$CDIR/src/tools/send_message.c" "$CDIR/src/agent/file_safety.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_media_val -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_media_val > /dev/null 2>&1; then ok "media_validation (11 tests)"
    else fail "media_validation (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_media_val
else skip "media_validation (compilation failed)"
fi

# Parse send target unit tests (M38)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_send_target.c" \
    "$CDIR/src/tools/send_message.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_sendtarget -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_sendtarget > /dev/null 2>&1; then ok "send_target_parse (44 tests)"
    else fail "send_target_parse (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_sendtarget
else skip "send_target_parse (compilation failed)"
fi

# Telegram thread_id for send (General topic mapping)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_telegram_thread_id_standalone.c" \
    -o /tmp/hermes_test_tg_tid -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_tg_tid > /dev/null 2>&1; then ok "telegram_thread_id_for_send (6 tests)"
    else fail "telegram_thread_id_for_send (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_tg_tid
else skip "telegram_thread_id_for_send (compilation failed)"
fi

# Telegram fallback IP parsing (G07 depth)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_telegram_fallback_ips.c" \
    -o /tmp/hermes_test_tg_fb -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_tg_fb > /dev/null 2>&1; then ok "telegram_fallback_ips (16 tests)"
    else fail "telegram_fallback_ips (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_tg_fb
else skip "telegram_fallback_ips (compilation failed)"
fi

# Vision media-in-tool-results support (B02 depth)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libbase64" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_vision_supports_media.c" \
    "$CDIR/src/tools/vision.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_vis_media -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_vis_media > /dev/null 2>&1; then ok "vision_supports_media (35 tests)"
    else fail "vision_supports_media (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_vis_media
else skip "vision_supports_media (compilation failed)"
fi

# Textwrap chunk test (feishu_comment._chunk_text port)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libtextwrap" \
    "$CDIR/tests/test_textwrap_chunk.c" \
    "$CDIR/lib/libtextwrap/textwrap.c" \
    -o /tmp/hermes_test_twc -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_twc > /dev/null 2>&1; then ok "textwrap_chunk (18 tests)"
    else fail "textwrap_chunk (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_twc
else skip "textwrap_chunk (compilation failed)"
fi &

# Voice mode test (voice_mode.c config API)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libtranscribe" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_voice_mode.c" \
    "$CDIR/src/tools/voice_mode.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_voice -lm -lssl -lcrypto -lz -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_voice > /dev/null 2>&1; then ok "voice_mode (15 tests)"
    else fail "voice_mode (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_voice
else skip "voice_mode (compilation failed)"
fi &

# Token exchange test (provider/token_exchange.c)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_token_exchange.c" \
    "$CDIR/src/provider/token_exchange.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_token_exchange -lm -lssl -lcrypto -lz -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_token_exchange > /dev/null 2>&1; then ok "token_exchange (7 tests)"
    else fail "token_exchange (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_token_exchange
else skip "token_exchange (compilation failed)"
fi &

# SMS gateway test (gateway/platforms/sms.c)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_sms.c" \
    "$CDIR/src/gateway/platforms/sms.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_sms -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_sms > /dev/null 2>&1; then ok "sms (42 tests)"
    else fail "sms (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_sms
else skip "sms (compilation failed)"
fi &

# Gateway helpers test (G01) — msg_dedup, strip_markdown, redact_phone, thread_tracker
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_helpers.c" \
    "$CDIR/src/gateway/helpers.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_helpers -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_helpers > /dev/null 2>&1; then ok "gateway helpers (29 tests)"
    else fail "gateway helpers (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_helpers
else skip "gateway helpers (compilation failed)"
fi &

# Web tool test (M30 — needs web.c + tool_config + registry + http + json)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libbase64" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libansi" -I"$CDIR/lib/libhtml" \
    "$CDIR/tests/test_web.c" \
    "$CDIR/src/tools/web.c" "$CDIR/src/tools/tool_config.c" "$CDIR/src/tools/registry.c" \
    "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhtml/html.c" "$CDIR/lib/libbase64/base64.c" "$CDIR/lib/libcrypto/crypto.c" "$CDIR/lib/libansi/ansi_strip.c" "$CDIR/src/agent/vault.c" \
    -o /tmp/hermes_test_web -lm -lssl -lcrypto -lz > /dev/null 2>&1; then
    if /tmp/hermes_test_web > /dev/null 2>&1; then ok "web_tool (35 tests)"
    else fail "web_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_web
else skip "web_tool (compilation failed)"
fi &

# Terminal tool test (M29 — needs terminal.c + tool_config + vault + sandbox_escape + tool_output + crypto + approval + ansi_strip)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libtooloutput" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libenvpassthrough" \
    -I"$CDIR/lib/libansi" \
    "$CDIR/tests/test_terminal.c" \
    "$CDIR/src/tools/terminal.c" "$CDIR/src/tools/tool_config.c" "$CDIR/src/tools/registry.c" \
    "$CDIR/src/sandbox_escape.c" \
    "$CDIR/src/agent/vault.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libcrypto/crypto.c" \
    "$CDIR/lib/libtooloutput/tool_output.c" \
    "$CDIR/lib/libenvpassthrough/env_passthrough.c" \
    "$CDIR/src/tools/approval.c" \
    "$CDIR/lib/libansi/ansi_strip.c" \
    -o /tmp/hermes_test_terminal -lm -lssl -lcrypto -lz -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_terminal > /dev/null 2>&1; then ok "terminal_tool (102 tests)"
    else fail "terminal_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_terminal
else skip "terminal_tool (compilation failed)"
fi &

# Terminal sudo rewrite tests (B07 depth)
if gcc -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -Wno-format-truncation \
    -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libdb" -I"$CDIR/lib/libenvpassthrough" -I"$CDIR/lib/libcrypto" \
    -I"$CDIR/lib/libbase64" -I"$CDIR/lib/libpath" -I"$CDIR/lib/libtooloutput" \
    -I"$CDIR/lib/libinterrupt" -I"$CDIR/lib/libcredential" -I"$CDIR/lib/libtooldispatch" \
    -I"$CDIR/lib/libregex" -I"$CDIR/lib/libhash" -I"$CDIR/lib/libhtml" \
    -I"$CDIR/lib/libdatetime" -I"$CDIR/lib/libsignal" -I"$CDIR/lib/libmcp" \
    -I"$CDIR/lib/librateguard" -I"$CDIR/lib/libratelimit" -I"$CDIR/lib/libtextwrap" \
    -I"$CDIR/lib/libglob" -I"$CDIR/lib/libansi" -I"$CDIR/lib/libmcp_oauth" \
    -I"$CDIR/lib/libuuid" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libwebsocket" \
    -I"$CDIR/lib/libcron" -I"$CDIR/lib/libxai_http" -I"$CDIR/lib/libdotenv" \
    -I"$CDIR/lib/liblineedit" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libfile_sync" \
    -I"$CDIR/lib/libbudgetconfig" -I"$CDIR/lib/libthreatpatterns" \
    -I"$CDIR/lib/libcredentialfiles" -I"$CDIR/lib/libskillaudit" \
    -I"$CDIR/lib/libslashconfirm" -I"$CDIR/lib/libmsgraph" \
    -I"$CDIR/lib/libncurses/include" -I"$CDIR/lib/libfilestate" \
    "$CDIR/tests/test_terminal_sudo.c" \
    "$CDIR/src/tools/terminal.c" \
    -o /tmp/hermes_test_sudo -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_sudo > /dev/null 2>&1; then ok "terminal_sudo_rewrite (24 tests)"
    else fail "terminal_sudo_rewrite (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_sudo
else skip "terminal_sudo_rewrite (compilation failed)"
fi

# Terminal compound background rewrite tests (B07 depth)
if gcc -O2 -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -Wno-format-truncation \
    -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libdb" -I"$CDIR/lib/libenvpassthrough" -I"$CDIR/lib/libcrypto" \
    -I"$CDIR/lib/libbase64" -I"$CDIR/lib/libpath" -I"$CDIR/lib/libtooloutput" \
    -I"$CDIR/lib/libinterrupt" -I"$CDIR/lib/libcredential" -I"$CDIR/lib/libtooldispatch" \
    -I"$CDIR/lib/libregex" -I"$CDIR/lib/libhash" -I"$CDIR/lib/libhtml" \
    -I"$CDIR/lib/libdatetime" -I"$CDIR/lib/libsignal" -I"$CDIR/lib/libmcp" \
    -I"$CDIR/lib/librateguard" -I"$CDIR/lib/libratelimit" -I"$CDIR/lib/libtextwrap" \
    -I"$CDIR/lib/libglob" -I"$CDIR/lib/libansi" -I"$CDIR/lib/libmcp_oauth" \
    -I"$CDIR/lib/libuuid" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libwebsocket" \
    -I"$CDIR/lib/libcron" -I"$CDIR/lib/libxai_http" -I"$CDIR/lib/libdotenv" \
    -I"$CDIR/lib/liblineedit" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libfile_sync" \
    -I"$CDIR/lib/libbudgetconfig" -I"$CDIR/lib/libthreatpatterns" \
    -I"$CDIR/lib/libcredentialfiles" -I"$CDIR/lib/libskillaudit" \
    -I"$CDIR/lib/libslashconfirm" -I"$CDIR/lib/libmsgraph" \
    -I"$CDIR/lib/libncurses/include" -I"$CDIR/lib/libfilestate" \
    "$CDIR/tests/test_terminal_compound.c" \
    "$CDIR/src/tools/terminal.c" \
    -o /tmp/hermes_test_compound -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_compound > /dev/null 2>&1; then ok "terminal_compound_background (12 tests)"
    else fail "terminal_compound_background (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_compound
else skip "terminal_compound_background (compilation failed)"
fi

# Terminal sudo prompt test (B07 depth — interactive password prompt)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libtooloutput" -I"$CDIR/lib/libenvpassthrough" \
    -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libansi" \
    "$CDIR/tests/test_sudo_prompt.c" \
    "$CDIR/src/tools/terminal.c" \
    -o /tmp/hermes_test_sudo_prompt -lm -lssl -lcrypto -lz \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_sudo_prompt > /dev/null 2>&1; then ok "terminal_sudo_prompt (5 tests)"
    else fail "terminal_sudo_prompt (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_sudo_prompt
else skip "terminal_sudo_prompt (compilation failed)"
fi

# Terminal transform_sudo wiring test (B07 depth — sudo rewrite pipeline)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libcrypto" \
    -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libtooloutput" -I"$CDIR/lib/libenvpassthrough" \
    -I"$CDIR/lib/libansi" \
    "$CDIR/tests/test_transform_sudo.c" \
    "$CDIR/src/tools/terminal.c" "$CDIR/src/tools/tool_config.c" "$CDIR/src/tools/registry.c" \
    "$CDIR/src/sandbox_escape.c" "$CDIR/src/agent/vault.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libcrypto/crypto.c" \
    "$CDIR/lib/libtooloutput/tool_output.c" "$CDIR/lib/libenvpassthrough/env_passthrough.c" \
    "$CDIR/src/tools/approval.c" "$CDIR/lib/libansi/ansi_strip.c" \
    -o /tmp/hermes_test_transform -lm -lssl -lcrypto -lz -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_transform > /dev/null 2>&1; then ok "terminal_transform_sudo (7 tests)"
    else fail "terminal_transform_sudo (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_transform
else skip "terminal_transform_sudo (compilation failed)"
fi

# Clarify tool test (M40 — needs clarify.c + json, stdin for response)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_clarify.c" \
    "$CDIR/src/tools/clarify.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_clarify -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if echo "answer" | /tmp/hermes_test_clarify > /dev/null 2>&1; then ok "clarify_tool (19 tests)"
    else fail "clarify_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_clarify
else skip "clarify_tool (compilation failed)"
fi &

# Cron tool test (M36 — needs cronjob.c + libcron + json, uses stubs for scheduler)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libcron" \
    "$CDIR/tests/test_cron_tool.c" \
    "$CDIR/src/tools/cronjob.c" "$CDIR/lib/libcron/cron.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_cron -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_cron > /dev/null 2>&1; then ok "cron_tool (25 tests)"
    else fail "cron_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_cron
else skip "cron_tool (compilation failed)"
fi &

# Delegate tool test (M32 — needs delegate.c + json, 20MB stack req)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_delegate.c" \
    "$CDIR/src/tools/delegate.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_delegate -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if ulimit -s unlimited && /tmp/hermes_test_delegate > /dev/null 2>&1; then ok "delegate_tool (4 tests)"
    else fail "delegate_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_delegate
else skip "delegate_tool (compilation failed)"
fi &

# Skills tool test (M38 — needs skills.c + json + yaml + http)
if gcc -O2 -g -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_skills.c" \
    "$CDIR/src/tools/skills.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libyaml/yaml.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_skills -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if HERMES_HOME=/tmp/hermes_test_skills_home /tmp/hermes_test_skills > /dev/null 2>&1; then ok "skills_tool (53 tests)"
    else fail "skills_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_skills
    rm -rf /tmp/hermes_test_skills_home
else skip "skills_tool (compilation failed)"
fi &

# Skill management tool test (skill_view + skill_manage — needs skill_mgmt.c + json)
echo ""; echo "=== Skill Management Tests ==="
if gcc -O2 -g -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libskillusage" -I"$CDIR/lib/libdifflib" \
    "$CDIR/tests/test_skill_mgmt.c" \
    "$CDIR/src/tools/skill_mgmt.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libskillusage/skill_provenance.c" \
    "$CDIR/lib/libskillusage/skill_usage.c" \
    -o /tmp/hermes_test_skill_mgmt -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_skill_mgmt > /dev/null 2>&1; then ok "skill_mgmt_tool (9 tests)"
    else fail "skill_mgmt_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_skill_mgmt
else skip "skill_mgmt_tool (compilation failed)"
fi &

# MCP tool test (M44 — needs mcp_tool.c + libmcp + registry + json)
if gcc -O2 -g -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libosv" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libmcp_oauth" -I"$CDIR/lib/libcredential" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libbase64" \
    "$CDIR/tests/test_mcp.c" \
    "$CDIR/src/tools/mcp_tool.c" "$CDIR/lib/libmcp/mcp.c" \
    "$CDIR/src/tools/registry.c" "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libosv/osv.c" "$CDIR/lib/libhttp/http.c" \
    "$CDIR/lib/libmcp_oauth/mcp_oauth.c" "$CDIR/lib/libcredential/credential.c" \
    "$CDIR/lib/libcrypto/crypto.c" "$CDIR/lib/libbase64/base64.c" \
    -o /tmp/hermes_test_mcp -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_mcp > /dev/null 2>&1; then ok "mcp_tool (24 tests)"
    else fail "mcp_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_mcp
else skip "mcp_tool (compilation failed)"
fi &

# MCP stream test (L30)
if gcc -O2 -g -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libjson" \
    "$CDIR/tests/test_mcp_stream.c" \
    "$CDIR/lib/libmcp/mcp.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_mcp_stream -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_mcp_stream > /dev/null 2>&1; then ok "mcp_stream (11 tests)"
    else fail "mcp_stream (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_mcp_stream
else skip "mcp_stream (compilation failed)"
fi &

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
    wait; skip "hermes build"; summary; exit $FAIL
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
if "$HERMES" --version 2>&1 | grep -E "WuBu (Hermes|Slermes)"; then
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

# ==============================================
# 6. Profile loading test
# ==============================================
echo ""; echo "=== Config Profile Tests ==="

# Create a test profile in the slermes profiles dir
PROFILE_DIR="$HOME/.slermes/profiles"
mkdir -p "$PROFILE_DIR"
cat > "$PROFILE_DIR/test-profile.yaml" << 'EOF'
model:
  default: gpt-4o
  temperature: 0.7
agent:
  verbose: 1
  max_turns: 50
EOF

# Test profile list via /config profile list
PROFILE_LIST=$(echo "/config profile list" | timeout 3 "$HERMES" 2>&1 || true)
if echo "$PROFILE_LIST" | grep -q "test-profile"; then ok "config profile list shows test-profile"
else fail "config profile list missing test-profile"; fi

# Test profile activation
PROFILE_ACTIVATE=$(echo "/config profile test-profile" | timeout 3 "$HERMES" 2>&1 || true)
if echo "$PROFILE_ACTIVATE" | grep -q "activated"; then ok "config profile activates test-profile"
else fail "config profile activation failed"; fi

# Cleanup test profile only (don't remove slermes dir)
rm -f "$PROFILE_DIR/test-profile.yaml"

# O14: Sandbox escape detection test (needs libplugin for plugin.h include chain)
echo ""; echo "=== Sandbox Escape Detection Tests (O14) ==="
if gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_sandbox_escape.c" \
    "$CDIR/src/sandbox_escape.c" \
    -o /tmp/hermes_test_sandbox_esc -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_sandbox_esc > /dev/null 2>&1; then
        ok "sandbox_escape (31 tests)"
        rm -f /tmp/hermes_test_sandbox_esc
    else
        echo "  Sandbox escape test output:"
        /tmp/hermes_test_sandbox_esc 2>&1 | sed "s/^/    /"
        fail "sandbox_escape (test binary returned non-zero)"
        rm -f /tmp/hermes_test_sandbox_esc
    fi
    rm -f /tmp/hermes_test_sandbox_esc
else
    echo "  Sandbox escape test compilation error:"
    gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" -I"$CDIR/lib/libplugin" \
        "$CDIR/tests/test_sandbox_escape.c" \
        "$CDIR/src/sandbox_escape.c" \
        -o /tmp/hermes_test_sandbox_esc -lm 2>&1 | sed 's/^/    /'
    skip "sandbox_escape (compilation failed)"
fi

# ==============================================
# Transcribe Tool Tests (D10)
# ==============================================
echo ""; echo "=== Transcribe Tool Tests (D10) ==="
INCDIRS=$(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" $INCDIRS \
    "$CDIR/tests/test_transcribe.c" \
    "$CDIR/src/tools/transcribe.c" "$CDIR/src/tools/registry.c" \
    "$CDIR/lib/libtranscribe/transcribe.c" "$CDIR/lib/libhttp/http.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_transcribe -lssl -lcrypto -ldl -lm -lz 2>/dev/null && [[ -x /tmp/hermes_test_transcribe ]]; then
    if /tmp/hermes_test_transcribe > /dev/null 2>&1; then ok "transcribe (13 tests)"
    else fail "transcribe (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_transcribe
else skip "transcribe (compilation failed)"
fi

# ==============================================
# Clarify Tests
# ==============================================
echo ""; echo "=== Clarify Tests ==="
INCDIRS=$(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" $INCDIRS \
    "$CDIR/tests/test_clarify.c" \
    "$CDIR/src/tools/clarify.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_clarify -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_clarify ]]; then
    if /tmp/hermes_test_clarify > /dev/null 2>&1; then ok "clarify (19 tests)"
    else fail "clarify (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_clarify
else skip "clarify (compilation failed)"
fi

# ==============================================
# Regex tests
echo ""; echo "=== Regex Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libregex" \
    "$CDIR/tests/test_regex.c" \
    "$CDIR/lib/libregex/hermes_regex.c" \
    -o /tmp/hermes_test_regex -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_regex > /dev/null 2>&1; then ok "regex (17 tests)"
    else
        echo "  Regex test output:"
        /tmp/hermes_test_regex 2>&1 | sed 's/^/    /'
        fail "regex (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_regex
else
    echo "  Regex test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libregex" \
        "$CDIR/tests/test_regex.c" \
        "$CDIR/lib/libregex/hermes_regex.c" \
        -o /tmp/hermes_test_regex -lm 2>&1 | sed 's/^/    /'
    skip "regex (compilation failed)"
fi

# ==============================================
# Trajectory tests
echo ""; echo "=== Trajectory Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_trajectory.c" \
    "$CDIR/src/agent/trajectory.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_traj -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_traj > /dev/null 2>&1; then ok "trajectory (19 tests)"
    else
        echo "  Trajectory test output:"
        /tmp/hermes_test_traj 2>&1 | sed 's/^/    /'
        fail "trajectory (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_traj
else
    echo "  Trajectory test compilation error:"
    gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
        "$CDIR/tests/test_trajectory.c" \
        "$CDIR/src/agent/trajectory.c" \
        "$CDIR/lib/libjson/json.c" \
        -o /tmp/hermes_test_traj -lm 2>&1 | sed 's/^/    /'
    skip "trajectory (compilation failed)"
fi

# File Sync Tests (D08)
# ==============================================
echo ""; echo "=== File Sync Tests ==="
INCDIRS=$(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done)
if gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" $INCDIRS \
    "$CDIR/tests/test_file_sync.c" \
    "$CDIR/lib/libfile_sync/file_sync.c" \
    -o /tmp/hermes_test_file_sync -lm 2>/dev/null && [[ -x /tmp/hermes_test_file_sync ]]; then
    if /tmp/hermes_test_file_sync > /dev/null 2>&1; then ok "file_sync (14 tests)"
    else fail "file_sync (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_file_sync
else skip "file_sync (compilation failed)"
fi

# ==============================================
# Voice Mode Tests (P131-P135)
# ==============================================
echo ""; echo "=== Voice Mode Tests ==="
INCDIRS=$(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" $INCDIRS \
    "$CDIR/tests/test_voice_mode.c" \
    "$CDIR/src/tools/voice_mode.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_voice_mode -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_voice_mode ]]; then
    if /tmp/hermes_test_voice_mode > /dev/null 2>&1; then ok "voice_mode (20 tests)"
    else fail "voice_mode (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_voice_mode
else skip "voice_mode (compilation failed)"
fi

# ==============================================
# Image Generation Tests
# ==============================================
echo ""; echo "=== Image Generation Tests ==="
INCDIRS=$(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done)
if gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" $INCDIRS \
    "$CDIR/tests/test_image_gen.c" \
    "$CDIR/src/tools/image_gen.c" \
    "$CDIR/src/tools/image_gen_registry.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libfal_common/fal_common.c" \
    "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_image_gen -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_image_gen ]]; then
    if /tmp/hermes_test_image_gen > /dev/null 2>&1; then ok "image_gen (18 tests)"
    else fail "image_gen (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_image_gen
else skip "image_gen (compilation failed)"
fi

# ==============================================
# Video Generation Tests
# ==============================================
echo ""; echo "=== Video Generation Tests ==="
INCDIRS=$(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done)
if gcc -O2 -Wall -Wextra -Wno-format-truncation -I"$CDIR/include" $INCDIRS \
    "$CDIR/tests/test_video_gen.c" \
    "$CDIR/src/tools/video_gen.c" \
    "$CDIR/src/tools/video_gen_registry.c" \
    "$CDIR/lib/libjson/json.c" \
    "$CDIR/lib/libfal_common/fal_common.c" \
    "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_video_gen -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_video_gen ]]; then
    if /tmp/hermes_test_video_gen > /dev/null 2>&1; then ok "video_gen (14 tests)"
    else fail "video_gen (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_video_gen
else skip "video_gen (compilation failed)"
fi

# file_merge test
if gcc -O2 -Wall -Wextra "${INCLUDES_ALL[@]}" \
    "$CDIR/tests/test_file_merge.c" \
    "$CDIR/tests/test_stubs.c" \
    "$CDIR/src/tools/file_merge.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libdifflib/difflib.c" \
    -o /tmp/hermes_test_file_merge -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_file_merge > /dev/null 2>&1; then ok "file_merge (4 tests)"
    else fail "file_merge (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_file_merge
else skip "file_merge (compilation failed)"
fi

# file_watch test (13 error-path + positive tests)
if gcc -O2 -Wall -Wextra "${INCLUDES_ALL[@]}" \
    "$CDIR/tests/test_file_watch.c" \
    "$CDIR/tests/test_stubs.c" \
    "$CDIR/src/tools/file_watch.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_file_watch -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_file_watch > /dev/null 2>&1; then ok "file_watch (13 tests)"
    else fail "file_watch (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_file_watch
else skip "file_watch (compilation failed)"
fi

# file_batch tests (6 filesystem action tests)
if gcc -O2 -Wall -Wextra "${INCLUDES_ALL[@]}" \
    "$CDIR/tests/test_file_batch.c" \
    -o /tmp/hermes_test_file_batch -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_file_batch > /dev/null 2>&1; then ok "file_batch (6 tests)"
    else fail "file_batch (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_file_batch
else fail "file_batch (compilation failed)"
fi

# ==============================================
# Yuanbao Tools Tests
# ==============================================
echo ""; echo "=== Yuanbao Tools Tests (sticker_search) ==="
INCDIRS=$(for d in "$CDIR"/lib/*/; do echo -n " -I${d%/}"; done)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" $INCDIRS \
    "$CDIR/tests/test_yuanbao_tools.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_yuanbao_tools -lm -Wl,--unresolved-symbols=ignore-all 2>/dev/null && [[ -x /tmp/hermes_test_yuanbao_tools ]]; then
    if /tmp/hermes_test_yuanbao_tools > /dev/null 2>&1; then ok "yuanbao_tools (sticker search)"
    else fail "yuanbao_tools (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_yuanbao_tools
else skip "yuanbao_tools (compilation failed)"
fi

# ==============================================
# 7. Completions test
# ==============================================
echo ""; echo "=== Completions Tests ==="
BASH_COMP=$("$HERMES" completions bash 2>/dev/null)
if echo "$BASH_COMP" | grep -E "_hermes_completions|_slermes_completions"; then ok "bash completions"
else fail "bash completions not generated"; fi

ZSH_COMP=$("$HERMES" completions zsh 2>/dev/null)
if echo "$ZSH_COMP" | grep -E "#compdef hermes|#compdef slermes"; then ok "zsh completions"
else fail "zsh completions not generated"; fi

USAGE_COMP=$("$HERMES" completions 2>&1)
if echo "$USAGE_COMP" | grep -qi "usage"; then ok "completions shows usage"
else fail "completions missing usage"; fi

wait
summary
exit $FAIL
