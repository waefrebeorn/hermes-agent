# Slermes C — 200-Gap Roadmap (DA v5, 2026-05-22)

**Overall: ~57% complete.** This roadmap replaces the original 200-phase-roadmap.md.
It tracks the remaining work needed for 1:1 parity with Python Hermes.

**Key:** ⬜ Not started | 🔄 In progress | ✅ Complete | ❓ Partially complete

---

## CONFIG SYSTEM — 116 keys remaining (~64% done)

### G1-G11: Auxiliary provider configs (56 keys) — ⬜
Each sub-provider needs {provider, model, base_url, api_key, timeout, extra_body}:
- G1: auxiliary.vision (7 keys) — model override, api_key, timeout
- G2: auxiliary.web_extract (6 keys)
- G3: auxiliary.compression (6 keys)
- G4: auxiliary.skills_hub (6 keys)
- G5: auxiliary.approval (6 keys)
- G6: auxiliary.mcp (6 keys)
- G7: auxiliary.title_generation (6 keys)
- G8: auxiliary.triage_specifier (6 keys)
- G9: auxiliary.kanban_decomposer (6 keys)
- G10: auxiliary.profile_describer (6 keys)
- G11: auxiliary.curator (6 keys)

### G12: TTS config (17 keys) — ⬜
provider, model, voice, speed, pitch, format, timeout, auto_play, playback_device, volume, cache_dir, cache_max_mb, fallback_provider, fallback_model, language, extra_body, enabled

### G13: Discord config (12 keys) — ⬜
token, guild_id, command_prefix, status, activity_type, activity_name, allowed_roles, allowed_channels, max_message_length, sync_permissions, slash_commands_enabled, thread_auto_archive

### G14: Kanban config (10 keys) — ⬜
board_dir, max_wip, default_sprint_days, auto_archive_days, ticket_template, label_colors, assignee_field, priority_field, estimate_field, auto_sync

### G15: Bedrock config (8 keys) — ⬜
region, access_key_id, secret_access_key, session_token, cross_region_inference, model_id_prefix, use_converse_api, timeout

### G16: Tool loop guardrails (8 keys) — ⬜
max_consecutive_failures, max_tool_calls_per_turn, max_result_bytes, abort_on_safety_violation, rate_limit_per_minute, cooldown_seconds, allowed_patterns, denied_patterns

### G17: Curator config (7 keys) — ⬜
model, provider, base_url, api_key, timeout, policy_file, auto_curate_threshold

### G18: STT config (6 keys) — ⬜
provider, model, language, timeout, endpoint, api_key

### G19: Voice config (6 keys) — ⬜
input_device, output_device, wake_word, vad_threshold, silence_timeout, auto_send

### G20: Approvals config (5 keys) — 🔄
mode, timeout, require_reason, notify_on_pending, auto_approve_patterns
(2 done in tools_config_t, 3 missing)

### G21: Tool output config (3 keys) — 🔄
max_bytes (done), truncation_strategy, include_timestamps

### G22-G34: Small groups (13 groups, 23 total keys) — ⬜
G22: x_search (3) — provider, api_key, engine
G23: Slack (3) — token, signing_secret, app_token
G24: Matrix (3) — homeserver, user_id, access_token
G25: Mattermost (3) — url, token, team_id
G26: Model catalog (3) — auto_update, cache_file, custom_models
G27: OpenRouter (3) — api_key, referer, title
G28: Human delay (3) — min_ms, max_ms, enabled
G29: Web (3) — backend, search_backend, extract_backend (all done in tools)
G30: Telegram (2) — bot_token, allowed_updates
G31: Updates (2) — check_interval, channel
G32: Dashboard (2) — port, theme
G33: Hooks auto-accept (1) — enabled
G34: Misc 1-key groups (prefill_messages_file, privacy, prompt_caching, network, code_execution, command_allowlist, context, timezone, toolsets, file_read_max_chars, goals, model, _config_version)

---

## PROVIDER SYSTEM — 20 remaining (~31% done)

### G35-G54: OpenAI-compatible providers (20) — ⬜
Each needs: provider_ops_t implementation, type enum entry, registration in provider_register_builtins()
Pattern follows existing openai_provider.c / custom_provider.c.

