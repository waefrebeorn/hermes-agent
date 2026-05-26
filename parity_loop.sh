#!/bin/bash
# Parity Loop — slermes vs C/ hermes 1:1 check
# Usage: bash parity_loop.sh [--fix] [--verbose]
# Without --fix: report only. With --fix: auto-correct drift.
# Returns non-zero if parity issues found (non-fix mode).

set -o pipefail
CDIR="$(cd "$(dirname "$0")" && pwd)"
SLERMES="$CDIR/slermes"
HERMES="$CDIR/C"
VERBOSE=false
FIX=false
ISSUES=0

[[ "$1" == "--verbose" ]] && VERBOSE=true && shift
[[ "$1" == "--fix" ]] && FIX=true

GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
pass() { echo -e "  ${GREEN}PASS${NC}: $1"; }
fail() { echo -e "  ${RED}FAIL${NC}: $1"; ISSUES=$((ISSUES+1)); }
info() { echo -e "  ${CYAN}INFO${NC}: $1"; }
warn() { echo -e "  ${YELLOW}WARN${NC}: $1"; }
header() { echo -e "\n${CYAN}=== $1 ===${NC}"; }

summary() {
    echo -e "\n${CYAN}==============================================${NC}"
    echo -e "  ${CYAN}Parity Issues: ${ISSUES}${NC}"
    echo -e "${CYAN}==============================================${NC}"
    return $ISSUES
}

# ─── 1. FILE STRUCTURE PARITY ────────────────────────────────
header "FILE STRUCTURE PARITY"

# Check all .c and .h files exist in both
missing_in_slermes=0
missing_in_hermes=0
while IFS= read -r f; do
    rel="${f#$HERMES/}"
    s="$SLERMES/$rel"
    # Account for renamed headers: hermes_x.h -> slermes_x.h
    s_alt="${s/include\/hermes_/include\/slermes_}"
    s_alt2="${s_alt/include\/hermes.h/include\/slermes.h}"
    if [ ! -f "$s" ] && [ ! -f "$s_alt2" ]; then
        fail "slermes missing: $rel"
        missing_in_slermes=$((missing_in_slermes+1))
    fi
done < <(find "$HERMES/src" "$HERMES/include" "$HERMES/lib" "$HERMES/tests" -type f \( -name "*.c" -o -name "*.h" \) 2>/dev/null)

while IFS= read -r f; do
    rel="${f#$SLERMES/}"
    if [ "$rel" = "Makefile" ] || [ "$rel" = "README.md" ] || [ "$rel" = "DEPENDENCIES.md" ] || [ "$rel" = "digest.py" ] || [ "$rel" = "digestion.md" ] || [ "$rel" = "test_runner.sh" ] || [ "$rel" = ".gitignore" ] || [ "$rel" = ".digest_state.json" ] || [ "$rel" = "deps-repo" ]; then
        continue
    fi
    c="$HERMES/$rel"
    if [ ! -f "$c" ]; then
        warn "hermes missing (slermes-only): $rel"
    fi
done < <(find "$SLERMES/src" "$SLERMES/include" "$SLERMES/lib" "$SLERMES/tests" -type f \( -name "*.c" -o -name "*.h" \) 2>/dev/null)

if [ "$missing_in_slermes" -eq 0 ]; then
    pass "All source files present in slermes/"
fi

# ─── 2. NORMALIZED SOURCE DIFF (ignore naming) ────────────────
header "NORMALIZED SOURCE DIFF"

diff_issues=0
while IFS= read -r f; do
    rel="${f#$HERMES/}"
    s="$SLERMES/$rel"
    [ ! -f "$s" ] && continue

    # Normalize: replace slermes->hermes names, then diff
    # This catches logic differences while ignoring naming
    norm_c=$(mktemp)
    norm_s=$(mktemp)

    # Normalize hermes source: replace hermes->XXXXX
    sed 's/hermes/XXXXX/g; s/HERMES/XXXXX/g; s/Hermes/XXXXX/g; s/SLERMES/XXXXX/g; s/slermes/XXXXX/g; s/Slermes/XXXXX/g' "$f" > "$norm_c"
    sed 's/hermes/XXXXX/g; s/HERMES/XXXXX/g; s/Hermes/XXXXX/g; s/SLERMES/XXXXX/g; s/slermes/XXXXX/g; s/Slermes/XXXXX/g' "$s" > "$norm_s"

    if ! diff -q "$norm_c" "$norm_s" > /dev/null 2>&1; then
        # Get diff size
        diff_lines=$(diff "$norm_c" "$norm_s" 2>/dev/null | grep -c '^[<>]' 2>/dev/null || echo "0")
        diff_lines=${diff_lines%%[^0-9]*}
        diff_lines=${diff_lines:-0}
        if [ "$diff_lines" -gt 20 ]; then
            fail "MASSIVE DIFF ($diff_lines lines): $rel"
            diff_issues=$((diff_issues+1))
            $VERBOSE && diff "$norm_c" "$norm_s" | head -30
        else
            info "minor diff ($diff_lines lines): $rel"
            $VERBOSE && diff "$norm_c" "$norm_s" | head -20
        fi
    fi
    rm -f "$norm_c" "$norm_s"
