# State — Hermes C Translation (2026-06-01, Session 32)

**~60% parity — ~300 of ~500 gaps closed (DA v13).**

## Dashboard
|| Category | Done | % | Notes |
||----------|------|---|-------|
|| Core | 12/16 | 75% | Solid |
|| Agent | 43/115 | 37% | ~25 SDK-specific not portable |
|| CLI | 79/95 | 83% | 79 commands registered |
|| Tools | 60/92 | 65% | 38 source + 44 lib modules |
|| Gateway | 22/64 | 34% | 19 platforms, 0 per-platform tests |
|| MCP | 4/11 | 36% | libmcp + libmcp_oauth + mcp_serve |
|| ACP | 2/9 | 22% | Basic JSON-RPC server |
|| Cron | 3/3 | 100% | Done |
|| TUI | 4/8 | 50% | 2865-line ncurses impl |
|| Plugins | 10/26 | 38% | 10 .so, 16 to port |
|| Config | 4/6 | 67% | 322 keys |
|| Build/Doc | 10/11 | 91% | Docker fixed |
|| Security | 7/10 | 70% | Sandbox, URL safety, file_safety |
|| Provider | 11/18 | 61% | 9 native + metadata |
|| Stubs | 10/10 | 100% | ALL stubs resolved |
|| Tests | 10/12 | 83% | T01-T09 + library tests |
|| CI/CD | 10/10 | 100% | All U gaps closed |
|| Upstream | 3/3 | 100% | Secrets ported (secrets.c) |
|| **Total** | **~300/500** | **~60%** | **~200 gaps remaining (DA v13)** |

## Session Log
- **Session 31 (Jun 1):** Added `submit` action to `process.c` — write + newline (Enter key) for interactive CLI prompts. Process tool now supports 11 actions: start, list, cleanup, poll, kill, wait, log, signal, write, submit, close. Suite: 196/0/0. Build: 0 errors. Commit `d886c966d`.
- **Session 30 (Jun 1):** Added `list` and `cleanup` actions to `process.c` — enumerate all processes and remove expired finished processes. Suite: 196/0/0. Commit `503f1bfd9`.
- **Session 29 (May 31):** Ported `agent/onboarding.py` (193 lines) to C. New `hermes_onboarding.h/c` with hint text generators, JSON-based state persistence (onboarding.json), and OpenClaw residue detection. OpenClaw residue check integrated into agent_run_conversation(). Suite: 196/0/0. Build: 0 errors. Parity: ~252/500 (+1).
- **Session 28 (May 31):** Ported `agent/subdirectory_hints.py` (224 lines) to C. Suite: 196/0/0. Parity: ~251/500 (+1). Commit `627fc2d92`.
- **Session 27 (May 31):** Comprehensive gap audit — found all walkway files stale by 35+ gaps. Verified 52 lib modules. Updated state.md.
- **Session 26 (May 29):** Ported `agent/i18n.py` (258 lines) to C. Commit `b38fa84f7`.
- **Session 25 (May 29):** Ported `agent/tool_guardrails.py` (475 lines) to C. Commit `2ea5cfb05`.
- **Sessions 12-24 (May 29):** Fixed 14 CLI commands (stubs→real) + dangling pointer + warnings fix.

## Build Status
```
Suite:  196/0/0     (~145 test files, ~1200 assertions)
Binary: ~10MB       (dynamic, -O2 -g)
Errors: 0           (make -j$(nproc))
Warnings: ~3        (pre-existing format-truncation in config.c only)
CI:     c-build.yml (Linux x86_64 + Docker)
```

## Files Created/Modified (Session 29)
- `include/hermes_onboarding.h` — new header (onboarding API)
- `src/agent/onboarding.c` — new source (hint text + JSON state persistence)
- `Makefile` — added `src/agent/onboarding.o` to AGENT_OBJ
- `src/agent/agent_loop.c` — integrated OpenClaw residue check at conversation init
