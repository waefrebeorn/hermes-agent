# Slermes (C) vs Python Hermes Agent — Comprehensive Gap Analysis

Generated: Analysis of every feature in Python Hermes Agent that has NO equivalent in the C slermes/ implementation.

---

## 1. GATEWAY PLATFORMS

C has: Telegram, Discord, Slack, Matrix, Mattermost, WhatsApp, Webhook (7 platforms)
Python has: ~20+ platforms + plugin system for more

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Signal** | Full adapter (signal.py + rate limiting) | MISSING | P1 | Signal encrypted messaging |
| **Email (IMAP/SMTP)** | Full adapter (email.py) | MISSING | P1 | Email bot platform |
| **SMS (Twilio)** | Full adapter (sms.py) | MISSING | P1 | SMS messaging platform |
| **HomeAssistant** | Full adapter (homeassistant.py) + HA tools | MISSING | P1 | Smart home control + monitoring |
| **DingTalk** | Full adapter (dingtalk.py) | MISSING | P2 | Enterprise messaging (China) |
| **WeCom** | Full adapter (wecom.py + wecom_callback.py + wecom_crypto.py) | MISSING | P2 | Enterprise WeChat (China) |
| **Weixin/iLink** | Full adapter (weixin.py) | MISSING | P2 | WeChat via iLink bridge |
| **Feishu/Lark** | Full adapter (feishu.py + feishu_comment*.py) | MISSING | P2 | Enterprise messaging (China) |
| **BlueBubbles** | Full adapter (bluebubbles.py) | MISSING | P2 | Apple iMessage via BlueBubbles |
| **Yuanbao** | Full adapter (yuanbao.py + yuanbao_media.py + yuanbao_proto.py + yuanbao_sticker.py) | MISSING | P2 | Tencent Yuanbao platform |
| **QQ Bot** | Full adapter (qqbot/ directory) | MISSING | P2 | QQ messaging via Official Bot API v2 |
| **Microsoft Graph (Teams)** | msgraph_webhook.py | MISSING | P2 | Teams/Office365 webhook support |
| **Plugin platforms (IRC, Line, Google Chat, Simplex, Teams)** | plugins/platforms/ (5 additional platforms) | MISSING | P2 | Extensible via plugin system |
| **Gateway session management** | Sophisticated session lifecycle (topics, threads, channels, home channels, pairing) | MISSING | P1 | Python's gateway run.py + session.py |
| **Gateway hooks** | Built-in hooks system (pre_gateway_dispatch, etc.) | MISSING | P1 | Plugin hook lifecycle in gateway |
| **Gateway streaming responses** | Streaming responses to platforms | MISSING | P1 | C sends complete response only |
| **Telegram topics/DM groups** | Topic support, group management | MISSING | P1 | Python has full topic support |
| **Dangerous command approval (gateway)** | /approve, /deny slash commands for gateways | MISSING | P1 | Security: approve dangerous commands remotely |
| **Multi-platform simultaneous** | Run multiple platforms at once from one gateway | MISSING | P1 | C runs one platform at a time |
| **Platform health monitoring** | Pause/resume failing platforms | MISSING | P2 | Platform auto-recovery |

---

## 2. TOOLS

