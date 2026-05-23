     1|     1|     1|     1|     1|# State — Hermes C Translation (2026-06-01, Session 49)
     2|     2|     2|     2|
     3|     3|     3|     3|**~64% parity — ~319 of ~500 gaps closed.**
     4|     4|     4|     4|
     5|     5|     5|     5|## Dashboard
     6|     6|     6|     6||| Category | Done | % | Notes |
     7|     7|     7|     7|||----------|------|---|-------|
     8|     8|     8|     8||| Core | 12/16 | 75% | Solid |
     9|     9|     9|     9||| Agent | 43/115 | 37% | ~25 SDK-specific not portable |
    10|    10|    10|    10||| CLI | 79/95 | 83% | 79 commands registered |
    11|| Tools | 65/92 | 71% | +fal_common shared lib, refactored image_gen+video_gen |
    12|    12|    12|    12||| Gateway | 22/64 | 34% | 19 platforms, 0 per-platform tests |
| MCP | 10/11 | 91% | +sampling/createMessage handler + YAML config |
    14|    14|    14|    14||| ACP | 9/9 | 100% | +permissions bridge (request_permission, permission_response) |
    15|    15|    15|    15||| Cron | 3/3 | 100% | Done |
    16|    16|    16|    16||| TUI | 5/8 | 63% | +session search filtering |
    17|    17|    17|    17||| Plugins | 10/26 | 38% | 10 .so, 16 to port |
    18|    18|    18|    18||| Config | 4/6 | 67% | 322 keys |
    19|    19|    19|    19||| Build/Doc | 10/11 | 91% | Docker fixed |
    20|    20|    20|    20||| Security | 7/10 | 70% | Sandbox, URL safety, file_safety |
    21|    21|    21|    21||| Provider | 11/18 | 61% | 9 native + metadata |
    22|    22|    22|    22||| Stubs | 10/10 | 100% | ALL stubs resolved |
    23|    23|    23|    23||| Tests | 10/12 | 83% | T01-T09 + library tests |
    24|    24|    24|    24||| CI/CD | 10/10 | 100% | All U gaps closed |
    25|    25|    25|    25||| Upstream | 3/3 | 100% | Secrets ported (secrets.c) |
| **Total** | **~319/500** | **~64%** | **~181 gaps remaining** |
    27|    27|    27|    27|
    28|    28|    28|    28|## Session Log
