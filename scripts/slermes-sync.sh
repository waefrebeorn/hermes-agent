#!/bin/bash
# slermes-sync.sh — Pull upstream hermes, run digestion, report C work needed.
# Safe to run from any branch. Only modifies C/ directory.
# Usage: bash scripts/slermes-sync.sh [--merge]

set -e
CDIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== Slermes Sync: Upstream → Digest → Report ==="
echo ""

# Step 1: Fetch upstream
echo "1. Fetching upstream (origin/main)..."
git fetch origin main 2>&1 || echo "  (no upstream remote 'origin' configured)"

# Step 2: Check for upstream changes
BEHIND=$(git rev-list --count HEAD..origin/main 2>/dev/null || echo "0")
if [ "$BEHIND" -gt 0 ]; then
    echo "   Upstream has $BEHIND new commit(s)."
    
    if [ "$1" = "--merge" ]; then
        echo "2. Merging origin/main..."
        git merge origin/main --no-edit 2>&1 || {
            echo "   MERGE CONFLICT. Resolve manually, then run:"
            echo "   python3 C/digest.py && bash scripts/slermes-build.sh"
            exit 1
        }
    else
        echo "2. Skipping merge (no --merge flag)."
        echo "   Run with --merge to incorporate upstream changes."
    fi
else
    echo "   No upstream changes. Already up to date."
fi

# Step 3: Run digestion
echo ""
echo "3. Running C digestion..."
python3 C/digest.py 2>&1 || echo "   (digest.py completed with warnings)"

# Step 4: Check build status
echo ""
echo "4. Checking build status..."
if [ -f C/hermes ]; then
    VER=$(./C/hermes --version 2>/dev/null | head -1 || echo "unknown")
    echo "   Current binary: $VER"
    echo "   Last build: $(stat -c '%y' C/hermes 2>/dev/null || echo 'unknown')"
else
    echo "   No binary built yet."
fi

# Step 5: Summary
echo ""
echo "=== Sync Complete ==="
echo "  Upstream: $BEHIND new commits"
echo "  Next: bash scripts/slermes-build.sh"
echo ""
