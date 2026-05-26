#!/bin/bash
# Hermes Triple Test — run all 3 side-by-side with 15s timeout
# Usage: bash triple_test.sh "your test message"

MSG="${1:-Say 'Hello' and nothing else.}"
TIMEOUT=15

GREEN='\033[0;32m'; CYAN='\033[0;36m'; YELLOW='\033[1;33m'; NC='\033[0m'

echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
echo -e "${CYAN}  HERMES TRIPLE TEST (${TIMEOUT}s timeout per binary)${NC}"
echo -e "${CYAN}  Msg: ${MSG}${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════${NC}"

for bin in hermes hermes-dev slermes; do
    echo
    echo -e "${YELLOW}── $bin ──${NC}"
    START=$(date +%s%N)

    # Python hermes uses -z for prompt, slermes C uses positional arg
    if [ "$bin" = "slermes" ]; then
        output=$(timeout $TIMEOUT $bin "$MSG" 2>&1)
    else
        output=$(timeout $TIMEOUT $bin -z "$MSG" 2>&1)
    fi
    EXIT=$?
    END=$(date +%s%N)
    DURATION=$(( (END - START) / 1000000 ))

    if [ $EXIT -eq 124 ]; then
        echo -e "  ${RED}TIMEOUT${NC} (${TIMEOUT}s exceeded)"
    elif [ $EXIT -ne 0 ]; then
        echo -e "  ${RED}EXIT $EXIT${NC} (${DURATION}ms)"
        echo "$output" | head -3
    else
        echo -e "  ${GREEN}OK${NC} (${DURATION}ms)"
        # Show first line of response
        echo "$output" | head -1
    fi
done

echo
echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
echo -e "${CYAN}  Done. Set SLERMES_API_KEY in ~/.slermes/.env ${NC}"
echo -e "${CYAN}  for slermes to make live API calls.${NC}"
echo -e "${CYAN}═══════════════════════════════════════════════${NC}"
