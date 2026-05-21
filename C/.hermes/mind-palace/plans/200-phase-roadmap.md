# Slermes C — 200-Phase Feature Roadmap (DA v3)

**DA Audit:** C = ~8% of Python (57K LOC vs 433K+). 72 CLI names exist but most are printf stubs. 16 config keys vs 424+ in Python (3.8%). 53 tools vs 64+dynamic MCP. 3 providers vs 29. 0 MCP. 0 plugins. 43 tests vs ~17,000.

**Previous DA claims inflated.** This roadmap corrects that — every phase is a specific, verifiable implementation.

---

## Config System (Phases 1–25)
*Current: 16 fields in hermes_config_t. Target: 424+ keys from Python config.yaml schema.*

- **P1:** Expand config struct — provider group (model, base_url, api_key, api_mode, max_tokens, temperature, top_p, stop_sequences, fallback_model, service_tier, reasoning_effort)
- **P2:** Expand config struct — display group (skin, banner, spinner_style, stream, show_thinking, indicator, statusbar, footer, compact_mode)
- **P3:** Expand config struct — agent group (max_iterations, max_tool_calls_round, max_output_tokens, system_prompt, profile, personality, verbose_level, fast_mode, quiet_mode, compress_threshold)
- **P4:** Expand config struct — tools group (allow_background, local_process_cleanup, approval_mode, approval_timeout, max_result_size, vision_model, terminal_timeout)
- **P5:** Expand config struct — delegation group (max_concurrent_children, max_spawn_depth, child_timeout, child_model, child_provider, child_max_turns)
- **P6:** Expand config struct — browser group (cdp_endpoint, browser_type, headless, viewport_width, viewport_height, timeout, enable_javascript)
- **P7:** Expand config struct — memory group (memory_provider, memory_limit, memory_model, memory_ttl_days, memory_auto_save)
- **P8:** Expand config struct — compression group (compression_model, compression_strategy, compression_threshold_ratio, min_messages_before_compress, preserve_system)
- **P9:** Expand config struct — cron group (cron_dir, max_concurrent_jobs, job_timeout, retention_days, notify_on_failure)
- **P10:** Expand config struct — notification group (notify_provider, notify_on_complete, notify_on_error, notify_on_approval, notification_sound)
- **P11:** Expand config struct — security group (website_blocklist, command_allowlist, redact_patterns, tirith_policy_path, url_safety_check_domains)
- **P12:** Expand config struct — session group (session_db_path, session_retention_days, auto_save_interval, session_compress, session_store_trajectories)
- **P13:** Expand config struct — plugin group (plugin_dirs, enabled_plugins, plugin_config_paths)
- **P14:** Expand config struct — MCP group (mcp_servers, mcp_timeout, mcp_max_tools, mcp_auth_enabled)
- **P15:** Implement config key validation — type checking, range validation, enum validation, required key checks
- **P16:** Implement config env var override — map each config key to HERMES_* env var, parse bool/int/string
- **P17:** Implement config profiles — named config sets in ~/.slermes/profiles/, switch with --profile
- **P18:** Implement config diff/show — `hermes config diff` between active and defaults, `hermes config show` with section filtering
- **P19:** Implement config watch — hot-reload config changes (inotify on config.yaml)
- **P20:** Implement config import/export — `hermes config export` to stdout, `hermes config import <file>`
- **P21:** Port Python's `hermes_constants.py` — get_hermes_home(), profile resolution, XDG path support
- **P22:** Port Python's config merge logic — deep_merge for config.yaml + profile + env + CLI flags
- **P23:** Implement config category groups — structured /config command with tab-completion per category
- **P24:** Implement config schema generation — auto-generate JSON Schema from config_t definition
- **P25:** Config migration — automatic upgrade path when config struct changes (version field)

## CLI Commands (Phases 26–40)
*Current: 72 command names, most are printf stubs. Target: 69+ commands with real implementations.*

