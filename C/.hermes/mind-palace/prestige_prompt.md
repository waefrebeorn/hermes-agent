# WuBu Hermes C — Prestige Prompt (v10 — 2026-05-23)

## Identity
1:1 reimplementation of Python hermes-agent in C. 147 source files (non-lib), 30 library archives, 10 plugins, 19 gateway platforms, ~148 CLI commands. 396 commits, 183 behind upstream.

## Real Current State (DA May 2026, source-verified counts)

| Category | Status | Key Facts |
|----------|--------|-----------|
| **Config** | ✅ **98%** | ~322 YAML keys, profiles, `${VAR}`, `!include` |
| **Providers** | ✅ **87%** | 9 ops + 10 provider .c files. 7 API quirks remain |
| **MCP** | ✅ **100%** | Transport, tools, resources, prompts, subs, sampling, serve |
| **Plugins** | ✅ **10/10** | 10 .so: honcho, kanban, spotify, disk-cleanup, file-memory, achievements, observability, skills, image_gen, google_meet |
| **Gateway** | ✅ **100%** | 19 platform adapters |
| **Tools** | ✅ **95%** | 28 registered tools. 6 CDP/plugin-blocked stubs |
| **Agent** | ✅ **86%** | Budget, fallback, checkpoint, interrupt, retry |
| **CLI** | ✅ **~148 commands** | Skin engine, spinner, TUI, `--json` output |
| **Libs** | ✅ **100%** | 30 .a archives — all library ports complete |
| **Tests** | ⚠️ **66%** | 116 files, ~573 assertions, **154/0/0 suite** |
| **Build/doc** | ✅ **95%** | Docker, CI, cross-compile, man page, Doxygen |
| **Error types** | ✅ **100%** | K01-K20: 58 error codes |
| **Cross-cut** | ✅ **100%** | Token counting, secure paths, key leakage, vendor keys, local trust |
| **Upstream** | ⚠️ **183 commits behind** | |

## Priority Queue

### P1 — Impactful Next Gaps
| # | Area | Est. effort | Why Now |
|---|------|-------------|---------|
| 1 | Test coverage (116→~160 test files) | Large | Catch bugs before users do |
| 2 | Upstream catch-up (183 commits) | Large | Feature parity erosion |
| 3 | Provider-specific API quirks (7) | Medium | Deep provider coverage |
| 4 | CLI feel (autocomplete, rich formatting) | Medium | User-facing polish |

### P2 — Quality + Depth
| # | Area | Notes |
|---|------|-------|
| 5 | O02 Windows build | MSVC/MinGW detection |
| 6 | CDP browser tools | Unblocked by external CDP server |

### P3 — Stretch
| # | Area | Notes |
|---|------|-------|
| 7 | Personality system | Configurable system prompt presets |

## Known Blockers
- computer_use — macOS-only (cua-driver), stubbed on WSL
- CDP browser tools — need external CDP server (Camofox/Playwright)
- Plugin backends needing live API tests

## Notable Bugfix History
See `vault/bug-bounty.md` for full list (16 bugs).
- temperature=0.0 silent drop (all 9 providers)
- response_format UAF — json_object_set then json_free (all 9 providers)
- NULL stream crash — strncmp before null check (6 providers)
- Config validation NULL SEGV
- cron_job_reset_retry(NULL), increment_retry(NULL) SEGV
- cron_template_instantiate placeholder broken
- title gen 6th word truncation
- x_search auth header — literal `***` instead of `%s`
- Redact heap overflow

## DA Audit History
- v6 (May 27): 339 gaps, ~60% structural parity
- v8 (Jun 1): ~220 gaps, ~69% parity
- v9 (May 23): 447 gaps, 161 closed = ~36% real parity (triple DA source survey)
- v10 (May 23): Docs overhaul, slermes kill, vault restructure, battleship index