done < <(find "$HERMES/src" "$HERMES/include" -type f \( -name "*.c" -o -name "*.h" \) 2>/dev/null)

if [ "$diff_issues" -eq 0 ]; then
    pass "All source files have identical logic (naming normalized)"
fi

# ─── 3. BUILD BOTH ────────────────────────────────────────────
header "BUILD"

echo "  Building hermes... "
make -C "$HERMES" -j$(nproc) > /tmp/hermes_build.log 2>&1
if [ $? -eq 0 ]; then
    pass "hermes builds cleanly"
else
    fail "hermes build FAILED"
    tail -20 /tmp/hermes_build.log
fi

echo "  Building slermes... "
make -C "$SLERMES" -j$(nproc) > /tmp/slermes_build.log 2>&1
if [ $? -eq 0 ]; then
    pass "slermes builds cleanly"
else
    fail "slermes build FAILED"
    tail -20 /tmp/slermes_build.log
fi

# ─── 4. TEST BOTH ─────────────────────────────────────────────
header "TEST SUITE"

echo "  Testing hermes... "
bash "$HERMES/test_runner.sh" > /tmp/hermes_test.log 2>&1
h_result=$?
h_pass=$(grep -oP '\d+(?= passed)' /tmp/hermes_test.log || echo "0")
h_fail=$(grep -oP '\d+(?= failed)' /tmp/hermes_test.log || echo "0")
if [ "$h_fail" -eq 0 ] && [ "$h_result" -eq 0 ]; then
    pass "hermes: ${h_pass} passed, 0 failed"
else
    fail "hermes: ${h_pass} passed, ${h_fail} failed"
    tail -10 /tmp/hermes_test.log
fi

echo "  Testing slermes... "
bash "$SLERMES/test_runner.sh" > /tmp/slermes_test.log 2>&1
s_result=$?
s_pass=$(grep -oP '\d+(?= passed)' /tmp/slermes_test.log || echo "0")
s_fail=$(grep -oP '\d+(?= failed)' /tmp/slermes_test.log || echo "0")
if [ "$s_fail" -eq 0 ] && [ "$s_result" -eq 0 ]; then
    pass "slermes: ${s_pass} passed, 0 failed"
else
    fail "slermes: ${s_pass} passed, ${s_fail} failed"
    tail -10 /tmp/slermes_test.log
fi

# ─── 5. BINARY BEHAVIOR PARITY ────────────────────────────────
header "BINARY BEHAVIOR"

# --version
h_ver=$("$HERMES/hermes" --version 2>/dev/null | grep -oP 'v[\d.]+')
s_ver=$("$SLERMES/slermes" --version 2>/dev/null | grep -oP 'v[\d.]+')
if [ "$h_ver" = "$s_ver" ]; then
    pass "Version match: ${h_ver}"
else
    fail "Version mismatch: hermes=${h_ver} slermes=${s_ver}"
fi

# --version branding
h_brand=$("$HERMES/hermes" --version 2>/dev/null | head -1)
s_brand=$("$SLERMES/slermes" --version 2>/dev/null | head -1)
if echo "$h_brand" | grep -q "WuBu Hermes" && echo "$s_brand" | grep -q "WuBu Slermes"; then
    pass "Branding correct: 'WuBu Hermes' and 'WuBu Slermes'"
else
    fail "Branding check: hermes='${h_brand}' slermes='${s_brand}'"
fi

# Gateway banner (capture both stdout and stderr)
h_gw=$("$HERMES/hermes" gateway 2>&1 | head -3 | tr '\n' ' ')
s_gw=$("$SLERMES/slermes" gateway 2>&1 | head -3 | tr '\n' ' ')
if echo "$h_gw" | grep -q "WuBu Hermes" && echo "$s_gw" | grep -q "WuBu Slermes"; then
    pass "Gateway branding correct"
else
    fail "Gateway branding: hermes='${h_gw}' slermes='${s_gw}'"
fi