C has: terminal, file/read_file/write_file, web/web_search/web_extract, patch, skills/skill_view/skill_manage, exec_code, clarify, memory, todo, process, send_message, cronjob, session_search, tts, vision, delegate, x_search (18 tools)
Python has: 40+ built-in tools + dynamic plugin tools

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **browser_navigate/browser_snapshot/browser_click/browser_type/browser_scroll/browser_back/browser_press** | Full browser automation (camofox/browser_tool.py + supervisor) | MISSING | P0 | 12 browser tools total |
| **browser_get_images/browser_vision/browser_console/browser_cdp/browser_dialog** | Advanced browser tools (CDP, console, dialogs) | MISSING | P0 | CDP-based browser control |
| **image_generate** | Multi-provider image generation | MISSING | P0 | DALL-E, Imagen, Flux, etc. |
| **video_analyze** | Video analysis tool | MISSING | P2 | Not in default toolset |
| **video_generate** | Video generation (FAL, xAI) | MISSING | P2 | Plugin-backed |
| **mixture_of_agents** | MoA tool for improved reasoning | MISSING | P2 | Multi-agent reasoning |
| **computer_use** | macOS desktop control (CUA) | MISSING | P2 | Background cursor control |
| **discord / discord_admin** | Discord interaction tools | MISSING | P1 | Read channels, threads, admin |
| **ha_list_entities / ha_get_state / ha_list_services / ha_call_service** | Home Assistant smart home tools | MISSING | P1 | Smart home control |
| **kanban_* (8 tools)** | Kanban multi-agent coordination | MISSING | P1 | Task orchestration |
| **feishu_doc_read / feishu_drive_*** | Feishu document tools | MISSING | P2 | Read/write Feishu docs |
| **spotify_* (7 tools)** | Spotify playback/library tools | MISSING | P2 | Music control |
| **yuanoa_* (5 tools)** | Yuanbao platform tools | MISSING | P2 | Messaging platform tools |
| **text_to_speech** | TTS tool (Edge TTS, ElevenLabs, OpenAI, xAI) | EXISTS | — | C has tts.c equivalent |
| **transcription_tools** | Audio transcription (Whisper) | MISSING | P1 | Speech-to-text |
| **voice_mode** | Real-time voice conversation mode | MISSING | P1 | Full voice interaction |
| **MCP tools** | Dynamic MCP server tools (tools/mcp_tool.py) | MISSING | P1 | MCP client integration |
| **Managed tools** | Tools/mcp_oauth.py, tools/mcp_oauth_manager.py | MISSING | P2 | OAuth for MCP |
| **File checkpoint/rollback** | tools/checkpoint_manager.py (1638 LOC) | MISSING | P1 | Filesystem snapshots |
| **Dangerous command approval** | tools/approval.py (1393 LOC) | MISSING | P0 | Security: approve dangerous commands |
| **URL safety validation** | tools/url_safety.py (351 LOC) | MISSING | P1 | SSRF protection |
| **Path security validation** | tools/path_security.py | MISSING | P1 | Path traversal protection |
| **Tirith security scanning** | tools/tirith_security.py (803 LOC) | MISSING | P1 | Pre-exec security scanning |
| **Credential file management** | tools/credential_files.py (436 LOC) | MISSING | P1 | Secure credential storage |
| **Schema sanitization** | tools/schema_sanitizer.py (445 LOC) | MISSING | P2 | Tool schema validation |
| **Budget config** | tools/budget_config.py | MISSING | P2 | Token/iteration budgets |
| **Slash confirmation** | tools/slash_confirm.py | MISSING | P2 | Gateway-side confirmation UI |
| **Tool output limits** | tools/tool_output_limits.py | MISSING | P2 | Truncation and limits |
| **Tool result storage** | tools/tool_result_storage.py | MISSING | P2 | Persistent tool results |
| **Skills hub/sync** | tools/skills_hub.py, tools/skills_sync.py | MISSING | P1 | Community skill sharing |
| **Skills guard/provenance** | tools/skills_guard.py, tools/skill_provenance.py | MISSING | P2 | Skill security/audit |
| **Website policy** | tools/website_policy.py | MISSING | P2 | robots.txt compliance |
| **OSV vulnerability check** | tools/osv_check.py | MISSING | P2 | Package vulnerability scanning |
| **Video generation tool** | Video generation (backends: FAL, xAI) | MISSING | P2 | Plugin-backed |
| **Mixture of agents** | tools/mixture_of_agents_tool.py | MISSING | P2 | Advanced reasoning |
| **Tool backend helpers** | tools/tool_backend_helpers.py | MISSING | P2 | Shared tool utilities |
| **Tool environment backends** | tools/environments/ (docker, ssh, modal, daytona, singularity) | MISSING | P1 | Multiple terminal backends |

---

## 3. CLI COMMANDS AND FEATURES