- G35: Nous — nousresearch.com, OpenAI-compat
- G36: Alibaba — dashscope.aliyuncs.com, OpenAI-compat (Qwen models)
- G37: StepFun — api.stepfun.com, OpenAI-compat
- G38: Minimax — api.minimax.chat, OpenAI-compat
- G39: Novita — api.novita.ai, OpenAI-compat
- G40: Z.AI — api.z.ai, OpenAI-compat
- G41: HuggingFace — huggingface.co/api, Inference API
- G42: Arcee — api.arcee.ai, OpenAI-compat
- G43: Ollama Cloud — api.ollama.cloud, OpenAI-compat
- G44: Nvidia — api.nvcf.nvidia.com, OpenAI-compat
- G45: GMI — api.gmi.com, OpenAI-compat
- G46: KiloCode — api.kilocode.ai, OpenAI-compat
- G47: Kimi Coding — api.moonshot.cn, OpenAI-compat
- G48: AI Gateway — configurable gateway, OpenAI-compat
- G49: Azure AI Foundry — Azure AI model catalog
- G50: Xiaomi — api.xiaomi.com, Chinese provider
- G51: Qwen OAuth — OAuth-based Qwen API
- G52: Copilot ACP — ACP protocol (subprocess)
- G53: OpenCode Zen — ACP protocol
- G54: OpenAI Codex — ACP protocol

---

## MCP SYSTEM — 4 phases remaining (~70% done)

- G55: MCP resource access — read_resource, subscribe_resource
- G56: MCP sampling — server-initiated LLM sampling request handling
- G57: MCP prompt templates — list_prompts, get_prompt
- G58: MCP root filesystem — expose workspace directories via roots/list

---

## PLUGIN SYSTEM — 12+ types remaining (~25% done)

- G59: Memory provider plugins (8 types) — honcho, mem0, supermemory, hindsight, byterover, holographic, retaindb, openviking
- G60: Context engine plugins — RAG, knowledge base integration
- G61: Model provider plugins — generic plugin interface for provider backends
- G62: Kanban plugin — board CRUD, WIP limits, sprint management
- G63: Image gen plugin — SD, DALL-E, Midjourney, ComfyUI
- G64: Video gen plugin — Runway, Pika, Kling
- G65: Browser plugin — CDP client, headed browser management
- G66: Spotify plugin — OAuth, playback, queue
- G67: Achievement plugin — gamification, XP, badges
- G68: Observability plugin — OpenTelemetry, metrics
- G69: Web plugin — form fill, cookie management
- G70: Google Meet plugin — meeting integration

---

## GATEWAY PLATFORMS — 2+ features remaining (~95% done)

- G71: Gateway OAuth — full OAuth flow for all platforms
- G72: Gateway rate limiting — per-platform message quotas
- G73: Per-platform media handling — inline queries, polls, forum topics (Telegram depth)

---

## TELEGRAM PLATFORM DEPTH (P104) — ⬜
- G74: Inline query handling
- G75: Callback query handling
- G76: Poll support (send/receive)
- G77: Forum topic management
- G78: Media group support

---

## DISCORD PLATFORM DEPTH (P105) — ⬜
- G79: Slash commands
- G80: Thread management
- G81: Voice channel text
- G82: Buttons/select menus

---

## TOOL SYSTEM — 14 tools remaining (~92% done)

- G83: Discord tool (P41) — WebSocket Gateway, REST API, message send/guild info
- G84: Discord admin tool (P42) — channel/member management, moderation
- G85: Feishu doc read (P43) — document read via API token
- G86: Feishu drive tools (4 tools) (P44) — add/list/reply comments on drive files
- G87: Mixture of agents tool (P45) — parallel LLM calls, response aggregation
- G88: Video analyze (P46) — ffmpeg frame extraction + vision model
- G89: Video generate (P47) — Runway, Pika API integration
- G90-G95: Yuanbao tools (6) (P48) — group info/members, stickers, DM

---

## AGENT LOOP — 5 phases remaining (~75% done)

- G96: Grace call (P88) — one extra LLM call after budget exhausted
- G97: Prefill messages (P92) — system-prompt-level prefill (Anthropic-style)
- G98: Stream diagnostic (P95) — token-level timing, latency breakdown
- G99: Background review (P100) — automatic code/plan review after tools
- G100: Manual compression feedback (P97) — user-rated compression quality

