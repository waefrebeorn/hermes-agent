# Hermes C Translation — 300-Gap Battleship Roadmap (v1 — 2026-06-01)

**Format:** `SECTOR##` — e.g., A01, B12, C05. Each cell is a discrete porting task.
**Legend:** ❌ Not started | 🔄 In progress | ✅ Complete
**Target:** 1:1 parity with Python hermes-agent at origin/main (729a778a)

---

## Sector A: Core (Python: 16 root modules → C: ~10 files)

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| A01 | `run_agent.py` | `src/agent/agent_loop.c` | ✅ | Core loop, retry, budget, checkpoint |
| A02 | `cli.py` | `src/cli/cli.c` + `commands.c` | ✅ | ~148 handlers |
| A03 | `model_tools.py` | `src/tools/registry.c` | ✅ | 28 tools registered |
| A04 | `toolsets.py` | `src/tools/registry.c` + toolset defs | 🔄 | Partial — need env/remote toolset support |
| A05 | `hermes_state.py` | `lib/libdb` + `src/tools/session_search.c` | ✅ | SQLite FTS5, CRUD |
| A06 | `hermes_constants.py` | inline in `hermes.h` | ✅ | |
| A07 | `hermes_logging.py` | inline in `hermes.h` | ✅ | |
| A08 | `hermes_time.py` | `lib/libdatetime` | ✅ | |
| A09 | `batch_runner.py` | — | ❌ | Parallel batch processing not ported |
| A10 | `mcp_serve.py` | `src/mcp_serve.c` + `lib/libmcp` | ✅ | |
| A11 | `utils.py` | inline + `lib/lib...` | ✅ | Various utilities distributed across libs |
| A12 | `mini_swe_runner.py` | — | ❌ | SWE-bench runner not ported |
| A13 | `hermes_bootstrap.py` | `src/main.c` | ✅ | CLI entry point |
| A14 | `trajectory_compressor.py` | — | ❌ | Trajectory compression not ported |
| A15 | `toolset_distributions.py` | — | ❌ | Toolset distribution logic not ported |
| A16 | `setup.py` | `Makefile` | ✅ | Build system |

**Sector A: 12/16 complete (75%)**

---

## Sector B: Agent (Python: 102 files → C: ~16 files)

### B01-B20: Agent Initialization & Core Loop

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| B01 | `agent/agent_init.py` | `src/agent/agent_loop.c` | 🔄 | Partial — initialization spread across C files |
| B02 | `agent/conversation_loop.py` | `src/agent/agent_loop.c` | ✅ | Core loop |
| B03 | `agent/agent_runtime_helpers.py` | — | ❌ | Runtime helpers not ported |
| B04 | `agent/async_utils.py` | — | ❌ | Async patterns — C is synchronous |
| B05 | `agent/chat_completion_helpers.py` | `src/agent/llm_client.c` | 🔄 | Partial |
| B06 | `agent/tool_dispatch_helpers.py` | `src/tools/registry.c` | 🔄 | Partial |
| B07 | `agent/tool_executor.py` | `src/main.c` (tool dispatch in agent loop) | 🔄 | Partial |
| B08 | `agent/tool_guardrails.py` | — | ❌ | Tool guardrails not ported |
| B09 | `agent/tool_result_classification.py` | — | ❌ | Result classification not ported |
| B10 | `agent/prompt_builder.py` | `src/agent/context.c` | 🔄 | Partial |
| B11 | `agent/prompt_caching.py` | — | ❌ | Prompt caching not ported |
| B12 | `agent/system_prompt.py` | `src/agent/context.c` | 🔄 | Partial |
| B13 | `agent/subdirectory_hints.py` | — | ❌ | Subdirectory detection |
| B14 | `agent/shell_hooks.py` | — | ❌ | Shell hooks |
| B15 | `agent/iteration_budget.py` | `src/agent/budget_tracker.c` | ✅ | |
| B16 | `agent/credential_pool.py` | `src/agent/credential_pool.c` | ✅ | |
| B17 | `agent/credential_sources.py` | `src/secrets.c` | 🔄 | Bitwarden ported, more sources needed |
| B18 | `agent/retry_utils.py` | `src/agent/fallback_routing.c` + agent loop | ✅ | Retry + fallback |
| B19 | `agent/rate_limit_tracker.py` | `src/tools/rate_limit.c` + `src/agent/fallback_routing.c` | 🔄 | Partial |
| B20 | `agent/error_classifier.py` | `src/hermes_error.c` | 🔄 | K01-K20 done, error classification incomplete |

