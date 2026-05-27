# Slermes C — Entry Point (v9)

```
Suite:   229/0/25  (219 test files, zero failures)
Binary:  30M     (dynamic ELF, 0 warnings)
Tools:   77      (unique, registry_register)
CLI:     98      (slash commands with handlers)
Gateways:19      (platforms)
Providers:9      (.c modules + metadata)
Libs:    59      (lib/*/)
Gaps:    ~373    (items across 11 phases, battleship-v16 Triple DA)
|Parity:  ~43%    (function-level — ~1,912 C vs ~3,251 Python)
```

## Orientation
- **Codebase:** `C/` — full C translation of Hermes Agent
- **Binary:** `slermes` (was `hermes`)
- **Config:** `~/.slermes/` (env: `SLERMES_HOME`)
- **Mind palace:** `.hermes/mind-palace/`
- **Battleship:** `battleship-v16.md` (~373 items across 11 phases)
- **Vault:** `.hermes/mind-palace/vault/` (achievements, historical data)

## Phase Progress
- Phase 0 (Display): 14/16 done (V07 TUI, V08 Python TUI, V09 voice)
- Phase 1 (CLI Args): ✅ ALL DONE (40 commands wired)
- Phase 2 (Provider Parity): claims HEAVILY STALE per DA v16
- Phase 3 (Tool Features): 47 remaining
- Phase 4 (Missing Tools): 46 — 10 new found in v16 audit
- Phase 5 (Gateway): 51
- Phase 6 (Agent Modules): 74 — 2 unwired stubs found (llm_background_review, api_server mock)
