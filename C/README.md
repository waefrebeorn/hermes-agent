# Slermes C

**Slermes — Full C translation of the Python [Hermes Agent](https://github.com/NousResearch/hermes-agent) by Nous Research.**  
One static binary. Zero runtime deps beyond libc + libssl. 31M ELF.

```text
||||||||||||||||||| Suite:  325/0/14 (289 test files, completes in <60s)
||||||||||||||||||| Binary: 31M    (dynamic ELF, -O2 -g)
|||||||||||||||||| Source: 456+ .c files (src/ + lib/ + tests/): 108K+ C LOC
|||||||||||||||||||| Gaps:  75 parity gaps (1000+ test case gaps) across 8 sectors
|||||||||| Phase 312: S5 C16 kanban CLI — /kanban list/show/create/complete/block/unblock/link
||||||||| Phase 272: S8 R02+R10 — batch 5 utility functions (convert_tools_to_converse, estimate_tokens_rough, resolve_verify, extract context/max_tokens)
||| 274 | S8 R10 depth — batch 4 token est (count_image_tokens, message_chars, messages_tokens_rough, request_tokens_rough)
||| 275 | S8 R10 — context probe tiers + get_next_probe_tier
||| 276 | S8 R10 — context cache layer (path/load/save/get/invalidate)
||| 277 | S8 R10 — provider_extract_first_int() + refactor context/max_completion extractors
||| 278 | S8 R10 — provider_add_model_aliases() + get_context_length_from_provider_error()
||| 279 | S8 R10 — provider_detect_local_server_type() — probes local inference endpoints
||| 280 | S8 R10 — provider_query_ollama_api_show() + provider_query_ollama_num_ctx() — Ollama context query
||| 281 | S8 R10 — provider_query_local_context_length() — multi-endpoint context probing. R10 PORTED!
```
||||||||||CLI:    98 cmd_ functions + 37 config sections — 85 unique tools registered
||||||||||Tools:  85 registered (100+ at runtime with MCP dynamic)
|||||||||Libraries: 65 C modules — zero external deps beyond libc+libssl
|||||||||Gateway: 19 platform adapters (Telegram, Discord, Slack, Signal, SMS, etc.)
|||||||||Providers: 10 .c modules + metadata (OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom, Copilot)
|```

> **Symlink note:** `README.md` → `C/README.md`. The canonical README lives at `C/README.md`. Edit that file; the root follows automatically.
>
> **History:** The C translation was developed over 277 commits on the [`c-work`](https://github.com/waefrebeorn/slermes/tree/c-work) branch, then squashed onto updated upstream main at commit `d00d2f1d`. See the `c-work` branch for full granular history.

---

## Table of Contents

- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [Build System](#build-system)
- [All Tools (85 Registered)](#all-tools-85-registered)
- [Gateway Platforms (19)](#gateway-platforms-19)
- [LLM Providers (10)](#llm-providers-10)
- [Plugins (10 .c)](#plugins-10-c)
- [Libraries (65 Units)](#libraries-65-units)
- [CLI Commands (98 CLI, Real)](#cli-commands-98-cli-real)
- [Battleship Roadmap (1000+ Gaps)](#battleship-roadmap-1000-gaps)
- [Verified Stubs (All Resolved)](#verified-stubs-all-resolved)
- [Bugfix History](#bugfix-history)
- [Project Structure](#project-structure)
- [The Agentic Process (.hermes)](#the-agentic-process-hermes)
- [Test Suite](#test-suite)
- [CI/CD](#cicd)
- [Development Guide](#development-guide)
- [Config Reference](#config-reference)
- [Upstream](#upstream)

---

## Quick Start

```bash
cd C/
make -j$(nproc)            # Build hermes binary
./hermes --help            # Usage
bash test_runner.sh        # 335/0/0
./hermes --version         # v0.14.1+

# Modes
./hermes "hello"                        # One-shot
./hermes                                # Interactive (/help for commands)
./hermes --gateway --platform telegram  # Gateway daemon
make tui && ./hermes-tui                # ncurses TUI (experimental)
```

### Docker

```bash
docker build -t hermes-c -f C/Dockerfile .
docker run --rm hermes-c --help
# ~20MB slim image (bookworm-slim, static-linked)
```

### Smoke Test

```bash
echo '/tools' | ./hermes     # List all 85 registered tools
echo "/providers" | ./hermes # List provider configurations
```

---

## Architecture

```
                    ┌─────────────────────────┐
                    │  CLI / Gateway / TUI     │
                    │  (3 entry points)        │
                    └──────────┬──────────────┘
                               │ User message
                    ┌──────────▼──────────────┐
                    │     Agent Loop           │
                    │  (context, budget,       │
                    │   fallback, checkpoint,  │
                    │   interrupt, audit)      │
                    └──────────┬──────────────┘
                               │ LLM call
                    ┌──────────▼──────────────┐
                    │   LLM Client + 10        │
                    │   Provider Adapters      │
                    │  (OpenAI-compat + native)│
                    └──────────┬──────────────┘
                               │ Tool call
                    ┌──────────▼──────────────┐
                    │   85 Tool Registry       │
                    │  (file, web, terminal,   │
                    │   skills, MCP, kanban,   │
                    │   browser, delegate...)  │
                    └──────────┬──────────────┘
                               │ Plugin dispatch
                    ┌──────────▼──────────────┐
                    │   10 Plugin .c           │
                    │  (kanban, honcho,        │
                    │   spotify, image_gen...) │
                    └──────────┬──────────────┘
                               │ System calls
                    ┌──────────▼──────────────┐
                    │   65 Library Units       │
                    │  (json, yaml, http,      │
                    │   crypto, mcp, cron...)  │
                    └─────────────────────────┘
```

**Data flow:** User message → entry point (CLI/Gateway/TUI) → Agent Loop (context management, budget tracking, fallback routing, interrupt handling, checkpoint/resume) → LLM Client → Provider adapter → LLM response → tool calls → plugin dispatch → response to user.

---

## Build System

```bash
make hermes           # Full binary (phase5) — 0 errors, 31M
make plugins          # 10 .so shared objects
make tui              # ncurses TUI → hermes-tui (experimental)
make libs             # 65 library compilation units
make install-plugins  # Build + copy .so → ~/.hermes/plugins/
make docs             # Doxygen HTML docs (if doxygen available)
```

### 5-Phase Build Pipeline

| Phase | What | Output |
|-------|------|--------|
| P1 | 65 library units (.o) | lib/*.o |
| P2 | Agent core + CLI + 10 providers | src/agent/*.o, src/cli/*.o |
| P3 | Tool handlers | src/tools/*.o |
| P4 | 19 gateway platforms | src/gateway/*.o |
| P5 | Cron scheduler + final link | hermes binary |

### Build Variants

```bash
# Release (default)
make -j$(nproc)

# Debug (more warnings, no optimization)
make CFLAGS="-O0 -g3 -Wall -Wextra -Wpedantic -Werror" hermes

# AddressSanitizer
make CFLAGS="-O1 -g -fsanitize=address" LDFLAGS="-fsanitize=address" hermes

# UndefinedBehaviorSanitizer
make CFLAGS="-O1 -g -fsanitize=undefined" LDFLAGS="-fsanitize=undefined" hermes
```

---

## All Tools (85 Registered)

Every tool is registered at startup via `registry_register(name, description, schema, handler)`. Tools are discovered by the agent loop and called with JSON arguments.

### File & Terminal

| Tool | File | Status |
|------|------|--------|
| `read_file` | `src/tools/file.c` | ✅ Complete |
| `write_file` | `src/tools/file.c` | ✅ Complete |
| `search_files` | `src/tools/file.c` | ✅ Complete |
| `file_batch` | `src/tools/file_batch.c` | ✅ Complete |
| `terminal` | `src/tools/terminal.c` | ✅ Complete |
| `process` | `src/tools/process.c` | ✅ Complete |
| `patch` | `src/tools/patch.c` | ✅ Complete |

### Web

| Tool | File | Status |
|------|------|--------|
| `web_get` | `src/tools/web.c` | ✅ Complete |
| `web_search` | `src/tools/web.c` | ✅ Complete |
| `web_extract` | `src/tools/web.c` | ✅ Complete |

### Browser (Text + CDP)

| Tool | File | Status |
|------|------|--------|
| `browser_navigate` | `src/tools/browser.c` | ✅ Text-based |
| `browser_snapshot` | `src/tools/browser.c` | ✅ Text-based |
| `browser_click` | `src/tools/browser.c` | ✅ Text-based |
| `browser_type` | `src/tools/browser.c` | ✅ Text-based |
| `browser_scroll` | `src/tools/browser.c` | ✅ Text-based |
| `browser_back` | `src/tools/browser.c` | ✅ Text-based |
| `browser_forward` | `src/tools/browser.c` | ✅ Text-based |
| `browser_get_images` | `src/tools/browser.c` | ✅ Text-based (HTML parse) |
| `browser_press` | `src/tools/browser.c` | ✅ Text-based |
| `browser_vision` | `src/tools/browser.c` | ✅ CDP-dependent |
| `browser_console` | `src/tools/browser.c` | ✅ CDP-dependent |
| `browser_dialog` | `src/tools/browser.c` | ✅ CDP-dependent |
| `browser_cdp` | `src/tools/browser.c` | ✅ CDP-dependent |

### Agent Communication

| Tool | File | Status |
|------|------|--------|
| `delegate_task` | `src/tools/delegate.c` | ✅ Complete |
| `clarify` | `src/tools/clarify.c` | ✅ Complete |
| `send_message` | `src/tools/send_message.c` | ✅ Complete |
| `approval_status` | `src/tools/approval.c` | ✅ Complete |

### Skills

| Tool | File | Status |
|------|------|--------|
| `skills_list` | `src/tools/skills.c` | ✅ Complete |
| `skill_scan` | `src/tools/skills.c` | ✅ Complete |
| `skill_validate` | `src/tools/skills.c` | ✅ Complete |
| `skill_provenance` | `src/tools/skills.c` | ✅ Complete |
| `skill_sync` | `src/tools/skills.c` | ✅ Complete |
| `skill_bundle` | `src/tools/skills.c` | ✅ Complete |
| `skill_usage` | `src/tools/skills.c` | ✅ Complete |
| `skill_cache` | `src/tools/skills.c` | ✅ Complete |
| `skill_search` | `src/tools/skills.c` | ✅ Complete |
| `skill_curator` | `src/tools/skills.c` | ✅ Complete |
| `skill_deps` | `src/tools/skills.c` | ✅ Complete |
| `skill_hub` | `src/tools/skills.c` | ✅ Complete |
| `skill_view` | `src/tools/skill_mgmt.c` | ✅ Complete |
| `skill_manage` | `src/tools/skill_mgmt.c` | ✅ Complete |

### Memory & Sessions

| Tool | File | Status |
|------|------|--------|
| `memory` | `src/tools/memory.c` | ✅ Complete |
| `session_search` | `src/tools/session_search.c` | ✅ Complete |
| `session_crud` | `src/tools/session_crud.c` | ✅ Complete |
| `todo` | `src/tools/todo.c` | ✅ Complete |

### MCP

| Tool | File | Status |
|------|------|--------|
| `mcp_status` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_tool_call` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_auth` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_resource_list` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_resource_read` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_prompt_list` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_prompt_get` | `src/tools/mcp_tool.c` | ✅ Complete |
| (dynamic) | `src/tools/mcp_tool.c` | Tools registered by connected servers |

### Media & Input

| Tool | File | Status |
|------|------|--------|
| `vision_analyze` | `src/tools/vision.c` | ✅ Complete |
| `text_to_speech` | `src/tools/tts.c` | ✅ Complete |
| `voice_listen` | `src/tools/voice_mode.c` | ✅ Complete |
| `voice_speak` | `src/tools/voice_mode.c` | ✅ Complete |
| `image_generate` | `src/tools/image_gen.c` | ✅ Complete (shells to plugin) |
| `video_generate` | `src/tools/video_gen.c` | ✅ Complete |

### Automation

| Tool | File | Status |
|------|------|--------|
| `cronjob` | `src/tools/cronjob.c` | ✅ Complete |
| `kanban_show` | `src/tools/kanban.c` | ✅ Complete |
| `kanban_list` | `src/tools/kanban.c` | ✅ Complete |
| `kanban_complete` | `src/tools/kanban.c` | ✅ Complete |
| `kanban_block` | `src/tools/kanban.c` | ✅ Complete |
| `kanban_heartbeat` | `src/tools/kanban.c` | ✅ Complete |
| `kanban_comment` | `src/tools/kanban.c` | ✅ Complete |
| `kanban_create` | `src/tools/kanban.c` | ✅ Complete |
| `kanban_unblock` | `src/tools/kanban.c` | ✅ Complete |
| `kanban_link` | `src/tools/kanban.c` | ✅ Complete |

### Integration

| Tool | File | Status |
|------|------|--------|
| `x_search` | `src/tools/x_search.c` | ✅ Complete |
| `ha_list_entities` | `src/tools/homeassistant.c` | ✅ Complete |
| `ha_get_state` | `src/tools/homeassistant.c` | ✅ Complete |
| `ha_list_services` | `src/tools/homeassistant.c` | ✅ Complete |
| `ha_call_service` | `src/tools/homeassistant.c` | ✅ Complete |
| `discord_send` | `src/tools/discord.c` | ✅ Complete |

### Internal

| Tool | File | Status |
|------|------|--------|
| `execute_code` | `src/tools/exec_code.c` | ✅ Complete |
| `rate_limit` | `src/tools/rate_limit.c` | ✅ Complete |
| `url_safety` | `src/tools/url_safety.c` | ✅ Complete |
| `tirith` | `src/tools/tirith.c` | ✅ Complete |
| `result_storage` | `src/tools/result_storage.c` | ✅ Complete |
| `tool_config` | `src/tools/tool_config.c` | ✅ Complete |
| `mixture_of_agents` | `src/tools/mixture_of_agents.c` | ✅ Complete |
| `video_analyze` | `src/tools/video_analyze.c` | ✅ Complete |
| `path_security` | `src/tools/path_security.c` | ✅ Complete |
| `xai_http` | `src/tools/xai_http.c` | ✅ Complete |
| `account_usage` | `src/tools/account_usage.c` | ✅ Complete |
| `ansi_strip` | `src/tools/ansi_strip.c` | ✅ Complete |
| `transcribe` | `src/tools/transcribe.c` | ✅ Complete |
| `file_watch` | `src/tools/file_watch.c` | ✅ Complete |
| `file_merge` | `src/tools/file_merge.c` | ✅ Complete |
| `feishu_tools` | `src/tools/feishu_tools.c` | ✅ Complete |
| `wecom_crypto` | `src/tools/wecom_crypto.c` | ✅ Complete |

---

## Gateway Platforms (19)

All platforms implement `(init, send, poll, shutdown)` lifecycle. They run co-operatively in a single thread with epoll/kqueue.

| # | Platform | Adapter | Status |
|---|----------|---------|--------|
| 1 | Telegram | `gateway/platforms/telegram.c` | ✅ |
| 2 | Discord | `gateway/platforms/discord.c` | ✅ |
| 3 | Slack | `gateway/platforms/slack.c` | ✅ |
| 4 | Matrix | `gateway/platforms/matrix.c` | ✅ |
| 5 | Mattermost | `gateway/platforms/mattermost.c` | ✅ |
| 6 | WhatsApp | `gateway/platforms/whatsapp.c` | ✅ |
| 7 | Email (IMAP/SMTP) | `gateway/platforms/email.c` | ✅ |
| 8 | Signal | `gateway/platforms/signal.c` | ✅ |
| 9 | SMS (Twilio) | `gateway/platforms/sms.c` | ✅ |
| 10 | Feishu/Lark | `gateway/platforms/feishu.c` | ✅ |
| 11 | WeCom (WeChat Work) | `gateway/platforms/wecom.c` | ✅ |
| 12 | DingTalk | `gateway/platforms/dingtalk.c` | ✅ |
| 13 | QQ Bot | `gateway/platforms/qqbot.c` | ✅ |
| 14 | BlueBubbles | `gateway/platforms/bluebubbles.c` | ✅ |
| 15 | MSGraph Webhook | `gateway/platforms/msgraph_webhook.c` | ✅ |
| 16 | WeChat Official | `gateway/platforms/weixin.c` | ✅ |
| 17 | YuanBao | `gateway/platforms/yuanbao.c` | ✅ |
| 18 | Webhook | `gateway/platforms/webhook.c` | ✅ |
| 19 | Home Assistant | `gateway/platforms/homeassistant.c` | ✅ |

---

## LLM Providers (10)

Every provider implements the same interface: `(init, chat, stream, count_tokens, build_headers, parse_response)`. OpenAI-compatible providers share the generic adapter; native providers have hand-tuned implementations.

| Provider | Adapter | Type | Status |
|----------|---------|------|--------|
| OpenAI | `provider_openai.c` | OpenAI-compat | ✅ |
| OpenRouter | `provider_openrouter.c` | OpenAI-compat | ✅ |
| DeepSeek | `provider_deepseek.c` | OpenAI-compat | ✅ |
| xAI | `provider_xai.c` | OpenAI-compat | ✅ |
| Anthropic | `provider_anthropic.c` | Native | ✅ |
| Google | `provider_google.c` | Native | ✅ |
| Azure | `provider_azure.c` | Native | ✅ |
| Bedrock | `provider_bedrock.c` | Native | ✅ |
| Custom | `provider_custom.c` | OpenAI-compat | ✅ |
| Copilot | `provider_copilot.c` | OpenAI-compat | ✅ |

---

## Plugins (10 .c files)

Plugins are `.so` files loaded at runtime via `dlopen`. Each exposes `plugin_init`, `plugin_process`, and `plugin_shutdown`.

| Plugin | File | Status |
|--------|------|--------|
| Kanban board | `plugin_kanban.so` | ✅ |
| Honcho memory | `plugin_honcho.so` | ✅ |
| Spotify control | `plugin_spotify.so` | ✅ |
| Disk cleanup | `plugin_disk_cleanup.so` | ✅ |
| File memory | `plugin_file_memory.so` | ✅ |
| Achievements | `plugin_achievements.so` | ✅ |
| Observability | `plugin_observability.so` | ✅ |
| Skills hub | `plugin_skills.so` | ✅ |
| Image generation | `plugin_image_gen.so` | ✅ |
| Google Meet | `plugin_google_meet.so` | ✅ |

---

## Libraries (65 Units)

Libraries are compiled directly into the binary. Each is a self-contained module under `lib/`.

| Library | Purpose | Status |
|---------|---------|--------|
| `libjson` | JSON parser/builder | ✅ |
| `libyaml` | YAML parser | ✅ |
| `libhttp` | HTTP/HTTPS client | ✅ |
| `libcrypto` | AES, SHA, HMAC | ✅ |
| `libmcp` | MCP transport layer | ✅ |
| `libmcp_oauth` | MCP OAuth PKCE | ✅ |
| `libpath` | Path manipulation | ✅ |
| `libdatetime` | Date/time parsing | ✅ |
| `libcsv` | CSV parsing | ✅ |
| `libhash` | Hash functions (SHA, MD5) | ✅ |
| `libuuid` | UUID generation | ✅ |
| `libbase64` | Base64 encode/decode | ✅ |
| `libhtml` | HTML parsing | ✅ |
| `libtextwrap` | Text wrapping | ✅ |
| `libglob` | File globbing | ✅ |
| `libsignal` | Signal handling | ✅ |
| `libenum` | Enum helpers | ✅ |
| `libdifflib` | Diff algorithms | ✅ |
| `libregex` | Regex matching | ✅ |
| `libjson5` | JSON5 parser | ✅ |
| `libwebsocket` | WebSocket client | ✅ |
| `libprotobuf` | Protobuf decode | ✅ |
| `libtoml` | TOML parser | ✅ |
| `libansi` | ANSI escape codes | ✅ |
| `libcron` | Cron expression parser | ✅ |
| `libproc` | Process management | ✅ |
| `libtui` | ncurses TUI helpers | ✅ |
| `libdb` | File-based session store (JSON) | ✅ |
| `libplugin` | Plugin loading | ✅ |
| `libskin` | Skin/theming engine | ✅ |
| `libtemplate` | Simple template engine | ✅ |
| `libfal_common` | Falcon common utilities | ✅ |
| `libtooloutput` | Tool output formatting | ✅ |
| `libxai_http` | xAI HTTP helpers | ✅ |
| `libenvpassthrough` | Environment passthrough | ✅ |
| `libcredential` | Credential management | ✅ |
| `libschemasanitizer` | Schema sanitization | ✅ |
| `libfuzzymatch` | Fuzzy matching | ✅ |
| `libinterrupt` | Interrupt handling | ✅ |
| `libtoolbackend` | Tool backend dispatch | ✅ |
| `libmangateway` | Managed gateway | ✅ |
| `libratelimit` | Rate limiting | ✅ |
| `libfilestate` | File state tracking | ✅ |
| `libtooldispatch` | Tool dispatch helpers | ✅ |
| `librateguard` | Rate guard | ✅ |
| `libskillutils` | Skill utilities | ✅ |
| `liberrorclassifier` | Error classification | ✅ |
| `liblineedit` | Line editing | ✅ |
| `libfile_sync` | File sync | ✅ |
| `libbudgetconfig` | Budget config | ✅ |
| `libthreatpatterns` | Threat patterns | ✅ |
| `libcredentialfiles` | Credential files | ✅ |
| `libskillaudit` | Skill audit | ✅ |
| `libslashconfirm` | Slash confirm | ✅ |
| `libmsgraph` | MS Graph API | ✅ |
| `libbinary` | Binary utilities | ✅ |
| `libbrowser` | Browser (camofox state) | ✅ |
| `libdebug` | Debug helpers | ✅ |
| `libosv` | OS version detection | ✅ |
| `libwebsite` | Website policy | ✅ |
| `libdotenv` | .env file parsing | ✅ |
| `libskillusage` | Skill usage tracking | ✅ |
| `libskillsync` | Skill sync | ✅ |
| `libtranscribe` | Transcription | ✅ |
| `libncurses` | ncurses wrapper | ✅ |

---

## CLI Commands (98 CLI, Real)

All real, tab complete + history. The CLI uses a central command registry (`cli/commands.c`) with alias resolution and subcommand dispatch.

**Categories:**
- **Core:** chat, session, history, model, provider, tools
- **Config:** config show/set/get/edit/migrate, env, profiles
- **Skills:** skills list/view/manage/scan/validate/sync/bundle/search
- **Gateway:** gateway start/stop/status/platforms
- **Cron:** cron list/add/remove/pause/resume/retry
- **Plugins:** plugins list/install/remove
- **MCP:** mcp status, mcp connect/disconnect/servers/tools
- **System:** version, help, doctor, update, logs

---

## Verified Stubs (All Resolved)

All codebase stubs have been resolved through Triple DA audits. The codebase contains zero `TODO`, `FIXME`, or `assert(0)` patterns in code logic. See `.hermes/mind-palace/vault/achievements.md` for the full resolution record.

**However:** 94 real parity gaps remain (9 sectors, 1000+ test case gaps) — display, conversation loop, agent modules, test coverage, provider adapters, and more. See `.hermes/mind-palace/battleship-v34.md` for the active gap map.

---

## Bugfix History

All bugs discovered through DA audits and runtime testing.

| Bug | Impact | Fix |
|-----|--------|-----|
| temperature=0.0 silent drop | All 9 providers used default temp | Pass 0.0 explicitly |
| response_format UAF | All 9 providers — `json_free` before use | `json_copy` before `json_free` |
| NULL stream chunk SIGSEGV | 6 providers crash on null chunks | NULL check before `strncmp` |
| `cron_job_reset_retry(NULL)` | SEGV on NULL job pointer | Guard with `if (!job) return` |
| Secrets `ow` pointer not advanced | Secrets truncated on write | Increment write pointer |
| x_search auth literal `***` | `printf` with `%s` missing | Correct format string |
| Dockerfile wrong WORKDIR | CI docker build fails immediately | `cd C && make` |
| `glob.h` shadows system | `path.c` can't find `GLOB_MARK` | Renamed to `hermes_glob.h` |
| `path.c` missing `_GNU_SOURCE` | `glob()` undeclared | `#define _GNU_SOURCE` before includes |

---

## Project Structure

```
C/
├── include/          # 70 header files
│   ├── hermes.h      # Master header (1084 lines)
│   └── hermes_*.h    # Subsystem headers
├── src/
│   ├── tools/        # Tool implementations
│   ├── agent/        # Agent core, providers
│   ├── cli/          # CLI, commands, config, paths, display
│   ├── gateway/      # Gateway adapters
│   └── deps/         # Core dependencies
├── lib/              # 65 self-contained library modules
| ├── tests/            # 283 test files
├── examples/         # Plugin examples
├── plugins/          # Plugin source
├── .hermes/
│   └── mind-palace/  # Agent state, plans, vault, battleship
├── Makefile
├── Dockerfile
└── test_runner.sh    # Bash test harness
```

---

## The Agentic Process (.hermes)

Development is managed through the `.hermes/mind-palace/` prestige system — a self-reinforcing loop that ensures ground-truth documentation stays synced with code state.

**Core files:**
- `state.md` — Live dashboard: suite stats, fork state, critical gaps
- `battleship-v34.md` — Canonical gap list (94 gaps across 8 sectors, 1000+ test case gaps)
- `prestige_prompt.md` — Priority-ordered gap summary
- `plan.md` — Sector-by-sector breakdown
- `vault/achievements.md` — Phase-by-phase resolved-gap history
- `goal-mantra.md` — Session-start ritual instructions

**The loop:** For each session: read walkway → pick next gap → verify against C source → implement → build → suite → doc sweep → commit.

The full development protocol is documented in the caveman skill (`~/.hermes/skills/caveman/SKILL.md`).

---

## Test Suite

| Suite | Count | Notes |
|-------|-------|-------|
|| Library tests | 335/0/0 | All pass, ~60s |
|| Test files | 287 | C files in tests/ |
| Gateway subsystem | 49 | JSON-RPC routing, auth |
| Gateway escape | 30 | Shell injection, pipe-to-interpreter |
| Provider depth | 54+ | OpenAI, Anthropic, Google, DeepSeek, Azure, Bedrock |
| Config | 103 | YAML parse, get/set, profiles |
| JSON | 83 | Parse, serialize, escape |
| TIRITH guardrails | 79 | Security scanning |
| File tool | 58+ | Read, write, patch, delete; file_watch, file_merge, file_batch |
| Browser | 32 | Navigation, CDP, vision |
| Skills | 53+ | List, view, manage, sync, search, bundle |
| Kanban | 38 | Task management |
| Feishu | 52 | Doc read, drive, comment |
| HomeAssistant | 26 | Entity query, service call |
| Approved edits | 44 | Edit approval flow |
| Budget tracker | 58 | Token/money budget |
| Rate limit | 24 | Rate limiting |
| Memory tool | 16 | File-based + plugin backends |
| TTS | 21 | Text-to-speech |
| MCP | 24+11 | MCP tool + stream tests |
| Patch tool | 37 | Find/replace, V4A, unescape |
| Process | 17 | Background process management |

### Running Tests

```bash
# Full suite
bash test_runner.sh

# Quick smoke (no parallel compilation)
bash test_runner.sh --verbose

# Individual
tests/test_json.c
tests/test_config.c
```

### Adding Tests

Each test file is a standalone `.c` with `main()`. Add to `test_runner.sh` as a `run_lib_test` entry.

---

## CI/CD

- **`c-build.yml`** — PR/merge: build + test_runner (clean compile, all 301 pass)
- **`c-release.yml`** — Tag push: `make clean && make && docker build && push to ghcr`
- **`c-asan.yml`** — Nightly: AddressSanitizer + UBSan on full test suite
- **`c-cppcheck.yml`** — Static analysis (cppcheck)
- **`c-coverage.yml`** — gcov coverage reports for library modules
- **`c-perf.yml`** — Performance regression detection (benchmark diffs)

---

## Development Guide

### Setup

```bash
git clone git@github.com:waefrebeorn/slermes.git
cd slermes/C
make -j$(nproc)
./hermes --version
```

### Workflow

1. Read `.hermes/mind-palace/` for current state and priority
2. Pick the next gap from battleship-v34.md
3. Build + test before PR
4. Update all docs — state, prestige, plan, overnight, goal-mantra, battleship, README, BANNER, vault
5. Commit with descriptive message

---

## Config Reference

Config is loaded from `~/.slermes/config.yaml` (or `$SLERMES_HOME/config.yaml`).

```yaml
model:
  default: gpt-4o
  temperature: 0.7

agent:
  verbose: true
  max_turns: 50
```

Full schema in `include/hermes_config.h`. Use `/config` in interactive mode to view/edit.

---

## Upstream

The C translation tracks [NousResearch/hermes-agent](https://github.com/NousResearch/hermes-agent). The fork is at [waefrebeorn/slermes](https://github.com/waefrebeorn/slermes).

- **Fork:** Diverged — upstream deleted C/; C/ lives exclusively on fork
- **C code:** Tracked in `C/` subdirectory of the slermes fork
- **History:** Original 277 commits on `c-work` branch → squashed onto upstream main as single commit (`d00d2f1d`)

### Upstream Drift

The C code was forked from upstream commit `2517917de` and later rebased onto upstream HEAD. Upstream subsequently deleted C/ entirely — C/ now lives exclusively on the fork. Key drift areas:
- Provider/API evolution (XAI retry, OAuth, fallback)
- Agent loop changes (retry buffer, credential pool)
- Gateway platform updates (Discord thread, Telegram heartbeat)
- Tool schema drift (patch unescape, TIRITH tar safety)
- MCP updates (mTLS, catalog picker)
- Security/auth overhaul (OAuth PKCE, API key enforcement)
- ~17k new tests

See `.hermes/mind-palace/battleship-v34.md` for the full gap map.