### B21-B40: Provider Adapters

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| B21 | `agent/anthropic_adapter.py` | `src/agent/provider_anthropic.c` | ✅ | |
| B22 | `agent/bedrock_adapter.py` | `src/agent/provider_bedrock.c` | ✅ | |
| B23 | `agent/azure_identity_adapter.py` | `src/agent/provider_azure.c` | ✅ | |
| B24 | `agent/codex_responses_adapter.py` | — | ❌ | Codex Responses API not ported |
| B25 | `agent/codex_runtime.py` | — | ❌ | Codex runtime not ported |
| B26 | `agent/gemini_cloudcode_adapter.py` | — | ❌ | Gemini Cloud Code adapter |
| B27 | `agent/gemini_native_adapter.py` | — | ❌ | Gemini native adapter |
| B28 | `agent/gemini_schema.py` | — | ❌ | Gemini schema helpers |
| B29 | `agent/google_code_assist.py` | — | ❌ | Google Code Assist |
| B30 | `agent/google_oauth.py` | — | ❌ | Google OAuth flow |
| B31 | `agent/lmstudio_reasoning.py` | — | ❌ | LM Studio reasoning |
| B32 | `agent/moonshot_schema.py` | — | ❌ | Moonshot AI schema |
| B33 | `agent/nous_rate_guard.py` | — | ❌ | Nous rate guard |
| B34 | `agent/openai_adapter.py` | `src/agent/provider_openai.c` | ✅ | |
| B35 | `agent/openrouter_adapter.py` | `src/agent/provider_openrouter.c` | ✅ | |
| B36 | `agent/deepseek_adapter.py` | `src/agent/provider_deepseek.c` | ✅ | |
| B37 | `agent/xai_adapter.py` | `src/agent/provider_xai.c` | ✅ | |
| B38 | `agent/google_adapter.py` | `src/agent/provider_google.c` | ✅ | |
| B39 | `agent/custom_adapter.py` | `src/agent/provider_custom.c` | ✅ | |
| B40 | `agent/provider_registry.py` | `src/agent/provider.c` | ✅ | |

### B41-B60: Memory, Context, Compression

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| B41 | `agent/context_compressor.py` | — | ❌ | Context compression |
| B42 | `agent/conversation_compression.py` | — | ❌ | Conversation compression |
| B43 | `agent/context_engine.py` | — | ❌ | Context engine plugin |
| B44 | `agent/context_references.py` | `src/agent/context.c` | 🔄 | Partial |
| B45 | `agent/memory_manager.py` | `src/tools/memory.c` + plugin system | 🔄 | Partial |
| B46 | `agent/memory_provider.py` | `lib/libplugin` + plugin_honcho | ✅ | Plugin-based |
| B47 | `agent/manual_compression_feedback.py` | — | ❌ | Compression feedback UI |
| B48 | `agent/markdown_tables.py` | — | ❌ | Markdown table formatting |

### B61-B80: Skills, Bundles, Curators

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| B61 | `agent/skill_commands.py` | `src/tools/skill_mgmt.c` | 🔄 | Partial |
| B62 | `agent/skill_utils.py` | `src/tools/skills.c` | 🔄 | Partial |
| B63 | `agent/skill_bundles.py` | — | ❌ | Skill bundles |
| B64 | `agent/skill_preprocessing.py` | — | ❌ | Skill preprocessing |
| B65 | `agent/curator.py` | — | ❌ | Session curator |
| B66 | `agent/curator_backup.py` | — | ❌ | Curator backup |
| B67 | `agent/trajectory.py` | — | ❌ | Trajectory recording |
| B68 | `agent/onboarding.py` | — | ❌ | Onboarding flow |
| B69 | `agent/portal_tags.py` | — | ❌ | Portal tags |
| B70 | `agent/process_bootstrap.py` | — | ❌ | Process bootstrap |

### B81-B100: Image, Video, Web Search, LSP, Transport, Misc

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| B81 | `agent/image_gen_provider.py` | `src/tools/image_gen.c` + plugin | 🔄 | Partial |
| B82 | `agent/image_gen_registry.py` | — | ❌ | Image gen registry |
| B83 | `agent/image_routing.py` | — | ❌ | Image routing |
| B84 | `agent/video_gen_provider.py` | — | ❌ | Video generation |
| B85 | `agent/video_gen_registry.py` | — | ❌ | Video gen registry |
| B86 | `agent/web_search_provider.py` | `src/tools/web.c` + `src/tools/x_search.c` | 🔄 | Partial |
| B87 | `agent/web_search_registry.py` | — | ❌ | Web search registry |
| B88 | `agent/lsp/` (9 files) | — | ❌ | LSP integration not ported |
| B89 | `agent/transports/` (11 files) | `lib/libmcp` + `src/mcp_serve.c` | 🔄 | Partial — MCP transport done, more needed |
| B90 | `agent/auxiliary_client.py` | — | ❌ | Auxiliary LLM client |
| B91 | `agent/background_review.py` | — | ❌ | Background review |
| B92 | `agent/browser_provider.py` | `src/tools/browser.c` | ❌ | Browser provider stub |
| B93 | `agent/browser_registry.py` | — | ❌ | Browser registry |
| B94 | `agent/account_usage.py` | — | ❌ | Account usage tracking |
| B95 | `agent/display.py` | `src/cli/display.c` + `display_core.c` | ✅ | |
| B96 | `agent/file_safety.py` | `src/tools/url_safety.c` + sandbox | 🔄 | Partial |
| B97 | `agent/i18n.py` | — | ❌ | Internationalization |
| B98 | `agent/insights.py` | — | ❌ | Agent insights |
| B99 | `agent/message_sanitization.py` | `src/agent/sanitize.c` | 🔄 | Partial |
| B100 | `agent/model_metadata.py` | `src/agent/provider_metadata.c` | ✅ | |
| B101 | `agent/models_dev.py` | — | ❌ | Model dev tools |
| B102 | `agent/plugin_llm.py` | `src/agent/plugin_ext.c` | 🔄 | Partial |
| B103 | `agent/redact.py` | `src/agent/redact.c` | ✅ | |
| B104 | `agent/stream_diag.py` | — | ❌ | Stream diagnostics |
| B105 | `agent/think_scrubber.py` | — | ❌ | Think block scrubber |
| B106 | `agent/title_generator.py` | `src/agent/title.c` | ✅ | |
| B107 | `agent/usage_pricing.py` | — | ❌ | Usage/pricing |