---

## CLI COMMANDS — 13 commands remaining (~85% done)

- G101: /undo (P28) — message history snapshots, restore previous turn
- G102: /save (P29) — session DB save with metadata
- G103: /load (P29) — session load with state restore
- G104: /stats (P30) — token counting, message count, iteration count
- G105: /conv /history (P31) — paginated conversation output, search
- G106: /model (P32) — list models, switch model mid-session
- G107: /topic /personality (P34) — system prompt management
- G108: /reset /retry (P37) — advanced session management
- G109: /branch /snapshot (P37) — state snapshots
- G110: /compress (P38) — configurable compression strategy
- G111: /approve /deny /yolo (P39) — approval queue
- G112: /plugins (P40) — plugin management UI
- G113: /cron /platform (P40) — scheduler/platform UIs

---

## TOOL INFRASTRUCTURE — 7 phases remaining (~70% done)

- G114: Tool result storage (P49) — store large results in temp files
- G115: Tool backend helpers (P51) — standardized HTTP/API pattern
- G116: Tool timeout (P52) — per-tool configurable timeout
- G117: Tool retry (P53) — exponential backoff for transient failures
- G118: Tool dependency injection (P54) — per-tool config injection
- G119: Tool wildcard/pattern matching (P55) — toolset-based enabling
- G120: Tool output limits expansion (P50) — per-tool truncation

---

## DELEGATION SYSTEM — 4 phases remaining (~80% done)

- G121: Orchestrator mode depth (P118) — parent→orchestrator→leaf
- G122: Result aggregation (P121) — dedup, formatting
- G123: Budget delegation (P123) — child inherits proportional budget
- G124: Nested delegation (P124) — max_spawn_depth > 1

---

## SECURITY — 7 phases remaining (~70% done)

- G125: URL safety check depth — website blocklist expansion
- G126: Command allowlist expansion — per-platform rules
- G127: Rate limiting per tool — configurable per-tool rates
- G128: Approval queue persistence — survive restarts
- G129: Credential vault encryption — at-rest encryption
- G130: Audit log export — structured log output
- G131: Session isolation — cross-session data boundaries

---

## TUI — 6 phases remaining (~50% done)

- G132: Theme engine — skin loading, color schemes
- G133: Session browser — list/search/load sessions from TUI
- G134: Config editor — visual config editing within TUI
- G135: Image viewer — inline image display (sixel/kitty)
- G136: Gateway backend — show gateway status/messages in TUI
- G137: Mobile mode — responsive layout for narrow terminals

---

## MEMORY SYSTEM — 4 phases remaining (~90% done)

- G138: Memory import — import from file/stdin
- G139: Memory export — export to JSON/Markdown
- G140: Memory search expansion — FTS5-backed memory search
- G141: Memory auto-save tuning — configurable interval

---

## CRON SYSTEM — 3 phases remaining (~90% done)

- G142: Cron notification channels — per-job delivery target
- G143: Cron job dependencies — job DAG execution order
- G144: Cron webhook trigger — HTTP-triggered jobs

---

## SKILLS SYSTEM — 3 phases remaining (~90% done)

- G145: Skill dependency resolution — skills that depend on other skills
- G146: Skill versioning — version constraints in skill metadata
- G147: Skill hub sync — pull from remote skill registry

---

## SESSION DB — 2 phases remaining (~100% done)

- G148: Session tags — metadata tagging for search
- G149: Session export — JSON/Markdown export with shareable link
- G150: Session starred — favorite/bookmark sessions

---

## CONFIG SYSTEM (depth) — 6 phases remaining

- G151: Config profiles depth (P17) — named profile sets, --profile flag
- G152: Config schema generation (P24) — auto-generate JSON Schema
- G153: Config env var depth (P16) — all 322 keys mappable to env
- G154: Config import (P20) — import from file
- G155: Config watch (P19) — inotify-based hot-reload (SIGHUP exists)
- G156: Config migration (P25) — auto-upgrade path (version field exists)

---

## PROVIDER SYSTEM (depth) — 4 phases remaining

