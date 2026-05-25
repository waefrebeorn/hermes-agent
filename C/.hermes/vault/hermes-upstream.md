# Upstream Hermes — Vault Reference

> **What the Python Hermes Agent provides that C must translate.**
> Target for C translation completeness.

## Version Reference
- **Python Hermes:** v0.14.0 (commit 6c5ca8441, May 23 2026)
- **C Translation:** v0.14.0-wubu (5,973 LOC)
- **Upstream:** NousResearch/hermes-agent (git@github.com:NousResearch/hermes-agent.git)

## Python Source Structure (What C Must Cover)

| Python Area | LOC (approx) | C Status | Notes |
|-------------|-------------|----------|-------|
| Agent loop (`run_agent.py`) | 12K | 🟧 218 lines | Tool calling broken |
| CLI (`cli.py`) | 11K | 🟧 156 lines | 4/50+ commands |
| Tools (30+ files) | 8K | 🟧 719 lines | 4/30+ tools |
| Gateway (server + 15 platforms) | 6K | 🟧 351 lines | Only Telegram |
| Cron (scheduler + jobs) | 2K | 🟥 223 lines | Memory-only |
| Provider adapters (20+ providers) | 3K | ⬜ 0 lines | Only OpenAI format |
| Plugin system | 2K | ⬜ 0 lines | Not started |
| Profile system | 1K | ⬜ 0 lines | Not started |
| MCP server | 1K | ⬜ 0 lines | Not started |
| ACP server | 1K | ⬜ 0 lines | Not started |
| Skin engine | 1K | ⬜ 0 lines | Not started |
| Testing (900+ files, 17K tests) | 20K | ⬜ 2 files | Smoke only |
| **Total** | **~68K** | **5,973 LOC** | **9% coverage** |

## Python→C Migration Work Log

### Phase 1: Foundation ✅ (Apr 29 - May 3)
- Ported JSON parser from cJSON concepts
- Wrote minimal YAML reader (config subset only)
- Built raw socket + OpenSSL HTTP client
- Implemented SHA-256, HMAC, base64, PKCE
- File-based session store (no SQLite dependency)

### Phase 2: Agent Core 🟧 (May 4 - May 10)
- Agent loop with message flow
- LLM client for OpenAI chat completions
- Context management (push/pop/clear/truncate)
- Title generation (extractive)
- Config loading (YAML + .env)
- CLI (fgets-based interactive loop)
- 4 slash commands implemented
- Display (ANSI color wrappers)

### Phase 3: Tools 🟧 (May 11 - May 15)
- Tool registry (register/find/dispatch)
- Terminal (popen + timeout)
- File (read/write/search schemas only)
- Web (HTTP GET only)
- Skills (list only)
- Tool init (4 tools registered)

### Phase 4: Gateway 🟧 (May 16 - May 18)
- Telegram long-poll server
- Message send/receive
- Response chunking (4K limit)
- No platform abstraction

### Phase 5: Cron/Advanced 🟥 (May 19 - May 22)
- Crontab schedule parser
- In-memory job list
- `system()` execution
- Jobs file is a stub

### Provider/Auth 🟧 (May 20 - May 23)
- Full PKCE OAuth token exchange (xAI, MiniMax, generic)
- Auth store (auth.json CRUD)
- Not wired into CLI

## Upstream Changes Since Fork Start

From git log since April 29 (first C commit):
```
04a616589 → C scaffold + digestion
e8530c40e → Phase 1
c257aa12f → Phase 2
b2011e736 → Phase 3
575e21fac → Phase 4
6c5ca8441 → Phase 5
```

Upstream Python has progressed concurrently with fixes/test additions:
```
78f6003e4 → fix(test_openclaw): add pathlib_file_write...
fed55c6c9 → fix(tests): align drain resume_pending...
9895657ce → test: add path_security test suite (49 tests)
```

## Translation Gap Analysis

The C translation covers ~9% of the Python Hermes codebase. The 436 remaining gaps represent ~91% of the work. The critical insight: Python has been continuously developed (new features, bug fixes, tests) while C translates the April 29 snapshot. Every git pull needs digestion to catch C up with Python changes.

## Recommended Upstream Sync Cadence
1. `git pull wubu main` weekly
2. Run `python3 C/digest.py`
3. Apply digestion-generated stubs
4. Fix P0 gaps first
5. Progress phases in order