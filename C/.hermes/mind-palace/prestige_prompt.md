# Slermes C — Prestige Prompt (v84)

## Current Priority Queue (Top gaps to close)

**P1 (Must fix — blocking):**
1. S03 — commands.c /restart stub (quick fix)
2. D01 — MCP depth: SSE transport + streaming (major feature)
3. D03 — Browser depth: CDP autofill, PDF, network interceptions
4. L02 — MCP library SSE transport

**P2 (Important):**
5. S01 — llm_client.c background_review unwired
6. S04 — Plugin hot-reload stub
7. S05 — context_engine noop handlers
8. S06 — Telegram editable draft
9. P01-P04 — 4 Yuanbao tools
10. D02 — TTS multi-provider
11. D04 — Terminal depth
12. D05 — Transcribe depth
13. D06 — Send message depth
14. D07 — Delegate depth
15. G02 — send_reaction implementations
16. L03 — @every/@daily cron shorthand

**P3 (Nice-to-have):**
17. S07 — ACP resource placeholder
18. S08 — HomeAssistant deprecation note
19. S09 — Memory in-memory noop
20. I01 — Dockerfile for C binary
21. I02 — GitHub Actions CI for C-only
22. X01-X05 — Test expansions
23. L01 — JSON schema validation

## Current State
- Suite: 282/0/0, 99 tools, 98 CLI, 19 gateways, 10 providers, 65 libs
- Battleship: v17 (34 gaps across 8 sectors, fresh audit 2026-05-27)
- All entry points working (--help, --version, --json, pipe mode, tools subcommand)