C has: /help, /exit, /clear, /model (4 commands)
Python has: 50+ slash commands + subcommands + plugin commands

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Slash command registry** | 50+ commands in COMMAND_REGISTRY | 4 commands | P0 | Massive gap |
| **Session management** | /new, /reset, /topic, /clear, /history, /save, /retry, /undo, /title, /handoff, /branch, /compress, /resume, /sessions, /snapshot | MISSING | P0 | Full session lifecycle |
| **Configuration** | /config, /model, /personality, /statusbar, /verbose, /footer, /yolo, /reasoning, /fast, /skin, /indicator, /voice, /busy | MISSING | P1 | Full config control |
| **Tools & Skills** | /tools, /toolsets, /skills, /bundles, /cron, /curator, /kanban, /reload, /reload-mcp, /reload-skills, /browser, /plugins | MISSING | P0 | Full tool management |
| **Info** | /commands, /help, /usage, /insights, /platforms, /platform, /copy, /paste, /image, /update, /debug | MISSING | P1 | Rich info commands |
| **Gateway-specific** | /restart, /approve, /deny, /commands (paginated), /sethome, /platform | MISSING | P1 | Gateway admin |
| **Plugin CLI commands** | Plugin CLI subcommands (hermes <plugin> ...) | MISSING | P1 | Plugin registration of CLI cmds |
| **Autocomplete** | prompt_toolkit autocomplete + subcommands | MISSING | P2 | Tab-completion |
| **CLI subcommands** | hermes chat, gateway, setup, cron, doctor, status, logs, checkpoints, mcp, models, plugins, tools, profiles, config, backup, uninstall... (194 CLI entry points) | Just --version, gateway, cron, --tui | P0 | 190+ missing CLI subcommands |
| **Setup wizard** | Interactive hermes setup (profile, provider, keys) | MISSING | P1 | First-run setup |
| **TUI (Terminal UI)** | Full React/Ink TUI (ui-tui + tui_gateway) | Partial (ncurses fullscreen) | P2 | C has basic ncurses TUI |
| **Log viewing** | hermes logs --follow, --level, --session | MISSING | P1 | Log management |
| **Soul/session recap** | /recap, session summaries | MISSING | P2 | Session recall |
| **Profile management** | Multiple profiles, profile switching | MISSING | P1 | Multi-profile support |
| **Goals system** | /goal, /subgoal — standing goals | MISSING | P2 | Persistent goals |
| **Background tasks** | /background, /queue, /steer | MISSING | P2 | Async prompts |
| **Agent status** | /agents, /tasks | MISSING | P2 | Running agents list |
| **Clipboard support** | /copy, /paste (with image attachment) | MISSING | P2 | Clipboard integration |
| **Skin engine** | /skin — data-driven YAML skins (6 built-in + user) | MISSING | P1 | Full skin system (see §7) |
| **Checkpoint management** | /checkpoints CLI subcommand (status, list, prune, clear) | MISSING | P1 | Filesystem snapshot management |
| **Doctor** | hermes doctor — system health check | MISSING | P2 | Diagnostic tool |
| **Debug reports** | hermes debug — upload debug info | MISSING | P2 | Support tool |
| **Kanban** | /kanban — multi-profile board management | MISSING | P2 | Task orchestration CLI |
| **MCP serve** | hermes mcp serve — MCP server for conversations | MISSING | P1 | MCP integration |
| **Proxy mode** | hermes proxy | MISSING | P2 | Proxy functionality |
| **Security advisories** | hermes_cli/security_advisories.py | MISSING | P2 | Security notifications |

---

## 4. AGENT LOOP FEATURES (run_agent.py)

C has: Basic loop (send → tool calls → send → ...) with interrupt and max iterations
Python AIAgent: ~60 constructor parameters, 4149 LOC

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Tool use enforcement** | Forces tool use when needed | MISSING | P1 | Loop can continue without tools |
| **Service tier** | _SERVICE_TIER priorities for API calls | MISSING | P1 | Priority-based API routing |
| **Checkpointing** | Filesystem checkpoint store + auto-save/restore | MISSING | P1 | Checkpoint per tool call |
| **Iteration budget** | IterationBudget class with dynamic adjustments | MISSING | P1 | Budget tracking and enforcement |
| **Budget grace call** | One extra turn allowed at end of budget | MISSING | P2 | Grace period |
| **Prefill messages** | /completion prefilling | MISSING | P2 | Pre-fill assistant responses |
| **Reasoning config** | Configurable reasoning effort (none/minimal/low/medium/high/xhigh) | MISSING | P1 | Reasoning level control |
| **Fallback model chain** | Fallback to alternate models on failure | MISSING | P1 | Model resilience |
| **Credential pool** | Multi-provider credential management | MISSING | P1 | Key rotation/sharing |
| **Streaming** | Streaming LLM responses to CLI/gateway | EXISTS | — | C has streaming support |
| **Conversation compression** | Automatic context compression (agent/conversation_compression.py) | MISSING | P1 | Smart context reduction |
| **Manual compression** | /compress command + feedback loop | MISSING | P2 | User-guided compression |
| **Background review** | agent/background_review.py | MISSING | P2 | Post-hoc review |
| **Trajectory recording** | Full trajectory capture (agent/trajectory.py) | MISSING | P2 | Training data capture |
| **Prompt caching** | agent/prompt_caching.py | MISSING | P1 | Cache optimization |
| **Context halving** | Automatic context halving when approaching token limit | MISSING | P1 | Context management |
| **Error classification** | agent/error_classifier.py | MISSING | P2 | Error pattern detection |
| **Retry utils** | agent/retry_utils.py (jittered backoff) | MISSING | P2 | Smart retry logic |
| **Rate limit tracking** | agent/rate_limit_tracker.py | MISSING | P1 | API rate limit management |
| **Think scrubbing** | agent/think_scrubber.py — clean up reasoning | MISSING | P2 | Reasoning cleanup |
| **Message sanitization** | agent/message_sanitization.py | MISSING | P2 | Input/output safety |
| **Thread safety** | Thread-safe agent loop with interrupt queuing | MISSING | P1 | Multi-thread safety |
| **Tool executor** | agent/tool_executor.py — async tool dispatch | MISSING | P1 | Tool execution management |
| **Tool guardrails** | agent/tool_guardrails.py | MISSING | P1 | Tool safety guardrails |
| **Tool result classification** | agent/tool_result_classification.py | MISSING | P2 | Tool result analysis |
| **Auxiliary clients** | agent/auxiliary_client.py — multi-provider | MISSING | P2 | Provider fallback orchestration |
| **ACP adapter transport** | Responses API via Codex transport | MISSING | P1 | Alternative API modes |
| **Transports (Anthropic, Bedrock, Codex, Chat)** | 4 transport implementations | MISSING | P1 | Multiple API format support |
| **Subagents** | Delegate tool + subagent management | EXISTS | — | C has delegate tool |
| **Architecture (tool use)** | model_tools.py — tool orchestration + handle_function_call | Partial | P2 | C has basic registry dispatch |

