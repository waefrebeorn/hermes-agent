# Triple Devil's Advocate — Hermes C (v15)

**Audit date:** 2026-05-24  
**Scope:** Fresh code survey, battleship reset for v4  
**Methodology:** Code inspection of every src/ directory, direct comparison vs Python reference at `/home/wubu/hermes-agent-dev/`

---

## Part 1: Fresh Code Survey — Ground Truth Counts

### Python Reference (what exists)
| Domain | Count | Notes |
|--------|-------|-------|
| Agent modules | 77 `.py` files | `/home/wubu/hermes-agent-dev/agent/*.py` |
| Tool files | 75 `.py` files | `/home/wubu/hermes-agent-dev/tools/*.py`, ~68 registry_register calls |
| CLI modules | 88 `.py` files | `/home/wubu/hermes-agent-dev/hermes_cli/*.py`, 69 CommandDef entries |
| Gateway platforms | 31 `.py` files | 20 core adapters, 8 plugin platforms, helpers |
| Provider plugins | 28 plugin directories | `plugins/model-providers/*/` |
| Plugin directories | 16 plugin dirs | `plugins/*/` |
| Config keys | 432 nested keys | From DEFAULT_CONFIG |
| Test files | 66 + 22 subdirs | ~28,027 test functions |
| ACP modules | 9 `.py` files | `acp_adapter/*.py` |
| Gateway root | 21 `.py` files | `gateway/*.py` |

### C Translated (what exists)
| Domain | Count | % of Python | Notes |
|--------|-------|-------------|-------|
| Agent .c files | 44 | 57% | 33 of 77 ported directly |
| Tool init functions | 31 | — | ~83 registry_register calls (C registers more tool variants per file) |
| CLI .c files | 8 | 9% | 237 cmd_ functions in commands.c (many are stubs) |
| Gateway platform .c | 19 | 61% | 19 of 31 Python platform modules |
| C provider adapters | 9 | 32% | 9 native vs 28 plugin providers |
| Plugin .c src files | 10 | 63% | 10 of 16 plugin dirs have C |
| Config keys | ~322 | ~75% | Estimated from hermes_config.h struct + config.c |
| Test .c files | 173 | — | Pure C tests, not 1:1 with Python pytest |
| ACP .c files | 5 | 56% | edit_approval, events, permissions, resource, server |
| CLI commands | ~237 | — | Many are incomplete (printf stubs) |

### Key Gap: Agent modules — 33 of 77 ported
**Ported** (44 .c files including provider adapters + infra):
agent_loop.c, audit.c, budget_tracker.c, checkpoint.c, context.c, credential_pool.c, fallback_routing.c, file_safety.c, gemini_schema.c, i18n.c, image_routing.c, llm_client.c, lmstudio_reasoning.c, manual_compression_feedback.c, markdown_tables.c, moonshot_schema.c, onboarding.c, plugin_ext.c, portal_tags.c, prompt_caching.c, provider.c, provider_anthropic.c, provider_azure.c, provider_bedrock.c, provider_custom.c, provider_deepseek.c, provider_google.c, provider_metadata.c, provider_openai.c, provider_openrouter.c, provider_xai.c, redact.c, retry_utils.c, sanitize.c, skill_bundles.c, skill_preprocessing.c, subdir_hints.c, system_prompt.c, think_scrubber.c, title.c, tool_guardrails.c, trajectory.c, usage_pricing.c, vault.c

**NOT ported** (52 Python modules with no C equivalent):
account_usage.py, agent_init.py, agent_runtime_helpers.py, async_utils.py, auxiliary_client.py, azure_identity_adapter.py, background_review.py, browser_provider.py, browser_registry.py, chat_completion_helpers.py, codex_responses_adapter.py, codex_runtime.py, context_compressor.py, context_engine.py, context_references.py, conversation_compression.py, conversation_loop.py, copilot_acp_client.py, credential_sources.py, curator.py, curator_backup.py, display.py, gemini_cloudcode_adapter.py, gemini_native_adapter.py, google_code_assist.py, google_oauth.py, image_gen_provider.py, image_gen_registry.py, insights.py, iteration_budget.py, memory_manager.py, memory_provider.py, message_sanitization.py, model_metadata.py, models_dev.py, nous_rate_guard.py, plugin_llm.py, process_bootstrap.py, prompt_builder.py, rate_limit_tracker.py, shell_hooks.py, skill_commands.py, stream_diag.py, subdirectory_hints.py (C has subdir_hints.c - different name), title_generator.py, tool_executor.py, tool_result_classification.py, video_gen_provider.py, video_gen_registry.py, web_search_provider.py, web_search_registry.py

