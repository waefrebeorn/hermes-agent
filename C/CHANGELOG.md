# Changelog

All notable changes to the C translation of Hermes Agent.

## [Unreleased] — 2026-05-26

### Added
- **Provider test suite** (M02-M05): Comprehensive per-provider tests
  - Anthropic: 80 assertions (URL/headers/thinking/cache/tools/streaming)
  - Google: 40 assertions (gen config/safety/tools/streaming)
  - Bedrock: 35 assertions (inferenceConfig/system/B39-B41/toolUse)
  - Azure: 45 assertions (deployment_id/api_version/12 LLM params/error parsing)

### Fixed
- **provider_google.c**: 3 bugs found by test coverage
  - Tool `functionDeclarations` set on array (no-op) — added wrapper object
  - Trailing slash in base_url produced `//models` — strip trailing slash
  - NULL crash in `google_parse_stream_chunk` — strncmp on NULL
- **provider_bedrock.c**: 3 bugs found by test coverage
  - UAF in metadata path (`json_object_set` then `json_free`)
  - UAF in tool_choice parsing (same pattern)
  - toolUse `input` object not serialized — `json_get_str` returns `{}` for objects
- **provider_azure.c**: 3 bugs found by test coverage
  - UAF in metadata path (same pattern as Bedrock)
  - No error object parsing in `azure_parse_response`
  - NULL crash in `azure_parse_stream_chunk`
- **provider_anthropic.c**: NULL crash in `anthropic_parse_stream_chunk`

### Suite
- 96/0/0 — 60 test files, ~2,574 assertions
- ~60% toward 1:1 Python parity (~360 gaps remaining)

## [0.7.0] — 2026-05-26

### Added
- **Build/doc infrastructure**: Dockerfile (multi-stage, ~20MB), CI workflow, cross-compile script (4 targets), .dockerignore, man page (hermes.1)
- **Plugin system**: All 3 plugins real — kanban (45 tests), honcho memory (27 tests), Spotify Web API (18 tests)
- **Config depth**: `${VAR:-default}` env var expansion, `!include path.yaml` directive
- **CLI features**: `--json` output mode, `/session-search`, `/session-export`, `hermes completions {bash|zsh}`
- **Provider features**: finish_reason tracking, json_mode, strict structured outputs (OpenAI), safety_settings (Google), top_k/candidate_count, deployment_id/api_version (Azure), inferenceProfile/guardrails/trace (Bedrock), provider preferences/route/data controls (OpenRouter)
- **Error types**: K01-K05 — ValueError, TypeError, RuntimeError, OSError, TimeoutError
- **Cross-cutting**: Token counting (model-aware, 9 families, 20+ known context windows), secure parent dir, key leakage prevention, vendor key derivation, local trust
- **Upstream sync**: 8/12 new feature gaps closed (xAI encrypted reasoning, web search, model retirement; Telegram observe/location/media; voice chunking; kanban sticky; skills hub; Bitwarden Secrets Manager; extra_body passthrough)

### Fixed
- temperature=0.0 silently dropped — `s/>0.0f/>=0.0f/` across 9 providers
- UAF in response_format path — all providers freed a node still in JSON tree
- Google parse_stream_chunk ordering — finishReason checked after early text return
- use-after-free in web_search_handler — backend arg read after json_free
- heap overflow in hermes_redact — strndup too small for redacted expansion
- double-counted pointer in hermes_redact

### Suite
- 92/0/0 — 56 test files, ~2,374 assertions

## [0.6.0] — 2026-05-24

### Added
- **Config**: Schema auto-generation, profiles (dev/prod/test), deep merge, category groups, migration support
- **CLI**: /new persist, /clear metadata, /undo snapshots, /save/load auto-open DB, /stats, /conv/history, /model enhanced, /config validate/diff/export
- **Tools**: send_message multi-platform routing + media attachment, exec_code sandbox isolation, TTS providers (elevenlabs/openai/xai), LLM vision description, batch file ops, cron job chaining
- **MCP**: Resource subscriptions (subscribe/unsubscribe/notifications), roots management (add/remove/count/get), SSE transport
- **Agent loop**: G01-G36 all filled — token tracking, tool_choice/parallel_tool_calls, toolset filtering, compression depth, iteration budget, checkpoint depth, prefill variants, steer queue
- **Gateway**: E01-E63 all closed — all send methods, message types, infrastructure (keepalive/dedup/batch/markdown/cooldown/reconnect/proxy), hooks, formatting, platform depth (Discord/Slack/Matrix/WhatsApp/Email/Signal/Telegram)

