# Hermes C Translation — Battleship Roadmap v7

**Triple DA + Stub Hunt (2026-06-01) — Fresh code survey. ~140 gaps catalogued.**
**Previous v5 (~394) was inflated by filename aliasing. v7 is function-level granular.**

**Verified State:**
- Suite: 232/0/0 (195 test files) | CLI: 88 cmds | Tools: 70 registered
- Gateway: 19 platforms | Providers: 10 C modules | Libs: 58 units
- Agent .c: 50 | Total src .c: 153 | Binary: 29MB, 0 warnings

---

## Sector S: Stubs & Form-Not-Function (17 gaps)

| ID | Gap | Location | Priority | Detail |
|----|-----|----------|----------|--------|
| S01 | stub_cdp_handler dead code | browser.c:1172 | P2 | No tool registers to this handler |
| S02 | noop_stop() empty body | computer_use.c:41 | P2 | noop backend stop is `{}` |
| S03 | x11_stop() empty body | computer_use.c:170 | P2 | X11 backend stop is `{}` |
| S04 | wayland_stop() empty body | computer_use.c:453 | P2 | Wayland stop is `{}` |
| S05 | memory ops all NULL | memory.c:1472-8 | P1 | import_json/export_json/get_by_hash/compress_old/get_prioritized = NULL |
| S06 | FAL mock returns fake URL | plugin_image_gen.c:197 | P1 | Mock returns `fal.ai/media/test.png`; real curl may fail |
| S07 | api_server echo fallback | api_server.c:218 | P2 | Echoes messages when no agent |
| S08 | X11 set_value unsupported | computer_use.c:410 | P1 | Error: set_value not supported on X11 |
| S09 | Wayland set_value unsupported | computer_use.c:729 | P1 | Error: set_value not supported on Wayland |
| S10 | macOS backend | computer_use.c:1146 | P2 | Returns NULL on non-macOS |
| S11 | Windows backend | computer_use.c:1467 | P2 | Returns NULL on non-Windows |
| S12 | plat.shutdown = NULL | server.c:1729 | P1 | Gateway platform shutdown no-op |
| S13 | unused tui functions | tui_fullscreen.c | P2 | 4 `__attribute__((unused))` tagged |
| S14 | weixin build_headers unused | weixin.c:58 | P2 | Dead code |
| S15 | qqbot post_api unused | qqbot.c:78 | P2 | Dead code |
| S16 | ~user not supported | subdir_hints.c:105 | P2 | Returns NULL for ~user/ paths |
| S17 | api_server non-streaming | api_server.c:6 | P1 | Only handles non-streaming requests |

## Sector A: Missing Agent Module Ports (28 gaps)

| ID | Gap | LOC | Priority | Notes |
|----|-----|-----|----------|-------|
| A01 | agent_init.py | 1638 | P0 | Python SDK deps, plugin_llm — massive |
| A02 | agent_runtime_helpers.py | 2189 | P0 | Runtime helpers — massive |
| A03 | chat_completion_helpers.py depth | 2097 | P1 | Streaming, stop reasons, content parsing |
| A04 | context_compressor.py | 1748 | P1 | Scaled budget + token-tail (partly in agent_loop.c) |
| A05 | conversation_compression.py | 603 | P1 | Related compression depth |
| A06 | insights.py | 930 | **P1** | Session analytics — per-model/platform |
| A07 | codex_responses_adapter.py | 1082 | P2 | Codex response format |
| A08 | google_oauth.py | 1059 | P2 | Google OAuth |
| A09 | plugin_llm.py | 1046 | P2 | LLM via plugin |
| A10 | gemini_cloudcode_adapter.py | 909 | P2 | Cloud Code adapter |
| A11 | models_dev.py | 726 | P2 | Dev model utilities |
| A12 | curator_backup.py | 696 | P2 | Backup mechanism |
| A13 | copilot_acp_client.py | 686 | P2 | Copilot ACP |
| A14 | background_review.py | 587 | P1 | Background review agent |
| A15 | azure_identity_adapter.py | 555 | P2 | Azure identity |
| A16 | google_code_assist.py | 452 | P2 | Google Code Assist |
| A17 | codex_runtime.py | 448 | P2 | Codex runtime |
| A18 | nous_rate_guard.py depth | 325 | P1 | Rate guard features |
| A19 | video_gen_provider.py | 299 | P2 | Multi-provider |
| A20 | memory_provider.py | 280 | P2 | Plugin interface |
| A21 | rate_limit_tracker.py depth | 246 | P1 | Rate limit tracking |
| A22 | image_gen_provider.py depth | 242 | P1 | Multi-provider (local SD) |
| A23 | browser_registry.py | 223 | P2 | Browser provider abstraction |
| A24 | web_search_provider.py depth | 221 | P1 | Multi-search-engine |
| A25 | browser_provider.py | 175 | P2 | Browser provider interface |
| A26 | process_bootstrap.py | 167 | P1 | HTTP proxy, safe stdio |
| A27 | stream_diag.py | 281 | P1 | Ttfb, headers, retry logging |
| A28 | async_utils.py | 68 | P2 | Async bridging (Python-specific) |

