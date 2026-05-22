# WuBu Hermes C — Prestige Prompt (v7 — 2026-05-24)

## Identity
1:1 reimplementation of Python hermes-agent in C. Python maintainers try C — can't tell the difference. Every feature, every handler, every API quirk has a C equivalent.

## Core Principles
- Count capability surface, not function count
- Internal Python helpers ≠ features
- Cleaner C approach that handles same behavior = parity
- Triple-check own counts. Delegation is lazy. ~WuBu~ strives for more.

---

## Real Current State (2026-05-24)

| Category | Progress | Key Facts |
|----------|----------|-----------|
| Config | 96% | 322/322 keys, profiles auto-load |
| Providers | 85% | 9 ops, 31 aliases, 18/18 LLM params |
| MCP | 100% ✅ | Transport, tools, resources, subs, sampling, serve |
| Plugins | 14% | 3 real .so: kanban/honcho/spotify |
| Gateway | 100% ✅ | 63 gaps all closed |
| Tools | 95% | 28 reg'd, 1:1 browser/memory/kanban |
| Agent | 86% | G01-G36 agent state all done |
| CLI | 87% | 70 slash commands, skin engine |
| Tests | 56% | 50 files, 2,374 assertions, 88/0/0 ✅ |
| Build/doc | 55% | Docker, CI, cross-compile, man page |
| **Plugins** | **14%** | **Biggest structural gap — 48/51 missing** |
| **Provider APIs** | **~50%** | **25 specific API quirks** |

## Priority Queue

### P0 — None remaining. All done:
- temperature=0.0 fix ✅
- B04-B05: response_format + metadata ✅
- F01-F08: 8 tool stubs → real ✅
- B01-B03: ACP providers ✅

### P1 — Feature Depth (pick highest impact)
| # | Area | Gaps | Est. sessions | Notes |
|---|------|------|---------------|-------|
| 1 | Provider-specific APIs (B22-B46) | 25 | 8 | Per-provider quirks most impactful |
| 2 | Test coverage (M01-M53) | 40 | 10 | Many need integration framework |
| 3 | CLI depth (H01-H34) | 33 | 8 | Slash commands, shell integration |
| 4 | Plugin depth (D01-D51) | 48 | 20 | Huge but 14% done; 45 backends |

### P2 — Quality + Docs
| # | Area | Gaps | Notes |
|---|------|------|-------|
| 5 | Error types | ~20 | 0% — add typed error hierarchy |
| 6 | Build/doc | 10 | Cross-compile, Windows, API docs |
| 7 | Upstream catch-up | 12 | 125 commits behind |
| 8 | Cross-cut depth | 6 | Token counting, secure paths |

### P3 — Porting
| # | Area | Gaps | Notes |
|---|------|------|-------|
| 9 | Python lib ports | 14 | Jinja2, rich, httpx equivalents |

## Known Blockers
- computer_use — macOS-only (cua-driver), platform stubbed on WSL
- CDP browser tools — need external CDP server (Camofox/Playwright)
- memory_sqlite — needs libsqlite3 integration
- Plugin depth — 45 backends, each needs test + config + lifecycle
- Most provider-specific API features need live API keys for testing

## Guidance for next session
Read state.md for latest session log. The 400-gap-mega-roadmap.md is stale from May 22 — don't trust its checkmarks. The state.md dashboard table is the authoritative source.