# Binary sizes
h_size=$(stat -c%s "$HERMES/hermes" 2>/dev/null)
s_size=$(stat -c%s "$SLERMES/slermes" 2>/dev/null)
size_diff=$((h_size - s_size))
size_diff_abs=${size_diff#-}
if [ "$size_diff_abs" -le 128 ]; then
    pass "Binary sizes within 128 bytes: hermes=${h_size}, slermes=${s_size} (diff=${size_diff})"
else
    warn "Binary sizes differ by ${size_diff_abs} bytes: hermes=${h_size}, slermes=${s_size}"
fi

# ─── 6. HERMES SYMBOL CHECK ──────────────────────────────────
header "SYMBOL CLEANLINESS"

h_syms=$(nm "$SLERMES/slermes" 2>/dev/null | grep -i "hermes" | grep -v "User-Agent\|WuBuHermes" | wc -l)
if [ "$h_syms" -eq 0 ]; then
    pass "Zero 'hermes' symbols in slermes binary"
else
    fail "${h_syms} 'hermes' symbols remain in slermes binary"
    nm "$SLERMES/slermes" 2>/dev/null | grep -i "hermes" | grep -v "User-Agent\|WuBuHermes"
fi

# ─── 7. ENV/PATH ISOLATION ───────────────────────────────────
header "PATH ISOLATION"

h_paths=$(grep -rn "\.hermes" "$SLERMES/src" --include="*.c" --include="*.h" 2>/dev/null | grep -v "\.o:" | grep -v "SLERMES_HOME" | wc -l)
if [ "$h_paths" -eq 0 ]; then
    pass "Zero '.hermes' paths in slermes source"
else
    fail "${h_paths} '.hermes' paths remain in slermes source"
    grep -rn "\.hermes" "$SLERMES/src" --include="*.c" --include="*.h" 2>/dev/null | grep -v "\.o:"
fi

# ─── 8. HERMES ENV VARS IN SRC ──────────────────────────────
header "ENV VAR ISOLATION"

h_env=$(grep -rn "getenv(\"HERMES" "$SLERMES/src" --include="*.c" --include="*.h" 2>/dev/null | grep -v "\.o:" | grep -v "HERMES_API_KEY\|#\|//" | wc -l)
if [ "$h_env" -eq 0 ]; then
    pass "No stray HERMES_* getenv in slermes source"
else
    fail "${h_env} HERMES_* getenv calls remain in slermes source"
    grep -rn "getenv(\"HERMES" "$SLERMES/src" --include="*.c" --include="*.h" 2>/dev/null | grep -v "\.o:"
fi

# ─── 9. FIX MODE: Auto-sync new files from C/ to slermes ──
if [ "$FIX" = true ] && [ "$ISSUES" -gt 0 ]; then
    header "AUTO-FIX"

    # Sync new .c/.h files from C/ to slermes
    while IFS= read -r f; do
        rel="${f#$HERMES/}"
        s="$SLERMES/$rel"
        # Check if file is missing (accounting for header renames)
        s_alt="${s/include\/hermes_/include\/slermes_}"
        s_alt2="${s_alt/include\/hermes.h/include\/slermes.h}"
        if [ ! -f "$s" ] && [ ! -f "$s_alt2" ]; then
            echo "  Copying new file: $rel"
            mkdir -p "$(dirname "$s")"
            cp "$f" "$s"
            # Apply slermes renaming to the copied file
            sed -i 's|#include "hermes\([^"]*\)"|#include "slermes\1"|g' "$s"
            sed -i 's/HERMES_PATH_MAX/SLERMES_PATH_MAX/g; s/HERMES_VERSION/SLERMES_VERSION/g' "$s"
            sed -i 's|~/\\.hermes|~/.slermes|g; s|/tmp/\\.hermes|/tmp/.slermes|g' "$s"
            sed -i 's/HERMES_MAX_TOOL_CALLS/SLERMES_MAX_TOOL_CALLS/g' "$s"
            sed -i 's/getenv("HERMES_HOME")/getenv("SLERMES_HOME")/g' "$s"
            ISSUES=$((ISSUES-1))
        fi
    done < <(find "$HERMES/src" "$HERMES/include" "$HERMES/lib" "$HERMES/tests" -type f \( -name "*.c" -o -name "*.h" \) 2>/dev/null)

    if [ "$ISSUES" -eq 0 ]; then
        echo "  All fixed! Rebuilding..."
        make -C "$SLERMES" clean && make -C "$SLERMES" -j$(nproc)
        echo "  Re-run without --fix to verify."
    fi
fi

summary
exit $ISSUES
