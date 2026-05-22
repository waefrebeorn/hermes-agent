# State — Hermes C Translation (2026-05-22)

## ~50% toward 1:1 Python parity (~400 gaps)

### Milestone Dashboard

| Category | Gaps | Done% | Key Stats |
|----------|------|-------|-----------|
| **Config** | 6 depth | 95% | 322/322 YAML keys parsed, validated, env-overridden |
| **Providers** | 46 | 80% | 9 ops + 31 aliases + 10/12 LLM params wired through config |
| **MCP** | 17 | 50% | stdio/SSE transport, tools/list/call, resources, prompts |
| **Plugins** | 51 | 8% | 3 .so stubs vs 45 Python backends (biggest structural gap) |
| **Gateway** | 63 | 35% | 19 platforms basic send/poll. Telegram 479 vs 5465 Python lines |
| **Tools** | 44 | 80% | 28 registered. Browser(13) / Memory(1) / Kanban(9) = 1:1 with Python |
| **Agent** | 32 | 55% | 23 state fields, 18 session DB functions, checkpoint/budget/compression |
| **CLI** | 34 | 80% | 70 slash commands, skin/theme engine, display system |
| **Libs** | 14 | 20% | libhttp/libcrypto/libcron ported. Jinja2/rich/httpx/pydantic/etc. missing |
| **Stdlib** | 5 | 30% | libproc/libcrypto basics. No event loop, logging, dataclasses |
| **Errors** | 5 | 0% | No typed error hierarchy at all |
| **Upstream** | 12 | new | 125 commits behind origin/main (52 Python) |
| **Tests** | 53 | 40% | 26 files, 1422 assertions. Python: 17K across 900+ files |
| **Cross-cut** | 5 | 40% | Token counting, secure parent dir, key leakage prevention |
| **Build/doc** | 15 | 30% | Cross-compile, Windows, Docker, CI, man pages, API docs |

### Key Corrections from Old DA
- **Browser:** C 13 handlers = 1:1 with Python 12 (C ahead: has browser_forward)
- **Memory:** 1 handler = parity
- **Kanban:** 9 handlers = parity  
- **Plugins:** 45 individual backends, not 19 categories
- **Provider APIs:** 25 per-provider quirks entirely missed before

### Known Bug
```c
// Fix applied: temperature=0.0 now correctly sent to API (>= 0.0f)
```

### Session 2026-05-22 Progress
- **P0 #1:** temperature=0.0 fix ✅ — s/>0.0f/>=0.0f/ × 9 providers
- **P0 #2:** B04-B05 response_format + metadata ✅ — new fields wired through all 3 forwarding layers + config pipeline + all 9 providers
- **P0 #3 partial:** F08 vision description — TODO replaced with Python delegation helper
- **Bug fix:** Streaming path in llm_client.c was missing config forwarding entirely (all params dropped) — now fixed with full forwarding block
