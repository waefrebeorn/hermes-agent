# Slermes C — Prestige Prompt (v83)

## Current Priority Queue (Top gaps to close)

**P1 (Must fix — blocking):**
1. S02 — server.c platform shutdown = NULL (quick fix)
2. S03 — commands.c /restart stub (quick fix)
3. D01 — MCP depth: SSE transport + streaming (major feature)
4. D03 — Browser depth: CDP autofill, PDF, network interceptions
5. L02 — MCP library SSE transport

**P2 (Important):**
6. S01 — llm_client.c background_review unwired
7. S04 — Plugin hot-reload stub
8. S05 — context_engine noop handlers
9. S06 — Telegram editable draft
10. P01-P04 — 4 Yuanbao tools
11. D02 — TTS multi-provider
12. D04 — Terminal depth
13. D05 — Transcribe depth
14. D06 — Send message depth
15. D07 — Delegate depth
16. G02 — send_reaction implementations
17. L03 — @every/@daily cron shorthand

**P3 (Nice-to-have):**
18. S07 — ACP resource placeholder
19. S08 — HomeAssistant deprecation note
20. S09 — Memory in-memory noop
21. I01 — Dockerfile for C binary
22. I02 — GitHub Actions CI for C-only
23. X01-X05 — Test expansions
24. L01 — JSON schema validation

## Current State
- Suite: 282/0/0, 99 tools, 98 CLI, 19 gateways, 10 providers, 65 libs
- Battleship: v17 (35 gaps across 8 sectors, fresh audit 2026-05-27)
- All entry points working (--help, --version, --json, pipe mode, tools subcommand)
