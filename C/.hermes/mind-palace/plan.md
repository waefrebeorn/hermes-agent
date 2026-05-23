# Plan — Hermes C Translation (v9 — 2026-06-01)

## Phase Completion

| Phase | Description | Status |
|-------|-------------|--------|
| P1 | Foundation deps (JSON/YAML/HTTP/DB/crypto/display) | ✅ **100%** |
| P2 | Agent core (loop, LLM client, CLI, config) | ✅ **100%** |
| P3 | Tools (registry, terminal, file, web, skills) | ✅ **95%** |
| P4 | Gateway (Telegram, Discord, Webhook, Slack, Matrix, Mattermost, WhatsApp, Email, Signal, HomeAssistant, SMS, Feishu, WeCom, DingTalk, QQBot, BlueBubbles, MSGraph Webhook, Weixin, Yuanbao) | ✅ **100%** |
| P5 | Cron (scheduler, job store, locking, scripts, CLI) | ✅ **95%** (tested: cron_lib, cron_tool, cron_sqlite, cron_extras) |
| P6 | Plugins (10 .so) | ✅ **100%** |
| P7 | Libraries (30 .a) | ✅ **100%** |
| P8 | Tests (116 files, 154/0/0) | ⚠️ **66%** |
| P9 | Upstream sync | ⚠️ **183 commits behind** |

## Next Steps

### Immediate (next session)
1. Continue cron test coverage: cron_cli.c, jobs.c, scheduler.c, cron_locking.c
2. Or: untested tool handlers (browser.c, computer_use.c, discord.c, homeassistant.c, image_gen.c, registry.c, session_crud.c, tirith.c, tool_init.c, voice_mode.c)
3. Or: CLI tests (cli.c, cli/main.c)
4. Or: upstream sync via `make upstream-sync`

### Medium-term
5. O02 Windows build detection
6. Provider-specific API deep tests (7 quirks)
7. CLI autocomplete polish

### Known Bugs to Fix
- cron_job_get_retry_count(NULL) — no NULL guard (works by accident when g_retry_count==0)
- cron_job_get_max_retries(NULL) — same conditional safety
- cron_job_increment_retry — calls sleep() in test, needs mocking
