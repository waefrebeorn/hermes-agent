# Slermes C — Overnight Map (May 21 HONEST)

## Quick Trunk

| Path | What |
|------|------|
| ~/hermes-agent-dev/C/ | C source (build: make -j$(nproc)) |
| ~/hermes-agent-dev/C/.hermes/mind-palace/plans/ | 100-phase roadmap + DA v2 |
| ~/hermes-agent-dev/C/parity_check.py | **count-only parity (misleading)** |

## Where We Are

**C is ~8-12% of Python.** Not ready for daily use.
Previous claims of "P0 complete" were based on count-only parity, not implementation depth.

### VERIFIED WORKING
- ✅ 69 CLI command names registered (many stubs)
- ✅ 24 tools with real handlers (many simplified)
- ✅ 19/31 gateway platform adapters
- ✅ 3 providers (OpenAI/Anthropic/Google)
- ✅ HTTP retry (3 attempts linear backoff)
- ✅ Config keys: personality/verbose/yolo/fast
- ✅ Persistent allowlist (~/.slermes/allowlist.json)
- ✅ CDP WebSocket client (needs external server)
- ✅ Smart compression via LLM (disabled by default)
- ✅ Session search ranked by match count

### NOT WORKING / MISSING
- ❌ 408/424 config keys (96.2%)
- ❌ MCP: 0% — 5,620 LOC missing
- ❌ 37 tools missing entirely
- ❌ 26+ providers missing
- ❌ Delegation (5% — basic subprocess only)
- ❌ Terminal: local only (no Docker/SSH/Modal)
- ❌ Browser: text-only (no CDP headed browser)
- ❌ Plugin system: skeletal (1%)
- ❌ TUI: bare ncurses (3%)
- ❌ Voice/TTS: 1 provider (8%)
- ❌ Session DB: no FTS5 (12%)
- ❌ Security: no redaction/blocklist (20%)

## Next Workstream

Phase 1: Config keys (408 missing) — parse ALL from config.yaml
Phase 2: Environment variables for all keys
Phase 3: Config validation + defaults

## Fallback

If blocked, read plans/100-phase-roadmap.md
DA audit: plans/devils-advocate-v2.md
