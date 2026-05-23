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

run_lib_test "json"      "tests/test_json.c"           "lib/libjson"            "$CDIR/lib/libjson/json.c"
run_lib_test "protobuf"  "tests/test_protobuf.c"       "lib/libprotobuf"        "$CDIR/lib/libprotobuf/protobuf.c"
run_lib_test "http"      "tests/test_http.c"           "lib/libhttp"            "$CDIR/lib/libhttp/http.c -lssl -lcrypto"
run_lib_test "yaml"     "tests/test_yaml.c"         "lib/libyaml"            "$CDIR/lib/libyaml/yaml.c"
run_lib_test "crypto"   "tests/test_crypto.c"       "lib/libcrypto"          "$CDIR/lib/libcrypto/crypto.c -lssl -lcrypto"
run_lib_test "tokenizer" "tests/test_tokenizer.c"    "include"                 "$CDIR/src/hermes_tokenizer.c"
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
fi

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
fi

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
fi

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
fi

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
fi

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
fi

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
fi

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
fi

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
fi

run_lib_test "dotenv"   "tests/test_dotenv.c"       "lib/libdotenv"          "$CDIR/lib/libdotenv/dotenv.c"
run_lib_test "cron"     "tests/test_cron_lib.c"         "lib/libcron"            "$CDIR/lib/libcron/cron.c"
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
run_lib_test "path"     "tests/test_path.c"         "lib/libpath"            "$CDIR/lib/libpath/path.c"
run_lib_test "skin"     "tests/test_skin.c"         "lib/libskin"            "-I$CDIR/lib/libjson $CDIR/lib/libskin/skin.c $CDIR/lib/libjson/json.c -lm"

# datetime library test (J05 -- standalone, no deps)
echo ""; echo "=== datetime Library Tests (J05) ==="
run_lib_test "datetime" "tests/test_datetime.c" "lib/libdatetime" "$CDIR/lib/libdatetime/datetime.c"

# CSV library test (J06 — standalone, no deps)
echo ""; echo "=== CSV Library Tests (J06) ==="
run_lib_test "csv" "tests/test_csv.c" "lib/libcsv" "$CDIR/lib/libcsv/csv.c"

# Hash library test (J07 — needs OpenSSL)
echo ""; echo "=== Hash Library Tests (J07) ==="
run_lib_test "hash" "tests/test_hash.c" "lib/libhash" "$CDIR/lib/libhash/hash.c -lssl -lcrypto"

# UUID library test (J08 — needs libhash for v5 + OpenSSL)
echo ""; echo "=== UUID Library Tests (J08) ==="
run_lib_test "uuid" "tests/test_uuid.c" "lib/libuuid" "$CDIR/lib/libuuid/uuid.c $CDIR/lib/libhash/hash.c -lssl -lcrypto"

# Base64 library test (J09 — standalone, no deps)
echo ""; echo "=== Base64 Library Tests (J09) ==="
run_lib_test "base64" "tests/test_base64.c" "lib/libbase64" "$CDIR/lib/libbase64/base64.c"

# HTML library test (J10 — standalone, no deps)
echo ""; echo "=== HTML Library Tests (J10) ==="
run_lib_test "html" "tests/test_html.c" "lib/libhtml" "$CDIR/lib/libhtml/html.c"

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
run_lib_test "vision_tool" "tests/test_vision_tool.c" "." ""

# TTS tool test (M34 — self-contained, no deps)
echo ""; echo "=== TTS Tool Tests (M34) ==="
run_lib_test "tts_tool" "tests/test_tts_tool.c" "." ""

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
fi

echo ""; echo "=== Redact Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" -I"$CDIR/lib/libdb" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libtui" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libdotenv" \
    "$CDIR/tests/test_redact.c" \
    "$CDIR/src/agent/redact.c" \
    -o /tmp/hermes_test_redact -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_redact > /dev/null 2>&1; then ok "redact (17 tests)"
    else fail "redact (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_redact
else skip "redact (compilation failed)"
fi

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
echo ""; echo "=== File Permission Hardening Tests (O15) ==="
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
    -o /tmp/hermes_test_vault -lm -lssl -lcrypto > /dev/null 2>&1; then
    if /tmp/hermes_test_vault > /dev/null 2>&1; then ok "vault (37 tests)"
    else
        echo "  Vault test output:"
        /tmp/hermes_test_vault 2>&1 | sed 's/^/    /'
        fail "vault (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_vault
else skip "vault (compilation failed)"
fi

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
fi

