# Slermes C — State Dashboard (v1 — 2026-05-25)

## Build Metrics (code-verified)

| Metric | Value | As Of |
|--------|-------|-------|
| Suite | **226/0/23** — 213 test files | 2026-05-25 |
| Binary | **30MB ELF**, 0 errors, ~15 pre-existing warnings | 2026-05-25 |
| Source `.c` files | 625 files (src/ + lib/ + tests/) | 2026-05-25 |
| Source code lines | **84,164** (src/) | 2026-05-25 |
| Library lines | **286,003** (lib/) | 2026-05-25 |
| Test lines | **48,262** (tests/) | 2026-05-25 |
| Header lines | **8,462** (include/) | 2026-05-25 |
| Total | **~427K lines** | 2026-05-25 |
| Tools registered | **85** (all real handlers) | 2026-05-25 |
| CLI commands | **79** slash commands | 2026-05-25 |
| Gateway platforms | **19** .c files | 2026-05-25 |
| Provider modules | **10** .c files | 2026-05-25 |
| Library directories | **59** lib/*/ | 2026-05-25 |
| Git commits | **858+** | 2026-05-25 |

## Known Gaps (from Triple DA verification)

| Category | C | Python | Status |
|----------|---|--------|--------|
| Agent modules | 52 .c | 77 .py | 28 name-match ported, rest merged/renamed |
| Tool files | 46 .c | 75 .py | 85 `registry_register` calls — many Python tools consolidated |
| Gateway platforms | 19 .c | 31 .py | 18 direct name-match, 13 sub-modules not ported |
| Providers | 10 .c | 11 .py | 10 C providers cover all major APIs |
| Environment backends | 1 (consolidated) | 11 .py | C has lib/libtoolbackend merged |

### Real Gaps (verified not ported):
1. **conversation_compression.py** — conversation compression logic
2. **curator_backup.py** — backup state management for curator
3. **title_generator.py** — automatic session title generation
4. **tool_executor.py** — tool execution orchestration
5. **process_bootstrap.py** — subprocess bootstrap helpers
6. **models_dev.py** — dev model configuration
7. **plugin_llm.py** — LLM calls from plugins
8. **13 gateway sub-platforms** — feishu_comment, wecom_callback/crypto, yuanbao_media/proto/sticker, signal_rate_limit, telegram_network, etc.

### Placeholder/TODO Items
1. src/acp/resource.c:263 — Non-file URI placeholder return
2. src/tools/mcp_tool.c:1287 — Placeholder auth entry for future server
3. src/agent/system_prompt.c:53 — TODO in system prompt text (not code)

## Usage
```
slermes --version           # WuBu Slermes v0.14.1-wubu
slermes init                # Create ~/.slermes/config.yaml + .env
slermes doctor              # System diagnostics
slermes completions install # Shell completion setup
slermes -q "hello"         # One-shot query
slermes                     # Interactive CLI (79 slash commands)
slermes gateway             # Multi-platform gateway (19 platforms)
make install                # Install to /usr/local/bin
```