- **P26:** /new — full session lifecycle (persist old, create new, reset agent state, allocate message buffer)
- **P27:** /clear — preserve session metadata, wipe messages, re-init agent loop
- **P28:** /undo — store message history snapshots, restore previous turn
- **P29:** /save, /load, /sessions — full session DB CRUD: list with search, save with metadata, load with state restore
- **P30:** /stats — token counting (input/output per turn, total, model breakdown), message count, iteration count
- **P31:** /conv, /history — paginated conversation output, filter by role, search within history
- **P32:** /model — list available models from provider, switch model mid-session, show model info
- **P33:** /config — categorized key browsing, set/get/delete with validation, tab complete keys
- **P34:** /topic, /personality — system prompt management, predefined presets, custom prompt injection
- **P35:** /tools, /tools-verify — per-tool status (available/unavailable), tool count, toolset membership
- **P36:** /help — command-specific help, category browsing, example usage
- **P37:** /reset, /retry, /branch, /snapshot — advanced session management with state snapshots
- **P38:** /compress — configurable compression strategy, preview compressed context before applying
- **P39:** /approve, /deny, /yolo — approval queue management, persistent allowlist editing
- **P40:** /plugins, /cron, /platform — full plugin/scheduler/platform management UIs

## Tool Infrastructure (Phases 41–55)
*Current: 53 tools registered, 15 missing static + MCP. Target: full 64+ dynamic tool system.*

- **P41:** Port discord tool — WebSocket Gateway, REST API, message send/guild info with Discord bot token auth
- **P42:** Port discord_admin tool — channel management, member management, moderation, audit log
- **P43:** Port feishu_doc_read tool — Feishu/Lark document read via API token
- **P44:** Port feishu_drive tools (4 tools) — add/list/reply comments on drive files
- **P45:** Port mixture_of_agents tool — parallel LLM calls, response aggregation, router model
- **P46:** Port video_analyze tool — ffmpeg frame extraction + vision model per frame
- **P47:** Port video_generate tool — video generation API integration (Runway, Pika, etc.)
- **P48:** Port yuanbao tools (6 tools) — query group info/members, search/upload/send stickers, send DM
- **P49:** Implement tool result storage — store large results in temp files, return file reference
- **P50:** Implement tool output limits — configurable per-tool max_result_size, auto-truncation
- **P51:** Implement tool backend helpers — standardized HTTP/API pattern for tool implementations
- **P52:** Implement tool timeout — per-tool configurable timeout, graceful cancellation
- **P53:** Implement tool retry — automatic retry with exponential backoff for transient failures
- **P54:** Implement tool dependency injection — per-tool config injection (API keys, endpoints)
- **P55:** Implement tool wildcard/pattern matching — toolset-based tool enabling/disabling

## MCP System (Phases 56–70)
*Current: 0%. Python has full stdio/SSE MCP client with 5,620 LOC. This is the single biggest missing feature.*

- **P56:** MCP client library — WebSocket/SSE transport, JSON-RPC message framing, request/response matching
- **P57:** MCP stdio transport — spawn MCP server process, pipe stdin/stdout JSON-RPC, process lifecycle
- **P58:** MCP tool registration — dynamic tool schema from MCP list_tools, register in tool registry
- **P59:** MCP tool dispatch — call_tools via MCP protocol, result wrapping, error handling
- **P60:** MCP config discovery — parse mcp_servers section from config.yaml, auto-connect at startup
- **P61:** MCP server lifecycle — start/stop/restart, health checks, crash recovery, max retries
- **P62:** MCP SSE transport — Server-Sent Events for HTTP-based MCP servers, reconnection
- **P63:** MCP auth — OAuth token exchange, API key injection into MCP requests
- **P64:** MCP OAuth manager — credential storage, refresh flow, per-server OAuth config
- **P65:** MCP tool namespace — prefix MCP tools with server name, collision detection
- **P66:** MCP tool filtering — allow/blocklist per MCP server, tool name patterns
- **P67:** MCP resource access — read_resource, subscribe_resource for MCP servers
- **P68:** MCP sampling — server-initiated LLM sampling request handling
- **P69:** MCP prompt templates — list_prompts, get_prompt from MCP servers
- **P70:** MCP root filesystem — expose workspace directories to MCP servers via roots/list

## Provider System (Phases 71–85)
*Current: 3 hardcoded providers. Python has 29 model-provider plugins + built-ins.*

