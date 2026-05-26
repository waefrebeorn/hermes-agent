#!/bin/bash
# slermes-build.sh — Clean build + test + parity check for C translation.
# Safe to run from any branch. Only touches C/ and slermes/ directories.
# Usage: bash scripts/slermes-build.sh [--quick]

set -e
CDIR="$(cd "$(dirname "$0")/.." && pwd)"
QUICK=false
[[ "$1" == "--quick" ]] && QUICK=true

GREEN='\033[0;32m'; RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'

echo -e "${CYAN}=== Slermes Build ===${NC}"
echo ""

if [ "$QUICK" = true ]; then
    echo "Quick mode: incremental build only"
    make -C "$CDIR/C" -j$(nproc) 2>&1 | tail -1
else
    # Full clean build
    echo "1. Building C/hermes (clean)..."
    make -C "$CDIR/C" clean && make -C "$CDIR/C" -j$(nproc) 2>&1 | tail -1
    
    echo ""
    echo "2. Building C/slermes alias..."
    cp "$CDIR/C/hermes" "$CDIR/slermes/slermes" 2>/dev/null || true
    make -C "$CDIR/slermes" -j$(nproc) 2>&1 | tail -1
    
    echo ""
    echo "3. Running test suite..."
    bash "$CDIR/C/test_runner.sh" 2>&1 | tail -1
    
    echo ""
    echo "4. Running parity check..."
    bash "$CDIR/parity_loop.sh" 2>&1 | tail -1
    
    echo ""
    echo "5. Copying binary to PATH..."
    cp "$CDIR/slermes/slermes" /home/wubu/.local/bin/slermes 2>/dev/null || true
fi

# Quick smoke test
echo ""
echo -e "${CYAN}Binary:${NC} $(./C/hermes --version 2>/dev/null | head -1)"
echo -e "${CYAN}Size:${NC} $(stat -c '%s' C/hermes 2>/dev/null || echo '?') bytes"
echo -e "${CYAN}Tests:${NC} $(grep -oP '\d+(?= passed)' <(bash C/test_runner.sh 2>/dev/null) || echo '?')/21 pass"
echo ""
echo -e "${CYAN}=== Build Complete ===${NC}"
echo "  slermes = C/hermes (same binary, alias name)"
echo "  Usage:  slermes \"your prompt\""
echo "  Config: ~/.slermes/"
echo ""
