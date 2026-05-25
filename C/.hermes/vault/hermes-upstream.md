# Upstream Reference — Python Hermes Agent

> Reference document mapping Python Hermes source to C translation status.
> Updated: May 25, 2026

## Python Source Overview

| Area | Count | C Count | Gap |
|------|-------|---------|-----|
| Agent modules | 78 .py | 4 .c | 74 |
| Tool files | 76 .py | 6 .c (4 registered tools) | 70 |
| Gateway platforms | 32 .py | 1 .c | 31 |
| Provider backends | ~10 | 1 | ~9 |
| Test files | 900+ | 2 | ~900 |

## Python Agent Modules (78)
Known ports: `context.py` → `context.c`, `title.py` → `title.c`, `llm_client.py` → `llm_client.c` (partial), `agent_loop.py` → `agent_loop.c` (partial)

Unported (74): insights, compress, hermes_state, checkpoint_manager, hermes_logging, hermes_constants, credential_pool, budget_config, skill_commands, model_tools, toolsets, batch_runner, agent/display, agent/prompt_caching, agent/context_engine, agent/profile, agent/curator, agent/debug_helpers, agent/session_manager, agent/cost_tracker, agent/skill_registry, agent/tool_guardrails, agent/tool_result, agent/tool_usage, agent/memory_manager, agent/compress_state, agent/conversation_splitter, agent/checkpoint, agent/rollback, agent/fork, agent/merge, agent/export, agent/import, agent/search, agent/browse, agent/screenshot, agent/voice, agent/tts, agent/stt, agent/ocr, agent/translate, agent/summarize, agent/classify, agent/embed, agent/rag, agent/knowledge_graph, agent/plan, agent/reason, agent/debate, agent/vote, agent/consensus, agent/delegate, agent/parallel, agent/monitor, agent/log, agent/metrics, agent/trace, agent/alert, agent/schedule, agent/trigger, agent/webhook, agent/callback, agent/cron, agent/event, agent/bus, agent/queue, agent/lock, agent/rate_limit, agent/throttle, agent/circuit_breaker, agent/retry, agent/timeout, agent/health, agent/heartbeat

## Python Tools (76)
Known ports: `terminal_tool.py` → `terminal.c`, `file_tool.py` → `file.c`, `web_tool.py` → `web.c`, `skills_tool.py` → `skills.c`

Unported (72): vision_analyze, execute_code, session_search, memory, clarify, delegate_task, cronjob, browser, browser_cdp, browser_dialog, browser_supervisor, computer_use, image_gen, voice_mode, text_to_speech, process, todo, send_message, kanban, curator, mcp, gateway, plugin, system, hermes, batch_runner, task_manager, path_security, code_analyzer, diff_tool, file_history, notepad, clipboard, env, dotenv, network_scanner, port_check, dns_check, ssl_check, cert_info, hash_file, checksum, compress_file, extract_archive, gpg_sign, gpg_verify, password_gen, qr_code, barcode, camera, microphone, ocr, translate, summarize, classify, embed, vector_search, knowledge_graph, note_graph, mind_map, flow_chart, diagram_gen, skills_install, skills_uninstall, skills_view, skills_list, tool_config, tool_permission, tool_approval, agent_config, mcp_tool, file_search

## Python Gateway Platforms (32)
Known ports: `telegram.py` → `telegram.c`

Unported (31): discord, slack, whatsapp, signal, matrix, mattermost, email, sms, dingtalk, wecom, weixin, feishu, qqbot, bluebubbles, yuanbao, webhook, websocket, mcp, acp, google_meet, voip, twilio, telegram_bot (rich version), discord_bot, slack_bot, teams, zoom, meet, jitsi, irc, xmpp

## Key Python Files for Reference Reading
- `run_agent.py` — AIAgent class, core conversation loop (~12K LOC)
- `cli.py` — HermesCLI, interactive CLI orchestrator (~11K LOC)
- `model_tools.py` — Tool orchestration, function call handling
- `hermes_state.py` — SessionDB with FTS5 search
- `gateway/run.py` — Gateway main entry
- `tools/registry.py` — Tool registration pattern
