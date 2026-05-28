# Slermes C — Triple DA Battle Map v17 (2026-05-27)

**Fresh audit based on live binary testing, stub hunt (18 patterns), module comparison, function-level depth audit, and library verification.**

- Suite: 282/0/0
- C registered tools: 99
- CLI commands: 98
- Gateways: 19
- Providers: 10
- Libraries: 65
- Src .c files: 173
- Test files: 239
- Binary: 30M

---

## SECTOR 1: Confirmed Stubs (verified, source-confirmed) — 8 gaps

| # | ID | File:Line | Issue | LOC | Priority |
|---|----|-----------|-------|-----|----------|
| 1 | S01 | llm_client.c:1522 | `perform_background_review` — NOT YET WIRED, no caller in C agent_loop | 30-50 | P2 |
| 3 | S03 | commands.c:2743 | `/restart` says "Use /exit and re-launch" instead of actual restart | 20-30 | P1 |
| 4 | S04 | commands.c:2595 | Plugin hot-reload: "hot-reload not yet supported" | 30-50 | P2 |
| 5 | S05 | context_engine.c:91,100 | `on_session_start/end` are noop default handlers | 15-25 | P2 |
| 6 | S06 | telegram.c:949 | `/* E07: Send editable draft with placeholder text (no keyboard) */` — missing editable draft feature | 40-60 | P2 |
| 7 | S07 | acp/resource.c:263 | Non-file URI / unreadable path returns placeholder | 15-25 | P3 |
| 8 | S08 | homeassistant.c:524 | `'data' parameter doc says "Deprecated: use 'data' instead"` — confusing self-referential deprecation | 5-10 | P3 |
| 9 | S09 | memory.c:544,549 | In-memory mode no-ops for save/load (intentional but no file persist) | 20-30 | P3 |

## SECTOR 2: Missing Python-Registered Tools — 4 gaps

| # | ID | Missing Tool | Python Source | LOC | Priority |
|---|----|-------------|---------------|-----|----------|
| 1 | P01 | `yb_query_group_info` | yuanbao_tools.py | 40-80 | P2 |
| 2 | P02 | `yb_query_group_members` | yuanbao_tools.py | 40-80 | P2 |
| 3 | P03 | `yb_search_sticker` | yuanbao_tools.py | 40-80 | P2 |
| 4 | P04 | `yb_send_sticker` | yuanbao_tools.py | 40-80 | P2 |

## SECTOR 3: Tool Depth Gaps (C has tool, Python has richer features) — 7 gaps

Biggest function-count gaps per the depth audit (C fns vs Python fns):

| # | ID | Tool | C Fns | Py Fns | Gap | Key Missing Features | LOC | Priority |
|---|-----|------|-------|--------|-----|---------------------|-----|----------|
| 1 | D01 | mcp_tool | 19 | 109 | -90 | SSE transport, subscriptions, roots, sampling, prompts, pagination, streaming, logging, resource templates | 400-800 | P1 |
| 2 | D02 | tts_tool | 7 | 62 | -55 | Multi-provider (OpenAI, Elevenlabs, Azure, edge-tts), voice listing, SSML, streaming, word timestamps, emotion | 300-600 | P2 |
| 3 | D03 | browser | 28 | 127 | -99 | CDP autofill, PDF gen, HAR capture, multi-window, frames, network interception, dialog state persistence | 500-1000 | P1 |
| 4 | D04 | terminal_tool | 7 | 49 | -42 | SSH config, Docker Compose, Singularity, Daytona, env passthrough, pty modes, cgroup limits | 200-400 | P2 |
| 5 | D05 | transcribe | 2 | 44 | -42 | Multi-provider (Whisper local, Groq, OpenAI, Deepgram), language detection, speaker diarization, streaming | 200-400 | P2 |
| 6 | D06 | send_message | 2 | 34 | -32 | Thread replies, inline buttons, attachments, reactions, embed cards, scheduled send, delivery receipts, editing | 200-400 | P2 |
| 7 | D07 | delegate_task | 15 | 45 | -30 | Parallel subagent spawning, lifecycle (pause/resume/cancel), result aggregation, timeout per subagent, checkpointing | 200-400 | P2 |

## SECTOR 4: Gateway Platform Depth — 3 gaps

| # | ID | Platform | Issue | LOC | Priority |
|---|----------|----------|-------|-----|----------|
| 1 | G01 | telegram.c:949 | Editable draft with placeholder text — E07 gap, no keyboard support | 40-60 | P2 |
| 2 | G02 | Multiple platforms | send_reaction vtable slot exists but only Signal + BlueBubbles have real impls | 20-40 per platform | P2 |
| 3 | G03 | email.c | IMAP idle/poll needs more robust reconnection handling | 30-50 | P2 |

## SECTOR 5: Docker/CI/Infrastructure — 2 gaps

| # | ID | Issue | LOC | Priority |
|---|-----|-------|-----|----------|
| 1 | I01 | No Dockerfile for C binary (Python has separate Docker image) | 20-40 | P3 |
| 2 | I02 | GitHub Actions CI workflows not vetted for C-only mode (currently runs both Python + C) | 30-60 | P3 |

## SECTOR 6: Test Coverage Gaps — 5 gaps

| # | ID | Module | Tests | LOC | Priority |
|---|--------|--------|-------|-----|----------|
| 1 | X01 | vision.c | 21 tests | Add image format edge cases (BMP, TIFF, SVG) | P3 |
| 2 | X02 | image_gen.c | 9 tests | Add provider dispatch tests | P3 |
| 3 | X03 | video_gen.c | 6 tests | Add edge case tests | P3 |
| 4 | X04 | transcribe.c | 9 tests | Provider-specific tests would need network | P3 |
| 5 | X05 | session_search.c | 13 tests | Add edge cases for query parsing | P3 |

## SECTOR 7: Library Depth — 3 gaps

| # | ID | Library | Issue | LOC | Priority |
|---|---------|---------|-------|-----|----------|
| 1 | L01 | libjson | JSON schema validation (Python has full jsonschema lib) | 100-200 | P2 |
| 2 | L02 | libmcp | SSE transport, streaming responses | 200-400 | P1 |
| 3 | L03 | libcron | @every/@daily shorthand (only @daily exists) | 20-40 | P2 |

## SECTOR 8: Refactoring & Cleanup — 2 gaps

| # | ID | Issue | LOC | Priority |
|---|-------|-------|-----|----------|
| 1 | R01 | test_web.c fixes dangling pointer + DNS crash on WSL — test 5 disabled | 10-20 | P2 |
| 2 | R02 | google_search URL builder has FORTIFY_SOURCE truncation warning | 10-20 | P3 |

---

**Summary:**

| Sector | Count | Priority Split |
|--------|-------|----------------|
| S1: Confirmed Stubs | 8 | P1: 1, P2: 5, P3: 2 |
| S2: Missing Tools | 4 | P2: 4 |
| S3: Tool Depth | 7 | P1: 2, P2: 5 |
| S4: Gateway Depth | 3 | P2: 3 |
| S5: CI/Infra | 2 | P3: 2 |
| S6: Test Coverage | 5 | P3: 5 |
| S7: Library Depth | 3 | P1: 1, P2: 2 |
| S8: Refactoring | 2 | P2: 1, P3: 1 |
| **TOTAL** | **34** | P1: 4, P2: 20, P3: 10 |

**P1 gaps:** S03 (/restart stub), D01 (MCP depth), D03 (browser depth), L02 (MCP SSE)