---

## 5. PLUGIN SYSTEM FEATURES

C has: Basic .so plugin loading (libplugin/plugin.c) with hello_plugin example
Python has: Full plugin system with lifecycle hooks, discovery, tools, commands

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Plugin manager** | PluginManager class with discovery from 4 sources | MISSING | P0 | 1593 LOC plugin system |
| **Plugin lifecycle hooks** | pre_tool_call, post_tool_call, transform_terminal_output, transform_tool_result, transform_llm_output, pre_llm_call, post_llm_call, pre_api_request, post_api_request, on_session_start/end/finalize/reset, subagent_stop, pre_gateway_dispatch, pre_approval_request, post_approval_response (18 hooks) | MISSING | P0 | Rich lifecycle hooks |
| **Plugin LLM access** | PluginLlm — host-owned LLM facade | MISSING | P1 | Plugins use host's LLM |
| **Plugin tool registration** | register_tool() with override capability | MISSING | P0 | Dynamic tool registration |
| **Plugin CLI commands** | register_cli_command() | MISSING | P1 | Plugin CLI integration |
| **Plugin slash commands** | register_command() in-session commands | MISSING | P1 | Plugin slash commands |
| **Plugin message injection** | inject_message() into active conversation | MISSING | P2 | Cross-plugin messaging |
| **Plugin context engine** | register_context_engine() replacement | MISSING | P2 | Custom context engines |
| **Plugin YAML manifest** | plugin.yaml with version, author, requires_env, kind | MISSING | P0 | Manifest system |
| **Plugin kinds** | standalone, backend, exclusive, platform, model-provider | MISSING | P1 | Plugin categorization |
| **Bundled plugins** | 15+ bundled plugins (memory, image_gen, kanban, spotify, google_meet, etc.) | MISSING | P0 | Plugin ecosystem |
| **Plugin enable/disable** | Opt-in via plugins.enabled config, deny-list | MISSING | P1 | Plugin governance |
| **Plugin debugging** | HERMES_PLUGINS_DEBUG=1 verbose logging | MISSING | P2 | Developer tooling |
| **Model provider plugins** | 30+ provider profiles as plugins | MISSING | P0 | Provider ecosystem |
| **Memory provider plugins** | 8 memory backends (honcho, mem0, supermemory, openviking, etc.) | MISSING | P1 | Memory providers |
| **Image generation plugins** | 3 image gen backends (openai, openai-codex, xai) | MISSING | P1 | Image gen providers |
| **Video generation plugins** | 2 video gen backends (fal, xai) | MISSING | P2 | Video gen providers |
| **Web search plugins** | 9 web search backends (brave, ddgs, exa, firecrawl, parallel, searxng, tavily, xai) | MISSING | P1 | Search providers |
| **Browser plugins** | 3 browser automation backends | MISSING | P2 | Browser providers |
| **Platform plugins** | 5 additional gateway platform plugins | MISSING | P2 | Platform extensibility |
| **Observability plugin** | Langfuse integration | MISSING | P2 | Tracing/monitoring |
| **Kanban plugin** | Multi-agent board dispatcher + dashboard | MISSING | P1 | Task orchestration plugin |
| **Google Meet plugin** | Meeting bot (audio bridge, realtime) | MISSING | P2 | Meeting integration |
| **Spotify plugin** | Native Spotify control (client, tools) | MISSING | P2 | Music integration |
| **Achievements plugin** | Gamified achievement tracking | MISSING | P2 | Engagement |
| **Teams pipeline plugin** | Teams meeting pipeline (subscriptions, runtime) | MISSING | P2 | Teams integration |

