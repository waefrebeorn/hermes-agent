# Slermes C — Devil's Advocate Audit v2 (May 21)

## Verdict

**C slermes is approximately 8-12% complete relative to Python hermes.**
Not ready for daily use. User correctly says "can't use the app yet."

## Overall Gap Table

| # | Subsystem | Python LOC | C LOC | Completeness | Verdict |
|---|-----------|-----------|-------|-------------|---------|
| 1 | CLI Commands | 1,756 | 1,301 | 45% | Count parity but many stubs |
| 2 | Tools | 67,263 | 16,571 | 25% | 37 tools missing, all C tools simplified |
| 3 | Config Keys | 5,555 | 240 | **3.8%** | **408/424 missing** |
| 4 | Gateway Platforms | 57,487 | ~8,000 | 35% | 12 platform files missing |
| 5 | Provider Support | ~62,063 | 2,871 | **10%** | 26+ providers missing |
| 6 | Agent Loop | 4,149+ | 332 | 15% | No budget/fallback/credential pool |
| 7 | Security | 2,590 | 789 | 20% | No redact/blocklist/allowlist |
| 8 | TUI | 41,187 | 926 | **3%** | Bare ncurses vs React/Ink |
| 9 | Plugin System | 39,406 | 200 | **1%** | 17/18 plugin types missing |
| 10 | Cron/Scheduler | 3,214 | 223 | 10% | Bare loop, no SQLite persistence |
| 11 | Session DB | 3,273 | ~300 | 12% | No FTS5, no SQL |
| 12 | Delegation | 2,801 | 135 | **5%** | Basic subprocess spawn |
| 13 | Memory System | ~2,000+ | 117 | 8% | No provider plugins |
| 14 | Skills System | ~4,000+ | 261 | 8% | No hub/sync/provenance |
| 15 | Streaming | built-in | basic | 30% | Basic token callback only |
| 16 | **MCP** | **5,620** | **0** | **0%** | **100% missing** |
| 17 | Web Search | ~2,500+ | 293 | 15% | No provider abstraction |
| 18 | Vision | 1,421 | 117 | 10% | No video/providers/routing |
| 19 | Image Gen | 1,098+ | 176 | 15% | Single provider |
| 20 | Voice/TTS | 4,323 | 329 | 8% | 1 TTS provider vs 7 |
| 21 | Browser | ~5,500+ | 1,495 | 20% | Text-only, no CDP headed browser |
| 22 | Approval System | 1,393 | 355 | 30% | No timeout/destructive confirm |

## Gaps That Block Daily Use

### P0 Blockers (will cause failures in normal use)

1. **MCP: 0%** — No dynamic tools from MCP servers. Agent can't use MCP tools.
2. **Config: 3.8%** — 408/424 keys missing. Can't configure terminal backends, delegation, memory, compression, display, security, etc.
3. **Delegation: 5%** — No concurrent children, no orchestrator mode, no timeout. Subagent workflows broken.
4. **Agent loop: 15%** — No budget tracking (infinite loop risk), no fallback model (provider failure kills conversation), no credential pool (single API key, no rotation).
5. **Terminal: local only** — No Docker/SSH/Modal backends. Can't run in sandboxed environments.
6. **Browser: text-only** — No CDP headed browser. Can't run JavaScript, no screenshots, no real browser interaction.
7. **Web search: single backend** — Hardcoded to X search. No Tavily/Brave/Google backends.
8. **Security: 20%** — No secrets redaction (API keys in output), no website blocklist, no command allowlist.

### P1 Blockers (missing features that will surprise users)

9. **Session DB: 12%** — No FTS5 search, no session metadata, no auto-prune. Grep-based search is slow.
10. **Plugin system: 1%** — Bare .so loading only. No plugin discovery, no lifecycle hooks.
11. **Provider support: 10%** — Only 3 hardcoded providers. No custom provider support.
12. **TTS/Voice: 8%** — Only basic TTS, no STT, no transcription, no provider selection.
13. **Vision: 10%** — Basic image analysis only. No video, no URL input.
14. **Approvals: 30%** — No timeout (prompt hangs forever), no cron mode.

## Critical Items vs Current Claims

| Claim in State Files | Actual | Gap |
|---------------------|--------|-----|
| "✅ CLI: 69/69 parity" | 69 command names exist, but most are `printf` stubs | Major — names match, implementations don't |
| "✅ Tools: 54 registered" | 24 tools have real impl, 30 are stubs/CDP stubs | Major — count was inflated |
| "✅ Security: 4/4 features" | URL safety + path traversal + Tirith + approval exist but are bare | Medium — no redaction/blocklist/allowlist |
| "✅ 1:1 parity verified" | Count parity only. Implementation depth is ~8-12% | Critical — claim is misleading |
| "✅ Config: critical keys OK" | 16/424 keys. Not "critical keys" at all | Critical — config is the biggest gap |

## What Needs To Happen

1. **Stop calling it "P0 complete."** It's P0 foundation only.
2. **Start Phases 1-3 immediately:** Config keys (408 missing), env vars, validation.
3. **Start Phase 4:** CLI command implementations (most are stubs).
4. **Start Phase 5:** Terminal backend abstraction (needed for any non-trivial use).

The 100-phase roadmap at `.hermes/mind-palace/plans/100-phase-roadmap.md` contains the full plan.
