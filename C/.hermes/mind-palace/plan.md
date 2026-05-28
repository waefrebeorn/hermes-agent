# Implementation Plan — Slermes C (v86)

## Phase 1: P1 Gaps (2 items)
1. **D01** — MCP SSE transport + streaming responses (400-800 LOC, largest gap)
2. **D03** — Browser CDP autofill, PDF gen, HAR capture (500-1000 LOC)

## Phase 2: P2 Gaps (20 items)
- S01 (background_review wiring), S04 (hot-reload), S05 (context noops), S06 (telegram draft)
- P01-P04 (4 Yuanbao tools)
- D02 (TTS), D04 (terminal), D05 (transcribe), D06 (send_message), D07 (delegate)
- G02 (send_reaction real impls)
- L03 (@every/@daily cron)
- L02 (MCP SSE streaming — persistent GET + event parsing)

## Phase 3: P3 Gaps (10 items)
- S07-S09 (minor stubs), I01-I02 (infra), X01-X05 (test expansions), L01 (JSON schema)

## Priority Rule
Always pick the highest P1 gap that's not blocked by another gap. Do NOT ask which gap to pick — pick the lowest-numbered unstarted P1 gap.
