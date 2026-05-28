# Slermes C — Prestige Prompt (v92 — 2026-06-02)

## Priority Queue

**P2 — Missing Tools (5):**
1. M03 — yb_query_group_info (SDK-dependent)
2. M04 — yb_query_group_members (SDK-dependent)
3. M05 — yb_search_sticker (SDK-dependent)
4. M06 — yb_send_dm (SDK-dependent)
5. M07 — yb_send_sticker (SDK-dependent)

**P2 — Tool Depth (11):**
6. D01 — MCP depth: SSE streaming + pagination
7. D02 — TTS multi-provider
8. D03 — Browser depth
9. D04 — Terminal depth
10. D05 — Transcribe depth
11. D06 — Send message depth
12. D07 — Delegate depth
13. D08 — Vision edge cases
14. D09 — Image gen provider tests
15. D10 — Web auth flows
16. D13 — Discord tool depth (12 actions vs 34 Python functions)

**P2 — Gateway:**
17. G02 — send_reaction vtable wiring (Signal has impl, different signature)

**P3:**
18. D11 — Video gen edge cases
19. D12 — File minor edge cases
20. I02 — GitHub Actions CI for C-only
21. X01 — Vision test edge cases
22. X02 — Image gen provider dispatch tests
23. X03 — Video gen edge case tests
24. X04 — Transcribe provider-specific tests
25. X05 — Session search query parsing edge cases

## Current State
- Suite: 282/0/0, 99 tools, 98 CLI, 19 gateways, 10 providers, 66 libs
- Battleship: v20 (25 gaps, Triple DA refresh 2026-06-02)
- 0 P0/P1 gaps remaining. All entry points verified.
- 8 stale claims retired to vault Phase 9