**Sector B: ~30/107 complete (28%) — BIGGEST GAP**

---

## Sector C: CLI (Python: 95 files → C: ~9 files)

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| C01 | `hermes_cli/main.py` | `src/cli/main.c` | ✅ | Entry point |
| C02 | `hermes_cli/commands.py` | `src/cli/commands.c` | ✅ | ~148 handlers |
| C03 | `hermes_cli/config.py` | `src/cli/config.c` | ✅ | |
| C04 | `hermes_cli/skin_engine.py` | `lib/libskin` + `src/cli/display.c` | ✅ | |
| C05 | `hermes_cli/completion.py` | — | ❌ | Tab completion |
| C06 | `hermes_cli/banner.py` | `src/cli/display.c` | ✅ | |
| C07 | `hermes_cli/colors.py` | `lib/libansi` | ✅ | |
| C08 | `hermes_cli/cli_output.py` | `src/cli/display.c` | ✅ | |
| C09 | `hermes_cli/curses_ui.py` | `src/cli/tui_fullscreen.c` | 🔄 | Partial TUI |
| C10 | `hermes_cli/setup.py` | — | ❌ | Setup wizard |
| C11 | `hermes_cli/auth.py` | `src/secrets.c` | 🔄 | Partial |
| C12 | `hermes_cli/auth_commands.py` | — | ❌ | Auth CLI commands |
| C13 | `hermes_cli/profiles.py` | `src/cli/config.c` | ✅ | |
| C14 | `hermes_cli/providers.py` | — | ❌ | Provider management CLI |
| C15 | `hermes_cli/models.py` | — | ❌ | Model management CLI |
| C16 | `hermes_cli/model_catalog.py` | — | ❌ | Model catalog |
| C17 | `hermes_cli/model_normalize.py` | — | ❌ | Model name normalization |
| C18 | `hermes_cli/model_switch.py` | — | ❌ | Model switching |
| C19 | `hermes_cli/plugins.py` | — | ❌ | Plugin management CLI |
| C20 | `hermes_cli/plugins_cmd.py` | — | ❌ | Plugin CLI commands |
| C21 | `hermes_cli/gateway.py` | — | ❌ | Gateway CLI commands |
| C22 | `hermes_cli/cron.py` | `src/cron/cron_cli.c` | 🔄 | Partial |
| C23 | `hermes_cli/kanban.py` | plugin_kanban | ✅ | Plugin-backed |
| C24 | `hermes_cli/kanban_db.py` | — | ❌ | Kanban DB operations |
| C25 | `hermes_cli/kanban_decompose.py` | — | ❌ | Kanban decomposition |
| C26 | `hermes_cli/kanban_diagnostics.py` | — | ❌ | Kanban diagnostics |
| C27 | `hermes_cli/kanban_specify.py` | — | ❌ | Kanban specification |
| C28 | `hermes_cli/kanban_swarm.py` | — | ❌ | Kanban swarm |
| C29 | `hermes_cli/logs.py` | — | ❌ | Log viewer CLI |
| C30 | `hermes_cli/debug.py` | — | ❌ | Debug CLI |
| C31 | `hermes_cli/doctor.py` | — | ❌ | Diagnostic CLI |
| C32 | `hermes_cli/dump.py` | — | ❌ | State dump CLI |
| C33 | `hermes_cli/status.py` | — | ❌ | Status CLI |
| C34 | `hermes_cli/backup.py` | — | ❌ | Backup CLI |
| C35 | `hermes_cli/migrate.py` | — | ❌ | Migration CLI |
| C36 | `hermes_cli/uninstall.py` | — | ❌ | Uninstall CLI |
| C37 | `hermes_cli/relaunch.py` | — | ❌ | Relaunch logic |
| C38 | `hermes_cli/voice.py` | — | ❌ | Voice CLI |
| C39 | `hermes_cli/web_server.py` | — | ❌ | Web server CLI |
| C40 | `hermes_cli/webhook.py` | — | ❌ | Webhook CLI |
| C41 | `hermes_cli/pty_bridge.py` | — | ❌ | PTY bridge |
| C42 | `hermes_cli/stdio.py` | — | ❌ | stdio transport |
| C43 | `hermes_cli/clipboard.py` | — | ❌ | Clipboard support |
| C44 | `hermes_cli/checkpoints.py` | — | ❌ | Checkpoint CLI |
| C45 | `hermes_cli/session_recap.py` | — | ❌ | Session recap |
| C46 | `hermes_cli/tips.py` | — | ❌ | Tips display |
| C47 | `hermes_cli/timeouts.py` | — | ❌ | Timeout configuration |
| C48 | `hermes_cli/env_loader.py` | — | ❌ | Environment loader |
| C49 | `hermes_cli/_parser.py` | — | ❌ | Config parser |
| C50 | `hermes_cli/_subprocess_compat.py` | — | ❌ | Subprocess compat |
| C51 | `hermes_cli/azure_detect.py` | — | ❌ | Azure detection |
| C52 | `hermes_cli/browser_connect.py` | — | ❌ | Browser connection |
| C53 | `hermes_cli/bundles.py` | — | ❌ | Bundle management |
| C54 | `hermes_cli/callbacks.py` | — | ❌ | Callback management |
| C55 | `hermes_cli/claw.py` | — | ❌ | CLAW mode |
| C56 | `hermes_cli/codex_models.py` | — | ❌ | Codex model info |
| C57 | `hermes_cli/codex_runtime_plugin_migration.py` | — | ❌ | Codex migration |
| C58 | `hermes_cli/codex_runtime_switch.py` | — | ❌ | Codex switch |
| C59 | `hermes_cli/copilot_auth.py` | — | ❌ | Copilot auth |
| C60 | `hermes_cli/curator.py` | — | ❌ | Curator CLI |
| C61 | `hermes_cli/default_soul.py` | — | ❌ | Default soul/personality |
| C62 | `hermes_cli/dep_ensure.py` | — | ❌ | Dependency ensure |
| C63 | `hermes_cli/dingtalk_auth.py` | — | ❌ | DingTalk auth |
| C64 | `hermes_cli/fallback_cmd.py` | — | ❌ | Fallback commands |
| C65 | `hermes_cli/gateway_windows.py` | — | ❌ | Windows gateway |
| C66 | `hermes_cli/goals.py` | — | ❌ | Goals CLI |
| C67 | `hermes_cli/hooks.py` | — | ❌ | Hooks CLI |
| C68 | `hermes_cli/inventory.py` | — | ❌ | Inventory CLI |
| C69 | `hermes_cli/mcp_config.py` | — | ❌ | MCP config CLI |
| C70 | `hermes_cli/memory_setup.py` | — | ❌ | Memory setup CLI |
| C71 | `hermes_cli/nous_subscription.py` | — | ❌ | Nous subscription |
| C72 | `hermes_cli/oneshot.py` | — | ❌ | One-shot mode |
| C73 | `hermes_cli/pairing.py` | — | ❌ | Pairing CLI |
| C74 | `hermes_cli/platforms.py` | — | ❌ | Platform management |
| C75 | `hermes_cli/profile_describer.py` | — | ❌ | Profile description |
| C76 | `hermes_cli/profile_distribution.py` | — | ❌ | Profile distribution |
| C77 | `hermes_cli/proxy/` (5 files) | — | ❌ | Proxy support |
| C78 | `hermes_cli/pt_input_extras.py` | — | ❌ | Prompt toolkit extras |
| C79 | `hermes_cli/runtime_provider.py` | — | ❌ | Runtime provider |
| C80 | `hermes_cli/secrets_cli.py` | `src/secrets.c` | 🔄 | Partial |
| C81 | `hermes_cli/security_advisories.py` | — | ❌ | Security advisories |
| C82 | `hermes_cli/send_cmd.py` | — | ❌ | Send command |
| C83 | `hermes_cli/skills_config.py` | — | ❌ | Skills config |
| C84 | `hermes_cli/skills_hub.py` | — | ❌ | Skills hub CLI |
| C85 | `hermes_cli/slack_cli.py` | — | ❌ | Slack CLI |
| C86 | `hermes_cli/tools_config.py` | `src/tools/tool_config.c` | ✅ | |
| C87 | `hermes_cli/vercel_auth.py` | — | ❌ | Vercel auth |
| C88 | `hermes_cli/xai_retirement.py` | `src/xai_retirement.c` | ✅ | |
| C89 | `hermes_cli/session_search_tool.py` | `src/tools/session_search.c` | ✅ | |

