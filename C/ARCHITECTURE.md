# Hermes C Architecture

## Overview

Hermes C is a 1:1 C port of the Python [Hermes AI Agent](https://github.com/NousResearch/hermes-agent). It provides the same agent loop, provider abstraction, tool system, gateway, plugin system, and CLI — all in portable C with zero Python dependencies.

```
┌─────────────────────────────────────────────────────────────┐
│                        CLI / Gateway                         │
│  (interactive shell, Telegram, Discord, Slack, 16+ others)  │
├──────────────────────┬──────────────────────────────────────┤
│     Agent Loop       │           Tool System                │
│  (run_conversation)  │  (terminal, file, web, vision, TTS,  │
│                      │   delegate, cron, MCP, memory, ...)  │
├──────────────────────┴──────────────────────────────────────┤
│                    Provider System                           │
│  (OpenAI, Anthropic, Google, Azure, Bedrock, DeepSeek,      │
│   xAI, OpenRouter, Custom + 31 aliases)                     │
├──────────────────────┬──────────────────────────────────────┤
│   Config System      │       Plugin System                   │
│  (YAML + .env,       │  (.so plugins: kanban, honcho,       │
│   profiles, diff)    │   spotify — 45 more backends planned)│
├──────────────────────┴──────────────────────────────────────┤
│                    Library Layer                              │
│  libjson · libhttp · libyaml · libcrypto · libcron          │
│  libproc · libdb · libplugin · libskin · libmcp              │
│  libprotobuf · libdotenv · libtemplate · libtui              │
│  libwebsocket                                                │
└──────────────────────────────────────────────────────────────┘
```

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
2. Provider system initializes (9 built-in providers + 31 aliases)
3. Plugin system loads `.so` files from `~/.slermes/plugins/`
4. Agent loop initializes with session state (SQLite-backed)
5. Gateway starts polling configured platforms (if enabled)
6. CLI enters interactive prompt (or runs one-shot command)

## Module Architecture

### Config System (`src/cli/config.c`)

- **Structure:** `hermes_config_t` — flat struct with 322+ fields, no nested pointers
- **Loading:** YAML file → defaults → env overrides (tiered: `HERMES_*` vars > YAML > hardcoded defaults)
- **Features:** `${VAR:-default}` expansion, `!include` directive, named profiles, diff detection, export/import
- **Notable:** `secure_parent_dir()` enforces 0700 on `~/.slermes/` at startup

### Provider System (`src/agent/provider_*.c`)

Each provider implements `provider_ops_t`:

| Operation | Purpose |
|-----------|---------|
| `build_url` | Construct API endpoint from base_url + model |
| `build_headers` | Auth headers (Bearer token, x-api-key, SigV4) |
| `build_request_body` | JSON request with all LLM params |
| `parse_response` | Extract content/reasoning/tool_calls from response |
| `parse_stream_chunk` | Incremental SSE chunk parsing |
| `free_response` | Release parsed response memory |

All providers share a common interface but implement different API formats:

- **OpenAI-style:** OpenAI, DeepSeek, OpenRouter, xAI, Custom (+ 31 aliases)
- **Anthropic:** Uses `content[]` blocks with `thinking`/`text`/`tool_use` variants
- **Google Gemini:** `candidates[] → content → parts[]` with `functionCall`
- **Azure OpenAI:** OpenAI-compatible with deployment ID routing + API version
- **AWS Bedrock:** SigV4 auth, Converse API with `inferenceConfig`

### Agent Loop (`src/agent/agent_loop.c`)

The core conversation loop in `agent_run_conversation()`:

1. Build message list (system + user/assistant/tool turns)
2. Select provider (with fallback routing)
3. Build request body with all LLM params (18 params wired)
4. Send HTTP request (or stream)
5. Parse response (content, tool calls, finish reason)
6. If tool calls → execute each tool → append results → loop
7. If text response → return to caller

State tracking: session tokens (input/output/reasoning/cache/cost), user/tool turn counters, iteration budget, checkpoint/rollback.

### Tool System (`src/tools/*.c`)

- **Registry:** `registry_register()` + `registry_get()` — ~28 registered tools
- **Discovery:** Tools auto-register at init time, filterable by toolset
- **Execution:** Each tool implements a handler function `(args_json, state) → result_json`
- **Key tools:** terminal, file, web, vision, TTS, delegate, cron, MCP, memory, kanban, skills, process, patch, clarify, exec_code, send_message, session CRUD, browser, image_gen

### Gateway (`src/gateway/server.c`)

- **Architecture:** Multi-threaded: one poll thread per platform + main dispatch thread
- **Platforms:** 19 platforms (Telegram, Discord, Slack, Matrix, Signal, WhatsApp, Email, SMS, Feishu, WeCom, DingTalk, QQBot, BlueBubbles, Webhook, HomeAssistant, Mattermost, Weixin, Yuanbao, MS Graph Webhook)
- **Flow:** Poll → `get_text()`/`get_chat_id()` → `process_update()` → `gateway_agent_chat()` → response → `gateway_send()`
- **Features:** Per-platform cooldown, exponential reconnect backoff, message dedup (TTL ring buffer), markdown→HTML conversion, MarkdownV2 escaping, message truncation

### Plugin System (`src/plugins/plugin_*.c`, `lib/libplugin/`)

- **Interface:** 3 exported symbols per plugin (`plugin_metadata`, `plugin_init`, `plugin_process`)
- **Lifecycle:** Load `.so` → resolve symbols → `plugin_init()` → `plugin_process()` → `plugin_cleanup()`
- **Current plugins:** kanban (in-memory board CRUD), honcho (in-memory memory store), spotify (Web API via curl popen)
- **Planned:** 45+ Python plugin backends to port

### Library Layer (`lib/*/`)

| Library | Purpose | Dependencies |
|---------|---------|-------------|
| libjson | JSON parser + builder (C89) | none |
| libhttp | Sync HTTP client (wraps libcurl) | libcurl, OpenSSL |
| libyaml | Minimal YAML subset parser | none |
| libcrypto | HMAC-SHA256, base64, hex | OpenSSL |
| libcron | Cron expression parsing + matching | none |
| libproc | Subprocess execution (popen wrapper) | POSIX |
| libdb | SQLite wrapper (session DB) | bundled sqlite3.c |
| libplugin | Dynamic `.so` loader | dlfcn |
| libskin | ANSI color + theme engine | none |
| libmcp | Model Context Protocol client | none |
| libprotobuf | Protobuf varint encode/decode | none |
| libdotenv | .env file parser | none |
| libtemplate | Simple string template engine | libjson |
| libtui | Terminal UI helpers | ncurses (optional) |
| libwebsocket | WebSocket client | libcurl |

## Testing

- **Test framework:** Standalone C test binaries (no framework dependency)
- **Test runner:** `test_runner.sh` — compiles each test file with gcc, runs it, aggregates results
- **Coverage:** 64+ test files, ~2,700+ assertions, 99% pass rate
- **Pattern:** Each test is a standalone `int main(void)` that returns 0 on pass, 1 on fail
- **Key test areas:** provider response parsing, gateway formatting, config load/validate/env, tool dispatch, plugin lifecycle, error handling, MCP protocol

## Key Design Decisions

1. **Flat structs, no inheritance** — C has no OOP; `provider_t` embeds `provider_ops_t` function pointer table
2. **Calloc-based allocation** — All structs calloc'd to ensure zero-initialized; explicit free throughout
3. **Synchronous I/O** — No async/await; HTTP calls block (thread-per-platform for gateway polling)
4. **Bundled libraries** — Critical deps vendored under `lib/` for reproducibility; system deps (libcurl, OpenSSL) via pkg-config
5. **Wl,--unresolved-symbols=ignore-all** — Test binaries use weakly-linked stubs for deps not needed by the test
6. **No memory pool** — Direct malloc/free; ASan used during development; no leak detection in CI yet

## Building

```bash
make          # Phase 5: full binary
make tui      # With ncurses TUI (if lib/libncurses/ exists)
make test     # Build + run test suite
make docs     # Doxygen HTML docs
make plugins  # Build .so plugin files
```

See `DEPENDENCIES.md` for system package requirements.
