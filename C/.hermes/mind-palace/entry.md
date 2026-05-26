# Slermes C — Entry Point (v7)

```
Suite:   227/0/24  (215 test files, zero failures)
Binary:  30MB     (dynamic ELF, 0 warnings)
Tools:   72      (unique, registry_register)
CLI:     80      (slash commands with handlers)
Gateways:19      (platforms)
Providers:9      (.c modules + metadata)
Libs:    59      (lib/*/)
Gaps:    ~369    (items across 11 phases, battleship-v15 Triple DA)
Parity:  ~43%    (function-level — ~1,412 C vs ~3,251 Python)
```

## Orientation
- **Codebase:** `C/` — full C translation of Hermes Agent
- **Binary:** `slermes` (was `hermes`)
- **Config:** `~/.slermes/` (env: `SLERMES_HOME`)
- **Mind palace:** `.hermes/mind-palace/`
- **Battleship:** `battleship-v15.md` (~369 items across 11 phases)
- **Vault:** `.hermes/mind-palace/vault/` (achievements, historical data)

## Phase Progress
- Phase 0 (Display): 14/16 done (V07 TUI, V08 Python TUI, V09 voice)
- Phase 1 (CLI Args): ✅ ALL DONE (40 commands wired)
- Phase 2 (Provider Parity): NEXT — deepen 8 + port 18
