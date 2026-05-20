# Dependency Map — Python → C Equivalents

Every Python dependency Hermes Agent uses, mapped to its C equivalent for the translation.

## Direct Dependencies (hermes-agent runtime)

| Python Package | Role | C Equivalent | Status |
|---------------|------|-------------|--------|
| `openai` | LLM API client | `libcurl` + custom JSON | 🔲 |
| `httpx` | HTTP/2 client | `libcurl` (HTTP/2 via nghttp2) | 🔲 |
| `httpcore` | HTTP lower layer | `libcurl` (same as above) | 🔲 |
| `h11` | HTTP/1.1 | `libcurl` built-in | 🔲 |
| `anyio` | Async runtime | POSIX threads + poll/select | 🔲 |
| `sniffio` | Async lib detection | N/A (no-op in C) | 🔲 |
| `pyyaml` | YAML config | `libyaml` | 🔲 |
| `ruamel.yaml` | YAML round-trip | `libyaml` (lossy — acceptable) | 🔲 |
| `pydantic` | Validation/models | Manual struct validation | 🔲 |
| `pydantic-core` | Validation core | Manual struct validation | 🔲 |
| `jiter` | JSON iteration | `cJSON` iteration | 🔲 |
| `jinja2` | Templating | Custom string builder / `mustache-c` | 🔲 |
| `rich` | Terminal display | `ncurses` + custom renderer | 🔲 |
| `prompt_toolkit` | Input handling | `linenoise` / `libreadline` | 🔲 |
| `pygments` | Syntax highlight | `highlight` lib or built-in | 🔲 |
| `croniter` | Cron parsing | Custom crontab parser (simple) | 🔲 |
| `psutil` | Process info | `/proc` filesystem | 🔲 |
| `python-dotenv` | `.env` loading | Manual file parse | 🔲 |
| `fire` | CLI argument parsing | `getopt` / `argparse3` | 🔲 |
| `cryptography` | TLS/crypto | `openssl` / `libsodium` | 🔲 |
| `PyJWT` | JWT tokens | `libjwt` / `openssl` | 🔲 |
| `cffi` | C FFI bridge | N/A (native C, no bridge needed) | 🔲 |
| `certifi` | CA bundle | System trust store (`/etc/ssl/certs`) | 🔲 |
| `requests` | HTTP (legacy) | `libcurl` | 🔲 |
| `urllib3` | HTTP low-level | `libcurl` | 🔲 |
| `socksio` | SOCKS proxy | `libcurl` SOCKS5 built-in | 🔲 |
| `tqdm` | Progress bars | Simple terminal progress | 🔲 |
| `termcolor` | Terminal color | ANSI escape codes | 🔲 |
| `wcwidth` | Unicode widths | `libc` wcwidth | 🔲 |
| `MarkupSafe` | HTML escaping | Manual string replace | 🔲 |
| `distro` | OS detection | `uname` + `/etc/os-release` | 🔲 |
| `tenacity` | Retry logic | Manual retry loop | 🔲 |
| `pytz` | Timezone data | `libc` timezone / `date` utilities | 🔲 |
| `six` | Py2/3 compat | N/A (C has no version concern) | 🔲 |

## Project Internal Modules (hermes-agent code → C)

| Python Module | Role | C File(s) | Status |
|--------------|------|-----------|--------|
| `run_agent.py` | Core agent loop | `src/agent/agent_loop.c` | 🔲 |
| `cli.py` | CLI orchestrator | `src/cli/cli.c` | 🔲 |
| `model_tools.py` | Tool orchestration | `src/agent/tool_dispatch.c` | 🔲 |
| `hermes_state.py` | SQLite session store | `src/deps/db.c` (uses sqlite3) | 🔲 |
| `hermes_constants.py` | Path resolution | `src/hermes.h` | 🔲 |
| `hermes_logging.py` | Logging | `src/hermes.h` (syslog) | 🔲 |
| `tools/registry.py` | Tool registration | `src/tools/registry.c` | 🔲 |
| `tools/*.py` | Tool implementations | `src/tools/*.c` | 🔲 |
| `gateway/run.py` | Gateway server | `src/gateway/server.c` | 🔲 |
| `gateway/platforms/*.py` | Platform adapters | `src/gateway/platforms/*.c` | 🔲 |
| `cron/scheduler.py` | Schedule engine | `src/cron/scheduler.c` | 🔲 |
| `cron/jobs.py` | Job management | `src/cron/jobs.c` | 🔲 |
| `agent/title_generator.py` | Session titles | `src/agent/title.c` | 🔲 |
| `agent/display.py` | Terminal display | `src/cli/display.c` | 🔲 |
| `agent/context.py` | Context management | `src/agent/context.c` | 🔲 |
| `agent/compression.py` | Context compression | `src/agent/compress.c` | 🔲 |
| `agent/memory.py` | Memory system | `src/agent/memory.c` | 🔲 |
| `agent/skill_commands.py` | Skill loading | `src/agent/skills.c` | 🔲 |
| `hermes_cli/main.py` | CLI entrypoint | `src/cli/main.c` | 🔲 |
| `hermes_cli/config.py` | Config loading | `src/cli/config.c` | 🔲 |
| `hermes_cli/commands.py` | Slash commands | `src/cli/commands.c` | 🔲 |

## External System Dependencies

| System | Role | C Equivalent |
|--------|------|-------------|
| Python runtime | Execution environment | `libc` + compiled binary |
| pip | Package management | Static linking / none needed |
| venv | Isolation | Static binary, no venv needed |
| SQLite (via Python) | Session DB | `libsqlite3` directly |
| libcrypto | Hashing/TLS | `libssl` / `libcrypto` |

## Translation Priority Order

```
Phase 1: Foundation (compile without Python)
  ├── types.h, config.h              → type system + config parser
  ├── deps/json.c                    → cJSON wrapper (replace pydantic)
  ├── deps/yaml.c                    → libyaml wrapper (replace pyyaml)
  ├── deps/http.c                    → libcurl wrapper (replace httpx)
  ├── deps/crypto.c                  → openssl wrapper (replace cryptography)
  ├── deps/db.c                      → sqlite3 wrapper (replace hermes_state)
  └── deps/cli_display.c             → ncurses wrapper (replace rich)

Phase 2: Agent Core (runs a basic conversation)
  ├── src/agent/agent_loop.c         → core loop
  ├── src/agent/llm_client.c         → LLM API calls (via deps/http)
  ├── src/agent/context.c            → message context
  └── src/cli/main.c                 → CLI entry point

Phase 3: Tools (fully functional agent)
  ├── src/tools/registry.c           → tool registration
  ├── src/tools/terminal.c           → shell execution
  ├── src/tools/file.c               → file I/O
  ├── src/tools/web.c                → web search
  └── src/tools/*.c                  → remaining tools

Phase 4: Gateway (messaging platforms)
  ├── src/gateway/server.c           → gateway event loop
  ├── src/gateway/platforms/         → per-platform adapters
  └── src/gateway/platforms/telegram.c  → Telegram API

Phase 5: Cron + Advanced
  ├── src/cron/scheduler.c           → cron engine
  └── src/agent/skills.c             → skill system
```
