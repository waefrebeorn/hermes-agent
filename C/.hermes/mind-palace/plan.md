# Slermes C — Plan (May 21 HONEST)

## Purpose
Phase roadmap for 1:1 C translation of Python Hermes Agent.
Current: ~8-12% complete (57K C LOC vs 433K Python LOC).
GOAL: 100% user-swappable parity.

## Honest Status

| Area | Previous | Reality |
|------|----------|---------|
| **Config** | ⚠️ "critical keys OK" | ❌ 16/424 (3.8%) |
| **CLI** | ✅ "69/69 parity" | ⚠️ count parity, many stubs |
| **Tools** | ✅ "54 reg" | ⚠️ 24 real, 37 missing entirely |
| **MCP** | (not mentioned) | ❌ 0% — 5,620 LOC missing |
| **Plugins** | ✅ "wired" | ❌ skeletal (1%) |
| **TUI** | ✅ "wired" | ❌ 3% of Python |
| **Providers** | ✅ "3/3" | ⚠️ 3 vs 29+ |
| **Delegation** | (not mentioned) | ❌ 5% — broken for daily use |
| **Tests** | ✅ "43/43" | ⚠️ 43 vs ~17,000 |

## Priority

P0: Config (408 keys), MCP (0%), terminal backends, delegation
P1: Missing tools (37), providers (26+), session DB, security
P2: Voice/TTS, image gen, browser CDP, skills expansion, TUI
P3: Performance, fancy UI, video, achievements

## Full Roadmap
See `.hermes/mind-palace/plans/100-phase-roadmap.md` (100 phases)
DA audit: `.hermes/mind-palace/plans/devils-advocate-v2.md`
