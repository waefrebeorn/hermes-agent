# DEPENDENCIES — Python → C Equivalent Map
**HONEST status | May 25, 2026 | See battleship.md for full gap breakdown**

> ⚠️ **Note:** This file was updated to reflect ACTUAL implementation status.
> Previous version had all 🔲. The real status is a mix of ✅ 🟧 🟥.
> Phase 1 deps have real implementations. Phases 2-5 have varying completeness.

Every Python dependency Hermes Agent uses, mapped to its C equivalent for the translation.

## Direct Dependencies (hermes-agent runtime)

| Python Package | Role | C Equivalent | Status |
|---------------|------|-------------|--------|
| `openai` | LLM API client | Custom HTTP + JSON client | 🟧 Partial — OpenAI format only, auth header broken |
| `httpx` | HTTP/2 client | POSIX sockets + OpenSSL | ✅ Working — HTTP/1.1 HTTPS, no HTTP/2 |
| `httpcore` | HTTP lower layer | (same as above) | ✅ Working |
| `h11` | HTTP/1.1 | Built-in | ✅ Working |
| `anyio` | Async runtime | N/A (synchronous) | ✅ Not needed |
| `sniffio` | Async lib detection | N/A | ✅ Not needed |
| `pyyaml` | YAML config | Custom YAML reader | ✅ Working — config subset only |
| `ruamel.yaml` | YAML round-trip | (same) lossy accept | ✅ Working |
| `pydantic` | Validation/models | Manual struct validation | 🟧 Partial — no schema enforcement |
| `pydantic-core` | Validation core | (same) | 🟧 Partial |
| `jiter` | JSON iteration | Custom JSON parser | ✅ Working — full pipe |
| `jinja2` | Templating | Not implemented | ⬜ |
| `rich` | Terminal display | ANSI escape codes | 🟧 Partial — color works, spinner/panel/box not implemented |
| `prompt_toolkit` | Input handling | fgets (no readline) | 🟥 Broken — no history, no autocomplete |
| `pygments` | Syntax highlight | Not implemented | ⬜ |
| `croniter` | Cron parsing | Custom crontab parser | 🟧 Partial — basic crontab, no "every 30m" |
| `psutil` | Process info | /proc filesystem | ⬜ |
| `python-dotenv` | .env loading | Manual file parse | ✅ Working |
| `fire` | CLI argument parsing | getopt | ⬜ |
| `cryptography` | TLS/crypto | OpenSSL wrapper | ✅ Working — SHA-256, HMAC, base64, PKCE |
| `PyJWT` | JWT tokens | Not implemented | ⬜ |
| `cffi` | C FFI bridge | N/A | ✅ Not needed (native C) |
| `certifi` | CA bundle | System trust store | ✅ Working |
| `requests` | HTTP (legacy) | (same as httpx) | ✅ Working |
| `urllib3` | HTTP low-level | (same) | ✅ Working |
| `socksio` | SOCKS proxy | Not implemented | ⬜ |
| `socks` | PySocks | Not implemented | ⬜ |
| `tqdm` | Progress bars | display_progress declared but not implemented | 🟥 Broken |
| `termcolor` | Terminal color | ANSI escape codes | ✅ Working |
| `wcwidth` | Unicode widths | libc wcwidth | ✅ Working |
| `MarkupSafe` | HTML escaping | Manual string replace | ⬜ |
| `distro` | OS detection | uname + /etc/os-release | ⬜ |
| `tenacity` | Retry logic | Manual retry loop | ⬜ |
| `pytz` | Timezone data | libc timezone | ⬜ |
| `six` | Py2/3 compat | N/A | ✅ Not needed |

## Project Internal Modules (hermes-agent code → C)

