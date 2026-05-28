# Slermes C — Triple DA Battle Map v18 (2026-06-02)

**Stale claim sweep from v17 → v18. Verified each gap against source code. 8 stale claims retired to vault.**

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

## SECTOR 1: Missing Tools (Python-registered, not in C) — 4 gaps

| # | ID | Missing Tool | Python Source | Complexity | Priority |
|---|----|-------------|---------------|------------|----------|
| 1 | P01 | `yb_query_group_info` | yuanbao_tools.py | SDK-dependent (needs WebSocket protocol + adapter) | P2 |
| 2 | P02 | `yb_query_group_members` | yuanbao_tools.py | SDK-dependent | P2 |
| 3 | P03 | `yb_search_sticker` | yuanbao_tools.py | SDK-dependent (sticker DB) | P2 |
| 4 | P04 | `yb_send_sticker` | yuanbao_tools.py | SDK-dependent (WebSocket send) | P2 |

## SECTOR 2: Tool Depth Gaps (C has tool, Python has more features) — 7 gaps

Corrected C function counts per verified grep. Python counts from audit.

| # | ID | Tool | C Fns | Py Fns | Gap | Key Missing Features | LOC | Priority |
|---|-----|------|-------|--------|-----|---------------------|-----|----------|
| 1 | D01 | mcp_tool | 22 | 109 | -87 | SSE persistent streaming (L02), pagination | 150-250 | P2 |
| 2 | D02 | tts_tool | 9 | 62 | -53 | Multi-provider (Elevenlabs, Azure, edge-tts), SSML, streaming | 200-400 | P2 |
| 3 | D03 | browser | 34 | 127 | -93 | CDP autofill, PDF gen, HAR capture, multi-window, network interception | 400-800 | P2 |
| 4 | D04 | terminal_tool | 8 | 49 | -41 | SSH config, Docker Compose, Singularity, env passthrough, pty modes | 200-400 | P2 |
| 5 | D05 | transcribe | 3 | 44 | -41 | Multi-provider (Whisper, Groq, OpenAI, Deepgram), diarization | 200-400 | P2 |
| 6 | D06 | send_message | 3 | 34 | -31 | Thread replies, inline buttons, attachments, reactions, embed cards | 200-400 | P2 |
| 7 | D07 | delegate_task | 17 | 45 | -28 | Parallel subagent spawning, lifecycle, result aggregation, checkpointing | 200-400 | P2 |

## SECTOR 3: Gateway Platform Depth — 2 gaps

| # | ID | Platform | Issue | LOC | Priority |
|---|----------|----------|-------|-----|----------|
| 1 | G02 | Multiple platforms | send_reaction vtable slot exists but no platform wires it (Signal has standalone fn, different signature) | 15-30 per platform | P2 |
| 2 | G03 | email.c | IMAP idle/poll needs more robust reconnection handling | 30-50 | P2 |

## SECTOR 4: Docker/CI/Infrastructure — 2 gaps

| # | ID | Issue | LOC | Priority |
|---|-----|-------|-----|----------|
| 1 | I01 | No Dockerfile for C binary (Python has separate Docker image) | 20-40 | P3 |
| 2 | I02 | GitHub Actions CI workflows not vetted for C-only mode | 30-60 | P3 |

## SECTOR 5: Test Coverage Gaps — 5 gaps

| # | ID | Module | Tests | LOC | Priority |
|---|--------|--------|-------|-----|----------|
| 1 | X01 | vision.c | 21 tests | Add image format edge cases (BMP, TIFF, SVG) | P3 |
| 2 | X02 | image_gen.c | 9 tests | Add provider dispatch tests | P3 |
| 3 | X03 | video_gen.c | 6 tests | Add edge case tests | P3 |
| 4 | X04 | transcribe.c | 9 tests | Provider-specific tests (needs network) | P3 |
| 5 | X05 | session_search.c | 13 tests | Add edge cases for query parsing | P3 |

## SECTOR 6: Library Depth — 2 gaps

| # | ID | Library | Issue | LOC | Priority |
|---|---------|---------|-------|-----|----------|
| 1 | L01 | libjson | JSON schema validation (Python has full jsonschema lib) | 100-200 | P2 |
| 2 | L02 | libmcp | SSE persistent streaming (persistent GET + event parsing for older MCP transport) | 150-250 | P2 |

## SECTOR 7: Refactoring & Cleanup — 1 gap

| # | ID | Issue | LOC | Priority |
|---|-------|-------|-----|----------|
| 1 | R01 | test_web.c DNS crash on WSL — test 5 disabled | 10-20 | P2 |

---

**Summary:**

| Sector | Count | Priority Split |
|--------|-------|----------------|
| S1: Missing Tools | 4 | P2: 4 |
| S2: Tool Depth | 7 | P2: 7 |
| S3: Gateway Depth | 2 | P2: 2 |
| S4: CI/Infra | 2 | P3: 2 |
| S5: Test Coverage | 5 | P3: 5 |
| S6: Library Depth | 2 | P2: 2 |
| S7: Cleanup | 1 | P2: 1 |
| **TOTAL** | **23** | P1: 0, P2: 16, P3: 7 |

**No P1 gaps remain.** All P1 depth gaps from v17 were partially stale (D01) or downgraded (D03) — actual code is further along than the audit captured.
