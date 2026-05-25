     1|# Hermes C — Fresh Battleship (v8 — Triple DA Verified)
     2|
     3|Generated 2026-05-31 by systematic Triple DA audit: stub hunt (placeholder, TODO, FIXME, stub, scaffolding, "for brevity", "for later", "not yet"), Python-vs-C module comparison, tool depth analysis, upstream scan. All prior stale claims retired to vault.
     4|
     5|Total: **110 active gaps** across 22 sectors (resolved items retired to vault Phase 57+).
     6|
     7|## SECTOR 1: Confirmed Stubs (0 gaps)
     8|
     9|All prior plugin stubs resolved. Remaining gaps in other sectors.
    10|
    11|Functions that return fake data or error. Implementation required. — All resolved (v8→v9).
    12|
    13|## SECTOR 2: Placeholder / "For Later" / Unwired Infrastructure (11 gaps)
    14|
    15|Code patterns found by stub hunt: `placeholder`, `for future`, `no-op`, `not supported`, `in future`, `for extension`.
    16|
    17|| # | ID | File:Line | Issue | LOC | Priority |
    18||---|----|-----------|-------|-----|----------|
    19|| 1 | P01 | telegram.c:949 | Editable draft with placeholder text — stub UX behavior | 20 | P3 |
    20|| 2 | P02 | mcp_tool.c:1285 | Placeholder auth entry for future server connections | 15 | P3 |
    21|| 3 | P03 | memory.c:544-549 | Plugin memory save/load — no-op returns true | 10 | P3 |
    22|| 4 | P04 | cron/cron_extras.c:319 | Cron placeholder replacement ({{param}} system incomplete) | 30 | P3 |
    23|| 5 | P05 | api_server.c:219 | "In future, use g_agent to dispatch" — incomplete routing | 200 | P2 |
    24|| 6 | P06 | qqbot.c:77 | "Reserved for future API mode" — never wired | 25 | P3 |
    25|| 7 | P08 | server.c:96 | Default gateway port unchanged from 8080 — no config read | 10 | P3 |
    26|| 8 | P09 | credential.c:432 | (void)cd->old_name placeholder for unused field | 5 | P3 |
    27|| 9 | P11 | commands.c:2002 | cmd_agents — "No active subagents" message, always | 5 | P3 |
    28|| 10 | P13 | commands.c:2239 | cmd_restart — "Use /exit and re-launch" message | 5 | P3 |
    29|| 11 | P14 | commands.c:1696 | cmd_background — "background mode not available" | 5 | P3 |
    30|
    31|## SECTOR 3: Dead Code / Unused Functions (12 gaps)
    32|
    33|Functions fully implemented but never called. Wire or remove.
    34|
    35|| # | ID | File:Line | Function | LOC | Priority |
    36||---|----|-----------|----------|-----|----------|
    37|| 1 | W02 | qqbot.c:78 | post_api — __attribute__((unused)) | 25 | P3 |
    38|| 2 | W03 | feishu.c:506 | feishu_upload_image — implements, never called | 40 | P3 |
    39|| 3 | W04 | tui_fullscreen.c:184 | tui_alloc_pair — unused | 20 | P3 |
    40|| 4 | W05 | tui_fullscreen.c:835 | tui_wprint_role — unused | 30 | P3 |
    41|| 5 | W06 | tui_fullscreen.c:1947 | tui_display_image_sixel — unused | 35 | P3 |
    42|| 6 | W07 | tui_fullscreen.c:1984 | tui_display_image_kitty — unused | 50 | P3 |
    43|| 7 | W08 | tui_fullscreen.c:2035 | tui_display_image — unused orchestrator | 30 | P3 |
    44|| 8 | W11 | memory.c:1412 | plugin_delete — returns false, never used | 20 | P3 |
    45|| 10 | W12 | server.c:96 | Default port 8080 (unchanged) | 10 | P3 |
    46|| 11 | W14 | feishu.c:506 | Image upload implemented but unwired (dup) | 40 | P3 |
    47|| 12 | W15 | cli/config.c:325 | Fragmented config — "Handled by get_slermes_home()" | 30 | P3 |
    48|| 13 | W16 | context_engine.c:91/100 | Default on_session_start/end = noop | 10 | P3 |
    49|
    50|## SECTOR 4: Missing Agent Modules — Truly Unported (10 gaps)
    51|
    52|Python agent modules with NO C equivalent at all. Not merged, not aliased.
    53|
    54|| # | ID | Python Module | Key Functionality | LOC (Python) | Priority |
    55||---|----|--------------|-------------------|--------------|----------|
    56|| 1 | A35 | background_review | Background review agent | 587 | P2 |
    57|| 2 | A01 | insights | Full session insights engine (931 LOC Python) | 931 | P1 |
    58|| 3 | A11 | curator_backup | Curator backup state management | 150 | P2 |
    59|| 4 | A25 | process_bootstrap | Subprocess bootstrap helpers | 167 | P2 |
    60|| 5 | A31 | title_generator | Session title generation | 150 | P2 |
    61|| 6 | A33 | tool_executor | Tool execution orchestration | 350 | P2 |
    62|| 7 | A36 | agent_runtime_helpers | Runtime state management helpers | 120 | P3 |
    63|| 8 | A37 | async_utils | Async coroutine utilities (N/A for C) | 80 | P3 |
    64|| 9 | A41 | display | CLI spinner/banner/UI rendering (C has inline, no standalone module) | 500 | P2 |
    65|| 10 | A10 | credential_sources | Multi-source credential resolution | 200 | P2 |
    66|| 11 | A07 | codex_responses_adapter | OpenAI Codex Responses API adapter | 300 | P3 |
    67|
