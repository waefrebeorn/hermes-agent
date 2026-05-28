# Slermes C — Triple DA Battle Map v19 (2026-06-02)

**Fresh heavy audit. Live binary integration test, 18-pattern stub hunt, module comparison (AST-level Python tools vs C registry), function-level depth audit, form-not-function gap sweep.**

- Suite: 282/0/0
- C registered tools: 99
- Binary: slermes (30M)
- CLI commands: 98
- Gateways: 19
- Providers: 10
- Libraries: 65
- Src .c files: 173
- Test files: 239
- Headers: 137

---

## SECTOR 0: Form-Not-Function (Integration Gaps — Entry Points Broken) — 4 gaps

| # | ID | Issue | Evidence | LOC | Priority |
|---|-----|-------|----------|-----|----------|
| 1 | F01 | `./slermes --bogus` sends to LLM instead of erroring | main.c: arg dispatch fallthrough — unknown flags consumed as LLM prompt | 10-20 | P0 |
| 2 | F02 | Multi-line pipe mode broken — all stdin sent as one blob | cli.c: stdin loop reads all input, dispatches as single "/help\n/tools\n/exit" | 30-50 | P0 |
| 3 | F03 | `--session` without value runs `session_crud` tool instead of erroring | main.c: missing arg not detected, next argv consumed as tool | 10-20 | P0 |
| 4 | F04 | Tools display count shows 83 (stale) vs 99 actually registered | commands.c:1695 — `state->tools.count` shows 83 at startup | 10-20 | P1 |

## SECTOR 1: Missing Tools (Python-registered, no C equivalent) — 7 gaps

| # | ID | Missing Tool | Python Source | Complexity | Priority |
|---|----|-------------|---------------|------------|----------|
| 1 | M01 | `discord` | discord_tool.py | Medium (HTTP API wrapper) | P2 |
| 2 | M02 | `discord_admin` | discord_tool.py | Medium (HTTP API wrapper) | P2 |
| 3 | M03 | `yb_query_group_info` | yuanbao_tools.py | SDK-dependent | P2 |
| 4 | M04 | `yb_query_group_members` | yuanbao_tools.py | SDK-dependent | P2 |
| 5 | M05 | `yb_search_sticker` | yuanbao_tools.py | SDK-dependent | P2 |
| 6 | M06 | `yb_send_dm` | yuanbao_tools.py | SDK-dependent | P2 |
| 7 | M07 | `yb_send_sticker` | yuanbao_tools.py | SDK-dependent | P2 |

## SECTOR 2: Tool Depth Gaps (C tool exists, Python has more features) — 13 gaps

| # | ID | Tool | C Fns | Py Fns | Gap | Key Missing Features | LOC | Priority |
|---|-----|------|-------|--------|-----|---------------------|-----|----------|
| 1 | D01 | mcp_tool | 20 | 80 | -60 | Pagination, SSE persistent streaming | 150-250 | P2 |
| 2 | D02 | tts | 8 | 60 | -52 | Multi-provider (Elevenlabs, Azure, edge-tts), SSML, streaming | 200-400 | P2 |
| 3 | D03 | browser | 27 | 76 | -49 | CDP autofill, PDF gen, HAR capture, multi-window | 400-800 | P2 |
| 4 | D04 | terminal | 7 | 49 | -42 | SSH, Docker Compose, Singularity, pty modes | 200-400 | P2 |
| 5 | D05 | transcribe | 2 | 44 | -42 | Multi-provider (Whisper, Groq, OpenAI, Deepgram), diarization | 200-400 | P2 |
| 6 | D06 | send_message | 2 | 31 | -29 | Thread replies, inline buttons, reactions, embed cards | 200-400 | P2 |
| 7 | D07 | delegate | 16 | 42 | -26 | Parallel spawning, lifecycle, checkpointing | 200-400 | P2 |
| 8 | D08 | vision | 5 | 19 | -14 | Image format edge cases, provider dispatch | 100-200 | P2 |
| 9 | D09 | image_gen | 3 | 15 | -12 | Provider dispatch tests, error handling | 100-200 | P2 |
| 10 | D10 | web | 11 | 21 | -10 | Cookie jar, auth flows, redirect handling | 100-200 | P2 |
| 11 | D11 | video_gen | 3 | 12 | -9 | Edge cases, format validation | 50-100 | P3 |
| 12 | D12 | file | 27 | 29 | -2 | Near parity, minor edge cases | 30-60 | P3 |
| 13 | D13 | discord | 0 | 34 | -34 | No C implementation at all | 200-400 | P2 |

## SECTOR 3: Library Depth — 1 gap

| # | ID | Library | Issue | LOC | Priority |
|---|---------|---------|-------|-----|----------|
| 1 | L02 | libmcp | SSE persistent streaming (persistent GET + event parsing for older MCP transport) | 150-250 | P2 |

## SECTOR 4: Gateway Platform — 1 gap

| # | ID | Platform | Issue | LOC | Priority |
|---|----------|---------|-------|-----|----------|
| 1 | G02 | send_reaction | vtable slot exists, no platform wires it (Signal has standalone fn with different signature) | 15-30 per platform | P2 |

## SECTOR 5: Docker/CI/Infrastructure — 2 gaps

| # | ID | Issue | LOC | Priority |
|---|-----|-------|-----|----------|
| 1 | I01 | No Dockerfile for C binary | 20-40 | P3 |
| 2 | I02 | GitHub Actions CI not vetted for C-only mode | 30-60 | P3 |

## SECTOR 6: Test Coverage — 5 gaps

| # | ID | Module | Tests | Issue | Priority |
|---|--------|--------|-------|-------|----------|
| 1 | X01 | vision.c | 21 | Image format edge cases (BMP, TIFF, SVG) | P3 |
| 2 | X02 | image_gen.c | 9 | Provider dispatch tests | P3 |
| 3 | X03 | video_gen.c | 6 | Edge case tests | P3 |
| 4 | X04 | transcribe.c | 9 | Provider-specific tests (needs network) | P3 |
| 5 | X05 | session_search.c | 13 | Edge cases for query parsing | P3 |

---

**Summary:**

| Sector | Count | Priority Split |
|--------|-------|----------------|
| S0: Form-Not-Function | 4 | P0: 3, P1: 1 |
| S1: Missing Tools | 7 | P2: 7 |
| S2: Tool Depth | 13 | P2: 11, P3: 2 |
| S3: Library Depth | 1 | P2: 1 |
| S4: Gateway | 1 | P2: 1 |
| S5: CI/Infra | 2 | P3: 2 |
| S6: Test Coverage | 5 | P3: 5 |
| **TOTAL** | **33** | P0: 3, P1: 1, P2: 20, P3: 9 |

**P0 gaps (fix before anything else):**
- F01: `--bogus` sends to LLM — unknown flags must error immediately
- F02: Multi-line pipe broken — stdin must be split by newline
- F03: `--session` without arg runs session_crud — missing arg must error