- **P71:** Abstract provider interface — provider_ops struct (init, chat, stream, embed, list_models, count_tokens)
- **P72:** Provider plugin system — .so/.dylib provider plugins loaded via plugin_registry
- **P73:** OpenAI provider — full chat completions, streaming, tool calling, function calling, vision
- **P74:** Anthropic provider — Messages API, streaming, tool use, thinking, extended thinking
- **P75:** Google provider — Gemini API, native tool calling, streaming, safety settings
- **P76:** OpenRouter provider — model routing, fallback, provider preferences, pricing display
- **P77:** AWS Bedrock provider — Converse API, cross-region inference, model selection
- **P78:** Azure provider — Azure OpenAI service, managed identity, deployment names
- **P79:** DeepSeek provider — chat completions, FIM, context caching
- **P80:** xAI provider — Grok API, function calling
- **P81:** Custom provider — user-defined base_url/api_key, model name passthrough
- **P82:** Credential pool — multiple API keys per provider, rotation on rate-limit, quota tracking
- **P83:** Fallback model routing — ordered fallback list, health-check before fallback, cool-off for failed models
- **P84:** Budget tracking — per-session token budget, per-turn cost tracking, spend alerts
- **P85:** Provider metadata — model capabilities (vision/function_calling/streaming/thinking), context window, pricing

## Agent Loop (Phases 86–100)
*Current: 332 lines in agent_loop.c. Python's run_agent.py is ~12,000 lines.*

- **P86:** Iteration budget — max_tokens, max_cost, max_time tracking, graceful shutdown
- **P87:** Tool call parallelism — parallel tool dispatch for independent calls, result collection
- **P88:** Grace call — one extra LLM call after budget exhausted to produce final response
- **P89:** Interrupt handling — SIGINT handler, clean shutdown mid-tool-call, state save on interrupt
- **P90:** Conversation history management — message eviction (oldest/cheapest), context window tracking
- **P91:** System prompt caching — cache system prompt across turns when provider supports it
- **P92:** Prefill messages — support for system-prompt-level prefill (Anthropic-style)
- **P93:** Tool result classification — mark tool results as error/fatal/success, conditionally abort loop
- **P94:** Reasoning content — extract and store reasoning from providers (Anthropic thinking, DeepSeek reasoning)
- **P95:** Stream diagnostic — token-level stream timing, latency breakdown per provider
- **P96:** Conversation compression — smart context summarization via LLM, configurable strategy
- **P97:** Manual compression feedback — user-rated compression quality, adaptive threshold
- **P98:** Checkpoint manager — auto-save agent state every N turns, rollback to checkpoint
- **P99:** Agent initialization — credential pool setup, plugin load, MCP connect, tool verification
- **P100:** Background review — automatic code/plan review after tool completion

## Gateway Platforms (Phases 101–115)
*Current: 19 platform C files. Python has more features per platform.*

- **P101:** Gateway server — full connection pool, message queue, per-platform rate limiting
- **P102:** Gateway session management — per-chat session binding, session persistence across restarts
- **P103:** Platform base class — common init/send/receive/shutdown interface for all platforms
- **P104:** Telegram — full feature parity: inline queries, callback queries, polls, forum topics, media groups
- **P105:** Discord — slash commands, thread management, voice channel text, buttons/select menus
- **P106:** Slack — block kit, message actions, slash commands, channel join/leave events
- **P107:** WhatsApp — business API, message templates, interactive messages, onboarding
- **P108:** Signal — group management, reactions, attachments, quote replies
- **P109:** Matrix — room discovery, event types, read receipts, typing indicators
- **P110:** Email — IMAP idle, attachments, HTML render, threading
- **P111:** SMS — Twilio provider, carrier lookup, MMS support
- **P112:** Webhook — HMAC verification, retry logic, custom headers
- **P113:** DingTalk, WeCom, Weixin, QQ Bot — Chinese platform full feature parity
- **P114:** Feishu — card messages, interactive buttons, doc integration
- **P115:** BlueBubbles — iMessage bridge, tapbacks, reactions

## Delegation System (Phases 116–125)
*Current: 135 lines, basic subprocess spawn. Python has concurrent children, orchestrator mode.*