---

## 6. CRON/SCHEDULER FEATURES

C has: Basic cron scheduler with file-based job store, cron tool for creating jobs
Python has: Full cron system integrated with agent

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Cron job types** | agent-run, shell-command, webhook | MISSING | P1 | Job variety |
| **Cron expressions** | Standard cron syntax | EXISTS | — | C has basic cron support |
| **File-based locking** | ~/.hermes/cron/.tick.lock | EXISTS | — | Both have locking |
| **Asynchronous job execution** | ThreadPoolExecutor for concurrent jobs | MISSING | P1 | C runs serial |
| **Persistent job storage** | JSON file at ~/.hermes/cron/jobs.json | EXISTS | — | Both file-based |
| **Cron UI/commands** | /cron list, add, edit, pause, resume, run, remove | MISSING | P1 | CLI management |
| **Webhook cron jobs** | HTTP/webhook triggering | MISSING | P2 | Remote trigger |
| **Cron job logging** | Job execution logging to agent.log | MISSING | P2 | Audit trail |
| **hermes cron subcommand** | Full CLI management of cron | MISSING | P1 | CLI subcommand |
| **Job environment management** | Per-job env vars, working directory | MISSING | P2 | Job configuration |

---

## 7. SKIN/THEMING FEATURES

C has: libskin with basic skin loading from JSON/YAML (ANSIColors, spinner config)
Python has: Full data-driven skin engine with 6 built-in skins + user skins

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Skin engine** | SkinConfig dataclass with full schema | MISSING | P1 | 57 color keys, spinner, branding |
| **Built-in skins** | default, ares, mono, slate, daylight, warm-lightmode, poseidon, sisyphus (8+) | MISSING | P1 | Pre-built themes |
| **User skins** | YAML files in ~/.hermes/skins/ | MISSING | P1 | User customization |
| **Banner artwork** | Custom ASCII art logo + hero per skin | MISSING | P2 | Visual identity |
| **Animated spinners** | Custom faces, verbs, wings per skin | MISSING | P1 | Animations |
| **Branding customization** | agent_name, welcome/goodbye, prompt, labels | MISSING | P1 | Text customization |
| **Tool emoji overrides** | Per-tool emoji customization | MISSING | P2 | Fun customization |
| **Color schema** | 57 color keys for every UI element | MISSING | P1 | Comprehensive colors |
| **Status bar styling** | Status bar colors (bg, text, strong, dim, good, warn, bad, critical) | MISSING | P1 | Status bar |
| **Completion menu styling** | Menu colors (bg, current, meta) | MISSING | P2 | Autocomplete styling |
| **Skin switching** | /skin command at runtime | MISSING | P1 | Dynamic switching |
| **Skin schema docs** | Full YAML schema documentation | MISSING | P2 | Developer docs |
| **Skin YAML parsing** | Load skins from YAML files | Partial | P1 | C has basic skin loading |

---

## 8. SESSION FEATURES

C has: Basic JSON file session store (libdb), simple save/load/list
Python has: Full SQLite session database with FTS5 search (3273 LOC hermes_state.py)

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **SQLite storage** | Full SQLite session store | MISSING | P1 | C uses flat JSON files |
| **FTS5 full-text search** | Full-text search across all sessions | MISSING | P1 | Search capabilities |
| **Session metadata** | Model, tokens, title, timestamp, tags, platform | MISSING | P1 | Rich metadata |
| **Session browsing** | Interactive session browser (fzf-style) | MISSING | P1 | Session recall |
| **Session naming** | Named sessions with /title | MISSING | P1 | User-friendly session IDs |
| **Session branching** | /branch, /fork — explore alternate paths | MISSING | P1 | Branching conversations |
| **Session compression** | Compress old sessions to save space | MISSING | P2 | Storage optimization |
| **Session export** | Export sessions to JSON/other formats | MISSING | P2 | Data portability |
| **Session deletion** | --delete flag on /quit | MISSING | P2 | Cleanup |
| **Session continuity** | Resume sessions across restarts | EXISTS | — | C has basic load/save |
| **Session search tool** | /session_search with summarization | EXISTS | — | C has session_search tool |
| **Session recaps** | /recap — AI-generated session summary | MISSING | P2 | Session recall |
| **Per-profile sessions** | Profile-aware session isolation | MISSING | P1 | Multi-profile isolation |
| **Legacy migration** | Migrate old session formats | MISSING | P2 | Upgrade path |

