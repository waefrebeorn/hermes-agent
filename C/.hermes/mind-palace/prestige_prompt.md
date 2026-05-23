# WuBu Hermes C — Prestige Prompt (v8 — 2026-05-27)

## Identity
1:1 reimplementation of Python hermes-agent in C. Python maintainers try C — can't tell the difference. Every feature, every handler, every API quirk has a C equivalent. 310 source files, 123K LOC, 9.1MB binary. 158 C-specific commits.

## Core Principles
- Count capability surface, not function count — 74 CLI commands vs Python's 69
- Internal Python helpers ≠ features — 18 C libraries replace Python stdlib
- Cleaner C approach that handles same behavior = parity
- Triple-check own counts. Delegation is lazy for audits. ~WuBu~ strives for more.

---

## Real Current State (2026-05-27, DA v6 verified)

| Category | Status | Key Facts |
|----------|--------|-----------|
| **Config** | ✅ **98%** | 322/322 YAML keys, profiles, `${VAR}`, `!include` |
| **Providers** | ✅ 87% | 9 ops + 27 metadata providers. 7 API quirks remain |
| **MCP** | ✅ **100%** | Transport, tools, resources, subs, sampling, serve |
| **Plugins** | ❌ 19% | 3 .so: kanban/honcho/spotify. 13 remaining |
| **Gateway** | ✅ **100%** | 19 platforms — one more than Python |
| **Tools** | ✅ 95% | 37 files, 67 ops. 6 CDP/plugin-blocked stubs |
| **Agent** | ✅ 86% | Budget, fallback, checkpoint, interrupt, retry |
| **CLI** | ✅ 87% | 74 commands, skin engine, `--json` |
| **Libs** | ⚠️ 38% | 18 libs. libpath + libdatetime ported. 12 to go |
| **Tests** | ⚠️ 63% | 80 files, 2,088 assertions, 116/0/0 |
| **Build/doc** | ✅ 95% | Docker, CI, cross-compile, man page, Doxygen. O02 Windows remains |
| **Error types** | ⚠️ 50% | K01-K05: 18 error codes |
| **Cross-cut** | ✅ **100%** | Token counting, secure paths, key leakage, vendor keys, local trust |

**Overall structural parity: ~60%**
**Full parity (counting plugins + tests): ~40%** — the "feel" gap is real.

## Priority Queue

### P0 — All foundational gaps closed
- temperature=0.0 fix ✅ | response_format UAF fix ✅ | Provider NULL crashes ✅
- Config 322/322 ✅ | MCP 100% ✅ | Gateway 100% ✅

### P1 — Impactful Next Gaps
| # | Area | Est. sessions | Why Now |
|---|------|---------------|---------|
| 1 | Plugin depth (13 backends) | 20 | Biggest structural hole (19%) |
| 2 | Library ports (J06-J17) | 6 | libcsv, libhash, libuuid, libdatetime (done ✅) |
| 3 | Test coverage (+40 files) | 10 | Catch bugs before users do |
| 4 | CLI feel (spinner, autocomplete) | 4 | The "Hermes feel" — spinner, activity feed |

### P2 — Quality + Depth
| # | Area | Notes |
|---|------|-------|
| 5 | Error types K06-K20 | Complete typed error hierarchy |
| 6 | O02 Windows build | MSVC/MinGW detection |
| 7 | Upstream catch-up | ~125 commits behind Python |

### P3 — Stretch
| # | Area | Notes |
|---|------|-------|
| 8 | TUI depth | React Ink equivalent in ncurses |
| 9 | Personality system | Configurable system prompt presets |
| 10 | Achievement system | Gamification (was in Python) |

## Known Blockers
- computer_use — macOS-only (cua-driver), stubbed on WSL
- CDP browser tools — need external CDP server (Camofox/Playwright)
- Plugin depth — 13 backends, each needs config + lifecycle + test
- Most plugin backends need live API tests

## DA Audit History
- v4 (May 15): Initial comprehensive audit — found 400+ gaps
- v5 (May 22): Updated counts — 340 gaps, 55% parity
- v6 (May 27): Triple cross-check vs Python source — 339 gaps, 60% structural parity. Essay + achievements vault created.

## Updated Guidance
Read state.md for latest session log. The DA v6 document has the only verified counts. 
Root README is now authoritative — matches state.md dashboard.
