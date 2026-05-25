# Hermes C — Entry (v14 — 2026-05-24)

## Build & Run
```bash
cd /home/wubu/hermes-agent-dev/C/
make -j$(nproc)                   # Build (0 errors, 0 warnings)
bash test_runner.sh                # 239/0/0
./hermes --version                 # v0.14.1+
./hermes --help                    # Usage
./hermes "hello"                   # One-shot
./hermes                          # Interactive (/help)
./hermes --gateway --platform telegram  # Gateway mode
make tui && ./hermes-tui          # ncurses TUI (experimental)
```

## Quick Stats
```
Suite:   243/0/0  (206 test files, zero failures)
Binary:  30MB     (dynamic ELF, 0 warnings)
Source:  154 .c + 66 .h = 220 files
Tools:   84 registered
Gateway: 19 platforms
Plugins: 10 .so
Parity:  ~78%    (131 gaps remaining — battleship v8)
```

## Key Paths
- **Source:** `/home/wubu/hermes-agent-dev/C/`
- **Config:** `~/.slermes/config.yaml` (~322 keys) + `~/.slermes/.env`
- **Mind palace:** `.hermes/mind-palace/`
- **Battleship:** `battleship-v8.md` (312 gaps across 22 sectors)
- **Vault:** `.hermes/vault/` (essays, bug bounty, credits)
- **DA v15:** `da-audit-v15.md`

## Architecture
```
CLI/Gateway → Agent Loop → LLM Client → 11 Provider modules (OpenAI-compat + native)
                 ↓
          85 Tools Registry
                 ↓
          10 Plugin .so (kanban, honcho, spotify, ...)
                 ↓
          58 Library Units (json, yaml, http, crypto, ...)
```

## 9 Verified Stubs (sector 1)
All in memory.c plugin_vtable NULL function pointers (import_json, export_json, get_by_hash, compress_old, get_prioritized, plugin_delete) + server.c shutdown no-op + video_gen.c generate NULL + commands.c background not-available message. All P2-P3.

## Upstream
847 commits. Python upstream ~183 commits merged, ~664 C-specific.