---

## 9. PROVIDER SUPPORT

C has: Single LLM provider — generic OpenAI-compatible chat completions (one base_url + api_key)
Python has: 30+ provider profiles + 4 transports + fallback chains + credential pool

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Provider profiles** | 30+ named profiles (openai, anthropic, gemini, deepseek, xai, etc.) | MISSING | P0 | Model config per provider |
| **Provider transports** | ChatCompletions, Anthropic, Bedrock, Codex (4 transports) | MISSING | P0 | C only supports chat_completions |
| **Anthropic API** | Anthropic-specific adapter + message format | MISSING | P1 | Different API format |
| **Bedrock (AWS)** | Bedrock adapter (agent/transports/bedrock.py) | MISSING | P1 | AWS Bedrock |
| **Codex Responses API** | Codex/OpenAI Responses API adapter | MISSING | P1 | Alternative API mode |
| **Codex App Server** | Codex App Server runtime | MISSING | P1 | Codex mode |
| **Gemini** | Gemini native adapter (agent/gemini_native_adapter.py) | MISSING | P1 | Google Gemini |
| **Gemini Code Assist** | Google Code Assist integration | MISSING | P2 | Free tier |
| **OpenRouter** | OpenRouter integration with model routing | MISSING | P1 | Model routing |
| **Provider plugins** | 30+ provider profiles as plugins | MISSING | P0 | Plugin-based providers |
| **Fallback model chain** | Primary → fallback → fallback chain | MISSING | P1 | Model resilience |
| **Credential pool** | Agent-level credential pool with rotation | MISSING | P1 | Multi-key management |
| **Credential sources** | Multiple credential sources (env, file, OAuth) | MISSING | P1 | auth providers |
| **Auxiliary providers** | Secondary providers for different tools | MISSING | P1 | Multi-provider orchestration |
| **Azure identity** | Azure identity adapter | MISSING | P2 | Azure auth |
| **Nous subscription** | Nous subscription management | MISSING | P2 | Subscription auth |
| **Model metadata** | Agent/model_metadata.py — model capabilities | MISSING | P2 | Model info |
| **Model catalog** | hermes models — browse available models | MISSING | P1 | Model discovery |
| **Account usage** | Agent/account_usage.py — usage tracking | MISSING | P2 | Usage monitoring |
| **Usage pricing** | Agent/usage_pricing.py — cost calculation | MISSING | P2 | Cost tracking |
| **Rate limit guard** | Nous rate guard (agent/nous_rate_guard.py) | MISSING | P2 | Rate limiting |

---

## 10. WEB/API FEATURES

C has: Simple webhook HTTP server (webhook.c) for basic API
Python has: Full web app, API server, MCP server, TUI

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Web app** | React/TypeScript web app (web/) with full UI | MISSING | P1 | Browser-based interface |
| **API server** | OpenAI-compatible API server (gateway/platforms/api_server.py) | MISSING | P2 | API endpoint |
| **MCP serve** | MCP stdio server exposing conversations (mcp_serve.py, 897 LOC) | MISSING | P1 | MCP protocol support |
| **ACP server** | ACP integration for editors (VS Code, Zed, JetBrains) | MISSING | P1 | Editor integration |
| **ACP auth** | ACP authentication | MISSING | P2 | ACP security |
| **ACP permissions** | ACP permission system | MISSING | P2 | ACP access control |
| **ACP tools** | ACP tool definitions | MISSING | P2 | ACP tool surface |
| **TUI** | Full React/Ink TUI with React components | Partial | P2 | C has basic ncurses TUI |
| **TUI gateway** | JSON-RPC backend for TUI | MISSING | P2 | TUI backend |
| **TUI streaming** | Live streaming in TUI | MISSING | P2 | Real-time updates |
| **Web server** | Web server CLI subcommand | MISSING | P2 | HTTP serving |
| **Webhook API** | Basic webhook server | EXISTS | — | C has webhook platform |

---

## 11. TESTING INFRASTRUCTURE

