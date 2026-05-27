# Slermes C — Prestige Prompt (v32 — ~351 Items)

## Phase 0 — Display (16)
V01 markdown ✅ → V02 inline diffs ✅ → V03 banner ✅ → V04 spinner ✅ → V05 multi-line ✅ → V06 rich errors ✅ → V07 TUI → V08 Python TUI → V09 voice → V10 /recap ✅ → V11 tips ✅ → V12 NO_COLOR ✅ → V13 output helpers ✅ → V14 skin ✅ → V15 spinner ✅ → V16 tool feed ✅
**14/16 done.** Remaining: V07 TUI, V08 Python TUI, V09 voice.

## Phase 1 — CLI Args (40) — ✅ ALL DONE
All 80 commands wired with proper argument parsing.

## Phase 2 — Provider Parity (~20 real gaps)
**DA v16 confirmed: deepen claims stale.** All 8 deepen features already in C.
Port targets (not OpenAI-compatible): copilot, copilot-acp, openai-codex, opencode-zen.
16/18 "port 18" providers already routed as PROVIDER_OPENAI aliases.

## Phase 3 — Tool Features (26)
Deepen mcp_tool, file_ops, tts, browser, terminal, transcribe, send_message, delegate, voice, exec_code, discord, vision, image_gen, web, x_search, video_gen, cron, file, todo, skills.

**Newly found missing tools (10):** feishu_drive_add_comment, feishu_drive_list_comments/list_comment_replies/reply_comment, video_analyze, yb_send_dm, yb_query_group_info/members, yb_send/search_sticker.

## Phase 4-11 (365 items)
Missing tools (47) → Gateway (51) → Agent modules (74 → includes 2 unwired stubs: llm_background_review, api_server mock) → Plugins (13) → Libraries (19) → Security (15) → Tests (51) → Config/Infra (10)

## DA v16 Stale Claims Documented
All Phase 2 deepen items verified stale per source code inspection.
New stubs documented in battleship-v16: llm_background_review (implemented, unwired), api_server mock fallback, 10 missing tool files.
