# Slermes C — Entry Point (v4)

```
Suite:   226/0/23  (213 test files, zero failures)
Binary:  30MB     (dynamic ELF, 0 warnings)
Tools:   83      (46 .c files, all real handlers)
CLI:     79      (slash commands, all real)
Gateways:19      (platforms)
Providers:10     (.c modules)
Libs:    59      (lib/*/)
Gaps:    168 (294+ function-level)     (battleship-v12, ~35,600 LOC to port)
Parity:  ~60% (verified Triple DA v12)    (CLI + tools ahead, adapters/plugins behind)
```

## Orientation
- **Codebase:** `C/` — full C translation of Hermes Agent
- **Binary:** `slermes` (was `hermes`)
- **Config:** `~/.slermes/` (env: `SLERMES_HOME`)
- **Mind palace:** `.hermes/mind-palace/`
- **Battleship:** `battleship-v12.md` (171 gaps across 14 sectors)
- **Vault:** `.hermes/vault/` (achievements, historical data)