## SECTOR 5: Agent Modules — Partial Merge Depth (11 gaps)

Python modules where C has a partial port merged into another file, missing significant features.

| # | ID | Python Module | C Location | Missing Features | Priority |
|---|----|--------------|-----------|-----------------|----------|
| 1 | M02 | bedrock_adapter | provider_bedrock.c | C has SigV4 + Converse, missing: streaming response handling, model-specific quirks | P2 |
| 2 | M03 | gemini_native_adapter | provider_google.c | C has parts/functionCall, missing: inlineContext caching, Google-native streaming format | P2 |
| 3 | M04 | google_oauth | (none) | No Google OAuth flow — needed for GCP services, Calendar, Drive | P2 |
| 4 | M05 | gemini_schema | agent/gemini_schema.c | C port exists, missing: Google Cloud Code schema extensions | P3 |
| 5 | M06 | memory_manager | tools/memory.c | C has store/search/delete, missing: multi-backend orchestration, priority ranking | P2 |
| 6 | M07 | memory_provider | plugins/plugin_*.c | C has 5 plugin backends, missing: unified memory_provider interface with auto-fallback | P2 |
| 7 | M08 | message_sanitization | (merged into various) | C sanitizes per-platform inline, missing: centralized sanitize table | P3 |
| 8 | M10 | models_dev | (none) | No dev model configuration (staging/test provider endpoints) | P3 |
| 9 | M11 | plugin_llm | (none) | No LLM calls from plugins — plugins can't make provider calls | P2 |
| 10 | M14 | conversation_loop | agent/agent_loop.c + llm_client.c | C has the loop split across files, missing: unified loop with interrupt coordination | P2 |
| 11 | M15 | azure_identity_adapter | provider_azure.c | C has Azure API, missing: managed identity auth, token refresh | P3 |
    89|
    90|## SECTOR 6: Missing Subdirectory Modules (22 gaps)
    91|
    92|Python subdirectory modules with NO C equivalent.
    93|
    94|| # | ID | Python Module | Key Functionality | Priority |
    95||---|----|--------------|-------------------|----------|
    96|| 1 | T01 | environments/local.py | Local terminal backend (C has inline in terminal.c) | P2 |
    97|| 2 | T02 | environments/docker.py | Docker terminal backend | P2 |
    98|| 3 | T03 | environments/ssh.py | SSH terminal backend | P2 |
    99|| 4 | T04 | environments/modal.py | Modal terminal backend | P2 |
   100|| 5 | T05 | environments/daytona.py | Daytona terminal backend | P2 |
   101|| 6 | T06 | environments/singularity.py | Singularity terminal backend | P2 |
   102|| 7 | T07 | environments/vercel_sandbox.py | Vercel sandbox backend | P3 |
   103|| 8 | T08 | environments/managed_modal.py | Managed modal backend | P3 |
   104|| 9 | T09 | environments/base.py | Environment base class | P2 |
   105|| 10 | T10 | environments/file_sync.py | File sync for environments | P3 |
   106|| 11 | T11 | environments/modal_utils.py | Modal utility functions | P3 |
   107|| 12 | T12 | computer_use/backend.py | Computer use backend abstraction (C has inline) | P2 |
   108|| 13 | T13 | computer_use/cua_backend.py | CUA (Computer Use Agent) backend | P2 |
   109|| 14 | T14 | computer_use/schema.py | Computer use schema definitions (C has schema inline) | P2 |
   110|| 15 | T15 | computer_use/tool.py | Computer use tool orchestration | P2 |
   111|| 16 | T16 | computer_use/vision_routing.py | Vision routing for computer use | P2 |
   112|| 17 | T21 | hermes_tools_mcp_server.py | Hermes MCP server transport | P3 |
   113|| 18 | T22 | transports/types.py | Transport type definitions | P3 |
   114|| 19 | T25 | lsp/* (8 files) | LSP protocol client for IDE integration | P3 |
   115|| 20 | T26 | codex_app_server_session.py | Codex session transport | P3 |
   116|| 21 | T27 | codex_event_projector.py | Codex event projector | P3 |
   117|| 22 | T28 | codex_app_server.py | Codex app server transport | P3 |
   118|
   119|## SECTOR 7: Missing Tool Features — Depth Gaps (1 gap)
   120|
   121|Existing C tools missing features that Python has.
   122|
   123|| # | ID | Tool | Missing Feature | LOC | Priority |
   124||---|-----|------|----------------|-----|----------|
   125|| 1 | D07 | terminal.c | ✅ Modal backend added: run_command_modal() wraps commands via `modal run`, schema mentions 'modal' backend option | P3 |
   126|| 2 | D08 | terminal.c | ✅ file_sync library (239+166 test) — collect, mkdir, upload_all | P3 |
   127|| 3 | D10 | computer_use.c | ✅ Backend registry: cu_register_backend/cu_list_backends/cu_clear_backends, CU_BACKEND env override, 5 backends auto-registered | P2 |
   128|| 4 | D11 | computer_use.c | ✅ Vision routing: vision→som fallback with notification, `vision_fallback` flag in capture response | P2 |
   129|| 5 | D14 | browser.c | ✅ Browser supervisor: cdp_supervisor_ping(), Browser.getVersion health check, connection state/command stats tracking, browser_supervisor tool registered | P2 |
   130|| 6 | D15 | browser.c | ✅ Camofox session save/load/delete to <home>/browser_auth/camofox/sessions/<task>.json | P2 |
   131|| 7 | D23 | web.c | Web search provider abstraction — ✅ STALE: web_search_registry.c (239+217 test) | 0 | P2 |
   132|
## SECTOR 8: Gateway Platform Depth (10 gaps)

C gateways with minimal or incomplete implementations vs Python.

| # | ID | Platform | Missing Feature | LOC | Priority |
|---|---------|----------|----------------|-----|----------|
| 1 | G03 | qqbot | post_api marked __attribute__((unused)) dead code | 25 | P3 |
| 2 | G08 | mattermost | One-channel only (no multi-channel support) | 200 | P2 |
| 3 | G12 | whatsapp | Interactive buttons/templates | 80 | P2 |
| 4 | G13 | matrix | End-to-end encryption | 200 | P3 |
| 5 | G17 | email | Multi-account support | 100 | P2 |
| 6 | G21 | yuanbao | Group management commands | 100 | P2 |
| 7 | G22 | platform/* | Missing 10 gateway platforms from Python | 3000 | P2 |
| 8 | G23 | platform/* | No codex_response platform | 150 | P3 |
| 9 | G24 | platform/* | No webhook_server platform | 200 | P3 |
| 10 | G25 | platform/* | Unified platform registration (matching Python's GATEWAY_PLATFORMS) | 200 | P2 |
   154|
   155|## SECTOR 9: Configuration & Environment (0 gaps)
   156|
   157|Missing config keys, env vars, or settings that Python handles but C doesn't.
   158|
   159|| # | ID | Config Key | Description | LOC | Priority |
   160||---|-----|-----------|-------------|-----|----------|
   161|| 1 | C08 | agent.codex_runtime | ✅ Config key added: `codex_runtime` (auto | codex_app_server), default="auto", reads from agent.codex_runtime YAML path | P3 |
   162|| 2 | C11 | agent.mixture_of_agents | ✅ Config keys added: moa_enabled, moa_model, moa_strategy, moa_workers — YAML path agent.mixture_of_agents.* | P3 |
   163|| 3 | C17 | agent.checkpoint.* | ✅ Already implemented — struct has 8 fields at hermes.h:846-854, defaults + YAML reader at config.c:490-496,1515-1526,2084 | P3 |
   164|
   165|## SECTOR 10: Library Depth (6 gaps)
   166|
   167|Libraries ported but missing features.
   168|
   169|| # | ID | Library | Missing Feature | LOC | Priority |
   170||---|--------|---------|----------------|-----|----------|
   171|| 1 | L01 | libhttp | HTTP/2 support | 500 | P3 |
   172|| 2 | L09 | libjson | JSON schema validation | 200 | P3 |
   173|| 3 | L10 | libjson | Streaming parse | 150 | P3 |
   174|| 4 | L13 | libcrypto | RSA key generation | 150 | P3 |
   175|| 5 | L22 | libdb | Batch operations | 40 | P3 |
   176|| 6 | L23 | libregex | Full PCRE support (not just POSIX) | 500 | P3 |
   177|
   178|## SECTOR 11: Bug Fixes & Known Issues (2 gaps)
   179|
   180|Confirmed bugs that need fixing.
   181|
   182|| # | ID | Bug | File | LOC | Priority |
   183||---|-----|-----|------|-----|----------|
   184|| 1 | B02 | Suite 237→262 gap (25 tests missing vs Python coverage) | test_runner.sh | 25 | P2 |
   185|| 2 | B04 | No ANSI color on Windows terminals | display.c | 30 | P3 |
   186|
   187|## SECTOR 12: Test Coverage (25 entries — 23 exist, 2 missing)
   188|
   189|C tools with test file status.
   190|
   191|| # | ID | Module | Test File | Priority |
   192||---|-------|--------|-----------|----------|
   193|| 1 | T01 | api_helpers.c | ✅ test_api_helpers.c exists | P2 |
   194|| 2 | T02 | approve.c | ✅ test_approve.c (34 tests) | P2 |
   195|| 3 | T03 | clarify.c | ✅ test_clarify.c (8 tests) | P2 |
   196|| 4 | T04 | cronjob.c | ✅ test_cronjob.c exists (has 23 tests elsewhere) | P2 |
   197|| 5 | T05 | delegate.c | ✅ test_delegate.c (4 tests) — in test_runner as delegate_tool | P2 |
   198|| 6 | T06 | discord.c | ❌ no test (token gate prevents validation testing) | P2 |
   199|| 7 | T07 | exec_code.c | ✅ test_exec_code.c exists (partial) | P2 |
   200|| 8 | T08 | file.c | ✅ test_file.c exists | P2 |
   201|| 9 | T09 | file_batch.c | ✅ test_file_batch.c exists | P2 |
   202|| 10 | T10 | homeassistant.c | ✅ test_homeassistant.c exists | P2 |
   203|| 11 | T11 | image_gen.c | ✅ test_image_gen.c (8 tests) | P2 |
   204|| 12 | T12 | kanban.c | ✅ test_kanban.c exists | P2 |
   205|| 13 | T13 | mcp_tool.c | ❌ no test (complex, 1300+ LOC) | P2 |
   206|| 14 | T14 | memory.c | ✅ test_memory.c exists | P2 |
   207|| 15 | T15 | patch.c | ✅ test_patch.c exists | P2 |
   208|| 16 | T16 | process.c | ✅ test_process.c exists | P2 |
   209|| 17 | T17 | send_message.c | ✅ test_send_message.c exists | P2 |
   210|| 18 | T18 | session_search.c | ✅ test_session_search.c exists | P2 |
   211|| 19 | T19 | skills.c | ✅ test_skills.c exists | P2 |
   212|| 20 | T20 | terminal.c | ✅ test_terminal.c exists | P2 |
   213|| 21 | T21 | tts.c | ✅ test_tts.c exists | P2 |
   214|| 22 | T22 | video_gen.c | ✅ test_video_gen.c (5 tests) | P2 |
   215|| 23 | T23 | vision.c | ✅ test_vision.c exists | P2 |
   216|| 24 | T24 | voice_mode.c | ✅ test_voice_mode.c (20 tests) | P2 |
   217|| 25 | T25 | web.c | ✅ test_web.c exists | P2 |
   218|
   219|## SECTOR 13: API Server Depth (0 gaps)
   220|
   221|C api_server.c (~1255 LOC, 16 endpoints) vs Python api_server.py (~3500 lines).
   222|All API server gaps resolved (E01-E05). See vault Phases 54-55.
   223|
   224|## SECTOR 14: TUI Depth (1 gaps)
   225|
   226|C tui_fullscreen.c (~3800 lines) vs Python TUI (Ink/React, ~5000 lines).
   227|
   228|| # | ID | Feature | LOC | Priority |
   229||---|-----|---------|-----|----------|
   230|| 1 | U05 | Plugin manager | 100 | P3 |
   231|
   232|## SECTOR 15: Curator Depth (0 gaps — all resolved)
   233|
   234|All curator features (status, run, review) implemented in CLI commands.c + curator.c using llm_background_review. Retired to vault Phase 21.
   235|
   236|## SECTOR 16: Prompt Caching Depth (0 gaps — all resolved)
   237|
   238|All depth gaps resolved (P04 multi-turn, P05 warmup, P06 per-provider config). See state.md Recently Resolved.
   239|
   240|## SECTOR 17: Shell Hooks Depth (0 gaps)
   241|
   242|All depth gaps (H01 priority, H02 async, H03 chaining) resolved. See vault Phase 58.
   243|
   244|| # | ID | Feature | LOC | Priority |
   245||---|-----|---------|-----|----------|
   246||   |     |         |     |          |
   247|
   248|## SECTOR 18: Vault Encryption Depth (3 gaps)
   249|
   250|| # | ID | Feature | LOC | Priority |
   251||---|-----|---------|-----|----------|
   252|| 1 | V01 | Key rotation | 60 | P2 |
   253|| 2 | V02 | Multiple vaults | 80 | P2 |
   254|| 3 | V03 | Vault sharing/export | 100 | P3 |
   255|
   256|## SECTOR 19: Security Depth (1 gap)
   257|
   258|C tirith_inline_scan covers 6 vectors. S01 (cloud/service patterns), S03 (credential leak detection), S04 (file path traversal depth), S05 (process arg injection) resolved.
   259||---|-----|---------|-----|----------|
   260|| 1 | S06 | DNS rebinding protection for web tool | 100 | P3 |
   261|
   262|## SECTOR 20: C-Only New Features (9 gaps)
   263|
   264|Features in Python that would benefit C.
   265|
   266|| # | ID | Feature | LOC | Priority |
   267||---|-----|---------|-----|----------|
   268|| 1 | N01 | Bitwarden Secrets Manager integration — ✅ STALE: secrets.c (330 LOC) + hermes_secrets.h (55 LOC) already exist | 0 | P2 |
   269|| 2 | N03 | Feishu doc and drive tools | 250 | P3 |
   270|| 3 | N04 | Microsoft Graph auth + client | 300 | P3 |
   271|| 4 | N05 | LSP protocol client (IDE integration) | 500 | P3 |
   272|| 5 | N06 | Codex app server integration | 400 | P3 |
   273|| 6 | N07 | Codex Responses API support | 300 | P3 |
   274|| 7 | N08 | Codex runtime environment | 200 | P3 |
   275|| 8 | N09 | Copilot ACP client integration | 300 | P3 |
   276|| 9 | N10 | Neutts synth (Neural TTS) | 100 | P3 |
   277|
   278|## SECTOR 21: Refactoring & Cleanup (9 gaps)
   279|
   280|| # | ID | Task | Description | LOC | Priority |
   281||---|-----|------|-------------|-----|----------|
   282|| 1 | R01 | Remove dead TUI display functions | tui_display_image, tui_wprint_role, etc. | -50 | P3 |
   283|| 2 | R02 | Fix qqbot dead code (post_api unused) | Remove __attribute__((unused)) or wire it | 5 | P3 |
   284|| 3 | R03 | Wire feishu_upload_image | Call from message send flow | 5 | P3 |
   285|| 4 | R05 | Standardize gateway platform creation | ADDING_A_PLATFORM.md for C | 30 | P3 |
   286|| 5 | R06 | Remove duplicate gateway/sms flags | Consolidate flag_send/flags | 10 | P3 |
   287|| 6 | R08 | Add make clean-all target | Deep clean (objects, binary, deps) | 5 | P3 |
   288|| 7 | R09 | Add .PHONY to all Makefile targets | Fix false-positive up-to-date | 10 | P3 |
   289|| 8 | R10 | Consolidate SLERMES_HOME vs HERMES_HOME | Some files use SLERMES_HOME, some HERMES_HOME | 20 | P2 |
   290|| 9 | R11 | Remove trailing whitespace across codebase | Pre-commit catches new, existing files have it | 5 | P3 |
   291|## SECTOR 22: Integration & CI (1 gap)
   292|
   293|| # | ID | Task | Description | Priority |
   294||---|-----|------|-------------|----------|
   295|| 1 | I04 | Static analysis (cppcheck/scan-build) | Automated lint — cppcheck added to c-build.yml CI | P2 |
   296|
   297|---
   298|
   299|## Upstream Sync: Python Features Not Yet in C
   300|
   301|Verified by scanning Python Hermes (597 .py files, 78 agent modules, 88+ tool files, 31 gateway platforms, 138 plugin dirs):
   302|
   303|### New Python modules with NO C equivalent (not in battleship sectors above)
   304|| Module | LOC | What It Does |
   305||--------|-----|-------------|
   306|| agent/onboarding | 400 | First-time user onboarding flow |
   307|| tools/slash_confirm.py | 80 | Confirmation dialog for destructive tool actions |
   308|| tools/budget_config.py | 120 | Per-session budget configuration |
   309|| tools/checkpoint_manager.py | 200 | Checkpoint save/load during agent runs |
   310|| tools/lazy_deps.py | 60 | Lazy dependency loading for tool files |
   311|| tools/openrouter_client.py | 150 | OpenRouter-specific tool interface |
   312|| tools/mcp_oauth_manager.py | 250 | OAuth token lifecycle for MCP connections (C has libmcp_oauth) |
   313|| tools/path_security.py | 100 | Path traversal prevention utilities |
   314|| tools/binary_extensions.py | 60 | Binary file type detection (C has libbinary) |
   315|| tools/debug_helpers.py | 90 | Debugging helpers for tool inspection |
   316|| tools/feishu_doc_tool.py | 300 | Feishu document create/edit tool |
   317|| tools/feishu_drive_tool.py | 250 | Feishu file upload/download tool |
   318|| tools/microsoft_graph_auth.py | 200 | Microsoft identity OAuth2 flow |
   319|| tools/microsoft_graph_client.py | 300 | Microsoft Graph API client |
   320|| tools/mixture_of_agents_tool.py | 250 | MoA ensemble tool |
   321|| tools/neutts_synth.py | 100 | Neural TTS synthesis |
   322|| tools/website_policy.py | 80 | Website access policy enforcement (C has libwebsite) |
   323|| tools/skills_guard.py | 100 | Skill sandbox execution |
   324|| tools/skill_provenance.py | 80 | Skill source tracking |
   325|
   326|## Form-Not-Function Gaps (verified by Triple DA)
   327|
   328|Functions that exist in C but are significantly less capable than Python equivalent:
   329|
   330|| # | C Function | Python Equivalent | Gap |
   331||---|-----------|-------------------|-----|
   332|| 8 | db_query_tool_stats (80 LOC) | insights.py (931 LOC) | C has raw SQL, Python has ML-based session scoring, skill breakdown, top sessions ranking, source filtering, cost analysis across providers |
   333|| 9 | prompt_caching.c (87 LOC) | prompt_caching.py (600+ LOC) | C has basic cache fields, Python has Anthropic ephemeral caching, automatic invalidation, hit-rate tracking, multi-turn optimization |
   334|| 10 | curator.c (160 LOC) | curator.py (800+ LOC) | C has basic state machine, Python has full review workflow, backup scheduling, meta-reviews, manual override |
   335|| 11 | shell_hooks.c (309 LOC) | shell_hooks.py (500+ LOC) | C has basic hook dispatch, Python has priority ordering, async execution, return value chaining, hook composition |
   336|| 12 | memory.c tool (full) | memory_tool.py + memory_manager.py | C has file + plugin backends, Python has multi-backend orchestration with priority ranking and auto-compression |
   337|| 13 | computer_use.c (~1540 LOC) | computer_use/ (4 files, ~2000 LOC) | C has inline backends, Python has modular backend/vision_routing/schema/tool split |
   338|| 14 | browser.c (~1400 LOC) | browser_tool.py + browser_cdp_tool.py + browser_dialog_tool.py + browser_supervisor.py (~2500 LOC) | C has single-file, Python has modular supervisor, CDP, dialog, state management |
   339|| 15 | tirith.c | tirith_security.py | C has basic scanning, Python has cloud patterns, credential leak detection, injection detection |
   340|| 16 | agent broker/routing | agent/fallback_routing.py (Python) | C has basic fallback_routing, Python has multi-provider failover with ordered priority |
   341|
   342|---
   343|
   344|## Summary
   345|
   346|| Sector | Count |
   347|||--------|-------|
   348||| S1: Stubs | 0 |
   349||| S2: Placeholder | 11 |
   350||| S3: Dead Code | 12 |
   351||| S4: Missing Agent | 10 |
   352||| S5: Agent Depth | 11 |
   353||| S6: Subdirectory | 22 |
   354||| S7: Tool Depth | 0 |
   355||| S8: Gateway | 10 |
   356||| S9: Config | 0 |
   357|||| S10: Library | 6 |
   358||| S11: Bug Fixes | 2 |
   359||| S12: Test Coverage | 2 |
   360|| S13: API Server | 0 |
   361|| S14: TUI | 1 |
   362|| S15: Curator | 0 |
   363||| S16: Prompt Cache | 0 |
   364|| S17: Shell Hooks | 0 |
   365|| S18: Vault Encrypt | 3 |
   366||| S19: Security | 1 |
   367||| S20: New Features | 8 |
   368|| S21: Refactoring | 9 |
   369|| S22: CI/Integrate | 1 |
   370|| **Total** | **110** |
   371|| (resolved items retired to vault/achievements.md) | |
   372|