C has: 10 test files (test_yaml.c, test_dotenv.c, test_tui.c, test_http.c, test_proc.c, test_template.c, test_json.c, test_cron.c, test_crypto.c, test_db.c)
Python has: ~17,000 tests across 1179+ test files

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Unit tests** | ~500+ test files for individual modules | 10 C tests | P0 | Massive gap in coverage |
| **Integration tests** | tests/integration/ | MISSING | P1 | Full-stack tests |
| **Gateway tests** | tests/gateway/ | MISSING | P1 | Platform-specific tests |
| **Plugin tests** | tests/plugins/ | MISSING | P2 | Plugin testing |
| **E2E tests** | tests/e2e/ | MISSING | P1 | End-to-end testing |
| **CLI tests** | tests/cli/ | MISSING | P1 | CLI testing |
| **Agent tests** | tests/agent/ | MISSING | P1 | Agent loop testing |
| **State tests** | tests/hermes_state/ | MISSING | P1 | Session DB testing |
| **Cron tests** | tests/cron/ | 1 test | P2 | Cron testing |
| **Script tests** | tests/scripts/ | MISSING | P2 | Build/CI testing |
| **Fakes** | tests/fakes/ (mock providers, LLMs) | MISSING | P1 | Mock infrastructure |
| **Stress tests** | tests/stress/ | MISSING | P2 | Performance testing |
| **Conftest** | tests/conftest.py | MISSING | P1 | Test fixtures |
| **Test runner** | scripts/run_tests.sh | test_runner.sh | P1 | C has basic runner |
| **CI/CD** | .github/ workflows | MISSING | P1 | GitHub Actions |
| **Honcho plugin tests** | tests/honcho_plugin/ | MISSING | P2 | Plugin testing |
| **Openviking tests** | tests/openviking_plugin/ | MISSING | P2 | Plugin testing |
| **Skills tests** | tests/skills/ | MISSING | P2 | Skills testing |
| **ACP tests** | tests/acp/ | MISSING | P2 | ACP testing |
| **Run agent tests** | tests/run_agent/ | MISSING | P1 | Core loop testing |
| **Benchmarks** | None explicit | MISSING | P3 | Performance benchmarks |

---

## 12. DOCUMENTATION

C has: README.md, DEPENDENCIES.md, digestion.md (basic docs)
Python has: Full Docusaurus documentation site + extensive inline docs

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Docusaurus site** | website/ — full documentation site | MISSING | P1 | Comprehensive docs |
| **Contributing guide** | CONTRIBUTING.md (44K) | MISSING | P1 | Developer onboarding |
| **Security policy** | SECURITY.md (15K) | MISSING | P1 | Security docs |
| **Release notes** | 10+ release notes (v0.2–v0.14) | MISSING | P2 | Changelog |
| **AGENTS.md** | AI coding assistant guide (51K) | MISSING | P1 | AI development guide |
| **Platform guide** | ADDING_A_PLATFORM.md | MISSING | P1 | Platform developer guide |
| **Plugin docs** | Inline plugin system docs | MISSING | P1 | Plugin development |
| **Config YAML example** | cli-config.yaml.example (57K) | MISSING | P1 | Configuration reference |
| **Environment example** | .env.example (23K) | MISSING | P1 | Environment reference |
| **API docs** | Inline docstrings throughout | Basic | P2 | Code documentation |
| **Chinese README** | README.zh-CN.md | MISSING | P3 | Internationalization |

---

## 13. SECURITY FEATURES

C has: Basic crypto (SHA-256, HMAC, Base64, random bytes via OpenSSL)
Python has: Comprehensive security tooling

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Dangerous command approval** | Pattern detection + approval system (1393 LOC) | MISSING | P0 | Critical: command safety |
| **URL safety/SSRF protection** | Block private IPs, cloud metadata endpoints | MISSING | P0 | Critical: SSRF prevention |
| **Path traversal protection** | Validate file paths against traversal | MISSING | P0 | Critical: path security |
| **Tirith pre-exec scanning** | Homograph URLs, pipe-to-interpreter, injection | MISSING | P0 | Critical: threat detection |
| **Credential file management** | Secure credential file detection/storage | MISSING | P0 | Critical: credential safety |
| **Schema sanitization** | Tool schema validation | MISSING | P1 | Tool input safety |
| **Skills guard** | Skill provenance and safety checks | MISSING | P1 | Skill safety |
| **Slash confirmation** | Confirmation for destructive commands | MISSING | P1 | Safety confirmation |
| **Approval lifecycle hooks** | Plugin hooks for approval events | MISSING | P1 | Extensible approval |
| **YOLO mode** | Skip all approvals (opt-in) | MISSING | P2 | User override |
| **Security advisories** | CLI security advisory display | MISSING | P2 | Security notifications |
| **OSV vulnerability check** | Package vulnerability scanning | MISSING | P2 | Supply chain security |
| **OCR for images in approvals** | Extract text from images for approval context | MISSING | P2 | Visual approval context |

