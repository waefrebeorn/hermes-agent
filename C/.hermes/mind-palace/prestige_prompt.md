# Slermes C — Prestige Prompt (v88)

## Current Priority Queue (Top gaps to close)

**P1 (Must fix — blocking):**
1. D01 — MCP depth: SSE transport + streaming (major feature)
2. D03 — Browser depth: CDP autofill, PDF, network interceptions

**P2 (Important):**
3. S01 — llm_client.c background_review unwired
4. S05 — context_engine noop handlers
5. S06 — Telegram editable draft
6. P01-P04 — 4 Yuanbao tools
7. D02 — TTS multi-provider
8. D04 — Terminal depth
9. D05 — Transcribe depth
10. D06 — Send message depth
11. D07 — Delegate depth
12. G02 — send_reaction implementations
13. L03 — @every/@daily cron shorthand

**P3 (Nice-to-have):**
13. S07 — ACP resource placeholder
14. S09 — Memory in-memory noop
15. I01 — Dockerfile for C binary
16. I02 — GitHub Actions CI for C-only
17. X01-X05 — Test expansions
19. L01 — JSON schema validation
20. L02 — MCP SSE streaming (persistent GET + event parsing)

## Current State
- Suite: 282/0/0, 99 tools, 98 CLI, 19 gateways, 10 providers, 65 libs
- Battleship: v17 (32 gaps across 8 sectors, fresh audit 2026-05-27)
- All entry points working (--help, --version, --json, pipe mode, tools subcommand)
