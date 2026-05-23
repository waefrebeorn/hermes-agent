# State — Hermes C Translation (2026-06-01, Session 38)

**~62% parity — ~306 of ~500 gaps closed.**

## Dashboard
|| Category | Done | % | Notes |
||----------|------|---|-------|
|| Core | 12/16 | 75% | Solid |
|| Agent | 43/115 | 37% | ~25 SDK-specific not portable |
|| CLI | 79/95 | 83% | 79 commands registered |
|| Tools | 62/92 | 67% | +tool_dispatch_helpers |
|| Gateway | 22/64 | 34% | 19 platforms, 0 per-platform tests |
|| MCP | 5/11 | 45% | +WebSocket transport |
|| ACP | 6/9 | 67% | +edit_approval_response, set_auto_approve, edit_proposal |
|| Cron | 3/3 | 100% | Done |
|| TUI | 5/8 | 63% | +session search filtering |
|| Plugins | 10/26 | 38% | 10 .so, 16 to port |
|| Config | 4/6 | 67% | 322 keys |
|| Build/Doc | 10/11 | 91% | Docker fixed |
|| Security | 7/10 | 70% | Sandbox, URL safety, file_safety |
|| Provider | 11/18 | 61% | 9 native + metadata |
|| Stubs | 10/10 | 100% | ALL stubs resolved |
|| Tests | 10/12 | 83% | T01-T09 + library tests |
|| CI/CD | 10/10 | 100% | All U gaps closed |
|| Upstream | 3/3 | 100% | Secrets ported (secrets.c) |
|| **Total** | **~305/500** | **~61%** | **~195 gaps remaining** |

## Session Log
- **Session 38 (Jun 1):** Ported ACP edit_approval module (`acp_adapter/edit_approval.py` → `src/acp/edit_approval.c` + `include/acp/edit_approval.h`). Added 2 new ACP methods: `edit_approval_response` (client resolves pending edit approval) and `set_auto_approve` (per-session auto-approval policy: ask/session/workspace_session). Core logic: `acp_build_edit_proposal()` builds edit proposals for write_file/patch tools, `acp_should_auto_approve_edit()` checks auto-approve policies with sensitive-path detection (.git/, .ssh/, .env, id_rsa), `acp_is_sensitive_path()` blocks auto-approve on security-sensitive files. Integrated into `handle_tools_call()` in server.c — when a file-mutation tool call is detected, sends `edit_proposal` notification to client and returns error code -32998. Client sends `edit_approval_response` to approve/deny, with cached approval consumed on retry. ACP sector: 6/9 (~67%) up from 56%. Build: 0 errors, 0 warnings. Suite: 195/1/0 (pre-existing process_tool failure).
- **Session 36 (Jun 1):** Ported `agent/tool_dispatch_helpers.py` (350 lines) to C. New `lib/libtooldispatch/` library with 5 stateless utility functions: `is_destructive_command()` (heuristic for terminal commands that modify files — rm, cp, mv, sed -i, git reset/clean/checkout, output redirects), `extract_error_preview()` (one-line error summary from tool results with JSON error field extraction), `extract_file_mutation_targets()` (file path extraction from write_file/patch args, supports replace mode and V4A multi-file patch headers), `is_multimodal_tool_result()` (multimodal envelope detection), and `paths_overlap()` (filesystem subtree overlap). All functions thread-safe. Verified at runtime: 43/43 tests pass. Build: 0 errors, 0 new warnings. Tools sector: +2 (62/92). Commit `91b2d8735`.
- **Session 35 (Jun 1):** Added WebSocket transport to MCP library (`lib/libmcp/mcp.c/h`). New API `mcp_server_set_websocket()` enables connecting to MCP servers via `ws://` or `wss://` URLs. Verified at runtime: 9/9 tests pass. MCP sector: 5/11 (~45%) up from 36%. Commit `95e07cfdf`.
- **Session 34 (Jun 1):** Added 3 new ACP protocol methods to `src/acp/server.c`: `tools/list` (enumerate all registered tools), `tools/call` (direct tool invocation by name with JSON args), `session/delete` (remove session + free agent state). ACP protocol methods now total 17 (up from 14). Verified at runtime: 8/8 ACP protocol tests pass (build: 0 errors, test binary passes). Parity updated: ACP sector ~5/9 (~56%) up from 22%. Commit `cc447245b`.
- **Session 33 (Jun 1):** Added checkpoint persistence to `process.c` — `proc_save_checkpoint()` serializes running process state to `SLERMES_HOME/processes.json` on every state change (start/kill/signal/cleanup). `proc_load_checkpoint()` restores state on first dispatch after restart, enabling crash recovery for background processes across gateway restarts. Suite: 196/0/0. Build: 0 errors. Commit `c97b59474`.
- **Session 32 (Jun 1):** DA v13 audit — comprehensive parity re-evaluation. All 4 critical stubs verified resolved. Parity revised to ~60% (300/500 gaps). State.md dashboard updated. New `da-audit-v13.md` written.
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