- **P116:** Concurrent children — thread pool for N parallel subagents, result collection
- **P117:** Child timeout — per-child configurable timeout, SIGKILL on timeout, partial result return
- **P118:** Orchestrator mode — parent delegates to orchestrator, orchestrator spawns leaf agents
- **P119:** Child model override — per-child model/provider selection, different from parent
- **P120:** Child isolation — separate message buffer, separate tool registry view, separate terminal
- **P121:** Result aggregation — collect all child results, deduplicate, format summary
- **P122:** Error propagation — child crash → error report to parent, configurable abort-on-fail
- **P123:** Budget delegation — child inherits proportional budget from parent
- **P124:** Nested delegation (max_spawn_depth > 1) — recursive subagent spawning
- **P125:** Child credential isolation — filtered credential view per child security level

## Plugin System (Phases 126–140)
*Current: example .so loader only. Python has 17 plugin directories.*

- **P126:** Plugin API — plugin_init/plugin_shutdown/plugin_tool/plugin_hook interface
- **P127:** Plugin discovery — scan plugin directories, load metadata, verify compatibility
- **P128:** Plugin lifecycle — version check, dependency resolution, init ordering, shutdown
- **P129:** Plugin config — per-plugin config injection from config.yaml
- **P130:** Memory provider plugins — port honcho, mem0, supermemory, hindsight, byterover, holographic, retaindb, openviking
- **P131:** Context engine plugins — context enrichment, RAG, knowledge base integration
- **P132:** Model provider plugins — generic plugin interface for provider backends
- **P133:** Kanban plugin — board CRUD, ticket assignment, WIP limits, sprint management
- **P134:** Image gen plugin — provider abstraction (Stable Diffusion, DALL-E, Midjourney, ComfyUI)
- **P135:** Video gen plugin — Runway, Pika, Kling provider abstraction
- **P136:** Browser plugin — CDP client plugin, headed browser management
- **P137:** Spotify plugin — OAuth, playback control, search, queue management
- **P138:** Achievement plugin — gamification: milestones, XP, badges
- **P139:** Observability plugin — OpenTelemetry traces, structured logs, metrics export
- **P140:** Web plugin — browser-based automation, form fill, cookie management

## Session DB (Phases 141–150)
*Current: grep-based search. Python has SQLite FTS5.*

- **P141:** SQLite integration — embed sqlite3, session table schema, message table schema
- **P142:** FTS5 full-text search — FTS5 virtual table for messages, ranked search results
- **P143:** Session CRUD — create/read/update/delete with metadata (model, token count, title)
- **P144:** Auto-save — periodic session save (configurable interval), crash recovery
- **P145:** Auto-prune — retention-based deletion, oldest-first eviction, size-based pruning
- **P146:** Session metadata — tags, starred sessions, searchable notes
- **P147:** Session search tool — FTS5-backed web_search equivalent for sessions
- **P148:** Session export — JSON/Markdown export, shareable session link
- **P149:** Session branch — fork session history at message N, new session with shared prefix
- **P150:** Session migration — schema upgrade path, backup before migration

## Memory System (Phases 151–158)
*Current: 117 lines, tool.c basic set/get. Python has provider-backed memory.*

- **P151:** Memory storage abstraction — in-memory, file-based, SQLite, provider plugin
- **P152:** Memory TTL — per-entry expiration, auto-cleanup thread
- **P153:** Memory prioritization — priority-ordered retrieval, sliding window
- **P154:** Memory deduplication — hash-based duplicate detection on insert
- **P155:** Memory search — keyword matching, semantic matching (via LLM rerank)
- **P156:** Memory auto-save — periodic save to persistent store, crash recovery
- **P157:** Memory import/export — JSON dump/load, merge into existing store
- **P158:** Memory compression — compact older entries, summarization via LLM

## Security (Phases 159–168)
*Current: URL safety + path traversal + basic approval.*

- **P159:** Secrets redaction — pattern-based redaction (API keys, tokens, passwords, JWTs)
- **P160:** Website blocklist — domain deny list, content category blocking
- **P161:** Command allowlist — per-tool accepted command patterns, shell injection prevention
- **P162:** Approval timeout — configurable auto-deny timeout, timeout notification
- **P163:** Tirith security policy — YAML-based rule definitions, path/method/content rules
- **P164:** Audit log — all security events log to file: approvals, denials, redactions, violations
- **P165:** Rate limiting — per-tool calls/minute, per-provider RPM, global RPM
- **P166:** Output sanitization — strip sensitive data from tool results before LLM injection
- **P167:** Credential vault — encrypted credential storage, master key unlock
- **P168:** File sandbox — restrict file tools to allowed directories, symlink attack prevention