**Sector C: ~13/89 complete (15%) — MASSIVE GAP**

---

## Sector D: Tools (Python: 95 files → C: 37 files)

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| D01 | `tools/terminal_tool.py` | `src/tools/terminal.c` | ✅ | |
| D02 | `tools/file_tools.py` | `src/tools/file.c` + `file_batch.c` | ✅ | |
| D03 | `tools/web_tools.py` | `src/tools/web.c` | ✅ | |
| D04 | `tools/skills_tool.py` | `src/tools/skills.c` | ✅ | |
| D05 | `tools/skills_guard.py` | — | ❌ | Skills security guard |
| D06 | `tools/skills_hub.py` | `src/skills_hub.c` | ✅ | |
| D07 | `tools/skills_sync.py` | — | ❌ | Skills sync |
| D08 | `tools/vision_tools.py` | `src/tools/vision.c` | ✅ | |
| D09 | `tools/tts_tool.py` | `src/tools/tts.c` | ✅ | |
| D10 | `tools/memory_tool.py` | `src/tools/memory.c` | ✅ | |
| D11 | `tools/cronjob_tools.py` | `src/tools/cronjob.c` | ✅ | |
| D12 | `tools/todo_tool.py` | `src/tools/todo.c` | ✅ | |
| D13 | `tools/delegate_tool.py` | `src/tools/delegate.c` | ✅ | |
| D14 | `tools/clarify_tool.py` | `src/tools/clarify.c` | ✅ | |
| D15 | `tools/discord_tool.py` | `src/tools/discord.c` | ✅ | |
| D16 | `tools/send_message_tool.py` | `src/tools/send_message.c` | ✅ | |
| D17 | `tools/session_search_tool.py` | `src/tools/session_search.c` | ✅ | |
| D18 | `tools/kanban_tools.py` | plugin_kanban | ✅ | |
| D19 | `tools/browser_tool.py` | `src/tools/browser.c` (stub) | ❌ | Browser tool stub |
| D20 | `tools/browser_camofox.py` | — | ❌ | Camofox browser |
| D21 | `tools/browser_camofox_state.py` | — | ❌ | Camofox state |
| D22 | `tools/browser_cdp_tool.py` | — | ❌ | CDP browser tool |
| D23 | `tools/browser_dialog_tool.py` | — | ❌ | Browser dialog |
| D24 | `tools/browser_supervisor.py` | — | ❌ | Browser supervisor |
| D25 | `tools/computer_use/` (6 files) | `src/tools/computer_use.c` (stub) | ❌ | Computer use stub |
| D26 | `tools/computer_use_tool.py` | — | ❌ | Computer use tool |
| D27 | `tools/code_execution_tool.py` | `src/tools/exec_code.c` | ✅ | |
| D28 | `tools/image_generation_tool.py` | `src/tools/image_gen.c` | 🔄 | Partial |
| D29 | `tools/video_generation_tool.py` | — | ❌ | Video generation |
| D30 | `tools/transcription_tools.py` | — | ❌ | Audio transcription |
| D31 | `tools/x_search_tool.py` | `src/tools/x_search.c` | ✅ | |
| D32 | `tools/homeassistant_tool.py` | `src/tools/homeassistant.c` | ✅ | |
| D33 | `tools/yuanbao_tools.py` | — | ❌ | Yuanbao tool |
| D34 | `tools/patch_parser.py` | `src/tools/patch.c` | ✅ | |
| D35 | `tools/approval.py` | `src/tools/approval.c` | ✅ | |
| D36 | `tools/url_safety.py` | `src/tools/url_safety.c` | ✅ | |
| D37 | `tools/slash_confirm.py` | — | ❌ | Slash confirm |
| D38 | `tools/tirith_security.py` | `src/tools/tirith.c` | ✅ | |
| D39 | `tools/tool_result_storage.py` | `src/tools/result_storage.c` | ✅ | |
| D40 | `tools/tool_backend_helpers.py` | — | ❌ | Tool backend helpers |
| D41 | `tools/tool_output_limits.py` | — | ❌ | Tool output limiting |
| D42 | `tools/fal_common.py` | — | ❌ | Fal AI common |
| D43 | `tools/feishu_doc_tool.py` | — | ❌ | Feishu doc tool |
| D44 | `tools/feishu_drive_tool.py` | — | ❌ | Feishu drive tool |
| D45 | `tools/mcp_tool.py` | `src/tools/mcp_tool.c` | ✅ | |
| D46 | `tools/mcp_oauth.py` | — | ❌ | MCP OAuth |
| D47 | `tools/mcp_oauth_manager.py` | — | ❌ | MCP OAuth manager |
| D48 | `tools/environments/` (12 files) | — | ❌ | Terminal environment backends |
| D49 | `tools/voice_mode.py` | `src/tools/voice_mode.c` | ✅ | |
| D50 | `tools/neutts_synth.py` | — | ❌ | Neural TTS synthesis |
| D51 | `tools/mixture_of_agents_tool.py` | — | ❌ | MoA tool |
| D52 | `tools/openrouter_client.py` | — | ❌ | OpenRouter client |
| D53 | `tools/osv_check.py` | — | ❌ | OSV vulnerability check |
| D54 | `tools/process_registry.py` | `src/tools/process.c` | ✅ | |
| D55 | `tools/schema_sanitizer.py` | — | ❌ | Schema sanitizer |
| D56 | `tools/skill_provenance.py` | — | ❌ | Skill provenance |
| D57 | `tools/skill_usage.py` | — | ❌ | Skill usage tracking |
| D58 | `tools/credential_files.py` | `src/secrets.c` | 🔄 | Partial |
| D59 | `tools/checkpoint_manager.py` | `src/agent/checkpoint.c` | ✅ | |
| D60 | `tools/budget_config.py` | `src/agent/budget_tracker.c` | ✅ | |
| D61 | `tools/binary_extensions.py` | — | ❌ | Binary file extensions |
| D62 | `tools/ansi_strip.py` | `lib/libansi` | ✅ | |
| D63 | `tools/fuzzy_match.py` | — | ❌ | Fuzzy matching |
| D64 | `tools/debug_helpers.py` | — | ❌ | Debug helpers |
| D65 | `tools/env_passthrough.py` | — | ❌ | Env passthrough |
| D66 | `tools/interrupt.py` | — | ❌ | Interrupt handling |
| D67 | `tools/lazy_deps.py` | — | ❌ | Lazy dependencies |
| D68 | `tools/managed_tool_gateway.py` | — | ❌ | Managed tool gateway |
| D69 | `tools/microsoft_graph_auth.py` | — | ❌ | Microsoft Graph auth |
| D70 | `tools/microsoft_graph_client.py` | — | ❌ | Microsoft Graph client |
| D71 | `tools/path_security.py` | `src/tools/url_safety.c` | 🔄 | Partial |
| D72 | `tools/website_policy.py` | — | ❌ | Website policy |
| D73 | `tools/xai_http.py` | — | ❌ | xAI HTTP helpers |
| D74 | `tools/registry.py` | `src/tools/registry.c` | ✅ | |

