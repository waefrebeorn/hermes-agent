# Slermes C — Entry Point (v8)

```
Suite:   226/0/25  (216 test files, zero failures)
Binary:  31MB     (dynamic ELF, 0 warnings)
Tools:   77      (unique, registry_register)
CLI:     80      (slash commands with handlers)
Gateways:19      (platforms)
Providers:9      (.c modules + metadata)
Libs:    59      (lib/*/)
Gaps:    ~366    (items across 11 phases, battleship-v15 Triple DA)
Parity:  ~43%    (function-level — ~1,412 C vs ~3,251 Python)
```

## Orientation
- **Codebase:** `C/` — full C translation of Hermes Agent
- **Binary:** `slermes` (was `hermes`)
- **Config:** `~/.slermes/` (env: `SLERMES_HOME`)
- **Mind palace:** `.hermes/mind-palace/`
- **Battleship:** `battleship-v15.md` (~366 items across 11 phases)
- **Vault:** `.hermes/mind-palace/vault/` (achievements, historical data)

## Phase Progress
- Phase 0 (Display): 14/16 done (V07 TUI, V08 Python TUI, V09 voice)
- Phase 1 (CLI Args): ✅ ALL DONE (40 commands wired)
- Phase 2 (Provider Parity): claims HEAVILY STALE per DA v15
- Phase 3 (Tool Features): #21 approval ✅, 52 remaining
