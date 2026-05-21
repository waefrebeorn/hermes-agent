# Slermes C — Current State Audit (May 24 DA-v1)

## Binary Truth Table — All Components

| Component | Status | LOC | Files | Verified |
|-----------|--------|-----|-------|----------|
| CLI | ✅ 69/69 parity | ~5K | commands.c + cli.c | make clean && make && test_runner |
| Tools (core) | ✅ 48/48 parity | ~15K | 25 .c files | /tools-verify: 54/54 found |
| Tools (kanban) | ✅ 9 registered | ~1.5K | kanban.c | gated on HERMES_KANBAN_TASK |
| Tools (CDP stubs) | ✅ 4 stubs | ~0.3K | browser.c | return error: needs CDP |
| Tools (computer_use) | ✅ stub | ~0.2K | computer_use.c | returns platform-unavailable |
| Gateway | ✅ 20/20 platforms | ~15K | 19 .c + server.c | make phase4 |
| Providers | ✅ 3/3 (OpenAI/Anthropic/Google) | ~1.4K | 3 .c files | provider_register_builtins() |
| LLM Client | ✅ provider-aware | ~2.3K | llm_client.c | 2 paths: provider + legacy fallback |
| Security | ✅ 4/4 (URL/Path/Tirith/Approval) | ~2K | 4 .c files | make phase3 |
| TUI | ✅ ncurses split-screen | ~11K | tui_fullscreen.c | make tui |
| Plugin | ✅ dlopen discovery | ~3K | libplugin/ | make phase5 |
| Streaming | ✅ true per-chunk | ~1K added | llm_client.c | provider-aware callback |
| Session DB | ⚠️ file-based JSON (no FTS5) | ~3K | libdb/ | grep-based search |
| Config | ⚠️ covers critical keys, misses ~30% | ~6K | config.c | reads .env + env vars |
| Tests | ✅ 43/43 | ~4K | test_runner + lib tests | bash test_runner.sh |

## Core vs Extra Tools

**Core parity (48 Python tools → 48 C tools):**
All _HERMES_CORE_TOOLS from toolsets.py registered in C.

**Extra C tools (6 beyond core):**
- web_get (HTTP GET, not in Python core)
- x_search (X/Twitter API, not in Python core)
- approval_status (approval query, not a separate tool in Python)
- voice_listen / voice_speak (STT/TTS, Python uses single voice_mode)
- web_extract alias (symlink to web_get)

## Binary Summary

```
Path:  /home/wubu/hermes-agent-dev/C/hermes
Size:  1.43 MB (ELF 64-bit)
Build: make clean && make -j$(nproc)
Config: ~/.slermes/config.yaml + ~/.slermes/.env
```

## What User Sees

Running `./hermes` → interactive CLI with:
- 69 slash commands
- 54 registered tools
- Provider-based LLM calls (OpenAI/Anthropic/Google)
- Threaded gateway (20 platforms)
- ncurses TUI via `--tui`
- Plugin discovery from ~/.slermes/plugins/