**Sector D: ~32/74 complete (43%)**

---

## Sector E: Gateway (Python: 61 files → C: ~20 files)

| ID | Python Module | C Equivalent | Status | Notes |
|----|--------------|-------------|--------|-------|
| E01 | `gateway/run.py` | `src/gateway/server.c` | ✅ | |
| E02 | `gateway/session.py` | `src/gateway/server.c` | ✅ | |
| E03 | `gateway/config.py` | inline | ✅ | |
| E04 | `gateway/delivery.py` | `src/gateway/server.c` | ✅ | |
| E05 | `gateway/hooks.py` | — | ❌ | Gateway hooks system |
| E06 | `gateway/builtin_hooks/` | — | ❌ | Built-in hooks |
| E07 | `gateway/channel_directory.py` | — | ❌ | Channel directory |
| E08 | `gateway/mirror.py` | — | ❌ | Mirror/crosspost |
| E09 | `gateway/pairing.py` | — | ❌ | Pairing system |
| E10 | `gateway/restart.py` | — | ❌ | Gateway restart |
| E11 | `gateway/runtime_footer.py` | — | ❌ | Runtime footer |
| E12 | `gateway/session_context.py` | — | ❌ | Session context |
| E13 | `gateway/shutdown_forensics.py` | — | ❌ | Shutdown forensics |
| E14 | `gateway/slash_access.py` | — | ❌ | Slash access control |
| E15 | `gateway/status.py` | — | ❌ | Gateway status |
| E16 | `gateway/sticker_cache.py` | — | ❌ | Sticker cache |
| E17 | `gateway/stream_consumer.py` | — | ❌ | Stream consumer |
| E18 | `gateway/display_config.py` | — | ❌ | Display config |
| E19 | `gateway/memory_monitor.py` | — | ❌ | Memory monitor |
| E20 | `gateway/platform_registry.py` | — | ❌ | Platform registry |
| E21 | `gateway/whatsapp_identity.py` | — | ❌ | WhatsApp identity |
| E22-E61 | 40 platform files (telegram, discord, slack, etc.) | `src/gateway/platforms/*.c` | ✅ | 19 C platforms match key Python ones |

