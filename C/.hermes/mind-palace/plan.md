# Implementation Plan — Slermes C (v92)

## Phase 0: COMPLETE — All P0/P1 gaps resolved

## Phase 1: Missing Tools (P2)
1. **M03** — Port yb_query_group_info from yuanbao_tools.py
2. **M04** — Port yb_query_group_members from yuanbao_tools.py
3. **M05** — Port yb_search_sticker from yuanbao_tools.py
4. **M06** — Port yb_send_dm from yuanbao_tools.py
5. **M07** — Port yb_send_sticker from yuanbao_tools.py

## Phase 2: Tool Depth (P2)
6. **D01** — MCP depth: SSE streaming + pagination
7. **D02** — TTS multi-provider (Elevenlabs, Azure, edge-tts)
8. **D03** — Browser depth (autofill, PDF, HAR, multi-window)
9. **D04** — Terminal depth (SSH, Docker Compose, pty)
10. **D05** — Transcribe multi-provider
11. **D06** — Send message depth (threads, buttons, reactions)
12. **D07** — Delegate depth (parallel, lifecycle)
13. **D13** — Discord tool depth (12→34 functions parity)

## Phase 3: Gateway + CI + Tests (P2/P3)
14. **G02** — send_reaction vtable wiring across platforms
15. **I02** — GitHub Actions CI for C-only build
16. **X01-X05** — Test edge case expansion
