#!/bin/bash
# release.sh — Hermes C release automation
# Usage: ./release.sh [patch|minor|major]
#   patch  : v0.8.0 → v0.8.1 (default)
#   minor  : v0.8.0 → v0.9.0
#   major  : v0.8.0 → v1.0.0
#
# Steps:
#   1. Check working tree is clean
#   2. Bump version in include/hermes.h
#   3. Build + run full test suite
#   4. Update CHANGELOG.md with new version header
#   5. Create git tag
#   6. Print summary (does NOT push — caller must push --tags)

set -euo pipefail
CDIR="$(cd "$(dirname "$0")/.." && pwd)"

BUMP="${1:-patch}"
case "$BUMP" in
    patch|minor|major) ;;
    *) echo "Usage: $0 [patch|minor|major]"; exit 1 ;;
esac

# 1. Check working tree
if ! git -C "$CDIR" diff --quiet HEAD; then
    echo "ERROR: Working tree has uncommitted changes. Commit or stash first."
    exit 1
fi

# 2. Extract current version from hermes.h
CUR=$(grep -oP 'define HERMES_VERSION\s+"v\K[0-9]+\.[0-9]+\.[0-9]+' "$CDIR/include/hermes.h" | head -1)
if [ -z "$CUR" ]; then
    echo "ERROR: Could not find HERMES_VERSION in include/hermes.h"
    exit 1
fi
echo "Current version: v$CUR"

IFS='.' read -r MAJ MIN PAT <<< "$CUR"
case "$BUMP" in
    patch) PAT=$((PAT + 1)) ;;
    minor) MIN=$((MIN + 1)); PAT=0 ;;
    major) MAJ=$((MAJ + 1)); MIN=0; PAT=0 ;;
esac
NEW="${MAJ}.${MIN}.${PAT}"
echo "New version:     v$NEW"

# 3. Bump version in hermes.h
sed -i "s/define HERMES_VERSION \"v${CUR}/define HERMES_VERSION \"v${NEW}/" "$CDIR/include/hermes.h"
echo "→ Version bumped in include/hermes.h"

# 4. Build
echo "→ Building..."
make -C "$CDIR" -j"$(nproc)" clean 2>/dev/null || true
make -C "$CDIR" -j"$(nproc)" 2>&1 | tail -3

# 5. Run tests
echo "→ Running test suite..."
if ! bash "$CDIR/test_runner.sh" 2>&1 | tail -1 | grep -q "0 failed"; then
    echo "ERROR: Test suite has failures. Release aborted."
    exit 1
fi
TEST_LINE=$(bash "$CDIR/test_runner.sh" 2>&1 | grep "^  Results:")
echo "  $TEST_LINE"

# 6. Update CHANGELOG.md — insert new version entry after header
CHANGELOG="$CDIR/CHANGELOG.md"
if [ -f "$CHANGELOG" ]; then
    DATE=$(date +%Y-%m-%d)
    NEW_ENTRY="## v${NEW} (${DATE})\n\n### Features\n- Release v${NEW}\n\n### Bug Fixes\n- \n\n### Test Suite\n- ${TEST_LINE}\n\n"
    # Insert after first line (the # Changelog header)
    HEADER_LINE=$(head -1 "$CHANGELOG")
    TAIL=$(tail -n +2 "$CHANGELOG")
    printf "%s\n%b%s\n" "$HEADER_LINE" "$NEW_ENTRY" "$TAIL" > "$CHANGELOG"
    echo "→ CHANGELOG.md updated"
else
    echo "→ CHANGELOG.md not found, skipping"
fi

# 7. Commit version bump
git -C "$CDIR" add -A
git -C "$CDIR" commit -m "chore(release): v${NEW}

Version bump: v${CUR} → v${NEW}
${TEST_LINE}"

# 8. Tag
git -C "$CDIR" tag -a "v${NEW}" -m "Hermes C v${NEW}"

echo ""
echo "=== Release v${NEW} ready ==="
echo "Tag created:  v${NEW}"
echo "Commit:       $(git -C "$CDIR" rev-parse HEAD)"
echo "Next step:    git push --tags origin main"
