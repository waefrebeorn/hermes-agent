# Slermes C — Project (May 22 DA v5)

## Mission
1:1 C translation of NousResearch/hermes-agent. Every CLI command, tool, gateway, TUI, GUI — identical form and function. slermes is the alias binary.

## Done
- ✅ 12 standalone libs
- ✅ Agent loop + LLM + streaming + context + titles
- ✅ 27 tools (18 core + 4 browser + 1 approval + provider framework)
- ✅ 16 CLI commands with registry dispatch
- ✅ 7 gateway platforms (Telegram/Discord/Slack/Matrix/Mattermost/Webhook/WhatsApp)
- ✅ Plugin system, skin engine, cron scheduler, ncurses TUI
- ✅ Security approval integrated into agent loop
- ✅ Abstract provider interface (OpenAI impl done, not wired)
- ✅ Config isolation ~/.slermes/, SLERMES_HOME env
- ✅ 21/21 tests passing, 0 parity issues
- ✅ 5 bugs fixed
- ✅ ROADMAP.md + mind palace + workflow scripts

## Constraint
No PRs to upstream until full 1:1 parity reached. C/ is additive only — never modifies Python files in ways that can't be stashed.

## P0 Next
1. Wire provider interface → llm_client.c
2. Browser click/type/scroll
3. CLI /reset /retry /compress /branch /snapshot
4. Security: URL safety, path traversal, Tirith
5. Gateway: multi-platform simultaneous

## P1 Next (after all P0)
Signal/Email/SMS/HA gateways, kanban(8), spotify(7), terminal backends, skills hub, transcription

## Full Plan
plans/roadmap-100-phases.md (170 phases)
