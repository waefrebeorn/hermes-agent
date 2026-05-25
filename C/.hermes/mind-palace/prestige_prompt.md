# Slermes C — Prestige Prompt

## Priority Queue

### P1 — Missing Agent Modules (critical parity)
1. conversation_compression.c — port 603 LOC from compression/cache logic
2. title_generator.c — auto-session-titling from LLM
3. tool_executor.c — tool execution orchestration

### P2 — Gateway Depth
4. feishu_comment — Feishu comment/document annotation
5. wecom_callback — WeCom callback verification
6. wecom_crypto — WeCom message encryption
7. signal_rate_limit — Signal rate limiting helper
8. telegram_network — Telegram network helpers
9. yuanbao_media/proto/sticker — YuanBao sub-modules

### P3 — Placeholder Cleanup
10. acp/resource.c:263 — non-file URI placeholder
11. mcp_tool.c:1287 — placeholder auth entry
12. system_prompt.c:53 — TODO in prompt text

## Build Gate
- `make clean && make -j$(nproc)` — 0 errors
- `bash test_runner.sh` — 226/0/23

## Config
- `SLERMES_HOME` or `HERMES_HOME` or `~/.slermes/`
- `slermes init` to create default config
- `slermes doctor` to verify
