# Slermes C — Prestige Prompt (v89 — 2026-06-02)

## Current Priority Queue (Top gaps to close)

**P2 (Important):**
1. P01-P04 — 4 Yuanbao tools (SDK-dependent, need gateway protocol)
2. D01 — MCP depth: SSE streaming + pagination (150-250 LOC)
3. D02 — TTS multi-provider (Elevenlabs, Azure, edge-tts)
4. D03 — Browser depth: CDP autofill, PDF gen, HAR capture
5. D04 — Terminal depth: SSH, Docker Compose, Singularity
6. D05 — Transcribe depth: multi-provider, diarization
7. D06 — Send message depth: buttons, reactions, embed cards
8. D07 — Delegate depth: parallel spawning, lifecycle
9. G02 — send_reaction vtable wiring across platforms
10. G03 — email IMAP reconnection handling
11. L01 — JSON schema validation for libjson
12. L02 — MCP SSE persistent streaming
13. R01 — test_web DNS crash guard

**P3 (Nice-to-have):**
14. I01 — Dockerfile for C binary
15. I02 — GitHub Actions CI for C-only
16. X01-X05 — Test expansions (vision, image_gen, video_gen, transcribe, session_search)

## Current State
- Suite: 282/0/0, 99 tools, 98 CLI, 19 gateways, 10 providers, 65 libs
- Battleship: v18 (23 verified gaps, fresh stale-claim sweep 2026-06-02)
- No P1 gaps remain
- All entry points working (--help, --version, --json, pipe mode, tools subcommand)
