# Hermes C — Fresh Battleship Plan v1 (2026-05-23)
## Source: DA v14 comprehensive audit
## Total verified gaps: 176 (~324/500 closed)

## Sector Key
| Prefix | Sector | Closed | Open | Total |
|--------|--------|:------:|:----:|:----:|
| AG | Agent modules | 45 | 70 | 115 |
| GW | Gateway platforms | 22 | 42 | 64 |
| TL | Tools | 66 | 26 | 92 |
| CL | CLI | 79 | 16 | 95 |
| PL | Plugins | 10 | 16 | 26 |
| PR | Provider | 11 | 7 | 18 |
| TUI | TUI | 5 | 3 | 8 |
| SC | Security | 7 | 3 | 10 |
| MC | MCP | 10 | 1 | 11 |
| AC | ACP | 9 | 0 | 9 |
| CF | Config | 6 | 0 | 6 |
| TB | Tests/Build | 20 | 4 | 24 |
| UP | Upstream | 3 | 0 | 3 |
| **Total** | | **~324** | **~176** | **~500** |

---

## 🔴 P0 — Dead Code & Critical Stubs

### P0-S01: browser_cdp tool wired to real CDP handler (Dead code)
Status: CDP WebSocket client exists (browser.c:1193+) but tool registration goes to `stub_cdp_handler`
- [ ] Wire `browser_cdp` registration to real CDP handler function
- [ ] Remove `stub_cdp_handler` or demote to fallback with descriptive error
- [ ] Verify CDP capture works end-to-end (screenshots)
- [ ] Unit test for CDP connection/disconnect

### P0-S02: NOT_IMPLEMENTED error code (Unused enum)
Status: `HERMES_ERR_NOT_IMPLEMENTED` defined but never raised
- [ ] Find any code paths that SHOULD return NOT_IMPLEMENTED
- [ ] Or remove the enum if truly unused

---

## 🟡 P1 — Missing Features (6 major gaps)

### P1-F01: Feishu doc & drive tools (569 LOC Python, 0 C)
Files: `feishu_doc_tool.py` (138L), `feishu_drive_tool.py` (431L)
- [ ] F01a: Feishu auth token acquisition API
- [ ] F01b: Feishu document CRUD (create, read, update, delete)
- [ ] F01c: Feishu document search/query
- [ ] F01d: Feishu drive file listing
- [ ] F01e: Feishu drive file upload/download
- [ ] F01f: Feishu drive folder management
- [ ] Tests: Feishu tool unit tests

### P1-F02: Mixture-of-Agents tool (542 LOC Python, 0 C)
File: `mixture_of_agents_tool.py`
- [ ] F02a: MoA orchestrator — parallel agent dispatch
- [ ] F02b: MoA aggregator — response merge/synthesis
- [ ] F02c: MoA config (rounds, proposers, aggregator model)
- [ ] F02d: Round management (aggregator → proposers → synthesize → next round)
- [ ] Tests: MoA unit tests

### P1-F03: Microsoft Graph auth & client (653 LOC Python, 0 C)
Files: `microsoft_graph_auth.py` (245L), `microsoft_graph_client.py` (408L)
- [ ] F03a: Graph OAuth2 device code flow
- [ ] F03b: Graph token management (refresh, cache)
- [ ] F03c: Graph user/group/team queries
- [ ] F03d: Graph mail (send, read, search)
- [ ] F03e: Graph calendar (list events, create)
- [ ] F03f: Graph OneDrive file operations
- [ ] Tests: Graph unit tests

### P1-F04: Managed tool gateway (167 LOC Python, 0 C)
File: `managed_tool_gateway.py`
- [ ] F04a: Gateway start/stop/restart commands
- [ ] F04b: Platform list and status queries
- [ ] F04c: Gateway config hot-reload
- [ ] Tests: Gateway management tests

### P1-F05: Environment backends (6 of 9 missing)
Files: `modal.py`, `managed_modal.py` (~600L), `daytona.py` (~400L), `singularity.py` (~300L), `vercel_sandbox.py` (~200L), `file_sync.py` (~150L)
- [ ] F05a: Modal backend — cloud sandbox execution
- [ ] F05b: Managed Modal — provisioned sandbox
- [ ] F05c: Daytona backend — remote dev environment
- [ ] F05d: Singularity backend — container runtime
- [ ] F05e: Vercel sandbox — edge function runtime
- [ ] F05f: File sync — bidirectional sync for remote backends
- [ ] Tests: Environment backend tests

