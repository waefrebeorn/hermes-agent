# Slermes C — Triple DA Battle Map v20 (2026-06-02)

**Heavy Triple DA refresh. v19 claims cross-checked against live binary + source. 8 stale claims retired → vault Phase 9. 24 verified gaps remain.**

- Suite: 282/0/0
- C registered tools: 99 (91 from registry_register calls + 8 dynamic)
- Binary: slermes (31M)
- CLI commands: 98
- Gateways: 19
- Providers: 10
- Libraries: 66
- Src .c files: 167
- Test files: 239
- Headers: 193 total (71 include/, rest in lib/)
- Stubs: 0 (18-pattern hunt: 0 "for brevity", 0 "for later" (non-stub), 0 "for extension", 0 "scaffolding", 0 "NOT_IMPLEMENTED", 0 "assert(0)", 0 TODO/FIXME in code logic)

---

## SECTOR 0: Missing Python Tool Ports — 5 gaps

| # | ID | Missing Tool | Python Source | Complexity | Priority |
|---|----|-------------|---------------|------------|----------|
| 1 | M03 | `yb_query_group_info` | yuanbao_tools.py | SDK-dependent | P2 |
| 2 | M04 | `yb_query_group_members` | yuanbao_tools.py | SDK-dependent | P2 |
| 3 | M05 | `yb_search_sticker` | yuanbao_tools.py | SDK-dependent | P2 |
| 4 | M06 | `yb_send_dm` | yuanbao_tools.py | SDK-dependent | P2 |
| 5 | M07 | `yb_send_sticker` | yuanbao_tools.py | SDK-dependent | P2 |

## SECTOR 1: Tool Depth Gaps (C tool exists, Python has more features) — 13 gaps

**Note:** Counts from v19 audit may be stale. These represent the ORDER OF MAGNITUDE gap, not exact function counts.

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
| 13 | D13 | discord | 0 | 34 | -34 | Discord (gateway exists, tool exists but with fewer features (12 actions vs 34 Python functions)) | 200-400 | P2 |

## SECTOR 2: Gateway Depth — 1 gap

| # | ID | Platform | Issue | LOC | Priority |
|---|----------|---------|-------|-----|----------|
| 1 | G02 | send_reaction | vtable slot in hermes_gateway.h exists, Signal has standalone implementation with different signature (recipient/target_author/target_timestamp/emoji vs chat_id/message_id/emoji). No platform wires the vtable. | 15-30 per platform | P2 |

## SECTOR 3: CI/Infrastructure — 1 gap

| # | ID | Issue | LOC | Priority |
|---|-----|-------|-----|----------|
| 1 | I02 | GitHub Actions CI not vetted for C-only mode (no `.github/workflows/` dir) | 30-60 | P3 |

## SECTOR 4: Test Edge Cases — 5 gaps

| # | ID | Module | Test File | Issue | Priority |
|---|--------|---------|-----------|-------|----------|
| 1 | X01 | vision.c | tests/test_vision.c (229L) | Image format edge cases (BMP, TIFF, SVG) | P3 |
| 2 | X02 | image_gen.c | tests/test_image_gen.c (76L) | Provider dispatch tests | P3 |
| 3 | X03 | video_gen.c | tests/test_video_gen.c (61L) | Edge case tests | P3 |
| 4 | X04 | transcribe.c | tests/test_transcribe.c (95L) | Provider-specific tests (needs network) | P3 |
| 5 | X05 | session_search.c | tests/test_session_search.c (285L) | Edge cases for query parsing | P3 |

---

## Summary

| Sector | Count | Priority Split |
|--------|-------|----------------|
| S0: Missing Tools | 5 | P2: 5 |
| S1: Tool Depth | 13 | P2: 11, P3: 2 |
| S2: Gateway Depth | 1 | P2: 1 |
| S3: CI/Infra | 1 | P3: 1 |
| S4: Test Coverage | 5 | P3: 5 |
| **TOTAL** | **25** | P0: 0, P1: 0, P2: 17, P3: 8 |

**Stale claims retired to vault:** F01, F02, F03, F04, M01, M02, L02, I01 (8 gaps closed — see vault/achievements.md Phase 9)