**Sector E: ~22/61 complete (36%) — Heavy on platform plumbing depth**

---

## Sector F: MCP / Transports (Python: 13 files → C: ~4 files)

| ID | Component | C Equivalent | Status | Notes |
|----|-----------|-------------|--------|-------|
| F01 | `mcp_serve.py` | `src/mcp_serve.c` + `lib/libmcp` | ✅ | |
| F02 | `agent/transports/base.py` | `lib/libmcp` | ✅ | |
| F03 | `agent/transports/chat_completions.py` | — | ❌ | Chat completions transport |
| F04 | `agent/transports/anthropic.py` | — | ❌ | Anthropic transport |
| F05 | `agent/transports/bedrock.py` | — | ❌ | Bedrock transport |
| F06 | `agent/transports/codex.py` | — | ❌ | Codex transport |
| F07 | `agent/transports/codex_app_server.py` | — | ❌ | Codex app server |
| F08 | `agent/transports/codex_app_server_session.py` | — | ❌ | Codex app session |
| F09 | `agent/transports/codex_event_projector.py` | — | ❌ | Codex events |
| F10 | `agent/transports/hermes_tools_mcp_server.py` | — | ❌ | Hermes MCP server |
| F11 | `agent/transports/types.py` | — | ❌ | Transport types |

**Sector F: 2/11 complete (18%)**

---

## Sector G: ACP (Python: 10 files → C: 1 file)

| ID | Component | C Equivalent | Status | Notes |
|----|-----------|-------------|--------|-------|
| G01 | `acp_adapter/server.py` | `src/acp/server.c` | 🔄 | Partial |
| G02 | `acp_adapter/auth.py` | — | ❌ | ACP auth |
| G03 | `acp_adapter/edit_approval.py` | — | ❌ | Edit approval |
| G04 | `acp_adapter/entry.py` | — | ❌ | Entry point |
| G05 | `acp_adapter/events.py` | — | ❌ | Event system |
| G06 | `acp_adapter/permissions.py` | — | ❌ | Permissions |
| G07 | `acp_adapter/session.py` | — | ❌ | Sessions |
| G08 | `acp_adapter/tools.py` | — | ❌ | Tool management |

**Sector G: 1/9 complete (11%)**

---

## Sector H: Cron (Python: 3 files → C: 8 files)

