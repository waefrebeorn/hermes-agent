# Slermes C Architecture

## Overview

Slermes C is a 1:1 C port of the Python Hermes AI Agent. Same agent loop, provider abstraction, tool system, gateway, plugin system, and CLI — in portable C with zero Python dependencies.

```
|CLI / Gateway → Agent Loop → LLM Client → 10 Providers → HTTP/JSON
                    ↓
             Tool Registry (85 tools)
                    ↓
             Plugin Registry (10 .so)
                    ↓
             59 Library Modules
```

## Module Overview

| Module | Role | Files | Count |
|--------|------|-------|-------|
| **Core Loop** | Agent conversation | `src/agent/agent_loop.c` | 1 |
| **LLM Client** | Provider abstraction | `src/agent/llm_client.c` | 1 |
| **Providers** | LLM API implementations | `src/agent/provider_*.c` | 9 + metadata |
| **CLI** | Interactive shell | `src/cli/*.c` | 9, ~148 commands |
| **Tools** | Action handlers | `src/tools/*.c` | 43, 85 registered |
| **Gateway** | Messaging platforms | `src/gateway/` | ~25 files, 19 platforms |
| **Cron** | Job scheduling | `src/cron/*.c` | 8 files |
| **Plugins** | Runtime .so extensions | `src/plugins/*.so` | 10 |
|**Libraries** | Reusable C modules | `lib/lib*` | 59 |
| **Tests** | Unit tests | `tests/test_*.c` | 213, 226/0/23 |
| **Config** | YAML/env configuration | `~/.slermes/` | ~322 keys |

## Core Data Flow

```
User Input → CLI/Gateway → Agent Loop → LLM Provider → Response
                 │              │
                 ▼              ▼
           Gateway Send    Tool Execution
           (platform)      (terminal, file, web, ...)
```

### Startup Sequence
1. `main()` parses CLI args, loads config (`~/.slermes/config.yaml` + `.env`)
2. Provider system initializes (9 built-in providers + 27 metadata aliases)
3. Plugin system loads `.so` files
4. Agent loop initializes with session state (SQLite-backed)
5. Gateway starts polling configured platforms (if enabled)
6. CLI enters interactive prompt (or runs one-shot command)

## Module Details

### Config System (`src/cli/config.c`)
- **Structure:** `hermes_config_t` — flat struct with ~322 fields
- **Loading:** YAML file → defaults → env overrides (tiered)
- **Features:** `${VAR:-default}` expansion, `!include` directive, named profiles, diff detection
- **Security:** `secure_parent_dir()` enforces 0700 on `~/.slermes/` at startup

### Provider System (`src/agent/provider_*.c`)
Each provider implements `provider_ops_t` with 6 operations: build_url, build_headers, build_request_body, parse_response, parse_stream_chunk, free_response.

Provider categories:
- **OpenAI-style:** OpenAI, DeepSeek, OpenRouter, xAI, Custom (+ 27 aliases)
- **Anthropic:** `content[]` blocks with thinking/text/tool_use variants
- **Google Gemini:** `candidates[] → content → parts[]` with functionCall
- **Azure OpenAI:** OpenAI-compatible with deployment ID + API version
- **AWS Bedrock:** SigV4 auth, Converse API with inferenceConfig

### Agent Loop (`src/agent/agent_loop.c`)
1. Build message list (system + user/assistant/tool turns)
2. Select provider (with fallback routing)
3. Build request body with 18 LLM params wired
4. Send HTTP request (or stream)
5. Parse response (content, tool calls, finish reason)
6. Tool calls → execute each tool → append results → loop
7. Text response → return to caller

### Tool System (`src/tools/*.c`)
- **Registry:** 85 registered tools, auto-discovered at init time
- **Execution:** Each tool implements `handler(args_json, state) → result_json`
- **Key tools:** terminal, file, web, vision, TTS, delegate, cron, MCP, memory, kanban, skills, process, patch, clarify, exec_code, send_message, session CRUD, browser, image_gen

