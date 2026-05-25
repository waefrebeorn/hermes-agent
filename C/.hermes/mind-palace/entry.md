# Slermes C — Entry Point (v2)

```
Suite:   226/0/23  (213 test files, zero failures)
Binary:  30MB     (dynamic ELF, ~15 warnings)
Tools:   85      (46 .c files, all real handlers)
CLI:     79      (slash commands, all real)
Gateways:19      (platforms)
Providers:10     (.c modules)
Libs:    59      (lib/*/)
Gaps:    ~55     (battleship-v9, ~35,600 LOC to port)
Parity:  ~82%    (CLI + tools ahead, adapters/plugins behind)
```

## Orientation
- **Codebase:** `C/` — full C translation of Hermes Agent
- **Binary:** `slermes` (was `hermes`)
- **Config:** `~/.slermes/` (env: `SLERMES_HOME`)
- **Mind palace:** `.hermes/mind-palace/`
- **Battleship:** `battleship-v9.md` (55 gaps across 9 sectors)
- **Vault:** `.hermes/vault/` (achievements, historical data)