## Cron/Scheduler (Phases 169–178)
*Current: 223 lines basic loop. Python has full SQLite-backed scheduler.*

- **P169:** SQLite job store — persistent job definitions, execution history, error tracking
- **P170:** Cron expression parser — full cron syntax (5-field standard), second precision
- **P171:** Job locking — prevent duplicate execution, graceful shutdown, SIGTERM handling
- **P172:** Job retry — configurable retry policy, exponential backoff, max retries
- **P173:** Job notification — delivery to home channel, per-job notification config
- **P174:** Job chaining — context_from field, passing output between jobs
- **P175:** Job templating — parameterized job definitions, reusable job templates
- **P176:** Scheduler CLI — full /cron command: list, add, edit, remove, pause, resume, run-now
- **P177:** Script-based jobs — shell/python script execution with output capture
- **P178:** Watchdog mode — no_agent mode, silent-until-failure pattern

## Skills System (Phases 179–188)
*Current: 261 lines basic skills tool. Python has hub/sync/provenance/skill bundles.*

- **P179:** Skill directory scanning — recursive scan, metadata extraction, caching
- **P180:** Skill validation — YAML frontmatter validation, duplicate detection, version check
- **P181:** Skill provenance — origin tracking (local/hub/bundle), last-updated timestamp
- **P182:** Skill sync — pull from remote hub, merge with local, conflict resolution
- **P183:** Skill bundles — alias groups loading multiple skills at once
- **P184:** Skill usage tracking — frequency counter, last-used timestamp, skill recommendations
- **P185:** Skill caching — preload skills into memory, LRU eviction
- **P186:** Skill search — text search across skill content, tag-based filtering
- **P187:** Skill curator — background maintenance, stale detection, auto-update
- **P188:** Skill dependencies — cross-skill references, dependency resolution, loading order

## TUI (Phases 189–200)
*Current: ncurses basic (926 lines). Python has full React/Ink UI (41,187 lines).*

- **P189:** TUI layout — split pane: message history | input | tool feed | status bar
- **P190:** TUI input — multi-line input, emoji picker, slash command autocomplete
- **P191:** TUI message display — role-colored messages, code syntax highlight, markdown render
- **P192:** TUI streaming — token streaming in output pane, real-time token counter
- **P193:** TUI tool feed — live tool call status, progress bar, result preview
- **P194:** TUI status bar — model/provider, token usage, iteration count, budget remaining
- **P195:** TUI session browser — session list with search, preview, load/del/export
- **P196:** TUI config editor — interactive config key browser, set/get/explain per key
- **P197:** TUI image viewer — inline image display (sixel/kitty protocol), zoom/pan
- **P198:** TUI theme engine — skin definition files, color schemes, font selection
- **P199:** TUI gateway — JSON-RPC backend for TUI, split TUI and agent processes
- **P200:** TUI mobile mode — responsive layout, touch input, smaller status pane

---

## Key Metrics

| Metric | Current | Target | Gap |
|--------|---------|--------|-----|
| Config keys | 16 | 424+ | 408 missing |
| Tools | 53 (30 stubs) | 64+dynamic | 15 static + MCP missing |
| CLI commands | 72 (most stubs) | 69+ | Most need full impl |
| Providers | 3 | 29+ | 26 missing |
| Gateway platforms | 19 | 19+ | Feature depth missing |
| MCP | 0 | full | 100% missing |
| Plugins | 0 | 17 | 17 missing |
| Tests | 43 | ~17,000 | 99.7% missing |
| Agent loop LOC | 332 | ~12,000 | 97% missing |
| TUI LOC | 926 | ~41,000 | 98% missing |
| Session DB | grep | FTS5 | Full SQLite missing |
| Delegation | basic | concurrent | 95% missing |

**TOTAL completeness: ~8%. 200 phases until parity.**