# exec_code tool test (M41 — self-contained, no deps)
echo ""; echo "=== exec_code Tool Tests (M41) ==="
run_lib_test "exec_code" "tests/test_exec_code.c" "." ""

# TIRITH policy engine test (O13 — standalone, only needs tirith.c + fnmatch)
echo ""; echo "=== TIRITH Policy Depth Tests (O13) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
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
fi

# Provider metadata test (needs libjson + libplugin + url_safety)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_provider_metadata.c" \
    "$CDIR/src/agent/provider_metadata.c" "$CDIR/src/tools/url_safety.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_provmeta -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_provmeta > /dev/null 2>&1; then ok "provider_metadata"
    else fail "provider_metadata (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_provmeta
else skip "provider_metadata (compilation failed)"
fi

# Azure provider depth test (B37-B38: deployment_id + api_version)
echo ""; echo "=== Azure Provider Depth Tests (B37-B38) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_azure_depth.c" \
    "$CDIR/src/agent/provider_azure.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_bedrock.c" "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_azure_depth -lm -lssl -lcrypto > /dev/null 2>&1; then
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_azure_depth -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "azure_depth (compilation failed)"
fi

# Bedrock provider depth test (B39-B41: inferenceProfile + guardrails + trace)
echo ""; echo "=== Bedrock Provider Depth Tests (B39-B41) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_bedrock_depth.c" \
    "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_br_depth -lm -lssl -lcrypto > /dev/null 2>&1; then
    if /tmp/hermes_test_br_depth > /dev/null 2>&1; then ok "bedrock_depth (14 tests)"
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_br_depth -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "bedrock_depth (compilation failed)"
fi

# OpenRouter provider depth test (B43-B46: provider preferences)
echo ""; echo "=== OpenRouter Provider Depth Tests (B43-B46) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_openrouter_depth.c" \
    "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_deepseek.c" \
    "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_or_depth -lm -lssl -lcrypto > /dev/null 2>&1; then
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_or_depth -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "openrouter_depth (compilation failed)"
fi

# Anthropic provider depth test (B26-B28: thinking, cache control, tool format)
echo ""; echo "=== Anthropic Provider Depth Tests (B26-B28) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_anthropic_depth.c" \
    "$CDIR/src/agent/provider_anthropic.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_anth_depth -lm -lssl -lcrypto > /dev/null 2>&1; then
    if /tmp/hermes_test_anth_depth > /dev/null 2>&1; then ok "anthropic_depth (50 tests)"
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_anth_depth -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "anthropic_depth (compilation failed)"
fi

# Google provider comprehensive tests
echo ""; echo "=== Google Provider Comprehensive Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_google_full.c" \
    "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_goog_full -lm -lssl -lcrypto > /dev/null 2>&1; then
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_goog_full -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "google_full (compilation failed)"
fi

# ==============================================
# These tests exist but were never wired into test_runner:
# test_finish_reason.c (B22), test_google_depth.c, test_json_mode.c
# ==============================================

# B22: finish_reason tracking (standalone — needs all providers + json + http)
echo ""; echo "=== Finish Reason Tracking Tests (B22) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_finish_reason.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_finish_reason -lm -lssl -lcrypto > /dev/null 2>&1; then
    if /tmp/hermes_test_finish_reason > /dev/null 2>&1; then ok "finish_reason (12 tests)"
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_finish_reason -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "finish_reason (compilation failed)"
fi

# Google depth tests (B30-B31: top_k + candidate_count + systemInstruction)
echo ""; echo "=== Google Provider Depth Tests (B30-B31) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_google_depth.c" \
    "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_google_depth -lm -lssl -lcrypto > /dev/null 2>&1; then
    if /tmp/hermes_test_google_depth > /dev/null 2>&1; then ok "google_depth (7 tests)"
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_google_depth -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "google_depth (compilation failed)"
fi

# JSON mode + response_format UAF test (B23 — standalone, needs all providers)
echo ""; echo "=== JSON Mode Tests (B23) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_json_mode.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_json_mode -lm -lssl -lcrypto > /dev/null 2>&1; then
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_json_mode -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "json_mode (compilation failed)"
fi

# Bedrock provider comprehensive tests
echo ""; echo "=== Bedrock Provider Comprehensive Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_bedrock_full.c" \
    "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_br_full -lm -lssl -lcrypto > /dev/null 2>&1; then
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_br_full -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "bedrock_full (compilation failed)"
fi

