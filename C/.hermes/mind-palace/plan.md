# Implementation Plan — Slermes C (v89)

## Phase 1: P2 Gaps (16 items)
1. **P01-P04** — 4 Yuanbao tools: query group info/members, search/send sticker
2. **D01** — MCP SSE streaming + pagination (150-250 LOC)
3. **D02** — TTS multi-provider (Elevenlabs, Azure, edge-tts, SSML, streaming)
4. **D03** — Browser CDP autofill, PDF gen, HAR capture
5. **D04** — Terminal SSH, Docker Compose, Singularity
6. **D05** — Transcribe multi-provider, diarization
7. **D06** — Send message buttons, reactions, embed cards
8. **D07** — Delegate parallel spawning, lifecycle
9. **G02** — send_reaction vtable wiring across platforms
10. **G03** — email IMAP reconnection handling
11. **L01** — JSON schema validation for libjson
12. **L02** — MCP SSE persistent streaming (persistent GET + event parsing)
13. **R01** — test_web DNS crash guard

## Phase 2: P3 Gaps (7 items)
14. I01 — Dockerfile for C binary
15. I02 — GitHub Actions CI for C-only
16. X01 — vision.c test expansions
17. X02 — image_gen.c test expansions
18. X03 — video_gen.c test expansions
19. X04 — transcribe.c test expansions
20. X05 — session_search.c test expansions

## Priority Rule
Pick the highest unclosed gap that's not blocked. R01 (test_web DNS guard) is quickest win at 10-20 LOC. Yuanbao tools (P01-P04) are SDK-dependent and require significant infrastructure. Tool depth gaps (D01-D07) are large (200-800 LOC each). L02 (SSE streaming) is focused at 150-250 LOC.
