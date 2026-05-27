# Slermes C

**Slermes — Full C translation of the Python [Hermes Agent](https://github.com/NousResearch/hermes-agent) by Nous Research.**  
One static binary. Zero runtime deps beyond libc + libssl. 30M ELF.

```text
||||||| Suite:  257/0/0 (224 test files, completes in <60s)
|||||| Binary: 30M    (dynamic ELF, -O2 -g)
|||||| Source: 449 .c files (src/ + lib/ + tests/): 108K C LOC
|||||||| Parity:  ~43%   (~319 item-level gaps — see battleship-v16)
||||||Stubs:  Phase 0a all resolved. Phase 1 CLI Args ✅ — all 98 commands wired.
|||||Display: 16 gaps — 14/16 done (V07 TUI, V08 Python TUI, V09 voice remain)
|||||Build:  gcc -O2 -g -Wall -Wextra -Wpedantic — 0 errors, 0 warnings
|||||CLI:    98 cmd_ functions — 83 unique tools registered
|||||Tools:  83 registered (86 at runtime with MCP dynamic)
||||Libraries: 59 C modules — zero external deps beyond libc+libssl
||||Gateway: 19 platform adapters (Telegram, Discord, Slack, Signal, SMS, etc.)
||||Providers: 10 .c modules + metadata (OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom, Copilot)
```

> **Symlink note:** `README.md` → `C/README.md`. The canonical README lives at `C/README.md`. Edit that file; the root follows automatically.

---

## Table of Contents

- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [Build System](#build-system)
- [All Tools (89 Registered)](#all-tools-89-registered)
- [Gateway Platforms (19)](#gateway-platforms-19)
- [LLM Providers (10)](#llm-providers-10)
- [Plugins (10 .c)](#plugins-10-c-0-so)
- [Libraries (59 Units)](#libraries-59-units)
- [CLI Commands (118 Slash, Real)](#cli-commands-78-slash-real)
- [Verified Stubs (All Resolved)](#verified-stubs-all-resolved)
- [Bugfix History](#bugfix-history)
- [Project Structure](#project-structure)
- [The Agentic Process (.hermes)](#the-agentic-process-hermes)
|||- [Battleship Roadmap (171 Gaps)](#battleship-roadmap-171-gaps)
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
bash test_runner.sh        # 230/0/25
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
                    │   LLM Client + 11         │
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
                    │   59 Library Units       │
                    │  (json, yaml, http,      │
                    │   crypto, mcp, cron...)  │
                    └─────────────────────────┘
```

**Data flow:** User message → entry point (CLI/Gateway/TUI) → Agent Loop (context management, budget tracking, fallback routing, interrupt handling, checkpoint/resume) → LLM Client → Provider adapter → LLM response → tool calls → plugin dispatch → response to user.

---

## Build System

```bash
make hermes           # Full binary (phase5) — 0 errors, 30M
make plugins          # 10 .so shared objects
make tui              # ncurses TUI → hermes-tui (experimental)
make libs             # 59 library compilation units
make install-plugins  # Build + copy .so → ~/.hermes/plugins/
make docs             # Doxygen HTML docs (if doxygen available)
```

### 5-Phase Build Pipeline

| Phase | What | Output |
|-------|------|--------|
|| P1 | 59 library units (.o) | lib/*.o |
| P2 | Agent core + CLI + 10 providers | src/agent/*.o, src/cli/*.o |
| P3 | 85 tool handlers | src/tools/*.o |
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

## All Tools (89 Registered)

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
| `browser_vision` | `src/tools/browser.c` | ✅ CDP-dependent (real) |
| `browser_console` | `src/tools/browser.c` | ✅ CDP-dependent (real) |
| `browser_dialog` | `src/tools/browser.c` | ✅ CDP-dependent (real) |
| `browser_cdp` | `src/tools/browser.c` | ✅ CDP-dependent (real) |

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
| `mcp_connect` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_disconnect` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_list_tools` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_call_tool` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_list_resources` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_read_resource` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_list_prompts` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_get_prompt` | `src/tools/mcp_tool.c` | ✅ Complete |
| (dynamic) | `src/tools/mcp_tool.c` | Tools registered by connected servers |

### Media & Input
| Tool | File | Status |
|------|------|--------|
| `vision_analyze` | `src/tools/vision.c` | ✅ Complete |
| `text_to_speech` | `src/tools/tts.c` | ✅ Complete |
| `voice_listen` | `src/tools/voice_mode.c` | ✅ Complete |
| `voice_speak` | `src/tools/voice_mode.c` | ✅ Complete |
| `image_generate` | `src/tools/image_gen.c` | ✅ Complete (shells to plugin) |
| `computer_use` | `src/tools/computer_use.c` | 🔴 **Full stub** (needs macOS) |

### Automation
| Tool | File | Status |
|------|------|--------|
| `cronjob` | `src/tools/cronjob.c` | ✅ Complete |
| `cron_cmd` | `src/cron/cron_cli.c` | ✅ Complete |
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
| `discord_read` | `src/tools/discord.c` | ✅ Complete |

### Internal
| Tool | File | Status |
|------|------|--------|
| `execute_code` | `src/tools/exec_code.c` | ✅ Complete |
| `rate_limit` | `src/tools/rate_limit.c` | ✅ Complete |
| `url_safety` | `src/tools/url_safety.c` | ✅ Complete |
| `tirith` | `src/tools/tirith.c` | ✅ Complete |
| `result_storage` | `src/tools/result_storage.c` | ✅ Complete |

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

**Test status:** Stub test only (escape_mode, slack_blocks, discord_interactions, whatsapp_msg). No per-platform integration tests yet — gap T01.

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

**Metadata:** Provider metadata system handles rate limits, cost tracking, model lists, streaming support, JSON mode, and API quirks. Provider-specific API quirks remaining: 7 (tracked as sector R in battleship).

---

## Plugins (10 .c files, 0 .so built)

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
| Image generation | `plugin_image_gen.so` | 🔴 **Fake URLs** |
| Google Meet | `plugin_google_meet.so` | ✅ |

---

## Libraries (59 Units)

Libraries are compiled directly into the binary (no intermediate `.a` archives). Each is a self-contained module under `lib/`.

| Library | Purpose | Status |
|---------|---------|--------|
| `libjson` | JSON parser/builder | ✅ |
| `libyaml` | YAML parser | ✅ |
| `libhttp` | HTTP/HTTPS client | ✅ |
| `libcrypto` | AES, SHA, HMAC | ✅ |
| `libmcp` | MCP transport layer | ✅ |
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
| `libdb` | SQLite wrapper | ✅ |
| `libplugin` | Plugin loading | ✅ |
| `libskin` | Skin/theming engine | ✅ |
| `libtemplate` | Simple template engine | ✅ |

---

|CLI: 79 commands (all real, tab complete + history)

The CLI uses a central command registry (`cli/commands.c`) with alias resolution and subcommand dispatch.

**Categories:**
- **Core:** chat, session, history, model, provider, tools
- **Config:** config show/set/get/edit/migrate, env, profiles
- **Skills:** skills list/view/manage/scan/validate/sync/bundle/search
- **Gateway:** gateway start/stop/status/platforms
- **Cron:** cron list/add/remove/pause/resume/retry
- **Plugins:** plugins list/install/remove
- **MCP:** mcp connect/disconnect/servers/tools
- **System:** version, help, doctor, update, logs

---

## Verified Stubs (9 Remaining — sector 1 in battleship-v8)

9 verified stubs remain — all documented with clear resolution paths in battleship-v8.md:
- **S01-S06** — memory.c plugin_vtable NULL function pointers (import_json, export_json, get_by_hash, compress_old, get_prioritized, plugin_delete always false)
- **S07** — server.c plat.shutdown = NULL (no-op, handled by gw_platform_shutdown_all)
- **S08** — video_gen.c fal_provider.generate = NULL (uses handler directly)
- **S09** — commands.c cmd_background "background mode not available" message

All are P2-P3. The 85 tools + 79 CLI commands are real implementations.

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
waefrebeorn/slermes/         ← Repo root
├── C/                            ← All source code (canonical README lives here)
│   ├── src/                      ← 154 .c files
│   │   ├── agent/                ←   Provider adapters, LLM client, fallback routing,
│   │   │                           budget tracker, checkpoint/resume, audit, redact/sanitize
│   │   ├── cli/                  ←   CLI orchestrator, command registry, config,
│   │   │                           display engine, TUI (ncurses)
│   │   ├── cron/                 ←   Scheduler, SQLite job store, locking, retry
│   │   ├── gateway/              ←   Server + 19 platform adapters
│   │   │   └── platforms/        ←     Individual platform implementations
│   │   ├── plugins/              ←   10 .so plugin implementations (.c + Makefile)
│   │   ├── tools/                ←   85 tool handler implementations
│   │   ├── acp/                  ←   ACP JSON-RPC server
│   │   └── main.c                ←   Entry point (CLI option parsing + dispatch)
│   ├── lib/                      ←   59 library units (compiled directly, no .a)
│   │   ├── libjson/              ←   JSON parser/builder
│   │   ├── libhttp/              ←   HTTP/HTTPS client (libcurl wrapper)
│   │   ├── libmcp/               ←   MCP transport layer (stdio, server, SSE)
│   │   ├── libdb/                ←   SQLite wrapping (sessions, cron state, store)
│   │   ├── libcrypto/            ←   AES, SHA256, HMAC, base64
│   │   └── ...                   ←   25 more
│   ├── include/                  ←   66 headers (64 include/ + 2 src/)
│   ├── tests/                    ←   200+ test files (via test_runner.sh)
│   ├── Dockerfile                ←   Multi-stage Docker build
│   ├── Makefile                  ←   5-phase build pipeline
│   └── .hermes/                  ←   Agentic process documentation (see below)
├── .github/workflows/            ←   CI (c-build.yml, nix-lockfile-fix.yml)
└── README.md → C/README.md       ←   Symlink (edit C/README.md)
```

---

## The Agentic Process (.hermes)

The `.hermes/mind-palace/` directory documents the entire development process — every DA audit, every decision, every gap discovered. This is the **agentic record**: how an AI agent systematically translates 75K LOC of Python to C through iterative discovery, testing, and verification.

### Reading Order (for a new AI agent assuming this project)

| Step | File | What It Contains |
|------|------|-----------------|
| 1 | `prestige_prompt.md` | Priority queue + current state snapshot |
| 2 | `mind-palace/goal-mantra.md` | Perpetual goal + the loop (one-page session kickoff) |
| 3 | `mind-palace/state.md` | Binary truth table — every sector's done/total |
| 4 | `plan.md` | Phase completion + next steps sorted by priority |
| 5 | `entry.md` | Build/run commands + architecture |
| 6 | `overnight-map.md` | Session navigation + fallback task |
| 7 | `testing.md` | Test suite coverage + known gaps |
| 8 | `da-v12-triple-audit.md` | Final Triple DA audit — methodology, findings, 8 verified stubs |
|| 9 | `plans/battleship-v8.md` | Complete ~156-gap roadmap by sector (Triple DA verified) |
| 10 | `plans/battleship-index.md` | Quick-reference dashboard |

### Vault (Documentation)

| File | Contents |
|------|----------|
| `vault/bug-bounty.md` | 16 bugs discovered through DA audits (6 critical, 10 high) |
| `vault/achievements.md` | Milestones reached |
| `vault/credits.md` | PBS credit tracking ($69.32 total, usage by provider) |
| `vault/legacy-plans-archive.md` | Pre-DA plans (archived for reference) |
| `vault/translation-essay.md` | Translation methodology — how Python→C porting works |
| `vault/archived-*.md` | Old DA audits (v6, v10) and stale roadmaps — kept for historical trace |

### Devil's Advocate Audits

The DA process is the project's quality backbone — every claim is triple-verified before being marked ✅:

1. **Code audit** — grep for struct fields, function definitions, test files
2. **Build verification** — `make clean && make -j$(nproc) && bash test_runner.sh`
3. **Upstream diff** — compare against Python origin/main for drift

| DA | Date | Key Findings |
|----|------|-------------|
| v6 | May 22 | First systematic audit. Found 9 false ✅. Real parity: ~36% |
| v10 | May 23 | Post-upstream-sync audit. 21 new gaps found. Parity: 34% |
| v11 | May 23 | Stub hunt (4 found). Dockerfile CI bug. Battleship 468→500. Parity: 32% |
| v12 | May 24 | Triple DA audit. 8 verified stubs, all claims verified. Parity: ~60% |
| **v18** | **May 28** | **CDP stub correction — all 3 CDP "stubs" were real handlers. 9 verified stubs. Parity: ~60%** |

---

## Battleship Roadmap (346 Gaps)

The battleship system tracks every gap needed to reach 1:1 Python parity. Gaps are organized by 22 sectors (S1-S22) with coordinate IDs.

| Sector | Area | Gaps | P1 | P2 | P3 |
|--------|------|------|-----|-----|------|
| S1 | Confirmed Stubs | 8 | 0 | 6 | 2 |
| S2 | Placeholder/Unwired | 14 | 0 | 4 | 10 |
| S3 | Dead Code | 14 | 0 | 2 | 12 |
| S4 | Missing Agent Modules | 17 | 4 | 8 | 5 |
| S5 | Agent Module Depth | 15 | 0 | 12 | 3 |
| S6 | Missing Subdirectory Modules | 22 | 0 | 12 | 10 |
| S7 | Tool Depth Gaps | 13 | 1 | 10 | 2 |
| S8 | Gateway Platform Depth | 25 | 0 | 21 | 4 |
| S9 | Configuration | 14 | 0 | 10 | 4 |
| S10 | Library Depth | 28 | 0 | 20 | 8 |
| S11 | Bug Fixes | 11 | 0 | 9 | 2 |
| S12 | Test Coverage | 25 | 0 | 25 | 0 |
| S13 | API Server Depth | 5 | 0 | 5 | 0 |
| S14 | TUI Depth | 8 | 0 | 7 | 1 |
| S15 | Curator Depth | 3 | 0 | 2 | 1 |
| S16 | Prompt Caching | 5 | 0 | 4 | 1 |
| S17 | Shell Hooks | 3 | 0 | 3 | 0 |
| S18 | Vault Encryption | 3 | 0 | 2 | 1 |
| S19 | Security Depth | 6 | 0 | 4 | 2 |
| S20 | C-Only New Features | 10 | 0 | 4 | 6 |
| S21 | Refactoring | 10 | 0 | 2 | 8 |
| S22 | Integration & CI | 7 | 0 | 6 | 1 |
| | **Total** | **266** | **5** | **168** | **93** |

**Full details:** [battleship-v8.md](.hermes/mind-palace/battleship-v8.md)

### Top 5 P1 Gaps (critical path)

| Priority | ID | What | LOC | Why |
|----------|----|------|-----|-----|
| P1 | A02 | context_compressor.py port | 1568 | Core agent infra missing |
| P1 | A03 | conversation_compression.py port | 603 | Related to A02 |
| P1 | D02 | Skill breakdown in insights | 80 | User-facing feature |
| P1 | A23 | nous_rate_guard.py port | 325 | Rate limiting from Nous Portal |
| P1 | A27 | rate_limit_tracker.py port | 246 | Token rate tracking |

---

## Test Suite

```bash
bash test_runner.sh                    # Full suite (202 files)
bash test_runner.sh --verbose          # Per-test output
```

**237 passed, 0 failed, 0 skipped** (2026-05-24).

### Coverage by Area

| Area | Files | Key Tests |
|------|-------|-----------|
| **Libraries** | ~30 | json, yaml, http, crypto, cron, csv, datetime, path, hash, uuid, base64, html, textwrap, glob, signal, enum, difflib, regex, ansi, json5, toml, websocket, protobuf, dotenv, proc, template, skin, tui, display |
| **Providers** | ~12 | provider_error (225 assertions, 9 providers), anthropic_depth, azure_full, bedrock_full, google_full, openrouter_depth, provider_metadata, finish_reason, json_mode |
| **Agent** | ~8 | context, title, fallback, budget, checkpoint, redact, sanitize, vault, secrets, tokenizer, xai_retirement |
| **CLI** | ~5 | commands, paths, display, TUI |
| **Cron** | 4 | cron_lib (51), cron_tool, cron_sqlite (48), cron_extras (41) |
| **Tools** | ~25 | file, web, terminal, exec_code, session, process, todo, memory, kanban, cronjob, skills, skill_mgmt, tts, vision, clarify, delegate, x_search, patch, tool_config, api_helpers, approval, url_safety, sandbox_escape, tirith, allowlist, result_storage |
| **Gateway** | ~5 | escape_mode, slack_blocks, discord_interactions, whatsapp_msg |
| **Plugins** | ~10 | honcho, kanban, spotify, achievements, disk_cleanup, file_memory, google_meet, image_gen, observability, skills |

### Known Test Gaps (Sector T)
- **T01**: No per-platform gateway integration tests (19 platforms)
- **T02**: CLI coverage ~60%, target >80%
- **T03**: No TUI component tests
- **T04**: No MCP server/transport integration tests
- **T05**: No ACP protocol tests
- **T06**: No browser CDP test harness (needs CDP server)
- **T07**: No plugin sandbox loading tests
- **T08**: No fuzz testing for JSON/config/message parsers
- **T09**: No memory leak detection in CI (valgrind/ASan)
- **T10**: No thread safety tests

---

## CI/CD

### C Build & Test (`c-build.yml`)
- **Trigger:** Push/PR to `main` modifying `C/**` or workflow files
- **Steps:** Install deps → `make -j$(nproc)` → `./hermes --help` → `bash test_runner.sh --verbose` → `make tui` → `make plugins`
- **Docker:** Multi-stage build, produces ~20MB image
- **Artifacts:** On failure, uploads full `C/` directory

### Status
- ✅ Linux x86_64 build (0 errors)
- ✅ Full test suite (217/0/0)
- ✅ Docker image build
- 🟡 ASan/UBSan not yet in CI (gap U04)
- 🟡 No cross-compilation matrix (gap U05)
- 🟡 No release automation (gap U06)

### CI/CD Roadmap (Sector U)
| ID | Gap | Priority |
|----|-----|----------|
| U01 | C build must pass before merge | P0 |
| U02 | Docker build CI step | P0 ✅ Fixed |
| U03 | Attach binary to PR as artifact | P1 |
| U04 | ASan/UBSan CI job | P1 |
| U05 | Cross-compilation (aarch64, arm) | P2 |
| U06 | Release automation | P2 |
| U07 | Code coverage upload (lcov→Codecov) | P2 |
| U08 | Pre-commit hooks (format, lint) | P1 |
| U09 | Dependency vulnerability scanning | P2 |
| U10 | Performance gate (binary size) | P3 |

---

## Development Guide

### Iteration Cycle
```bash
make -j$(nproc)           # Build (3-8 seconds incremental)
bash test_runner.sh       # Test (60-90 seconds full suite)
./hermes --help           # Smoke test
# Repeat after every change
```

### Debugging SIGSEGV (CUDA, large context, async)
When the release binary crashes and GDB is impractical:
1. Insert `fprintf(stderr, "MARK_fnname\n");` at function boundaries
2. Rebuild affected `.o`: `make -j$(nproc)`
3. Run — last visible MARK = crash point
4. Read that section, fix, remove MARKers, verify

**Don't:** Read 15+ source files before acting. The crash is in one function.

### Address Sanitizer
```bash
make CFLAGS="-O1 -g -fsanitize=address" LDFLAGS="-fsanitize=address" hermes
./hermes "test" 2>&1 | tail -50
```

### Debug Build (stricter warnings)
```bash
make CFLAGS="-O0 -g3 -Wall -Wextra -Wpedantic -Werror" hermes
```

### Memory Leak Detection
```bash
make CFLAGS="-O1 -g" hermes
valgrind --leak-check=full --show-leak-kinds=all ./hermes "hello" 2>&1
```

### Cross-Referencing Python
The Python reference lives at `~/.hermes/hermes-agent/` (or the upstream repo). To find the Python equivalent of a C file:
```bash
grep -rn "function_name" ~/.hermes/hermes-agent/ --include='*.py'
```

---

## Config Reference

Config is loaded from `$SLERMES_HOME/config.yaml` with `$SLERMES_HOME/.env` for secrets.

| Feature | Support | Example |
|---------|---------|---------|
| YAML keys | ~322 | `model: deepseek/deepseek-v4` |
| Profiles | ✅ | `profile: work` → `~/.slermes/profiles/work.yaml` |
| `${VAR}` expansion | ✅ | `token: ${OPENAI_API_KEY}` |
| `!include` | ✅ | `config: !include ./base.yaml` |
| Env override | ✅ | `export HERMES_MODEL=gpt-4` |
| Migration | ✅ | Auto-migrates old config formats |
| Validation | ✅ | Schema validation on load |

Key config sections: `provider`, `model`, `tools`, `gateway`, `plugins`, `cron`, `mcp_servers`, `terminal`, `display`, `session`, `skills`.

---

## Upstream

- **Python Hermes Agent:** [NousResearch/hermes-agent](https://github.com/NousResearch/hermes-agent)
- **Slermes repo:** [waefrebeorn/slermes](https://github.com/waefrebeorn/slermes)
- **Delta:** 0 behind upstream (183 Python commits merged), 400 ahead (C-specific commits)

The upstream sync strategy:
1. `git fetch origin` (NousResearch)
2. `git rebase origin/main` — merge Python changes
3. Rebuild C — any compilation errors mean upstream changed a shared interface
4. Run full suite — any test failures mean a feature mismatch
5. Update battleship with new gaps from Python files that have no C equivalent
6. Commit + push

---

## License

MIT. See root `LICENSE` file.
