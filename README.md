# WuBu Hermes — Development Workspace

**Location:** `~/hermes-agent-dev/`  
**Tracks:** `wubu/main` (waefrebeorn/slermes fork)  
**Purpose:** Development + C translation of Hermes Agent

This is the development workspace for WuBu Hermes — a fork of NousResearch/hermes-agent
with the long-term goal of translating the entire agent into C for zero-dependency operation.

## Fork Structure

```
waefrebeorn/slermes  (GitHub)
  ├── wubu main                  ← All our commits + upstream. Default branch.
  ├── feat/c-translation         ← C/ directory translation work
  ├── feat/some-feature          ← Individual feature branches (for PRs)
  └── fix/some-bug               ← Individual fix branches (for PRs)
```

## Remotes

| Remote | URL | Direction |
|--------|-----|-----------|
| `origin` | `https://github.com/NousResearch/hermes-agent.git` | Pull upstream updates |
| `wubu` | `git@github.com:waefrebeorn/slermes.git` | Push our work |

## Workflow

### Daily Pull (both Python + C updates)

```bash
git pull wubu main          # Get latest from our fork
python3 C/digest.py          # Check what C work is needed
```

### Making Changes

```bash
git checkout -b feat/my-thing wubu/main   # Branch from our main
# ... make changes (Python + C/ together) ...
git add -A
git commit -m "feat(x): description"
git push wubu feat/my-thing               # Push to our fork
# Open PR on GitHub: feat/my-thing → wubu/main
```

### Upstream Sync

```bash
git fetch origin main                     # Get latest upstream
git merge origin/main                     # Merge into current branch
python3 C/digest.py                       # Run digestion on changes
# Fix any C translation gaps from new upstream code
git push wubu HEAD                        # Push merged result
```

### C Translation

All C work lives in `C/`. See `C/README.md` for full details.

```bash
# Build current C phase
make -C C phase3

# Full build
make -C C

# Run digestion after any git pull
python3 C/digest.py
```

## Key Directories

| Path | Purpose |
|------|---------|
| `C/` | C translation (parallel implementation) |
| `skills/` | Custom WuBu Hermes skills (mind-palace, session-goal-paste, etc.) |
| `tools/` | Tool implementations (security-hardened) |
| `tests/tools/test_path_security.py` | Security test suite (49 tests) |

## Unique Commits (14 on wubu/main)

```
60a91505f  fix(test_openclaw): add pathlib to KNOWN_FALSE_POSITIVES
fed55c6c9  fix(tests): align drain resume_pending tests
9895657ce  test: add path_security test suite (49 tests)
d316ea08a  fix: V4A traversal bypass, regex gaps, search_tool guard
e3ec8a1a1  fix(security): remove leading space in setuid/setgid
7720f7df6  fix(security): fix backtick_subshell regex
4b14fd684  fix(security): add path traversal guards
cbc065880  fix(ci): guard nix-lockfile-fix workflow for upstream
01ad48b0c  fix: resolve npm vulnerabilities
1eb529ce4  Add session-goal-paste skill: templates
b06ae4ed3  Add optimizer-research-2026 skill
71557b4e3  Add session-goal-paste skill
e5171ff4c  Add mind-palace skill
d9fab3636  fix: skip auto-title for local endpoints
```