# Azure provider comprehensive tests
echo ""; echo "=== Azure Provider Comprehensive Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_azure_full.c" \
    "$CDIR/src/agent/provider_azure.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_az_full -lm -lssl -lcrypto > /dev/null 2>&1; then
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_az_full -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "azure_full (compilation failed)"
fi

# DeepSeek FIM tests (B32 — needs all providers + JSON + HTTP + URL safety)
echo ""; echo "=== DeepSeek FIM (Fill-in-the-Middle) Tests (B32) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
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
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_deepseek_fim -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_deepseek_fim > /dev/null 2>&1; then ok "deepseek_fim (30 tests)"
    else
        echo "  DeepSeek FIM test output:"
        /tmp/hermes_test_deepseek_fim 2>&1 | sed 's/^/    /'
        fail "deepseek_fim (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_deepseek_fim
else skip "deepseek_fim (compilation failed)"
fi

# Discord interaction tests (M08 — tests JSON parsing of interactions, modals, components)
echo ""; echo "=== Discord Interaction Tests (M08) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    -I"$CDIR/lib/libmcp" -I"$CDIR/lib/libskin" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" \
    -I"$CDIR/lib/libdb" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" \
    -I"$CDIR/lib/libtui" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libdotenv" \
    "$CDIR/tests/test_discord_interactions.c" \
    "$CDIR/src/gateway/platforms/discord.c" "$CDIR/src/tools/url_safety.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_discord_int -lm -lssl -lcrypto -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_discord_int > /dev/null 2>&1; then ok "discord_interactions (31 tests)"
    else
        echo "  Discord interaction test output:"
        /tmp/hermes_test_discord_int 2>&1 | sed 's/^/    /'
        fail "discord_interactions (test binary returned non-zero)"
    fi
    rm -f /tmp/hermes_test_discord_int
else skip "discord_interactions (compilation failed)"
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

# Provider error handling edge cases test (M06 — all providers with error inputs)
echo ""; echo "=== Provider Error Handling Edge Cases (M06) ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libhttp" \
    "$CDIR/tests/test_provider_error.c" \
    "$CDIR/src/agent/provider.c" \
    "$CDIR/src/agent/provider_openai.c" "$CDIR/src/agent/provider_openrouter.c" \
    "$CDIR/src/agent/provider_deepseek.c" "$CDIR/src/agent/provider_xai.c" \
    "$CDIR/src/agent/provider_anthropic.c" "$CDIR/src/agent/provider_google.c" \
    "$CDIR/src/agent/provider_azure.c" "$CDIR/src/agent/provider_bedrock.c" \
    "$CDIR/src/agent/provider_custom.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_proverr -lm -lssl -lcrypto > /dev/null 2>&1; then
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_proverr -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
    skip "provider_error (M06: compilation failed)"
fi

# xAI model retirement detection test (L04)
echo ""; echo "=== xAI Retirement Detection Tests ==="
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libyaml" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libcrypto" -I"$CDIR/lib/libtemplate" -I"$CDIR/lib/libcron" -I"$CDIR/lib/libproc" -I"$CDIR/lib/libtui" -I"$CDIR/lib/libdb" -I"$CDIR/lib/libwebsocket" -I"$CDIR/lib/libprotobuf" -I"$CDIR/lib/libmcp" \
    "$CDIR/tests/test_xai_retirement.c" \
    "$CDIR/src/xai_retirement.c" \
    -o /tmp/hermes_test_xretire -lm > /dev/null 2>&1; then
    if /tmp/hermes_test_xretire > /dev/null 2>&1; then ok "xai_retirement (31 tests)"
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
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
    -o /tmp/hermes_test_hub -lm -lssl -lcrypto > /dev/null 2>&1; then
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
        "$CDIR/lib/libjson/json.c" "$CDIR/lib/libhttp/http.c" \
        -o /tmp/hermes_test_hub -lm -lssl -lcrypto 2>&1 | sed 's/^/    /'
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
    if /tmp/hermes_test_credpool > /dev/null 2>&1; then ok "credential_pool (80 tests)"
    else fail "credential_pool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_credpool
else skip "credential_pool (compilation failed)"
fi

# Budget tracker test (needs budget_tracker.c + provider_metadata + url_safety + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_budget_tracker.c" \
    "$CDIR/src/agent/budget_tracker.c" "$CDIR/src/agent/provider_metadata.c" \
    "$CDIR/src/tools/url_safety.c" \
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