- **Session 49 (Jun 1):** Ported D80 (fal_common.py) to C as `lib/libfal_common/`. New shared API: `fal_get_api_key()` (key resolution), `fal_escape_json()` (JSON escape), `fal_post_json()` (FAL HTTP POST with auth), `fal_error_response()`/`fal_error_from_http()` (error JSON builders). Refactored `image_gen.c` and `video_gen.c` to remove duplicated FAL API key lookup + HTTP client + error handling. Fixed broken pre-existing `libfal_common` implementation. Tests: 28/28 pass (was SKIP due to compilation failure). Build: 0 errors. Suite: 195/1/0 (pre-existing process_tool). Tools: 65/92 (71%). Parity: ~319/500 (~64%). Commit `963510968`.
- **Session 48 (Jun 1):** Wired MCP sampling/createMessage handler. New `mcp_sampling_handler()` callback — receives MCP server sampling requests, parses JSON messages, calls `llm_chat_completion()`, returns LLM response. Added YAML config reading: `mcp_servers.<name>.sampling.{enabled, model, max_tokens_cap, timeout, max_rpm}`. Wired into all 3 transports (SSE, HTTP, stdio). MCP sector: 10/11 (91%). Parity: ~318/500 (~64%). Build: 0 errors. Commit `248014815`.
- **Session 47 (Jun 1):** Added Streamable HTTP transport to libmcp (MCP_TRANSPORT_HTTP). New API `mcp_server_set_http(srv, url)` — connects to MCP servers via HTTP POST JSON-RPC to a single URL. Full connect/send/recv/disconnect lifecycle with auth headers, workspace roots, and tool discovery. New `connect_http_server()` in mcp_tool.c — wired into config.yaml loading. When a server has `url` but no `command`, defaults to HTTP transport. Explicit `transport: sse` preserves SSE behavior. MCP sector: 9/11 (82%). Parity: ~317/500 (~63%). Build: 0 errors. Commit `b19172342`.
- **Session 46 (Jun 1):** Added dynamic MCP server name discovery from config.yaml. Replaced hardcoded `known_servers[8]` array with `yaml_map_keys()` — new libyaml API that retrieves key names from nested YAML maps. Any custom server name in `mcp_servers: {name: ...}` now works instead of only 8 hardcoded names. Added `yaml_map_keys()` to libyaml.h/c. Verified: custom server name "my-custom-server" discovered correctly. Build: 0 errors. MCP sector: 8/11 (73%). Parity: ~316/500 (~63%). Commit `b2d94148e`.
- **Session 45 (Jun 1):** Added `hermes mcp-serve [port]` CLI subcommand to start MCP HTTP server. mcp_serve.c was compiled but unreachable — `mcp_serve_start()` was never callable. Now `hermes mcp-serve 9104` starts an HTTP server on the given port that serves all 78 Hermes tools over the MCP JSON-RPC protocol. Verified: health endpoint returns OK, initialize returns capabilities, tools/list returns 78 tools. Build: 0 errors. Commit `069b3f9e4`.
    29|- **Session 44 (Jun 1):** Ported `transcription_tools.py` standalone `transcribe_audio` tool. New `registry_init_transcribe()` in voice_mode.c wraps existing `transcribe_audio()` from libtranscribe. Supports Groq, OpenAI, xAI providers. Previously transcription only accessible via voice_listen (mic+transcribe combo). Now `transcribe_audio` tool allows transcribing any audio file. Build: 0 errors. Suite: 195/1/0. Commit `1c1f86ce4`.
    30|    29|- **Session 43 (Jun 1):** Wired mcp_init_all() into main.c startup. MCP server connections from config.yaml `mcp_servers` were never initialized — the function compiled but was never called. Added call to both TUI mode and default CLI mode startup paths. Build: 0 errors. Binary boots clean. MCP sector: 6/11 (55%). Commit `e73e6c51c`.
    31|    30|    29|- **Session 42 (Jun 1):** ACP resource link processing. New `include/acp/resource.h` + `src/acp/resource.c`. `acp_content_to_text()` converts ACP content arrays with resource_link, embedded_resource, and image blocks into inline text. Handles file:// URI → filesystem path (with WSL drive translation), images as data: URIs, text files with [Attached file] headers, binary placeholders. Integrated into `handle_user_message()` in server.c. ACP sector: 9/9 (100%). Build: 0 errors. Suite: 195/1/0 (pre-existing process_tool). Commit `257763257`.
    32|    31|    30|    29|- **Session 41 (Jun 1):** Ported video generation tool (`tools/video_generation_tool.py` → `src/tools/video_gen.c`). New `video_generate` tool using FAL.ai Veo3 REST API. Supports: text-to-video (generate), image-to-video (image_url param), video extend/edit (operation param). Parameters: prompt, operation, aspect_ratio, duration, resolution, image_url, video_url, negative_prompt, audio, seed. Follows same pattern as image_gen.c — HTTP POST to fal.run/fal-ai/veo3 with auth header, extracts video URL from response. Registered in tool_init.c as toolset "video_gen". Build: 0 errors, 0 warnings. Suite: 194/2/0 (pre-existing process_tool + intermittent file_state race condition).
    33|    32|    31|    30|- **Session 39 (Jun 1):** Ported ACP events module (`acp_adapter/events.py` → `src/acp/events.c` + `include/acp/events.h`). Added `tool_event_cb_t` callback type and `tool_event_cb`/`tool_event_data` fields to `agent_state_t` in hermes.h. Instrumented agent_loop.c to fire `tool.started`/`tool.completed`/`tool.failed` events around tool dispatch (Phase 1 / Phase 3). Created ACP notification bridge: `acp_tool_event_cb()` converts tool events into `session_update` notifications (ToolCallStart, ToolCallComplete, ToolCallFailed) over ACP stdio JSON-RPC. Added `acp_build_plan_update_notification()` — translates Hermes todo tool results into ACP plan_update entries for IDE integration (VS Code, Zed). Tool call IDs tracked per (session_id, tool_name) pair with FIFO ordering for parallel dispatch. Wired into ACP server at session creation (handle_new_session, handle_fork_session). ACP sector: 7/9 (~78%) up from 67%. Build: 0 errors, 0 warnings. Suite: 195/1/0 (pre-existing process_tool failure).
    34|    33|    32|    31|- **Session 36 (Jun 1):** Ported `agent/tool_dispatch_helpers.py` (350 lines) to C. New `lib/libtooldispatch/` library with 5 stateless utility functions: `is_destructive_command()` (heuristic for terminal commands that modify files — rm, cp, mv, sed -i, git reset/clean/checkout, output redirects), `extract_error_preview()` (one-line error summary from tool results with JSON error field extraction), `extract_file_mutation_targets()` (file path extraction from write_file/patch args, supports replace mode and V4A multi-file patch headers), `is_multimodal_tool_result()` (multimodal envelope detection), and `paths_overlap()` (filesystem subtree overlap). All functions thread-safe. Verified at runtime: 43/43 tests pass. Build: 0 errors, 0 new warnings. Tools sector: +2 (62/92). Commit `91b2d8735`.
    35|    34|    33|    32|- **Session 35 (Jun 1):** Added WebSocket transport to MCP library (`lib/libmcp/mcp.c/h`). New API `mcp_server_set_websocket()` enables connecting to MCP servers via `ws://` or `wss://` URLs. Verified at runtime: 9/9 tests pass. MCP sector: 5/11 (~45%) up from 36%. Commit `95e07cfdf`.
    36|    35|    34|    33|- **Session 34 (Jun 1):** Added 3 new ACP protocol methods to `src/acp/server.c`: `tools/list` (enumerate all registered tools), `tools/call` (direct tool invocation by name with JSON args), `session/delete` (remove session + free agent state). ACP protocol methods now total 17 (up from 14). Verified at runtime: 8/8 ACP protocol tests pass (build: 0 errors, test binary passes). Parity updated: ACP sector ~5/9 (~56%) up from 22%. Commit `cc447245b`.
    37|    36|    35|    34|- **Session 33 (Jun 1):** Added checkpoint persistence to `process.c` — `proc_save_checkpoint()` serializes running process state to `SLERMES_HOME/processes.json` on every state change (start/kill/signal/cleanup). `proc_load_checkpoint()` restores state on first dispatch after restart, enabling crash recovery for background processes across gateway restarts. Suite: 196/0/0. Build: 0 errors. Commit `c97b59474`.
    38|    37|    36|    35|- **Session 32 (Jun 1):** DA v13 audit — comprehensive parity re-evaluation. All 4 critical stubs verified resolved. Parity revised to ~60% (300/500 gaps). State.md dashboard updated. New `da-audit-v13.md` written.
    39|    38|    37|    36|- **Session 31 (Jun 1):** Added `submit` action to `process.c` — write + newline (Enter key) for interactive CLI prompts. Process tool now supports 11 actions: start, list, cleanup, poll, kill, wait, log, signal, write, submit, close. Suite: 196/0/0. Build: 0 errors. Commit `d886c966d`.
    40|    39|    38|    37|- **Session 30 (Jun 1):** Added `list` and `cleanup` actions to `process.c` — enumerate all processes and remove expired finished processes. Suite: 196/0/0. Commit `503f1bfd9`.
    41|    40|    39|    38|- **Session 29 (May 31):** Ported `agent/onboarding.py` (193 lines) to C. New `hermes_onboarding.h/c` with hint text generators, JSON-based state persistence (onboarding.json), and OpenClaw residue detection. OpenClaw residue check integrated into agent_run_conversation(). Suite: 196/0/0. Build: 0 errors. Parity: ~252/500 (+1).
    42|    41|    40|    39|- **Session 28 (May 31):** Ported `agent/subdirectory_hints.py` (224 lines) to C. Suite: 196/0/0. Parity: ~251/500 (+1). Commit `627fc2d92`.
    43|    42|    41|    40|- **Session 27 (May 31):** Comprehensive gap audit — found all walkway files stale by 35+ gaps. Verified 52 lib modules. Updated state.md.
    44|    43|    42|    41|- **Session 26 (May 29):** Ported `agent/i18n.py` (258 lines) to C. Commit `b38fa84f7`.
    45|    44|    43|    42|- **Session 25 (May 29):** Ported `agent/tool_guardrails.py` (475 lines) to C. Commit `2ea5cfb05`.
    46|    45|    44|    43|- **Sessions 12-24 (May 29):** Fixed 14 CLI commands (stubs→real) + dangling pointer + warnings fix.
    47|    46|    45|    44|
    48|    47|    46|    45|## Build Status
    49|    48|    47|    46|```
    50|    49|    48|    47|Suite:  196/0/0     (~145 test files, ~1200 assertions)
    51|    50|    49|    48|Binary: ~10MB       (dynamic, -O2 -g)
    52|    51|    50|    49|Errors: 0           (make -j$(nproc))
    53|    52|    51|    50|Warnings: ~3        (pre-existing format-truncation in config.c only)
    54|    53|    52|    51|CI:     c-build.yml (Linux x86_64 + Docker)
    55|    54|    53|    52|```
    56|    55|    54|    53|
    57|    56|    55|    54|## Files Created/Modified (Session 29)
    58|    57|    56|    55|- `include/hermes_onboarding.h` — new header (onboarding API)
    59|    58|    57|    56|- `src/agent/onboarding.c` — new source (hint text + JSON state persistence)
    60|    59|    58|    57|- `Makefile` — added `src/agent/onboarding.o` to AGENT_OBJ
    61|    60|    59|    58|- `src/agent/agent_loop.c` — integrated OpenClaw residue check at conversation init
    62|    61|    60|    59|
