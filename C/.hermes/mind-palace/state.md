# State — Hermes C Translation (2026-05-31, Session 28)

**~50% parity — ~250 of ~500 gaps closed (est. battleship v3).**

## Dashboard
| Category | Done | % | Notes |
|----------|------|---|-------|
| Core | 12/16 | 75% | Solid |
| Agent | 42/115 | 37% | subdir_hints ported, tool_guardrails/i18n/provenance/trajectory all ported |
| CLI | 34/95 | 36% | All major slash commands functional |
| Tools | 41/92 | 45% | 68 registered + 52 lib modules compiled in |
| Gateway | 22/64 | 34% | 19 platforms |
| MCP | 3/11 | 27% | libmcp_oauth includes manager (D86) |
| ACP | 1/9 | 11% | Basic server |
| Cron | 3/3 | 100% | Done |
| TUI | 1/8 | 12% | Experimental |
| Plugins | 10/26 | 38% | 10 .so, 16 to port |
| Config | 4/6 | 67% | 322 keys |
| Build/Doc | 9/11 | 82% | Docker fixed |
| Security | 7/10 | 70% | Sandbox, URL safety, file_safety |
| Provider | 11/18 | 61% | 9 native + metadata |
| Stubs | 10/10 | 100% | ALL stubs resolved |
| Tests | 10/12 | 83% | T01-T09 + library tests |
| CI/CD | 10/10 | 100% | All U gaps closed |
| **Total** | **~251/500** | **~50%** | **~249 gaps remaining (est.)** |

## Session Log
- **Session 28 (May 31):** Ported `agent/subdirectory_hints.py` (224 lines) to C. New `hermes_subdir_hints.h/c` with `subdir_hints_init()`, `subdir_hints_check()`, `subdir_hints_cleanup()`. Progressive context discovery: when agent calls tools referencing file paths (read_file, terminal, patch, etc.), checks for AGENTS.md/CLAUDE.md/.cursorrules in the target directories and appends to tool result. Integrated into agent_loop.c Phase 3 tool result processing. Suite: 196/0/0. Build: 0 errors. Parity: ~251/500 (+1).
- **Session 27 (May 31):** Comprehensive gap audit. Found battleship.md, prestige_prompt.md, digest.py all STALE by 35+ gaps. Verified all 52 lib modules exist, all 4 stubs resolved. Updated state.md with accurate numbers. Build: 0 errors, 0 warnings. Suite: 196/0/0.
- **Session 26 (May 29):** Ported `agent/i18n.py` (258 lines) to C. Suite: 196/0/0. Parity: 241/500 (+1). Commit `b38fa84f7`.
- **Session 25 (May 29):** Ported `agent/tool_guardrails.py` (475 lines) to C. Suite: 196/0/0. Parity: 240/500 (+1). Commit `2ea5cfb05`.
- **Session 24 (May 29):** Fixed `/sethome` — was printing without storing. Suite: 195/0/0. Parity: 239/500.
- **Session 23 (May 29):** Fixed `/title` — was printing without storing. Suite: 195/0/0. Parity: 237/500.
- **Session 22 (May 29):** Fixed `/bundles` — was a stub. Suite: 195/0/0. Parity: 235/500.
- **Session 21 (May 29):** Fixed dangling pointer + 3 warnings. **0 errors, 0 warnings.** Suite: 195/0/0. Parity: 233/500.
- **Sessions 12-20 (May 29):** Fixed 12 CLI commands (stubs→real). Suite: 195/0/0. Parity: 216-231/500.

## Build Status
```
Suite:  196/0/0     (~145 test files, ~1200 assertions)
Binary: ~10MB       (dynamic, -O2 -g)
Errors: 0           (make -j$(nproc))
Warnings: ~3        (pre-existing format-truncation in config.c only)
CI:     c-build.yml (Linux x86_64 + Docker)
```

## Files Created/Modified (Session 28)
- `include/hermes_subdir_hints.h` — new header (subdirectory hint discovery API)
- `src/agent/subdir_hints.c` — new source (payload: ~350 lines)
- `Makefile` — added `src/agent/subdir_hints.o` to AGENT_OBJ
- `src/agent/agent_loop.c` — integrated init + check calls