| ID | Component | C Equivalent | Status | Notes |
|----|-----------|-------------|--------|-------|
| H01 | `cron/scheduler.py` | `src/cron/scheduler.c` | 🔄 | Partial |
| H02 | `cron/jobs.py` | `src/cron/jobs.c` | 🔄 | Partial |
| H03 | `cron/__init__.py` | various | ✅ | |

**Sector H: Mostly covered by C's 8 cron files**

---

## Sector I: TUI (Python: 8 tui_gateway files + TypeScript UI)

| ID | Component | C Equivalent | Status | Notes |
|----|-----------|-------------|--------|-------|
| I01 | `tui_gateway/server.py` | — | ❌ | TUI server |
| I02 | `tui_gateway/render.py` | `lib/libtui` | 🔄 | Partial |
| I03 | `tui_gateway/slash_worker.py` | — | ❌ | TUI slash worker |
| I04 | `tui_gateway/transport.py` | — | ❌ | TUI transport |
| I05 | `tui_gateway/event_publisher.py` | — | ❌ | TUI events |
| I06 | `tui_gateway/entry.py` | — | ❌ | TUI entry |
| I07 | `tui_gateway/ws.py` | — | ❌ | WebSocket server |
| I08 | React Ink UI (TypeScript) | `src/cli/tui_fullscreen.c` | 🔄 | ncurses-based partial |

**Sector I: 1/8 complete (12%)**

---

## Sector J: Plugins (Python: extensive plugin/ directory)

C has 10 .so plugins built. Need to audit which Python plugin backends exist vs C.

| ID | Python Plugin | C Plugin | Status | Notes |
|----|--------------|----------|--------|-------|
| J01 | `plugins/honcho-memory/` | `plugin_honcho.so` | ✅ | |
| J02 | `plugins/kanban/` | `plugin_kanban.so` | ✅ | |
| J03 | `plugins/spotify/` | `plugin_spotify.so` | ✅ | |
| J04 | `plugins/hermes-achievements/` | `plugin_achievements.so` | ✅ | |
| J05 | `plugins/observability/` | `plugin_observability.so` | ✅ | |
| J06 | `plugins/images_gen/` | `plugin_image_gen.so` | ✅ | |
| J07 | `plugins/disk-cleanup/` | `plugin_disk_cleanup.so` | ✅ | |
| J08 | `plugins/file-memory/` | `plugin_file_memory.so` | ✅ | |
| J09 | `plugins/skills/` | `plugin_skills.so` | ✅ | |
| J10 | `plugins/google-meet/` | `plugin_google_meet.so` | ✅ | |
| J11 | `plugins/memory/mem0/` | — | ❌ | mem0 memory provider |
| J12 | `plugins/memory/supermemory/` | — | ❌ | Supermemory provider |
| J13 | `plugins/context_engine/` | — | ❌ | Context engine plugin |
| J14 | `plugins/model-providers/` | — | ❌ | Model provider plugins |
| J15 | `plugins/example-dashboard/` | — | ❌ | Dashboard |
| J16 | `plugins/strike-freedom-cockpit/` | — | ❌ | Strike plugin |
| J17 | `plugins/platforms/` | — | ❌ | Platform plugins |

**Sector J: 10/17 complete (59%)**

---

## Sector K: Skills (extensive skills/ + optional-skills/ directories)

Not a primary C translation target — these are runtime skill scripts. C needs the skills runtime (loading, execution, guardrails) which is partially done.

---

## Sector L: Config

| ID | Feature | Status | Notes |
|----|---------|--------|-------|
| L01 | 322 YAML keys | ✅ | Complete |
| L02 | Profiles | ✅ | |
| L03 | `${VAR:-default}` env expansion | ✅ | |
| L04 | `!include` directive | ✅ | |
| L05 | Config watching (A05) | ❌ | Hot-reload on file change |
| L06 | Config migration (A06) | ❌ | Version migration |

**Sector L: 4/6 complete (67%)**

---

## Sector M: Tests (Python: 1,192 files → C: 116 files)

| ID | Area | C Test Files | Python Test Files | Status |
|----|------|-------------|-------------------|--------|
| M01 | Libraries | 30 | ~200 | ✅ |
| M02 | Agent core | ~8 | ~150 | 🔄 |
| M03 | Provider tests | ~12 | ~100 | 🔄 |
| M04 | CLI tests | ~5 | ~120 | ❌ |
| M05 | Tool tests | ~25 | ~200 | 🔄 |
| M06 | Gateway tests | ~5 | ~80 | ❌ |
| M07 | Cron tests | 4 | ~20 | 🔄 |
| M08 | Plugin tests | ~8 | ~50 | 🔄 |
| M09 | Integration/E2E | 0 | ~200 | ❌ |
| M10 | ACP tests | 0 | ~30 | ❌ |
| M11 | TUI tests | 0 | ~15 | ❌ |
| M12 | Security tests | ~8 | ~50 | 🔄 |
| M13 | Config tests | 1 | ~30 | 🔄 |

**Sector M: ~48/116 test parity (incomplete, C tests cover different ground)**

---

## Sector N: Build/Doc