- G157: Credential pool expansion (P82) — auto-rotation, quota tracking
- G158: Budget tracking depth (P84) — per-session budgets with alerts
- G159: Provider plugin system (P72) — .so plugins for providers
- G160: Model catalog auto-update — fetch latest model lists

---

## TESTING — CRITICAL GAP

- G161: Config test suite — all 322 keys, validation, merge, profiles
- G162: Provider test suite — all 29 providers, streaming, tool calls
- G163: CLI command test suite — all 85 commands, flags, edge cases
- G164: Tool test suite — all 80+ tools, input validation, error paths
- G165: Gateway test suite — platform message routing, OAuth
- G166: Agent loop test suite — budget, interrupt, parallel dispatch
- G167: MCP test suite — transport, tool dispatch, lifecycle
- G168: Plugin test suite — load, lifecycle, config injection
- G169: Security test suite — redaction, vault, rate limit, audits
- G170: Integration test suite — end-to-end LLM calls, config→response
- G171: Performance/benchmark tests — token throughput, latency
- G172: Concurrency/stress tests — multi-session, multi-tool
- G173: CI pipeline setup — GitHub Actions, coverage reports
- G174: Golden output tests — regression against captured outputs
- G175: Cross-platform tests — Linux, macOS, WSL

---

## POLICY / GOVERNANCE

- G176: Contribute upstream — submit verified C translation as upstream contribution
- G177: Security disclosure policy — responsible disclosure process
- G178: API key rotation policy — automated key rotation for all 29 providers

---

## CONFIG KEY EXPANSION (catch-all for remaining ~15 small groups)

- G179: credential_pool_strategies config — rotation strategy definitions
- G180: toolsets config — named toolset definitions
- G181: hooks config — extension point registration
- G182: onboarding config — first-run wizard
- G183: personalities config — named personality presets
- G184: quick_commands config — user-defined slash command aliases
- G185: honcho config — honcho memory provider settings
- G186: providers config — provider-specific settings
- G187: whatsapp config — WhatsApp Business API settings

---

## DOCUMENTATION

- G188: API documentation — Doxygen-style headers for all public functions
- G189: Config reference — complete config.yaml schema doc
- G190: CLI reference — command docs with examples
- G191: Architecture guide — design decisions, data flow
- G192: Contributor guide — how to add providers, tools, platforms
- G193: Migration guide — Python→C migration notes
- G194: Gateway platform guide — ADDING_A_PLATFORM.md equivalent

---

## BUILD & PACKAGING

- G195: CMake build system — alternative to Makefile
- G196: Static analysis — clang-tidy, cppcheck integration
- G197: Packaging — APT/Homebrew/container packages
- G198: Versioned releases — semantic versioning, changelog

---

## WRAP-UP & VERIFICATION

- G199: Triple DA v6 audit — full sweep with all remaining gaps assessed
- G200: 1:1 parity confirmed — Python ground truth verified for every feature

---

## Summary

| Area | Progress | Remaining |
|------|----------|-----------|
| Config keys | 64% | ~116 keys (G1-G34, G151-G156, G179-G186) |
| Providers | 31% | 20 (G35-G54, G157-G160) |
| MCP | 70% | 4 (G55-G58) |
| Plugins | 25% | 12 (G59-G70) |
| Gateway | 95% | 3 (G71-G82) |
| Tools | 92% | 14 + 7 (G83-G95, G114-G120) |
| Agent loop | 75% | 5 (G96-G100) |
| CLI commands | 85% | 13 (G101-G113) |
| Delegation | 80% | 4 (G121-G124) |
| Security | 70% | 7 (G125-G131) |
| TUI | 50% | 6 (G132-G137) |
| Memory | 90% | 4 (G138-G141) |
| Cron | 90% | 3 (G142-G144) |
| Skills | 90% | 3 (G145-G147) |
| Session DB | 100% | 2 (G148-G150) |
| Tests | <1% | 15 (G161-G175) |
| Docs | <1% | 7 (G188-G194) |
| Build/Pkg | <1% | 4 (G195-G198) |
| Wrap-up | — | 2 (G199-G200) |

**Total: 200 gaps. ~57% complete by weighted effort.**
**Biggest impact per effort:** Config G1-G11 (auxiliary, 56 keys) or Providers G35-G40 (6 simple OpenAI-compat providers).