| Python Module | Role | C File(s) | Status |
|--------------|------|-----------|--------|
| `run_agent.py` | Core agent loop | `src/agent/agent_loop.c` | 🟥 Broken — tool calling loop returns early |
| `cli.py` | CLI orchestrator | `src/cli/cli.c` | 🟧 Partial — basic REPL, no readline |
| `model_tools.py` | Tool orchestration | `src/agent/tool_dispatch.c` | 🟧 Partial — registry works |
| `hermes_state.py` | SQLite session store | `src/deps/db.c` | ✅ Working — file-based, no SQLite |
| `hermes_constants.py` | Path resolution | `src/hermes.h` | ✅ Working |
| `hermes_logging.py` | Logging | printf (no syslog) | 🟧 Partial |
| `tools/registry.py` | Tool registration | `src/tools/registry.c` | ✅ Working |
| `tools/*.py` | Tool implementations | `src/tools/*.c` | 🟧 Partial — 4/30+ tools |
| `gateway/run.py` | Gateway server | `src/gateway/server.c` | 🟧 Partial — Telegram only |
| `gateway/platforms/*.py` | Platform adapters | `src/gateway/platforms/*.c` | 🟧 Partial — Telegram only |
| `cron/scheduler.py` | Schedule engine | `src/cron/scheduler.c` | 🟥 Broken — memory-only |
| `cron/jobs.py` | Job management | `src/cron/jobs.c` | 🟥 Broken — returns "[]" stub |
| `agent/title_generator.py` | Session titles | `src/agent/title.c` | ✅ Working — extractive |
| `agent/display.py` | Terminal display | `src/cli/display.c` | 🟥 Broken — no spinner/panel/progress |
| `agent/context.py` | Context management | `src/agent/context.c` | ✅ Working |
| `agent/compression.py` | Context compression | Not implemented | ⬜ |
| `agent/memory.py` | Memory system | Not implemented | ⬜ |
| `agent/skill_commands.py` | Skill loading | `src/agent/skills.c` | 🟧 Partial — list only |
| `hermes_cli/main.py` | CLI entrypoint | `src/cli/main.c` | ✅ Working |
| `hermes_cli/config.py` | Config loading | `src/cli/config.c` | ✅ Working |
| `hermes_cli/commands.py` | Slash commands | `src/cli/commands.c` | 🟧 Partial — 4/50+ |

## External System Dependencies

| System | Role | C Equivalent | Status |
|--------|------|-------------|--------|
| Python runtime | Execution | libc + binary | ✅ Not needed |
| pip | Package mgmt | Static linking | ✅ Not needed |
| venv | Isolation | Static binary | ✅ Not needed |
| SQLite | Session DB | File-based JSON store | ✅ Working |
| libcrypto | Hashing/TLS | libssl/libcrypto | ✅ Working |
| ncurses | Terminal UI | ANSI codes (no ncurses) | 🟧 Partial |

## Translation Priority Order (HONEST)

```
Phase 1: Foundation ✅
  ├── types.h, config.h              ✅
  ├── deps/json.c                    ✅ cJSON wrapper
  ├── deps/yaml.c                    ✅ libyaml wrapper
  ├── deps/http.c                    ✅ raw socket + OpenSSL
  ├── deps/crypto.c                  ✅ OpenSSL SHA/HMAC/base64/PKCE
  ├── deps/db.c                      ✅ File-based session store
  └── deps/cli_display.c             ✅ ANSI codes (no ncurses)

Phase 2: Agent Core 🟧 (Partial)
  ├── src/agent/agent_loop.c         🟥 Tool calling broken
  ├── src/agent/llm_client.c         🟥 Auth header malformed
  ├── src/agent/context.c            ✅ Working
  └── src/cli/main.c                 ✅ Entry point

Phase 3: Tools 🟧 (Partial, 4/30+)
  ├── src/tools/registry.c           ✅ Working
  ├── src/tools/terminal.c           ✅ Working (partial — no PTY/background)
  ├── src/tools/file.c               🟧 Partial (no patch/search)
  ├── src/tools/web.c                🟥 web_search is alias
  └── src/tools/*.c                  ⬜ Missing 25+ tools

Phase 4: Gateway 🟧 (Partial, 1/18 platforms)
  ├── src/gateway/server.c           🟧 Telegram only
  └── src/gateway/platforms/         ⬜ Only Telegram

Phase 5: Cron + Advanced 🟥 (Broken)
  ├── src/cron/scheduler.c           🟥 Memory-only
  └── src/agent/skills.c             🟧 List only
```