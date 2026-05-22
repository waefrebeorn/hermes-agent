# WuBu Hermes C — Prestige Prompt (v6)

## Identity
1:1 reimplementation of Python hermes-agent in C. Python maintainers try C — can't tell the difference. Every feature, every handler, every API quirk has a C equivalent.

## Core Principles
- Count capability surface, not function count
- Internal Python helpers ≠ features
- Cleaner C approach that handles same behavior = parity
- Triple-check own counts. Delegation is lazy. ~WuBu~ strives for more.

---

## Priority Queue

### P0 — Bugs + Immediate (0.5-4 sessions)
| # | Gap | Est. |
|---|-----|------|
| 1 | Fix temperature=0.0 bug (s/>0.0f/>=0.0f/ × 9 providers) | 10 min |
| 2 | B04-B05: response_format + metadata LLM params | 1 session |
| 3 | F01-F08: 8 tool stubs → real (CDP×4, computer_use, memory×2, vision) | 3 sessions |
| 4 | B01-B03: 3 ACP providers (Copilot, OpenCode, Codex) | 4 sessions |

### P1 — Feature Depth (2-8 sessions)
| # | Area | Gaps |
|---|------|------|
| 5 | E01-E34: Gateway depth (Telegram sends, msg types, observe groups, infra) | 34 |
| 6 | G01-G36: Agent state + session tracking | 32 |
| 7 | H01-H34: CLI features + command handler depth | 34 |
| 8 | F09-F44: Tool sub-features (terminal, web, delegate, cron, process, tts, etc.) | 36 |

### P2 — Platform Depth (3-15 sessions)
| # | Area | Gaps |
|---|------|------|
| 9 | D01-D43: Plugin depth (stubs→real + 42 missing backends) | 51 |
| 10 | C01-C17: MCP depth (sub/sampling/roots/serve/multi/auth) | 17 |
| 11 | B22-B46: Provider-specific API features (25 per-provider quirks) | 25 |
| 12 | B10-B21: Provider infrastructure (pool rotation, catalog, metadata) | 12 |

### P3 — Upstream + Quality (5-10 sessions)
| # | Area | Gaps |
|---|------|------|
| 13 | L01-L12: Upstream catch-up (125 commits behind origin/main) | 12 |
| 14 | E40-E63: Gateway formatting + platform depth | 24 |
| 15 | M01-M53: Test coverage | 53 |

### P4 — Porting + Infra (2-10 sessions)
| # | Area | Gaps |
|---|------|------|
| 16 | I01-I14: Python lib ports (Jinja2, rich, httpx, etc.) | 14 |
| 17 | J01-J05: Stdlib ports (asyncio, logging, subprocess, pathlib, dataclasses) | 5 |
| 18 | K01-K05: Error handling (typed error hierarchy) | 5 |
| 19 | N01-N05: Cross-cutting (token counting, security) | 5 |
| 20 | O01-O15: Build/doc/security depth | 15 |

---

## Assessment

| Metric | Value |
|--------|-------|
| Total gaps | ~400 |
| Complete | ~50% |
| Largest structural gap | Plugins (8% — 3/45 backends) |
| Most overestimated in old DA | Browser "shallow" — actually 1:1 (C has browser_forward Python doesn't) |
| Most underestimated in old DA | Provider-specific APIs — 25 entirely missed |
| Known bugs | temperature=0.0 dropped by > vs >= guard |

## Repository

```yaml
path: /home/wubu/hermes-agent-dev/C/
remote: wubu=waefrebeorn/hermes-agent (main)
upstream: origin=NousResearch/hermes-agent (1264fab15, 125 commits ahead)
build: make -j$(nproc)
test: bash test_runner.sh (58/59 pass)
```