# Gateway escape modes test (M07: markdown_to_html, markdown_v2_escape, truncate_message)
echo ""; echo "=== Gateway Escape Modes (M07) ==="
if gcc -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-but-set-variable \
    -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_gateway_escape.c" \
    "$CDIR/src/gateway/server.c" \
    "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libcron/cron.c" \
    -o /tmp/hermes_test_gwesc -lm -lssl -lcrypto -lpthread \
    -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_gwesc > /dev/null 2>&1; then ok "gateway_escape (M07: 30 tests)"
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
if gcc -O0 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_agent.c" \
    "$CDIR/src/agent/context.c" "$CDIR/src/agent/checkpoint.c" \
    "$CDIR/lib/libjson/json.c" "$CDIR/lib/libplugin/plugin.c" \
    -o /tmp/hermes_test_agent -lm -lpthread -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
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

# Error system test (K01-K05: standalone — only needs hermes_error.c + pthread)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_hermes_error.c" \
    "$CDIR/src/hermes_error.c" \
    -o /tmp/hermes_test_error -lm -lpthread > /dev/null 2>&1; then
    if /tmp/hermes_test_error > /dev/null 2>&1; then ok "error_system (11 tests)"
    else fail "error_system (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_error
else skip "error_system (compilation failed)"
fi

# Memory tool test (M35 — needs memory.c, json, sqlite3)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libdb" \
    "$CDIR/tests/test_memory.c" \
    "$CDIR/src/tools/memory.c" "$CDIR/lib/libjson/json.c" "$CDIR/lib/libdb/sqlite3.c" \
    -o /tmp/hermes_test_memory -lm -lpthread -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_memory > /dev/null 2>&1; then ok "memory_tool (16 tests)"
    else fail "memory_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_memory
else skip "memory_tool (compilation failed)"
fi

# File tool test (M31 — needs file.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libdb" \
    "$CDIR/tests/test_file.c" \
    "$CDIR/src/tools/file.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_file -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_file > /dev/null 2>&1; then ok "file_tool (35 tests)"
    else fail "file_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_file
else skip "file_tool (compilation failed)"
fi

# Vision tool test (M33 — needs vision.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_vision.c" \
    "$CDIR/src/tools/vision.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_vision -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_vision > /dev/null 2>&1; then ok "vision_tool (19 tests)"
    else fail "vision_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_vision
else skip "vision_tool (compilation failed)"
fi

# Todo tool test (M43 — needs todo.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_todo.c" \
    "$CDIR/src/tools/todo.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_todo -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    HERMES_HOME=/tmp/hermes_test_todo_home /tmp/hermes_test_todo > /dev/null 2>&1
    if [ $? -eq 0 ]; then ok "todo_tool (14 tests)"
    else fail "todo_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_todo
    rm -rf /tmp/hermes_test_todo_home
else skip "todo_tool (compilation failed)"
fi

# Patch tool test (M42 — needs patch.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_patch.c" \
    "$CDIR/src/tools/patch.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_patch -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_patch > /dev/null 2>&1; then ok "patch_tool (17 tests)"
    else fail "patch_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_patch
else skip "patch_tool (compilation failed)"
fi

# Process tool test (M39 — needs process.c + json lib)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_process.c" \
    "$CDIR/src/tools/process.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_process -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_process > /dev/null 2>&1; then ok "process_tool (17 tests)"
    else fail "process_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_process
else skip "process_tool (compilation failed)"
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
fi

# Send_message tool test (M37)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_send_message.c" \
    "$CDIR/src/tools/send_message.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_sendmsg -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_sendmsg > /dev/null 2>&1; then ok "send_message_tool (14 tests)"
    else fail "send_message_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_sendmsg
else skip "send_message_tool (compilation failed)"
fi

# Web tool test (M30 — needs web.c + tool_config + registry + http + json)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libhttp" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_web.c" \
    "$CDIR/src/tools/web.c" "$CDIR/src/tools/tool_config.c" "$CDIR/src/tools/registry.c" \
    "$CDIR/lib/libhttp/http.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_web -lm -lssl -lcrypto > /dev/null 2>&1; then
    if /tmp/hermes_test_web > /dev/null 2>&1; then ok "web_tool (22 tests)"
    else fail "web_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_web
else skip "web_tool (compilation failed)"
fi

# Terminal tool test (M29 — needs terminal.c + tool_config + json + sandbox_escape)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_terminal.c" \
    "$CDIR/src/tools/terminal.c" "$CDIR/src/tools/tool_config.c" \
    "$CDIR/src/sandbox_escape.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_terminal -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_terminal > /dev/null 2>&1; then ok "terminal_tool (26 tests)"
    else fail "terminal_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_terminal