**Note:** Some "unported" modules are partially covered:
- rate_limit_tracker.py → covered by libratelimit
- credential_sources.py → covered by credential_pool.c
- file_safety.py → ported as file_safety.c
- skill_commands.py → covered by skill_bundles.c
- subdirectory_hints.py → ported as subdir_hints.c

---

## Part 2: Tool Parity — Which Tools Are REAL and Which Are STUBS

### Claim: "All tools are real, 0 stubs" — DA v14 claim
**Verify:** Spot-checked every tool file. All 31 init functions have handlers with real logic.

**True stubs found:**
1. **browser_cdp tool** still registered to `stub_cdp_handler` — even though CDP client code exists (browser.c:1193+), the registration at line 1471+ points to the stub. Dead code hazard. 🟡
2. **HERMES_ERR_NOT_IMPLEMENTED** — enum exists, never raised. 🟢 Low severity.

**Functional limitations (NOT stubs):**
- computer_use: X11 works via xdotool+ImageMagick. Wayland partial. macOS CUA driver not integrated. 🟡
- voice_mode: Shell-based (espeak/edge-tts). No real-time streaming. 🟡
- terminal: Local/docker/SSH backends. Missing: modal, daytona, singularity, vercel_sandbox 🟡
- transcribe: lib/libtranscribe/ exists but may not be wired as a registered tool 🟡

### Verification: All 31 tool init functions exist and are called
```
registry_init_terminal(), registry_init_file(), registry_init_web(),
registry_init_skills(), registry_init_patch(), registry_init_exec_code(),
registry_init_clarify(), registry_init_memory(), registry_init_todo(),
registry_init_process(), registry_init_send_message(), registry_init_cronjob(),
registry_init_skill_view(), registry_init_session_search(), registry_init_session_crud(),
registry_init_tts(), registry_init_vision(), registry_init_delegate(),
registry_init_x_search(), registry_init_browser(), registry_init_approval(),
registry_init_voice(), registry_init_transcribe(), registry_init_image_gen(),
registry_init_video_gen(), registry_init_homeassistant(), registry_init_kanban(),
registry_init_computer_use(), registry_init_discord(), registry_init_mcp(),
registry_init_file_batch()
```

**Verdict:** 31 init functions, all called in tool_init.c. 1 true stub (browser_cdp handler). Remaining are functional with varying depth. ✅ Verified

---

## Part 3: Gateway Platform Parity

### Claim: "19 platforms" ✅ Verified
C has 19 .c files in src/gateway/platforms/:
bluebubbles, dingtalk, discord, email, feishu, homeassistant, matrix, mattermost, msgraph_webhook, qqbot, signal, slack, sms, telegram, webhook, wecom, weixin, whatsapp, yuanbao

**Missing from C** (Python has these):
- api_server.py — REST API server gateway
- feishu_comment.py, feishu_comment_rules.py — Feishu comment support
- signal_rate_limit.py — Signal rate limiting
- telegram_network.py — Telegram network abstraction
- wecom_callback.py, wecom_crypto.py — WeCom callback/crypto
- yuanbao_media.py, yuanbao_proto.py, yuanbao_sticker.py — Yuanbao sub-modules
- base.py, helpers.py — Shared platform infrastructure

**Verdict:** 19/31 platform modules (61%). Core adapters: 19/20 (95%, missing only api_server). Helper modules: 0/12 ported. 🟡

---

## Part 4: CLI Parity

### Claim: "~148 commands" — COMPILATION CLAIM, not verified
**Verify by reading commands.c:** 237 cmd_ functions declared, but many are stubs that print "not available" or printf("---") instead of executing real logic.

**Real vs stub in commands.c:**
| Function | Status |
|----------|--------|
| cmd_help | ✅ Real |
| cmd_exit | ✅ Real |  
| cmd_clear | ✅ Real |
| cmd_model | ✅ Real (model switching) |
| cmd_sessions | ✅ Real (DB-backed) |
| cmd_save/load | ✅ Real (session persist) |
| cmd_stats | ✅ Real |
| cmd_config | ✅ Real |
| cmd_commands | ✅ Real |
| cmd_tools | ✅ Real |
| cmd_retry | ✅ Real |
| cmd_undo | ✅ Real |
| cmd_history | ✅ Real |
| cmd_topic | ❌ Stub |
| cmd_reset | ❌ Stub |
| cmd_new | ✅ Real |
| cmd_conv | ✅ Real |
| Many auxiliary cmds | ❌ Printf stubs |