### P1-F06: Batch runner (0 C)
File: `batch_runner.py` (~600L)
- [ ] F06a: Parallel batch processing framework
- [ ] F06b: Batch job scheduling and queue management
- [ ] F06c: Result aggregation and error handling
- [ ] Tests: Batch runner tests

---

## 🟡 P2 — Gateway Platform Completeness

### P2-G01: Gateway webhook mode support
- [ ] G01a: Telegram webhook mode (vs polling)
- [ ] G01b: Slack Events API webhook mode
- [ ] G01c: Webhook registration/cleanup lifecycle
- [ ] Tests: Webhook platform tests

### P2-G02: Missing gateway platform helpers (12 files)
- [ ] G02a: `feishu_comment.py` — Feishu comment CRUD
- [ ] G02b: `signal_rate_limit.py` — Signal rate limiting
- [ ] G02c: `telegram_network.py` — Telegram network abstraction
- [ ] G02d: `wecom_callback.py` — WeCom callback handler
- [ ] G02e: `wecom_crypto.py` — WeCom crypto utilities
- [ ] G02f: `yuanbao_media.py` — Yuanbao media handling
- [ ] G02g: `yuanbao_proto.py` — Yuanbao protobuf
- [ ] G02h: `yuanbao_sticker.py` — Yuanbao sticker support
- [ ] G02i: `api_server.py` — REST API gateway
- [ ] G02j: Platform base class + helpers
- [ ] Tests: Gateway helper tests

### P2-G03: Gateway per-platform tests (0 existing)
- [ ] G03a: Telegram platform tests
- [ ] G03b: Discord platform tests
- [ ] G03c: Slack platform tests
- [ ] G03d: Matrix platform tests
- [ ] G03e: Email platform tests
- [ ] G03f: Webhook platform tests
- [ ] G03g: SMS platform tests
- [ ] G03h: Signal platform tests
- [ ] G03i: WhatsApp platform tests
- [ ] G03j: Feishu platform tests
- [ ] G03k: WeCom platform tests
- [ ] G03l: DingTalk platform tests
- [ ] G03m: QQ bot platform tests
- [ ] G03n: BlueBubbles platform tests
- [ ] G03o: Weixin platform tests
- [ ] G03p: Yuanbao platform tests
- [ ] G03q: Mattermost platform tests
- [ ] G03r: HomeAssistant platform tests
- [ ] G03s: MSGraph webhook platform tests

---

## 🟡 P3 — CLI Polish

### P3-C01: Interactive CLI polish
- [ ] C01a: Tab-completion for known commands
- [ ] C01b: Spinner animation during agent response
- [ ] C01c: Setup wizard (first-run onboarding)
- [ ] C01d: Rich panel display for structured output
- [ ] C01e: History navigation (up/down arrows)
- [ ] C01f: Multi-line input mode
- [ ] Tests: CLI interaction tests

### P3-C02: CLI subcommand depth
- [ ] C02a: `hermes config` — extended display (diffs, validation)
- [ ] C02b: `hermes model` — model info + capability query
- [ ] C02c: `hermes usage` — token usage display
- [ ] C02d: `hermes insights` — session analytics
- [ ] C02e: `hermes copy` — clipboard integration
- [ ] Tests: CLI subcommand tests

---

## 🟢 P4 — Agent Module Gaps (70 remaining)

### P4-A01: Large agent modules
- [ ] A01a: `agent_init.py` (1638L) — complex Python SDK deps
- [ ] A01b: `agent_runtime_helpers.py` (2189L) — runtime helpers
- [ ] A01c: `auxiliary_client.py` (5289L) — large, async Python
- [ ] A01d: `insights.py` (930L) — session analytics from DB
- [ ] A01e: `usage_pricing.py` (877L) — token cost estimation
- [ ] A01f: `background_review.py` (587L) — code review via curator fork