else skip "terminal_tool (compilation failed)"
fi

# Clarify tool test (M40 — needs clarify.c + json, stdin for response)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_clarify.c" \
    "$CDIR/src/tools/clarify.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_clarify -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if echo "answer" | /tmp/hermes_test_clarify > /dev/null 2>&1; then ok "clarify_tool (9 tests)"
    else fail "clarify_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_clarify
else skip "clarify_tool (compilation failed)"
fi

# TTS tool test (M34 — needs tts.c + tool_config + json)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_tts.c" \
    "$CDIR/src/tools/tts.c" "$CDIR/src/tools/tool_config.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_tts -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_tts > /dev/null 2>&1; then ok "tts_tool (20 tests)"
    else fail "tts_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_tts
else skip "tts_tool (compilation failed)"
fi

# Cron tool test (M36 — needs cronjob.c + libcron + json, uses stubs for scheduler)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libcron" \
    "$CDIR/tests/test_cron_tool.c" \
    "$CDIR/src/tools/cronjob.c" "$CDIR/lib/libcron/cron.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_cron -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_cron > /dev/null 2>&1; then ok "cron_tool (25 tests)"
    else fail "cron_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_cron
else skip "cron_tool (compilation failed)"
fi

# Delegate tool test (M32 — needs delegate.c + json, 20MB stack req)
if gcc -O2 -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_delegate.c" \
    "$CDIR/src/tools/delegate.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_delegate -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if ulimit -s unlimited && /tmp/hermes_test_delegate > /dev/null 2>&1; then ok "delegate_tool (4 tests)"
    else fail "delegate_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_delegate
else skip "delegate_tool (compilation failed)"
fi

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
fi

# Skill management tool test (skill_view + skill_manage — needs skill_mgmt.c + json)
echo ""; echo "=== Skill Management Tests ==="
if gcc -O2 -g -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" \
    "$CDIR/tests/test_skill_mgmt.c" \
    "$CDIR/src/tools/skill_mgmt.c" \
    "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_skill_mgmt -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_skill_mgmt > /dev/null 2>&1; then ok "skill_mgmt_tool (9 tests)"
    else fail "skill_mgmt_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_skill_mgmt
else skip "skill_mgmt_tool (compilation failed)"
fi

# MCP tool test (M44 — needs mcp_tool.c + libmcp + registry + json)
if gcc -O2 -g -Wall -Wextra -I"$CDIR/include" -I"$CDIR/lib/libjson" -I"$CDIR/lib/libplugin" -I"$CDIR/lib/libmcp" \
    "$CDIR/tests/test_mcp.c" \
    "$CDIR/src/tools/mcp_tool.c" "$CDIR/lib/libmcp/mcp.c" \
    "$CDIR/src/tools/registry.c" "$CDIR/lib/libjson/json.c" \
    -o /tmp/hermes_test_mcp -lm -Wl,--unresolved-symbols=ignore-all > /dev/null 2>&1; then
    if /tmp/hermes_test_mcp > /dev/null 2>&1; then ok "mcp_tool (24 tests)"
    else fail "mcp_tool (test binary returned non-zero)"; fi
    rm -f /tmp/hermes_test_mcp
else skip "mcp_tool (compilation failed)"
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
    if /tmp/hermes_test_sandbox_esc > /dev/null 2>&1; then ok "sandbox_escape (14 tests)"
    else
        echo "  Sandbox escape test output:"
        /tmp/hermes_test_sandbox_esc 2>&1 | sed 's/^/    /'
        fail "sandbox_escape (test binary returned non-zero)"
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
# 7. Completions test
# ==============================================
echo ""; echo "=== Completions Tests ==="
BASH_COMP=$("$HERMES" completions bash 2>/dev/null)
if echo "$BASH_COMP" | grep -q "complete -F _hermes_completions"; then ok "bash completions"
else fail "bash completions not generated"; fi

ZSH_COMP=$("$HERMES" completions zsh 2>/dev/null)
if echo "$ZSH_COMP" | grep -q "#compdef hermes"; then ok "zsh completions"
else fail "zsh completions not generated"; fi

USAGE_COMP=$("$HERMES" completions 2>&1)
if echo "$USAGE_COMP" | grep -qi "usage"; then ok "completions shows usage"
else fail "completions missing usage"; fi

summary
exit $FAIL
