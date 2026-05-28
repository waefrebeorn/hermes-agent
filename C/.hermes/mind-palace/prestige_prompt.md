# Slermes C — Prestige Prompt (v91 — 2026-06-02)

## Priority Queue

**P0 (CRITICAL — fix before anything else):**
1. F01 — `--bogus` sends to LLM (10-20 LOC)
2. F02 — Multi-line pipe stdin broken (30-50 LOC)
3. F03 — `--session` without arg runs session_crud (10-20 LOC)

**P1 (Important):**
4. F04 — Tools count display shows 83 vs 99 (10-20 LOC)

**P2:**
5. M01-M07 — 7 missing tools (discord, yuanbao)
6. D01 — MCP depth: SSE streaming + pagination
7. D02 — TTS multi-provider
8. D03 — Browser depth
9. D04 — Terminal depth
10. D05 — Transcribe depth
11. D06 — Send message depth
12. D07 — Delegate depth
13. D13 — Discord tool (34 Python fns, no C)
14. D08-D12 — Vision, image_gen, web, video_gen, file depth
15. L02 — MCP SSE persistent streaming
16. G02 — send_reaction vtable wiring

**P3:**
17. I01 — Dockerfile for C binary
18. I02 — GitHub Actions CI for C-only
19. X01-X05 — Test expansions

## Current State
- Suite: 282/0/0, 99 tools, 98 CLI, 19 gateways, 10 providers, 65 libs
- Battleship: v19 (33 gaps, Triple DA audit 2026-06-02)
