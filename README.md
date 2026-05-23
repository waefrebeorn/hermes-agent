# C/ — Hermes Agent in C

**HONEST STATUS: ~69% structural parity toward 1:1 Python replacement (~220 gaps).**
One-binary replacement for the [Python hermes-agent](https://github.com/waefrebeorn/hermes-agent). Zero runtime dependencies beyond libc + libssl.

```
Binary:     hermes (~9.2M ELF, debug symbols, dynamically linked)
C LOC:      ~380K total (src + lib + tests — includes sqlite3 amalgamation)
Source:     105 src/ + 32 lib directories + 10 plugins = 147 modules
Tests:      116 test files, ~573 assertions, suite 154/0/0
Gateway:    19 platform adapters
Tools:      28 registered, 6 CDP/plugin-blocked stubs
Commands:   ~148 CMD handler functions
Libraries:  30 .a archives (libjson, libyaml, libhttp, libcrypto, libmcp, libpath,
            libdatetime, libcsv, libhash, libuuid, libbase64, libhtml, libtextwrap,
            libglob, libsignal, libenum, libdifflib, libregex, libjson5, libwebsocket,
            libtoml, libansi, libcron, libproc, libtui, libdb, libplugin, libskin,
            libtemplate, libprotobuf)
Providers:  9 native + metadata registry (7 provider-specific API quirks remain)
Plugins:    10 .so (kanban, honcho, spotify, disk-cleanup, file-memory,
            achievements, observability, skills, image_gen, google_meet)
Config:     ~322 YAML keys supported — profiles, `${VAR}`, `!include`, env override
```

## Quick Start

```bash
cd C/
make -j$(nproc)          # Build full hermes binary (phase5)
./hermes --version       # "WuBu Hermes v0.14.1"
bash test_runner.sh      # 154 suites, all passed
echo "/tools" | ./hermes # List registered tools
```

### Config

```bash
export SLERMES_HOME=~/.slermes
# Config:   $SLERMES_HOME/config.yaml (322 keys)
# Env vars: $SLERMES_HOME/.env
```

## Build Targets

```bash
make phase1        # 30 standalone libraries (.a archives)
make phase2        # Agent + CLI + 9 LLM providers
make phase3        # All tools (28 registered)
make phase4        # 19 gateway platforms
make phase5        # Cron scheduler (full hermes binary)
make hermes        # Same as phase5
make tui           # ncurses full-screen TUI → hermes-tui (stub)
make libs          # Standalone library archives only
make plugins       # Build all 10 .so plugins
make install-plugins  # Build + install .so plugins to ~/.hermes/plugins/
make docs          # Generate Doxygen HTML API docs (if doxygen available)
make release       # Automated release: version bump, build, test, tag
```

## Test

```bash
bash test_runner.sh                    # Full suite — 116 test files
bash test_runner.sh --verbose          # Detailed output per test
```

Suite: **154 passed, 0 failed, 0 skipped** (2026-06-01)

Test coverage areas:
- **Libraries**: json, yaml, http, crypto, cron, csv, datetime, path, hash, uuid, base64, html, textwrap, glob, signal, enum, difflib, regex, ansi, json5, toml, websocket, protobuf, dotenv, proc, template, skin, tui, display
- **Agent**: provider error handling (225 assertions, 9 providers), provider metadata, smoke tests, anthropic/azure/bedrock/google/openrouter depth tests, finish_reason, json_mode, fallback_routing, budget tracker, checkpoint, context, title, redact, sanitize, vault, secrets, tokenizer, xai_retirement
- **CLI**: commands, paths, display, TUI
- **Cron**: cron_lib, cron_tool, cron_sqlite, cron_extras
- **Tools**: file, web, terminal, exec_code, session_search, process, todo, memory, kanban, cronjob, skills, skill_mgmt, tts, vision, clarify, delegate, x_search, patch, tool_config, api_helpers, approval, url_safety, sandbox_escape, tirith, allowlist, result_storage
- **Gateway**: escape mode, slack_blocks, discord_interactions, whatsapp_msg
- **Plugins**: honcho, kanban, spotify, achievements, disk_cleanup, file_memory, google_meet, image_gen, observability, skills
- **Cross-cut**: file_permissions, audit, audit_rotate, redact

## Architecture

```
CLI/Gateway → Agent Loop → LLM Client → 9 Providers → HTTP/JSON
                 ↓
          Tool Registry (28 tools)
                 ↓
          Plugin Registry (10 .so)
                 ↓
          30 Library Archives
```

## Notable Bugfix History

- temperature=0.0 silent drop — all 9 providers
- response_format use-after-free — all 9 providers
- NULL stream chunk SIGSEGV — 6 providers
- cron_job_reset_retry(NULL) SEGV — strcmp on NULL
- cron_template_instantiate placeholder broken — json_get_str misuse
- title gen 6th word truncation
- x_search auth header — literal `***` instead of `%s`
- Secrets ow pointer not advanced

## Project Structure

```
C/
├── src/                # Source code
│   ├── agent/          # Provider adapters, LLM client, fallback, budget
│   ├── cli/            # CLI orchestrator, commands, display, config, paths
│   ├── cron/           # Scheduler, job store, locking, chaining
│   ├── gateway/        # Platform adapters (19 platforms)
│   ├── plugins/        # Plugin .so implementations (10)
│   ├── tools/          # Tool handlers (28 registered)
│   ├── acp/            # ACP server
│   └── main.c          # Entry point
├── lib/                # 30 standalone libraries (.a archives)
├── tests/              # 116 test files
├── include/            # Public headers
├── .hermes/mind-palace/ # Project state, plans, DA audits
└── references/         # Design patterns and documentation
```

## Upstream

183 commits behind Python hermes-agent at HEAD.

## License

See root LICENSE file.