**Verdict:** ~40 real commands, ~197 stub commands. Python has 69 CommandDef entries, each with full implementations. About 58% real by count. ❌ Stale claim.

---

## Part 5: Provider Parity

### Claim: "11/18 providers, 61%" — COMPILATION CLAIM
**Verify:**
- 9 native C provider adapters: openai, deepseek, openrouter, xai, anthropic, google, azure, bedrock, custom
- 1 provider.c (dispatch/creation)
- 1 provider_metadata.c (provider info)
- All 9 are real implementations with API calls, streaming, tool use, vision where supported
- Python has 28 model-provider plugins (including all C 9 + 19 more)

**Missing providers (19):** alibaba, alibaba-coding-plan, arcee, azure-foundry, copilot, copilot-acp, gmi, huggingface, kilocode, kimi-coding, minimax, nous, novita, nvidia, ollama-cloud, openai-codex, opencode-zen, qwen-oauth, stepfun, xiaomi, zai

**Verdict:** 9/28 provider plugins = 32%. But C's 9 native providers cover the main APIs. Plugin rest is a significant gap. 🟡

---

## Part 6: Remaining Gaps — Fresh Count

### Fresh Estimation by Sector

| Sector | C Count | Python Count | % Done | Estimated Gaps |
|--------|---------|-------------|--------|----------------|
| **A: Core** | 12 modules | ~16 features | 75% | 4 |
| **B: Agent** | 44 .c files | 77 .py files | 57% | 33 |
| **C: CLI** | 8 .c files | 88 .py files | 9% | 80 |
| **D: Tools** | 31 init funcs | 75 .py files | — | 20 (feature gaps) |
| **E: Gateway** | 19 .c files | 31 platform modules | 61% | 12 |
| **F: MCP** | 1 .c | 1 module | — | 2 |
| **G: ACP** | 5 .c files | 9 .py files | 56% | 4 |
| **H: Cron** | 3 .c | 3 modules | 100% | 0 |
| **I: TUI** | 2 .c + lib | 1 full Ink app | — | 6 |
| **J: Plugins** | 10 .c src | 16 plugin dirs | 63% | 6 |
| **K: Providers** | 9 native | 28 plugin dirs | 32% | 19 |
| **L: Config** | ~322 keys | 432 keys | 75% | 10 |
| **M: Libraries** | 57 lib dirs | — | — | 5 |
| **N: Build/Doc** | — | — | — | 5 |
| **O: Upstream Sync** | — | — | — | variable |
| **P: Security** | — | — | — | 5 |
| **Q: CLI Depth** | — | — | — | 10 |
| **R: Provider quirks** | — | — | — | 10 |
| **S: Tests** | 173 .c files | ~28K tests | — | 20 |
| **T: CI/CD** | — | — | — | 5 |
| **U: Stubs** | 1 true stub | — | — | 1 |

**Estimated total: ~260 gaps** (fresh, conservative, not including upstream drift)

---

## Part 7: Risk Assessment

| Risk | Description | Severity |
|------|-------------|----------|
| **Stale battleship confidence** | Old 500-gap count had many phantom gaps (stubs already fixed). New count of ~260 is real but may still miss sub-features. | 🟡 |
| **CLI depth illusion** | 237 cmd_ functions looks impressive but ~197 are printf stubs. Python CLI has 69 full implementations with autocomplete, Rich output, setup wizard. | 🔴 |
| **Plugin provider gap** | 19 missing providers means users of alibaba, huggingface, minimax, etc. can't use C Hermes. | 🟡 |
| **No per-platform tests** | 19 gateways, 0 per-platform tests. A broken platform won't be caught until runtime. | 🟡 |
| **Test suite timeout** | 173 test files can't complete in 120s. Need faster test execution or parallelization. | 🟡 |
| **Dead CDP code** | Browser.c has 300+ lines of real CDP client code that's never wired to the tool registration. Maintenance burden. | 🟡 |

*Generated 2026-05-24. DA v15 — fresh code survey, battleship reset. 260 gaps conservatively identified.*