## Sector T: Tool Ports (20 gaps)

| ID | Gap | LOC | Priority | Notes |
|----|-----|-----|----------|-------|
| T01 | browser_tool.py | 3796 | **P1** | Core browser wrapper |
| T02 | skills_hub.py | 3456 | **P1** | Skills hub (C skills.c basic) |
| T03 | browser_supervisor.py | 1457 | **P1** | Browser supervisor |
| T04 | process_registry.py | 1544 | **P1** | Process registry |
| T05 | checkpoint_manager.py | 1638 | **P1** | Checkpoint management |
| T06 | skills_tool.py | 1587 | **P1** | Skills management |
| T07 | tirith_security.py depth | 803 | **P1** | C tirith.c exists, needs depth |
| T08 | discord_tool.py | 959 | **P1** | Discord send tool |
| T09 | skills_guard.py | 1007 | **P1** | Skills security guard |
| T10 | browser_camofox.py | 699 | P2 | Camofox browser |
| T11 | browser_cdp_tool.py | 570 | **P1** | CDP browser |
| T12 | mcp_oauth.py | 648 | **P1** | MCP OAuth |
| T13 | mcp_oauth_manager.py | 607 | **P1** | MCP OAuth manager |
| T14 | mixture_of_agents_tool.py | 542 | P2 | MoA tool |
| T15 | patch_parser.py | 622 | P2 | Patch parser depth |
| T16 | schema_sanitizer.py | 445 | P2 | Schema sanitizer |
| T17 | fal_common.py | 163 | **P1** | FAL common utilities |
| T18 | url_safety.py | — | **P1** | URL safety |
| T19 | feishu_doc_tool.py | 138 | P2 | Feishu doc |
| T20 | feishu_drive_tool.py | 431 | P2 | Feishu drive |

## Sector G: Gateway Sub-Modules (11 gaps — all P2)

| ID | Gap | Notes |
|----|-----|-------|
| G01 | feishu_comment.py + rules | Comment formatting |
| G02 | signal_rate_limit.py | Rate limiting |
| G03 | telegram_network.py | Network abstraction |
| G04 | wecom_callback.py | Callback handling |
| G05 | wecom_crypto.py | Crypto support |
| G06 | yuanbao_media.py | Media handling |
| G07 | yuanbao_proto.py | Protobuf |
| G08 | yuanbao_sticker.py | Stickers |
| G09 | base.py adapter helpers | Shared base |
| G10 | helpers.py | Platform helpers |
| G11 | ntfy platform port | Push notifications |

## Sector P: Provider API Quirks (15 gaps)

| ID | Gap | Priority |
|----|-----|----------|
| P01 | Anthropic: content block tool_use depth | P1 |
| P02 | Anthropic: prompt caching headers | P1 |
| P03 | OpenAI: response_format strict mode | P1 |
| P04 | OpenAI: service tier | P2 |
| P05 | Google: context caching | P1 |
| P06 | Google: safety settings | P2 |
| P07 | DeepSeek: FIM integration | P1 |
| P08 | DeepSeek: reasoning_effort config | P1 |
| P09 | xAI: reasoning_effort config | P1 |
| P10 | Azure: API version management | P1 |
| P11 | Bedrock: cross-region inference | P2 |
| P12 | Structured content parsing | P1 |
| P13-P15 | Provider plugins (Alibaba, HF, Ollama, etc.) | P2 |

