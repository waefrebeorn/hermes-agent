# Slermes C — Prestige Prompt (v85)

## Current Priority Queue (Top gaps to close)

**P1 (Must fix — blocking):**
1. D01 — MCP depth: SSE transport + streaming (major feature)
2. D03 — Browser depth: CDP autofill, PDF, network interceptions
3. L02 — MCP library SSE transport

**P2 (Important):**
4. S01 — llm_client.c background_review unwired
5. S04 — Plugin hot-reload stub
6. S05 — context_engine noop handlers
7. S06 — Telegram editable draft
8. P01-P04 — 4 Yuanbao tools
9. D02 — TTS multi-provider
10. D04 — Terminal depth
11. D05 — Transcribe depth
12. D06 — Send message depth
13. D07 — Delegate depth
14. G02 — send_reaction implementations
15. L03 — @every/@daily cron shorthand

**P3 (Nice-to-have):**
16. S07 — ACP resource placeholder
17. S08 — HomeAssistant deprecation note
18. S09 — Memory in-memory noop
19. I01 — Dockerfile for C binary
20. I02 — GitHub Actions CI for C-only
21. X01-X05 — Test expansions
22. L01 — JSON schema validation

## Current State
- Suite: 282/0/0, 99 tools, 98 CLI, 19 gateways, 10 providers, 65 libs
- Battleship: v17 (33 gaps across 8 sectors, fresh audit 2026-05-27)
- All entry points working (--help, --version, --json, pipe mode, tools subcommand)
