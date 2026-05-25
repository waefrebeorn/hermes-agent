# Slermes C — Plan (v3 — 2026-05-25)

## Mission
P0: WuBu Slermes — C Translation. 1:1 parity with Python Hermes.
~67 real gaps remaining (~45,000 LOC to port).

## Verified State

| Metric | Value |
|--------|-------|
| Suite | 226/0/23 (213 test files) |
| Binary | 30MB, 0 errors, ~15 warnings |
| Tools | 85 registered (46 .c files) |
| CLI | 79 slash commands |
| Providers | 10 .c files |
| Gateways | 19 .c files |
| Library dirs | 59 lib/*/ |
| **Real gaps** | **67** (12 P0, 4 P1, 33 P2, 18 P3) |

## Phase Order
**Phase 0** (12 gaps) — Display & Visual Parity. C CLI must look identical to Python.
**Phase 1** (4 gaps) — Critical agent modules (error_classifier, chat_completion_helpers, tool_executor, process_registry)
**Phase 2** (33 gaps) — Missing tools, provider adapters, gateway sub-modules, agent modules
**Phase 3** (18 gaps) — Smaller modules, tool depth improvements, plugin system

## Phase 0 Display Gaps (Sector 0)
1. □ V01 — Skin Engine (YAML themes, 30+ colors)
2. □ V02 — KawaiiSpinner (animated faces, 9 types, verbs)
3. □ V03 — Rich Banner (ASCII art, colored panels)
4. □ V04 — Status Bar (context, model, session)
5. □ V05 — Tool Activity Feed (┊ prefix, emoji)
6. □ V06 — Response Box (colored border, label)
7. □ V07 — Rich Help (table formatting, categories)
8. □ V08 — ANSI 256/TrueColor (hex → 24-bit)
9. □ V09 — Prompt Input (tab complete, history)
10. □ V10 — Markdown Rendering (LLM responses)
11. □ V11 — Kawaii Faces (faces, verbs, styles)
12. □ V12 — Tool Emoji Registry (per-tool emoji)

## First Action
Pick V01 (Skin Engine). Port `hermes_cli/skin_engine.py` → C. YAML parser exists in lib. Color config → struct. Wire into cli.c.
