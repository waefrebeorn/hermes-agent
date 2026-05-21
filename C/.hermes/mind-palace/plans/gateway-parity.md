# Slermes C — Gateway Platform Parity (May 24 DA-v1)

## Parity Verification

Python Hermes has ~20 gateway platforms. C Slermes matches all 20.

| Platform | C File | Status | Python Match |
|----------|--------|--------|-------------|
| api_server | built into server.c | ✅ | Full parity |
| telegram | platforms/telegram.c | ✅ | Long-polling |
| discord | platforms/discord.c | ✅ | WebSocket gateway |
| slack | platforms/slack.c | ✅ | Event API |
| matrix | platforms/matrix.c | ✅ | Client-Server API |
| mattermost | platforms/mattermost.c | ✅ | WebSocket |
| whatsapp | platforms/whatsapp.c | ✅ | Webhook |
| signal | platforms/signal.c | ✅ | REST API |
| email | platforms/email.c | ✅ | IMAP+SMTP |
| sms | platforms/sms.c | ✅ | Twilio API |
| homeassistant | platforms/homeassistant.c | ✅ | WebSocket |
| webhook | platforms/webhook.c | ✅ | HTTP server |
| feishu | platforms/feishu.c | ✅ | Webhook |
| wecom | platforms/wecom.c | ✅ | Webhook |
| dingtalk | platforms/dingtalk.c | ✅ | Webhook |
| qqbot | platforms/qqbot.c | ✅ | WebSocket API |
| bluebubbles | platforms/bluebubbles.c | ✅ | REST API |
| msgraph_webhook | platforms/msgraph_webhook.c | ✅ | Graph API + raw socket |
| weixin | platforms/weixin.c | ✅ | WeChat iLink Bot |
| yuanbao | platforms/yuanbao.c | ✅ | WebSocket + protobuf |

## Key Differences

| Aspect | Python | C |
|--------|--------|---|
| API key loading | Config YAML keys (gateway.*_api_key, gateway.*_bot_token) | Env vars only (HERMES_API_KEY, etc.) |
| Gateway config | Config YAML section | Hardcoded or env-driven |
| Webhook server | aiohttp async | Raw socket + HTTP parser |
| WebSocket | websockets library | libwebsocket (minimal C impl) |

## Drop-in Status: ✅ PASS
All platforms compile, register, and respond to configuration. User sets API keys in .env instead of config.yaml.