### Gateway (`src/gateway/server.c`)
- **Architecture:** Multi-threaded: one poll thread per platform + main dispatch thread
- **19 platforms:** Telegram, Discord, Slack, Matrix, Signal, WhatsApp, Email, SMS, Feishu, WeCom, DingTalk, QQBot, BlueBubbles, Webhook, HomeAssistant, Mattermost, Weixin, Yuanbao, MS Graph Webhook
- **Features:** Per-platform cooldown, exponential reconnect backoff, message dedup (TTL ring buffer), markdown→HTML conversion, message truncation

### Plugin System (`src/plugins/plugin_*.c`)
- **Interface:** 3 exported symbols per plugin (plugin_metadata, plugin_init, plugin_process)
- **Lifecycle:** Load .so → resolve symbols → init → process → cleanup
- **10 plugins:** kanban, honcho, spotify, disk-cleanup, file-memory, achievements, observability, skills, image_gen, google_meet

### Library Layer (`lib/lib*/`) — 58 Modules

| Library | Purpose | Deps |
|---------|---------|------|
| libjson | JSON parser + builder | none |
| libjson5 | JSON5 preprocessor (comments, trailing commas) | libjson |
| libhttp | Sync HTTP client (libcurl wrapper) | libcurl |
| libyaml | Minimal YAML subset parser | none |
| libcrypto | HMAC-SHA256, base64, hex helpers | OpenSSL |
| libcron | Cron expression parsing + matching | none |
| libproc | Subprocess execution | POSIX |
| libdb | SQLite wrapper for session store | sqlite3.c |
| libplugin | Dynamic .so loader | dlfcn |
| libskin | ANSI color + theme engine | none |
| libmcp | Model Context Protocol client | none |
| libprotobuf | Protobuf varint encode/decode | none |
| libdotenv | .env file parser | none |
| libtemplate | String template engine | libjson |
| libtui | Terminal UI helpers | ncurses |
| libwebsocket | WebSocket client | libcurl |
| libpath | Path join/resolve/glob | none |
| libdatetime | ISO-8601, date math, relative time | none |
| libcsv | CSV parsing | none |
| libhash | SHA-256/SHA-1/MD5/HMAC | OpenSSL |
| libuuid | UUID v4/v5 generation | none |
| libbase64 | RFC 4648 base64/url | none |
| libhtml | HTML escape/unescape/strip | none |
| libtextwrap | Text wrap/fill/dedent | none |
| libglob | Recursive ** glob matching | none |
| libsignal | Signal handling helpers | POSIX |
| libenum | X-macro enum-to-string | none |
| libdifflib | Unified diff, similarity ratio | none |
| libregex | POSIX regex wrapper | POSIX |
| libansi | ANSI terminal codes | none |

## Testing
- **207 test files, 226/0/23 suite** (all passed, 0 failed, 1 skipped)
- **Pattern:** Each test is `int main(void)` returning 0 on pass
- **Areas:** Libraries, providers, agent, CLI, cron, tools, gateway, plugins

## Key Design Decisions
1. **Flat structs, function pointer tables** — no OOP, provider_t embeds provider_ops_t
2. **Calloc-based allocation** — zero-initialized structs throughout
3. **Synchronous I/O** — no async; thread-per-platform for gateway polling
4. **Bundled libraries** — critical deps under lib/ for reproducibility
5. **Direct malloc/free** — no memory pool; ASan during development

## Building
```bash
make -j$(nproc)         # Full binary
bash test_runner.sh      # 237 tests
make tui                 # With ncurses TUI
make docs                # Doxygen HTML docs
make plugins             # Build 10 .so plugins
```

## Current State
- **Suite:** 226/0/23 — 207 test files
- **Binary:** 30MB ELF, 0 warnings
- **Commits:** 817+ C-specific
- **Upstream:** synced via battleship-v8
- **Parity:** ~78% (see `.hermes/mind-palace/battleship-v8.md`)
