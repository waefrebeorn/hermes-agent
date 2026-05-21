# Slermes C — Prestige Prompt (May 21 HONEST)

## Mission
1:1 C translation of Python Hermes Agent. Current: ~8-12% complete.
Previous parity claims were count-only, not depth.

## Architecture Stack
```
CLI (69 names, many stubs) → Agent Loop (no budget/fallback/credentials) → LLM API
                                    ↕
                           Tool Registry (24 real tools, 37 missing)
                                    ↕
                          Gateway Server (19/31 platforms)
                                    ↕
                           Session DB (file-based JSON, no FTS5)
```

## Honest State

| Module | Previous | Reality |
|--------|----------|---------|
| CLI | ✅ 69/69 | ⚠️ names match, most handlers are printf stubs |
| Tools | ✅ 54 reg | ❌ 24 real tools, 37 missing, all simplified |
| Gateway | ✅ 20/20 | ✅ 19/31 core platforms |
| Providers | ✅ 3/3 | ❌ 3 hardcoded vs 29+ plugins |
| Security | ✅ 4/4 | ⚠️ bare — no redaction/blocklist/allowlist |
| TUI | ✅ wired | ❌ bare ncurses (3% of 41K Python TUI) |
| Plugin | ✅ wired | ❌ skeletal .so loading (1% of 39K) |
| Streaming | ✅ true | ⚠️ basic token callback, no SSE/tool-stream |
| Config | ⚠️ subset | ❌ 16/424 keys (3.8%) |
| MCP | (not mentioned) | ❌ 0% — 5,620 LOC missing entirely |
| Session DB | ⚠️ grep | ❌ no FTS5, no SQL, no metadata |
| Delegation | (not mentioned) | ❌ 5% — basic subprocess only |
| Tests | ✅ 43/43 | ⚠️ 43 vs ~17,000 Python tests |

## What Blocks Daily Use
- **Config**: 408/424 keys missing
- **MCP**: 0% — no dynamic tools
- **Delegation**: 5% — subagents broken
- **Agent loop**: no budget/fallback/credential pool
- **Terminal**: local only, no Docker/SSH
- **Browser**: text-only, no JS/CDP
- **Security**: no redaction/blocklist/allowlist

## Priority Stream
| Stream | Priority | Status |
|--------|----------|--------|
| Config keys (408 missing) | P0 | ❌ Not started |
| MCP infrastructure | P0 | ❌ 0% |
| Terminal backends | P0 | ❌ local only |
| Delegation system | P0 | ❌ 5% |
| Missing tools (37) | P1 | ❌ Not started |
| Providers (26+) | P1 | ❌ 3/29 |
| Session DB + FTS5 | P1 | ❌ grep only |
| Security expansion | P1 | ⚠️ 20% |
| Voice/TTS full | P2 | ⚠️ 8% |
| Browser CDP | P2 | ⚠️ 20% |
| TUI full | P3 | ❌ 3% |

## Roadmap
100-phase roadmap: `.hermes/mind-palace/plans/100-phase-roadmap.md`
DA audit v2: `.hermes/mind-palace/plans/devils-advocate-v2.md`