---

## 14. OTHER FEATURES

| Feature | Python Status | C Status | Priority | Notes |
|---------|-------------|---------|----------|-------|
| **Batch runner** | batch_runner.py (1321 LOC) — parallel batch processing | MISSING | P1 | Bulk data processing |
| **Mini SWE runner** | mini_swe_runner.py — software engineering agent | MISSING | P2 | SWE automation |
| **Trajectory compressor** | trajectory_compressor.py (1508 LOC) | MISSING | P2 | Training data compression |
| **Data generation** | toolset_distributions.py (364 LOC) — toolset sampling | MISSING | P2 | Training data generation |
| **Skills hub** | tools/skills_hub.py — community skill sharing | MISSING | P1 | Skill ecosystem |
| **Skills curator** | agent/curator.py — background skill maintenance | MISSING | P2 | Skill management |
| **Onboarding** | agent/onboarding.py — new user onboarding | MISSING | P2 | First-run experience |
| **i18n** | agent/i18n.py + locales/ | MISSING | P3 | Internationalization |
| **Nix flake** | flake.nix + flake.lock + nix/ | MISSING | P2 | Nix packaging |
| **Docker** | Dockerfile + docker-compose.yml + docker/ | MISSING | P2 | Container support |
| **Packaging** | packaging/ — distribution packaging | MISSING | P2 | Release packaging |
| **Shell hooks** | agent/shell_hooks.py — shell integration | MISSING | P2 | Shell enhancement |
| **Subdirectory hints** | agent/subdirectory_hints.py — workdir hints | MISSING | P2 | Workdir management |
| **Portal tags** | agent/portal_tags.py | MISSING | P3 | Metadata tagging |
| **Codex runtime** | agent/codex_runtime.py + agent/codex_runtime_switch.py | MISSING | P2 | Codex mode |
| **Plans** | .plans/ plans/ — development plans | MISSING | P3 | Planning docs |
| **Assets** | assets/ — static resources | MISSING | P3 | Brand assets |
| **Makefile targets** | C Makefile has upstream-sync, upstream-merge, what-changed, test-libs | — | P2 | Maintenance tooling |
| **Digest script** | digest.py (C project health tracking) | Python-only | P2 | C project has digest.py too |

---

## SUMMARY

### Priority Distribution

| Priority | Count | Description |
|----------|-------|-------------|
| **P0** | ~25 | Critical gaps — security, core infrastructure, essential tools |
| **P1** | ~75 | High priority — major feature gaps, platform coverage, testing |
| **P2** | ~60 | Medium priority — nice-to-have features, advanced capabilities |
| **P3** | ~5 | Low priority — internationalization, minor polish |

### Key Findings

1. **Security is the biggest concern** — C has NO dangerous command approval, NO URL safety/SSRF protection, NO path traversal protection, NO Tirith scanning, and NO credential management. These are P0 gaps.

2. **Plugin system is entirely missing** — Python has a 1593 LOC plugin manager with 18 lifecycle hooks, 30+ provider plugins, 8 memory plugins, 5 platform plugins, and plugin-based extensibility for virtually everything. C only has basic .so loading.

3. **Provider support is trivial** — C only supports generic OpenAI-compatible chat completions. Python has 30+ provider profiles, 4 different API transports (Anthropic, Bedrock, Codex, ChatCompletions), fallback chains, credential pools, and plugin-based providers.

4. **CLI is 50x richer** — C has 4 slash commands (/help, /exit, /clear, /model). Python has 50+ slash commands plus 194 CLI entry points with autocomplete, subcommands, and plugin extensions.

5. **Gateway has 3x fewer platforms** — C has 7 (Telegram, Discord, Slack, Matrix, Mattermost, WhatsApp, Webhook). Python has 20+ including Signal, Email, SMS, HomeAssistant, DingTalk, WeCom, Feishu, and more — plus 5 more via plugins.

6. **Testing is ~2% coverage** — C has 10 test files. Python has ~17,000 tests across 1179+ test files. No comparison.

7. **Tools are 2x fewer** — C has 18 tools. Python has 40+ tools including browser automation (12 tools), kanban (8 tools), spotify (7 tools), Home Assistant (4 tools), and many more.

8. **Total C codebase**: ~7,042 lines across 42 source files + libraries
   **Total Python codebase**: ~26,500+ lines just in the top-level .py files (run_agent.py, cli.py, model_tools.py, hermes_state.py, toolsets.py, trajectory_compressor.py, batch_runner.py) + thousands more in agent/, tools/, plugins/, gateway/, hermes_cli/
