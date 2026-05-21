# Hermes C — Gateway Platform Parity Spec (May 21 AM)

## Current: 7 platforms (Telegram, Discord, Slack, Matrix, Mattermost, Webhook, WhatsApp)
## Target: 20+ platforms matching Python

## 1. Platform Comparison

| Platform | Python | C | Diff | Priority | Est LOC |
|----------|--------|---|------|----------|---------|
| Telegram | ✅ | ✅ | same | done | ~200 |
| Discord | ✅ | ✅ | same | done | ~250 |
| Slack | ✅ | ✅ | same | done | ~250 |
| Matrix | ✅ | ✅ | same | done | ~300 |
| Mattermost | ✅ | ✅ | same | done | ~250 |
| Webhook HTTP API | ✅ | ✅ | same | done | ~500 |
| WhatsApp | ✅ | ✅ | same | done | ~300 |
| Signal | ✅ | ❌ | missing | P1 | ~300 |
| Email (IMAP/SMTP) | ✅ | ❌ | missing | P1 | ~400 |
| SMS (Twilio) | ✅ | ❌ | missing | P1 | ~300 |
| HomeAssistant | ✅ | ❌ | missing | P1 | ~400 |
| DingTalk | ✅ | ❌ | missing | P2 | ~300 |
| WeCom | ✅ | ❌ | missing | P2 | ~400 |
| Weixin/iLink | ✅ | ❌ | missing | P2 | ~300 |
| Feishu/Lark | ✅ | ❌ | missing | P2 | ~400 |
| BlueBubbles (iMessage) | ✅ | ❌ | missing | P2 | ~300 |
| Yuanbao | ✅ | ❌ | missing | P2 | ~500 |
| QQ Bot | ✅ | ❌ | missing | P2 | ~300 |
| MS Graph (Teams) | ✅ | ❌ | missing | P2 | ~300 |
| IRC (plugin) | ✅ | ❌ | missing | P2 | ~300 |
| Line (plugin) | ✅ | ❌ | missing | P3 | ~300 |
| Google Chat (plugin) | ✅ | ❌ | missing | P3 | ~300 |
| Simplex (plugin) | ✅ | ❌ | missing | P3 | ~300 |
| Teams (plugin) | ✅ | ❌ | missing | P3 | ~300 |

## 2. Gateway Architecture Gaps

| Feature | Python | C | Priority | Est LOC |
|---------|--------|---|----------|---------|
| Multi-platform simultaneous | ✅ runs all | ❌ 1 at a time | P1 | ~500 |
| Gateway session lifecycle | ✅ topics, threads, channels | ❌ flat | P1 | ~500 |
| Streaming responses to platforms | ✅ stream tokens | ❌ complete response | P1 | ~300 |
| Gateway health monitoring | ✅ auto-recovery | ❌ none | P1 | ~300 |
| Platform pause/resume | ✅ graceful | ❌ crash | P1 | ~200 |
| Gateway hooks system | ✅ plugin hooks | ❌ none | P1 | ~300 |
| Gateway plugin platforms | ✅ 5 extras | ❌ none | P2 | ~500 |
| Gateway webhook auth | ✅ HMAC/signatures | ❌ none | P1 | ~200 |
| Gateway rate limiting | ✅ per-platform | ❌ none | P1 | ~300 |
| Gateway admin UI | ✅ web dashboard | ❌ none | P2 | ~1,000 |
| Gateway session home channel | ✅ paired | ❌ none | P1 | ~200 |
| Gateway channel topic support | ✅ Telegram topics | ❌ none | P1 | ~200 |

## 3. Implementation Order

### Phase 1: Multi-platform support
- Refactor gateway to run N platform poll loops in threads
- Add platform lifecycle management (start/stop/pause/resume)

### Phase 2: Top 4 missing platforms (Signal, Email, SMS, HA)
- Signal: REST polling + encryption
- Email: IMAP poll + SMTP send
- SMS: Twilio REST API
- HomeAssistant: WebSocket + REST API

### Phase 3: Gateway session lifecycle
- Track platform-level message metadata (threads, topics, channels)
- Store session-to-platform mapping
- Support threaded conversations

### Phase 4: Streaming + health monitoring
- Stream LLM tokens to platform message API
- Add health check + auto-recovery per platform

### Phase 5: Auth + rate limiting
- Webhook signature verification (HMAC-SHA256)
- Per-platform rate limit buckets

### Phase 6: Remaining platforms
- DingTalk, WeCom, Feishu, BlueBubbles, Yuanbao, QQ Bot, MS Graph
