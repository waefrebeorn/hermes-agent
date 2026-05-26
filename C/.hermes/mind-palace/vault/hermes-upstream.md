# Hermes Upstream — Python Modules Not Ported to C

Last updated: 2026-05-25 (battleship-v11)

## Agent Modules (10+ not ported, ~4500 LOC)

| Module | Python LOC | C Status | Notes |
|--------|-----------|----------|-------|
| error_classifier | ~500 | Not ported | Classifies LLM errors into categories |
| chat_completion_helpers | ~400 | Partial | Some in llm_client.c |
| tool_executor | ~600 | Partial | Some in agent_loop.c |
| process_registry | ~400 | Not ported | Background process lifecycle |
| memory_manager | ~500 | Basic file-based | Python has multi-backend orchestration |
| memory_provider | ~400 | Not ported | Plugin-based memory backends |
| message_sanitization | ~300 | Partial | Helper for message cleanup |
| insights | ~900 | Partial | cmd_insights is basic |
| iteration_budget | ~200 | Partial | In agent_loop |
| prompt_builder | ~600 | Partial | In system_prompt.c |

## Tool Files (15+ not ported, ~5000+ LOC)

| Tool Module | Python LOC | Notes |
|------------|-----------|-------|
| feishu_doc_tool.py | ~400 | Feishu/Lark document tools |
| feishu_drive_tool.py | ~300 | Feishu drive integration |
| microsoft_graph_auth.py | ~300 | Microsoft Graph OAuth |
| microsoft_graph_client.py | ~400 | Microsoft Graph API client |
| skills_guard.py | ~300 | Skill security guard system |
| skill_provenance.py | ~200 | Skill origin tracking |
| xai_http.py | ~200 | XAI HTTP client |
| yuanbao_tools.py | ~300 | Tencent Yuanbao tools |
| slack_blocks.py | ~400 | Slack Block Kit builder |
| discord_tool.py | ~600 | Discord bot tools |
| homeassistant_tool.py | ~500 | Home Assistant integration |
| image_generation_tool.py | ~700 | Image gen tool features |
| cronjob_tools.py | ~400 | Cron job management |
| credential_files.py | ~300 | Credential file management |

## Gateway Platform Files (14 not ported, ~10500+ LOC)

| Platform Module | Python LOC | Notes |
|----------------|-----------|-------|
| api_server.py | 3524 | Full API server implementation |
| feishu_comment.py | 1382 | Feishu doc commenting system |
| feishu_comment_rules.py | 429 | Comment auto-rules |
| signal_rate_limit.py | 369 | Signal rate limiting |
| telegram_network.py | 259 | Telegram network helpers |
| wecom_callback.py | 403 | WeCom callback verification |
| wecom_crypto.py | 142 | WeCom encryption |
| yuanbao_media.py | 645 | Yuanbao media upload |
| yuanbao_proto.py | 1209 | Yuanbao protocol (large!) |
| yuanbao_sticker.py | 558 | Yuanbao stickers |
| helpers.py | 278 | Gateway helpers |
| _http_client_limits.py | 84 | HTTP client config |
| base.py | 3812 | Base gateway platform class |
| __init__.py | 45 | Package init |

## Model Provider Plugins (20+ not ported)

All Python model providers are plugins (plugins/model-providers/*/).
Only the core 10 providers are ported in C (provider_*.c).
The full list of plugin providers: ai-gateway, alibaba, alibaba-coding-plan,
anthropic, arcee, azure-foundry, bedrock, copilot, copilot-acp, custom,
deepseek, gemini, gmi, huggingface, kilocode, kimi-coding, minimax, nous,
novita, nvidia, ollama-cloud, openai-codex, opencode-zen, openrouter,
qwen-oauth, stepfun, xai, xiaomi, zai.

## Test Coverage Gap

| Metric | Python | C | Coverage % |
|--------|--------|---|------------|
| Test files | ~1140 | 213 | 19% |
| LOC | ~200,000 | 48,301 | 24% |

## Total Upstream Delta

- ~30,000+ LOC of Python agent/tool/gateway code not ported to C
- 20+ model provider plugins not ported
- 14 gateway modules not ported
- ~80% test coverage gap
