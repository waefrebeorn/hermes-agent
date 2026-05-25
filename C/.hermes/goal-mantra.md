# GOAL MANTRA — Hermes C Translation (HONEST)

**Mission:** Translate Hermes Agent Python → C for zero-dependency single-binary operation.

## Current State
- **Binary:** 386KB, compiles with 0 warnings on `make`
- **Source:** 5,973 lines C across 29 `.c` + 9 `.h` files
- **Build:** All 5 phase targets compile (`phase1`–`phase5`/`all`)
- **Gaps:** 394 identified (58 form-not-function, 330 missing, 1 stub, 6 fixed)
- **Target:** Close to ~300 productive gaps, all critical P0 fixed

## Phase Status (HONEST)

| Phase | Claimed | Actual | Real Status |
|-------|---------|--------|-------------|
| 1: Foundation Deps | ✅ | ✅ | JSON, YAML, HTTP, DB, Crypto, Display wrappers work |
| 2: Agent Core | ✅ | 🟧 | Loop runs with tool execution, multi-turn works. No memory, compression, persistence |
| 3: Tools | ✅ | 🟧 | Only 4 tools registered (need 30+). web_search is an alias. No patch/search |
| 4: Gateway | ✅ | 🟧 | Telegram long-poll works minimally. No other platforms. No approval |
| 5: Cron/Advanced | ✅ | 🟥 | Jobs memory-only. cron_list returns "[]" stub. No skill system |

## Priority Queue

### P0 — Fix Now (blocking):
1. ~~Fix tool call loop — agent_loop.c dead code (return before TODO)~~ ✅ DONE
2. ~~Fix auth header — llm_client.c malformed Content-Type~~ ✅ DONE
3. ~~Implement real web_search~~ ✅ DONE (DuckDuckGo Instant Answer API)
4. Implement cron job persistence
5. Write HONEST state.md (replace the lying version)

### P1 — Next Wave:
6. Add session persistence (wire db.c into agent loop)
7. Add remaining 38 slash commands
8. Add remaining 25+ tool implementations
9. Add gateway platform abstraction
10. Build test suite

### P2 — Ongoing:
11. Profile system
12. Credential pool
13. Display spinner/progress/panel
14. Build system improvements (CMake, CI, packaging)

### P3 — Future:
15. Advanced communication features
16. Static/musl/WASM builds
17. Plugin system
18. MCP/ACP server

## Rules
- No Python runtime dependency — single binary
- No time estimates — loop until done
- Every F-N-F gap gets fixed before new features
- Tests before code (where practical)
- HONEST docs always — no aspirational claims