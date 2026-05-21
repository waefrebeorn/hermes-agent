# WuBu Hermes — Development Workspace

**Location:** `~/hermes-agent-dev/`  
**Tracks:** `wubu/main` (waefrebeorn/hermes-agent fork)  
**Purpose:** Development + C translation of Hermes Agent → **slermes**

---

## Slermes C Translation Dashboard

| Metric | Value |
|--------|-------|
| **Phase** | Phase 5 (Advanced) — 170-phase roadmap |
| **Binary** | `./C/hermes` (alias `slermes`) — ~809KB |
| **Build** | `make -C C` — 0 errors, ~8 pre-existing warnings |
| **Tests** | 21/21 pass (10 lib + 2 plugin + 9 integration) |
| **Tools** | 27 (18 core + 4 browser + security + provider framework) |
| **CLI commands** | 16 (registry-dispatched, target: 50+) |
| **Gateway** | 7 platforms (Telegram, Discord, Slack, Matrix, Mattermost, Webhook, WhatsApp) |
| **Live API** | DeepSeek `deepseek-v4-flash` via `api.deepseek.com` — ~1.2s avg |
| **Config** | `~/.slermes/` (SLERMES_HOME) — isolated from Python hermes |
| **Bugs fixed** | 5 (Content-Type header, chunked TE, skin default, .env parser, WEBHOOK_PORT) |

```bash
# Quick start
slermes "Say hello"           # Run C binary
hermes -z "Say hello"         # Run Python prod side-by-side
bash scripts/slermes-build.sh # Build + test + parity check
```

**Full roadmap:** `ROADMAP.md`  
**Mind palace:** `C/.hermes/mind-palace/`  
**170-phase plan:** `C/.hermes/mind-palace/plans/roadmap-100-phases.md`

---

## Fork Structure

```
waefrebeorn/hermes-agent  (GitHub)
  ├── wubu main                  ← All our commits + upstream. Default branch.
  ├── feat/c-translation         ← C/ directory translation work
  ├── feat/some-feature          ← Individual feature branches (for PRs)
  └── fix/some-bug               ← Individual fix branches (for PRs)
```

## Remotes

| Remote | URL | Direction |
|--------|-----|-----------|
| `origin` | `https://github.com/NousResearch/hermes-agent.git` | Pull upstream updates |
| `wubu` | `git@github.com:waefrebeorn/hermes-agent.git` | Push our work |

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

### C Translation (slermes)

All C work lives in `C/`. The binary is aliased as `slermes`.

```bash
# Build + test + parity (recommended)
bash scripts/slermes-build.sh

# Or step by step:
make -C C              # Build C translation
bash C/test_runner.sh  # Run 21 tests
slermes "Say hi"       # Quick smoke test

# Sync from upstream:
bash scripts/slermes-sync.sh --merge
```

See `ROADMAP.md` for the full C translation status and 170-phase plan.

## Workflow Scripts

| Script | Purpose |
|--------|---------|
| `scripts/slermes-sync.sh` | Fetch upstream + merge + run digest + report C gaps |
| `scripts/slermes-build.sh` | Clean build + test + parity check + deploy to PATH |
| `parity_loop.sh` | Verify C/ ↔ slermes/ 1:1 parity |
| `triple_test.sh` | Compare hermes + hermes-dev + slermes side-by-side |

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
