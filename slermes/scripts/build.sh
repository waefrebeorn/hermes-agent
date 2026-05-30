#!/usr/bin/env bash
# build.sh — WuBu Hermes C build helper
set -euo pipefail

cd "$(dirname "$0")/.."

PHASE="${1:-5}"

echo "═══ Building WuBu Hermes C — Phase ${PHASE} ═══"
case "$PHASE" in
  1) make phase1 ;;
  2) make phase2 ;;
  3) make phase3 ;;
  4) make phase4 ;;
  5) make clean && make ;;
  clean) make clean ;;
  test) make test ;;
  *) echo "Usage: $0 [1|2|3|4|5|clean|test]" && exit 1 ;;
esac

echo "═══ Done ═══"