## Sector C: Config Keys (10 gaps — all P1)

| ID | Key |
|----|-----|
| C01 | agent.tool_use_guard_level |
| C02 | agent.image_input_mode |
| C03 | agent.reasoning_effort per-provider |
| C04 | agent.quiet_mode integration |
| C05 | tools.enabled_toolsets depth |
| C06 | agent.budget (token/cost) depth |
| C07 | agent.session_db config |
| C08 | agent.plugin_paths |
| C09 | agent.compress_cooldown_secs |
| C10 | tool_output max_bytes/line |

## Sector L: CLI Enhancements (12 gaps)

| ID | Gap | Priority |
|----|-----|----------|
| L01 | Gateway CLI — start/stop/status | P2 |
| L02 | Plugins CLI — install/remove/list | P1 |
| L03 | Secrets CLI — manage API keys | P1 |
| L04 | Setup wizard — interactive first-run | P2 |
| L05 | Shell completion — bash/zsh | P2 |
| L06 | Auth CLI — OAuth (7601L Python) | P2 |
| L07 | Webhook CLI | P2 |
| L08 | Kanban CLI | P1 |
| L09 | Config CLI — get/set | P1 |
| L10 | Skills hub CLI | P1 |
| L11 | Portal tags CLI | P2 |
| L12 | Env loader CLI | P2 |

## Sector M: MCP/ACP (10 gaps)

| ID | Gap | Priority |
|----|-----|----------|
| M01 | MCP: Sampling/createMessage handler | P1 |
| M02 | MCP: Roots list support | P1 |
| M03 | MCP: Resource templates | P2 |
| M04 | MCP: Prompts system | P2 |
| M05 | MCP: Streaming transport depth | P1 |
| M06 | MCP: Auth/OAuth depth | P1 |
| M07 | ACP: Sampling/chat completions | P1 |
| M08 | ACP: File diff streaming | P1 |
| M09 | ACP: Edit approval notification | P1 |
| M10 | ACP: Server broadcasting | P2 |

## Sector J: Plugin System (10 gaps)

| ID | Gap | Priority |
|----|-----|----------|
| J01 | Plugin .so build (10 targets, 0 built) | P1 |
| J02 | Kanban plugin | P2 |
| J03 | Memory plugins (honcho, mem0) | P2 |
| J04 | Context engine plugins | P2 |
| J05 | Disk cleanup plugin | P2 |
| J06 | Achievements plugin | P2 |
| J07 | Observability plugin | P2 |
| J08 | Google Meet plugin | P2 |
| J09 | Spotify plugin | P2 |
| J10 | Plugin auto-discovery + sandbox | P2 |

## Sector X: Test Coverage (10 gaps)

| ID | Gap | Priority |
|----|-----|----------|
| X01 | test_provider_xai.c | P1 |
| X02 | test_provider_azure.c | P1 |
| X03 | test_provider_bedrock.c | P1 |
| X04 | test_provider_openrouter.c | P1 |
| X05 | test_provider_custom.c | P1 |
| X06 | test_llm_client.c | P2 |
| X07 | test_agent_loop.c | P2 |
| X08 | Gateway integration tests | P1 |
| X09 | Provider streaming + tool call | P1 |
| X10 | Config parsing edge cases | P1 |

---

## Summary

| Sector | Gaps | P1 | P2 |
|--------|------|----|-----|
| S: Stubs/Form-Not-Function | 17 | 5 | 12 |
| A: Agent Module Ports | 28 | 11 | 17 |
| T: Tool Ports | 20 | 12 | 8 |
| G: Gateway Sub-modules | 11 | 0 | 11 |
| P: Provider Quirks | 15 | 9 | 6 |
| C: Config Keys | 10 | 10 | 0 |
| L: CLI Enhancements | 12 | 5 | 7 |
| M: MCP/ACP | 10 | 7 | 3 |
| J: Plugin System | 10 | 1 | 9 |
| X: Test Coverage | 10 | 7 | 3 |
| **Total** | **~143** | **~67** | **~76** |

Generated 2026-06-01 from Triple DA + stub hunt: fresh code survey, form-not-function check, upstream diff, alias-corrected.
