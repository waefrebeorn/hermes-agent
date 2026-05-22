# State — Hermes C Translation (2026-05-22)

## ~50% toward 1:1 Python parity (~400 gaps)

### Milestone Dashboard

| Category | Gaps | Done% | Key Stats |
|----------|------|-------|-----------|
| **Config** | 6 depth | 95% | 322/322 YAML keys parsed, validated, env-overridden |
| **Providers** | 40 | 85% | 9 ops + 31 aliases + **18/18 LLM params** fully wired through all layers |
| **MCP** | 17 | 50% | stdio/SSE transport, tools/list/call, resources, prompts |
| **Plugins** | 51 | 8% | 3 .so stubs vs 45 Python backends (biggest structural gap) |
| **Gateway** | 63 | 35% | 19 platforms basic send/poll. Telegram 479 vs 5465 Python lines |
| **Tools** | 38 | 85% | 28 registered. Browser(13) / Memory(1) / Kanban(9) = 1:1 with Python. SQLite memory now real. 6 stubs + 36 sub-features |
| **Agent** | 32 | 55% | 23 state fields, 18 session DB functions, checkpoint/budget/compression |
| | | | **G01-G12, G15-G17, G19 filled (2026-05-22):** token tracking, toolsets, system_message override, routing IDs, deep token tracking, cost, steer, interrupt |
| **CLI** | 34 | 80% | 70 slash commands, skin/theme engine, display system. **+2: H08 batch mode, H09 pipe input** |
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
- **P0 #2:** B04-B05 response_format + metadata ✅ — 2 new JSON fields, full wiring
- **P0 #3:** F01-F04 CDP tools already real ✅
- **P0 #3:** F06 SQLite memory storage ✅ — real 16-function vtable, sqlite3 amalgamation embedded
- **P0 #3:** F08 vision description ✅ — real Python Hermes CLI delegation (vision_delegate.py)
- **P0 #5:** B06-B09 tool_choice + parallel_tool_calls + max_tool_calls + n ✅ — 4 LLM params
- **E01-E05:** Telegram sendPhoto/sendDocument/sendVoice/sendVideo/sendAnimation ✅
- **E14:** Telegram forwardMessage ✅
- **Bug fix:** Streaming path in llm_client.c was missing config forwarding entirely — fixed ✅
- **F41:** Image format validation (extension check + 50MB limit)
- **B01:** ACP server Sessions 2-4 ✅ — full session CRUD, tool dispatch, user_message + streaming, auth
- **B02-B03:** Covered by B01 ACP server infrastructure ✅
- **Commits:** e57746cdd (F08+Telegram), f9deca6cb (F41), eb83701fb (E15 pin), 6e091d8c-e149c172e-aa6dd5cf8-b6195e0f0 (B01 S1-S4)