### Suite
- 88/0/0 — 50 test files, ~2,374 assertions

## [0.5.0] — 2026-05-22

### Added
- **Gateway multi-platform**: 19 platforms — Telegram (long-polling), Discord (REST), Slack (REST), Matrix, Mattermost, WhatsApp (Cloud API), Email, Signal, SMS, DingTalk, WeCom, Feishu, QQ Bot, BlueBubbles, HomeAssistant, webhook API server, MSGraph, WeChat, Yuanbao
- **Tools parity**: 28 registered handlers — browser (13/CDP), memory (1), kanban (9), web search (5 backends), terminal (PTY/env/Docker), file (sandbox/glob/batch), delegate, cron, TTS, vision, process, send_message, exec_code, session_search, image_generate, HomeAssistant, skills hub
- **Voice mode**: STT + TTS tools
- **Plugin discovery**: .so loading at startup
- **LLM streaming**: True streaming via provider-aware chunk callback
- **API retry**: Exponential backoff in LLM client

### Suite
- 82/0/1 — 36 test files, ~6,920 lines

## [0.4.0] — 2026-05-21

### Added
- **Provider system**: 9 native ops tables + 31 aliases
  - OpenAI, OpenRouter, DeepSeek, xAI (Grok), Anthropic (Messages API), Google (Gemini), Azure (OpenAI), Bedrock (Converse/SigV4), Custom (OpenAI-compat)
  - All 18/18 LLM params fully wired: max_tokens, temperature, top_p, stop, presence/frequency penalty, seed, logprobs/top_logprobs, user, service_tier, response_format, metadata, tool_choice, parallel_tool_calls, max_tool_calls, n, reasoning_effort, extra_body
- **Config**: 322/322 YAML keys — 14 config groups with validation, profiles, deep merge, schema generation, migration, env var override
- **CLI**: 69 slash commands (full Python parity) — /new, /reset, /retry, /compress, /branch, /snapshot, /stats, /config, /model, /save, /load, /history, /undo, /tools-verify, /skills, /cron, /goal, /agent, /debug, /completions, etc.
- **TUI**: ncurses full-screen terminal UI

### Fixed
- LLM client auth header formatting
- session_search SLERMES_HOME awareness
- .env key coverage

### Suite
- 75/0/1 — 42 test files

## [0.3.0] — 2026-05-20

### Added
- **Standalone C libraries** (Python dep parity):
  - `libjson` — JSON parser/builder (replaces `json`/`orjson`)
  - `libhttp` — HTTP client with TLS (replaces `httpx`/`requests`)
  - `libyaml` — YAML parser (replaces `PyYAML`)
  - `libcrypto` — Base64/SHA-256/HMAC (replaces `hashlib`/`base64`)
  - `libdotenv` — .env file parser (replaces `python-dotenv`)
  - `libcron` — Cron expression parser (replaces `croniter`)
  - `libproc` — Process monitoring via /proc (replaces `psutil`)
  - `libtemplate` — String templates (replaces `Jinja2`)
  - `libtui` — Terminal UI (replaces `prompt_toolkit`/`rich`/`tqdm`)
  - `libdb` — SQLite session store
- **Agent core**: Agent loop, LLM client, context management, session persistence, token counting, compression
- **Tools**: Terminal, file, web (5 backends), skills, delegate, session_search, vision_analyze, text_to_speech
- **Streaming**: SSE output for all providers
- **Skin engine**: Theme/display color system

### Suite
- Makefile + automated test harness
- Plugin examples

## [0.2.0] — 2026-05-20 (Phase 2)

### Added
- Agent loop implementation
- LLM client with 8 provider adapters
- CLI with 36 commands
- Config system with 90+ fields
- Session persistence via libdb
- Context window management
- SSE streaming output

## [0.1.0] — 2026-05-20 (Phase 1)

### Added
- C translation scaffold with digestion system
- Foundation dependencies: JSON, YAML, HTTP, DB, crypto, display
- Super Fork upstream tracking (`digest.py --upstream` & `make upstream-sync`)
- Makefile build system
- Initial test harness
- Upstream: NousResearch/hermes-agent (125 commits behind, 52 Python files)

---

## Version Scheme

- **0.x.0**: Feature milestones
- **0.x.y**: Bugfix/iteration releases within a milestone
- **1.0.0**: Full 1:1 Python parity achieved (~360 gaps remaining)
