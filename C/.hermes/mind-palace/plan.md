# Slermes C — Plan (May 21 DA-v3)

## Current State
- Overall: **~8% complete**
- Config keys: 16/424+ (3.8%) ← #1 priority
- Tools: 53/64+ (75% name count, 30 stubs)
- CLI: 72 names, 45% impl depth
- MCP: 0% ← #2 priority
- Providers: 3/29+
- Plugins: 0/17 types
- Tests: 43/~17,000

## Active Phase
Start **P1-P25: Config System**. Expand hermes_config_t from 16 → 424+ keys.
Group sub-phases: provider config → display → agent → tools → delegation → browser → memory → compression → cron → notification → security → session → plugin → MCP.

## Next Phases In Queue
1. P1-P25: Config keys (408 missing)
2. P26-P40: CLI commands (full implementations)
3. P41-P55: Tool infrastructure (15 missing + deepen stubs)
4. P56-P70: MCP system (5,620 LOC)
5. P71-P85: Provider system (26+ provider ports)
6. P86-P100: Agent loop (budget, fallback, credential pool)
7. P101-P115: Gateway platform depth
8. P116-P125: Delegation system (concurrent, orchestrator)
9. P126-P140: Plugin system (17 plugin types)
10. P141-P150: Session DB (SQLite FTS5)
11. P151-P158: Memory system (provider-backed)
12. P159-P168: Security (redaction, blocklist, allowlist)
13. P169-P178: Cron/Scheduler (SQLite-backed)
14. P179-P188: Skills system (hub/sync/provenance)
15. P189-P200: TUI (React/Ink parity)

## Milestones
- **M1 (P1-P25 done):** Config parity — 424+ keys, validation, env override, profiles
- **M2 (P1-P55 done):** Tool parity — all 64 static tools, all 72 CLI commands real
- **M3 (P1-P70 done):** MCP parity — dynamic tools, transport, auth
- **M4 (P1-P85 done):** Provider parity — 29+ providers, credential pool, budget tracking
- **M5 (P1-P100 done):** Agent loop parity — budget, fallback, checkpoints, streaming
- **M6 (P1-P200 done):** Full 1:1 parity with Python Hermes

Estimated: Q3/Q4 2026 at current velocity (~3 phases/day).