### P4-A02: Medium agent modules (~20 modules)
- [ ] A02a: `account_usage.py` (326L) — provider account usage
- [ ] A02b: `configuration_assistant.py` — config guidance
- [ ] A02c: `context_compression_optimizer.py` — compression
- [ ] A02d: `skill_improvement.py` — skill auto-improvement
- [ ] A02e: `plugin_orchestrator.py` — plugin lifecycle
- [ ] A02f: `session_analytics.py` — session stats
- [ ] A02g: `memory_manager.py` — memory management
- [ ] A02h: `tool_result_processors.py` — result processing
- [ ] A02i: `multimodal_handlers.py` — multimodal support
- [ ] A02j: `prompt_caching.py` — prompt cache management
- [ ] (20+ more small modules — ~200-400L each)

---

## 🟢 P5 — Tools Depth

### P5-T01: skill_manager_tool.py (931L)
- [ ] T01a: Skill CRUD operators
- [ ] T01b: Skill curator (quality checks, auto-fixes)
- [ ] T01c: Skill hub (publish, browse, install)
- [ ] T01d: Skill dependencies/resolution
- [ ] T01e: Skill versioning
- [ ] Tests: Skill management tests

### P5-T02: skills_guard.py (1007L)
- [ ] T02a: Skill permission analysis
- [ ] T02b: Safe/unsafe action classification
- [ ] T02c: Guard policy configuration
- [ ] T02d: Sandbox enforcement
- [ ] Tests: Skills guard tests

### P5-T03: Remaining tool gaps
- [ ] T03a: `computer_use_tool.py` — backport D75-D79 upstream changes
- [ ] T03b: `browser_camofox.py` — Camofox CDP integration
- [ ] T03c: `browser_dialog_tool.py` — browser dialog handler
- [ ] T03d: `browser_supervisor.py` — browser safety supervisor
- [ ] T03e: `budget_config.py` — budget config per-tool
- [ ] T03f: `checkpoint_manager.py` — checkpoint management
- [ ] T03g: `clarify_gateway.py` — gateway clarifications
- [ ] T03h: `credential_files.py` — credential file management
- [ ] T03i: `debug_helpers.py` — debug utilities
- [ ] T03j: `environment_details.py` — env info tool
- [ ] (10+ more small files)

---

## 🟢 P6 — Plugin Porting (16 remaining)

- [ ] P01: memory/honcho — persistent memory backend
- [ ] P02: memory/mem0 — memory provider
- [ ] P03: memory/supermemory — supermemory integration
- [ ] P04: context_engine plugins — context management
- [ ] P05: model-provider/openrouter — OpenRouter provider
- [ ] P06: model-provider/anthropic — Anthropic provider
- [ ] P07: model-provider/gmi — GMI provider
- [ ] P08: kanban worker — Kanban board worker
- [ ] P09: hermes-achievements — gamified tracking
- [ ] P10: observability — metrics/traces/logs
- [ ] P11: image_gen providers (Fal, etc.)
- [ ] P12-P16: 5 more provider/utility plugins

---

## 🟢 P7 — Tests & Quality (ongoing)

### P7-Q01: New fresh suite targets
- [ ] Q01a: Image routing integration test (needs config loading)
- [ ] Q01b: browser_cdp integration test
- [ ] Q01c: Feishu doc tool tests
- [ ] Q01d: MoA tool tests
- [ ] Q01e: Graph auth/client tests
- [ ] Q01f: Environment backend tests
- [ ] Q01g: Gateway platform tests (19 platforms)
- [ ] Q01h: MCP transport tests (SSE, HTTP, WebSocket)
- [ ] Q01i: ACP protocol tests
- [ ] Q01j: Plugin loading tests
- [ ] Q01k: Config migration tests
- [ ] Q01l: CLI subcommand tests

### P7-Q02: Pre-existing test failures
- [ ] Q02a: Fix process_tool test failure (suite 197/1/0)

---

## 🔧 Implementation Order (Recommended)
1. **P0-S01** — Wire CDP handler (1 session, quick win)
2. **P4-A01f** — background_review.py (1 session, next agent module)
3. **P3-C01a** — Tab completion (1 session, high UX impact)
4. **P1-F01** — Feishu tools (2 sessions, high parity impact)
5. **P1-F05** — Environment backends (2 sessions, high parity impact)
6. **P2-G03** — Gateway per-platform tests (2 sessions, quality)
7. **P5-T01** — skill_manager_tool.py (1 session)
8. **P1-F02** — MoA tool (1 session)
9. **P4-A01** — Large agent modules (3 sessions)
10. **P6** — Plugin porting (2 sessions)
11. **P2-G01** — Webhook mode (1 session)
12. **P7** — Test suite expansion (ongoing)