| ID | Feature | Status | Notes |
|----|---------|--------|-------|
| N01 | Docker build | ✅ | |
| N02 | CI workflow | ✅ | |
| N03 | Cross-compile | ✅ | |
| N04 | Man page | ✅ | |
| N05 | Doxygen docs | ✅ | |
| N06 | CHANGELOG | ✅ | |
| N07 | SECURITY.md | ✅ | |
| N08 | ARCHITECTURE.md | ✅ | |
| N09 | Windows build (O02) | ❌ | MSVC/MinGW |
| N10 | Release automation | ✅ | |

**Sector N: 9/10 complete (90%)**

---

## Sector O: Upstream Sync

| ID | Task | Status | Notes |
|----|------|--------|-------|
| O01 | Catch up 183 commits | ❌ | ~180 Python changes without C equivalents |
| O02 | Digest.py — fix file map | 🔄 | FILE_MAP needs updating |
| O03 | Auto-sync workflow | ❌ | CI should auto-detect upstream changes |

**Sector O: 0/3 complete (0%)**

---

## Sector P: Security

| ID | Feature | Python Source | C Status | Notes |
|----|---------|--------------|----------|-------|
| P01 | File safety (read-deny) | `agent/file_safety.py` | 🔄 | Partial |
| P02 | Path security | `tools/path_security.py` | 🔄 | Partial |
| P03 | Credential files | `tools/credential_files.py` | 🔄 | Partial |
| P04 | TIRITH policy engine | `tools/tirith_security.py` | ✅ | |
| P05 | Sandbox escape detection | `src/sandbox_escape.c` | ✅ | |
| P06 | URL safety | `tools/url_safety.py` | ✅ | |
| P07 | Redact secrets | `agent/redact.py` | ✅ | |
| P08 | Skill guardrails | `tools/skills_guard.py` | ❌ | |
| P09 | Slash access control | `gateway/slash_access.py` | ❌ | |
| P10 | Approval system | `tools/approval.py` | ✅ | |

**Sector P: 6/10 complete (60%)**

---

## Sector Q: Cross-Cutting

| ID | Feature | Status | Notes |
|----|---------|--------|-------|
| Q01 | Token counting | ✅ | |
| Q02 | Secure parent dir | ✅ | |
| Q03 | Key leakage prevention | ✅ | |
| Q04 | Vendor key derivation | ✅ | |
| Q05 | Local trust | ✅ | |

**Sector Q: 5/5 complete (100%)**

---

## Sector R: Provider-Specific API Quirks

| ID | Provider | API Quirk | Status |
|----|----------|-----------|--------|
| R01 | Anthropic | Thinking blocks (reasoning_effort) | ✅ |
| R02 | Anthropic | Cache control headers | ✅ |
| R03 | Anthropic | Tool format conversion (OpenAI→Anthropic) | ✅ |
| R04 | Google | safety_settings array | ✅ |
| R05 | Google | systemInstruction separate field | ✅ |
| R06 | xAI | Encrypted reasoning replay | ✅ |
| R07 | xAI | Web search (Responses API) | ✅ |
| R08 | DeepSeek | FIM (Fill-in-the-Middle) | ✅ |
| R09 | DeepSeek | Context caching TTL | ✅ |
| R10 | Azure | deployment_id + api_version | ✅ |
| R11 | Bedrock | AWS Sigv4 signing | ✅ |
| R12-R18 | Various | (7 remaining quirks) | ❌ |

**Sector R: 11/18 complete (61%)**

---

## Summary

| Sector | Total Gaps | Complete | % |
|--------|-----------|----------|---|
| A: Core | 16 | 12 | 75% |
| B: Agent | 107 | 30 | 28% |
| C: CLI | 89 | 13 | 15% |
| D: Tools | 74 | 32 | 43% |
| E: Gateway | 61 | 22 | 36% |
| F: MCP/Transports | 11 | 2 | 18% |
| G: ACP | 9 | 1 | 11% |
| H: Cron | 3 | 3 | 100% |
| I: TUI | 8 | 1 | 12% |
| J: Plugins | 17 | 10 | 59% |
| L: Config | 6 | 4 | 67% |
| N: Build/Doc | 10 | 9 | 90% |
| O: Upstream | 3 | 0 | 0% |
| P: Security | 10 | 6 | 60% |
| Q: Cross-cut | 5 | 5 | 100% |
| R: Provider quirks | 18 | 11 | 61% |
| **Total** | **~447** | **~161** | **36%** |

**Remaining: ~286 gaps** — targeting ~300 after refinement.

### Priority Tiers by Sector

**P0 — Unlocks core user experience**
- C01-C89: CLI commands (74 missing → piecemeal completion)
- D20-D28: Browser tools (stubs → real implementations)
- B41-B48: Context compression (reduces token usage)

**P1 — Fills major functional gaps**
- B04, B05, B08, B09: Agent runtime, tool dispatch, guardrails
- E01-E61: Gateway depth (hooks, mirror, pairing, config)
- G01-G09: ACP adapter (VS Code/JetBrains integration)
- F01-F11: Transport backends (Codex, Gemini, Bedrock)

**P2 — Quality / polish**
- M01-M13: Test coverage (116→300+)
- O01: Upstream catch-up (183 commits)
- I01-I08: TUI depth
- L05-L06: Config watching + migration

**P3 — Stretch / niche**
- B88: LSP integration
- B94: Account usage
- B97: i18n
- D48: Environment backends (Docker, SSH, Modal, etc.)
