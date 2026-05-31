<p align="center">
  <a href="README.md">
    <img src="assets/slermes-logo-clean.svg" alt="Slermes" width="300">
  </a>
</p>

# Slermes ☤

<p align="center">
  <img src="https://img.shields.io/badge/Tests-338%2F0%2F13-00AA55?style=flat-square" alt="Tests">
  <img src="https://img.shields.io/badge/Version-v446-8A2BE2?style=flat-square" alt="Version">
  <img src="https://img.shields.io/badge/Binary-31MB%20ELF-FFD700?style=flat-square" alt="31MB">
  <img src="https://img.shields.io/badge/Source-108K%20LOC%20C-blue?style=flat-square" alt="108K LOC">
  <img src="https://img.shields.io/badge/Gaps-53-FF6B35?style=flat-square" alt="53 gaps">
  <img src="https://img.shields.io/badge/Tools-85-8B5CF6?style=flat-square" alt="85 tools">
  <img src="https://img.shields.io/badge/CLI-98%20commands-00AA55?style=flat-square" alt="98 CLI">
  <img src="https://img.shields.io/badge/Libs-65%20modules-0066FF?style=flat-square" alt="65 libs">
  <img src="https://img.shields.io/badge/Gateways-19%20platforms-FF6B35?style=flat-square" alt="19 gateways">
  <img src="https://img.shields.io/badge/Phase-386-FFD700?style=flat-square" alt="Phase 386">
</p>

**Full C translation of [Hermes Agent](https://github.com/NousResearch/hermes-agent) by Nous Research.** One static binary, zero runtime deps beyond libc + libssl. 31M ELF.

---

## Quick Build

```bash
git clone https://github.com/waefrebeorn/slermes.git
cd slermes/slermes/
make -j$(nproc)            # 0 errors, 31M binary
./slermes --help           # Usage
bash test_runner.sh        # 338/0/13 suite, <60s
```

**Docker:**
```bash
docker build -t slermes -f slermes/Dockerfile .
docker run --rm slermes --help
# ~20MB slim image (bookworm-slim, static-linked)
```

**Smoke test:**
```bash
echo '/tools' | ./slermes       # List all 85 registered tools
echo '/providers' | ./slermes   # List provider configurations
```

---

## Architecture

```
                    ┌─────────────────────────┐
                    │  CLI / Gateway / TUI     │
                    │  (3 entry points)        │
                    └──────────┬──────────────┘
                               │ User message
                    ┌──────────▼──────────────┐
                    │     Agent Loop           │
                    │  (context, budget,       │
                    │   fallback, checkpoint,  │
                    │   interrupt, audit)      │
                    └──────────┬──────────────┘
                               │ LLM call
                    ┌──────────▼──────────────┐
                    │   LLM Client + 10        │
                    │   Provider Adapters      │
                    │  (OpenAI-compat + native)│
                    └──────────┬──────────────┘
                               │ Tool call
                    ┌──────────▼──────────────┐
                    │   85 Tool Registry       │
                    │  (file, web, terminal,   │
                    │   skills, MCP, kanban,   │
                    │   browser, delegate...)  │
                    └──────────┬──────────────┘
                               │ Plugin dispatch
                    ┌──────────▼──────────────┐
                    │   10 Plugin .so          │
                    │  (kanban, honcho,        │
                    │   spotify, image_gen...) │
                    └──────────┬──────────────┘
                               │ System calls
                    ┌──────────▼──────────────┐
                    │   65 Library Units       │
                    │  (json, yaml, http,      │
                    │   crypto, mcp, cron...)  │
                    └─────────────────────────┘
```

---

## Build System

5-phase pipeline: libraries → agent core → tools → gateway → final link.

```bash
make hermes           # Phase 5 — full binary (0 errors, 31M)
make plugins          # 10 .so shared objects
make tui              # ncurses TUI → hermes-tui (experimental)
make libs             # 65 library compilation units
make docs             # Doxygen HTML (if doxygen available)
make install-plugins  # Build + copy .so → ~/.hermes/plugins/
```

| Phase | What | Output |
|-------|------|--------|
| P1 | 65 library units (.o) | lib/*.o |
| P2 | Agent core + CLI + 10 providers | src/agent/*.o, src/cli/*.o |
| P3 | Tool handlers | src/tools/*.o |
| P4 | 19 gateway platforms | src/gateway/*.o |
| P5 | Cron scheduler + final link | `hermes` binary |

**Build variants:**
```bash
make -j$(nproc)                                             # Release
make CFLAGS="-O0 -g3 -Wall -Wextra -Wpedantic -Werror"     # Debug
make CFLAGS="-O1 -g -fsanitize=address" LDFLAGS="-fsanitize=address"  # ASan
```

---

## Tools (85 Registered)

Every tool registered at startup via `registry_register()`. All compiled in — no interpreter overhead.

### File & Terminal

| Tool | File | Status |
|------|------|--------|
| `read_file`, `write_file`, `search_files` | `src/tools/file.c` | ✅ Complete |
| `file_batch` | `src/tools/file_batch.c` | ✅ Complete |
| `terminal`, `process` | `src/tools/terminal.c`, `process.c` | ✅ Complete |
| `patch` | `src/tools/patch.c` | ✅ Complete |

### Web & Browser

| Tool | File | Status |
|------|------|--------|
| `web_get`, `web_search`, `web_extract` | `src/tools/web.c` | ✅ Complete |
| `browser_navigate`, `snapshot`, `click`, `type`, `scroll` | `src/tools/browser.c` | ✅ Text-based |
| `browser_vision`, `console`, `dialog`, `cdp` | `src/tools/browser.c` | ✅ CDP |

### Agent & Delegation

| Tool | File | Status |
|------|------|--------|
| `delegate_task` | `src/tools/delegate.c` | ✅ Complete |
| `clarify`, `send_message` | `src/tools/clarify.c`, `send_message.c` | ✅ Complete |
| `approval_status` | `src/tools/approval.c` | ✅ Complete |

### Skills System

| Tool | File | Status |
|------|------|--------|
| `skills_list`, `skill_scan`, `skill_validate`, `skill_provenance` | `src/tools/skills.c` | ✅ Complete |
| `skill_sync`, `skill_bundle`, `skill_usage`, `skill_cache` | `src/tools/skills.c` | ✅ Complete |
| `skill_search`, `skill_curator`, `skill_deps`, `skill_hub` | `src/tools/skills.c` | ✅ Complete |
| `skill_view`, `skill_manage` | `src/tools/skill_mgmt.c` | ✅ Complete |

### Memory & Sessions

| Tool | File | Status |
|------|------|--------|
| `memory`, `session_search`, `session_crud`, `todo` | `src/tools/memory.c` etc. | ✅ Complete |

### MCP

| Tool | File | Status |
|------|------|--------|
| `mcp_status`, `mcp_tool_call`, `mcp_auth` | `src/tools/mcp_tool.c` | ✅ Complete |
| `mcp_resource_list/read`, `mcp_prompt_list/get` | `src/tools/mcp_tool.c` | ✅ Complete |
| (dynamic) | `src/tools/mcp_tool.c` | Tools from connected servers |

### Media

| Tool | File | Status |
|------|------|--------|
| `vision_analyze` | `src/tools/vision.c` | ✅ Complete |
| `text_to_speech`, `voice_listen`, `voice_speak` | `src/tools/tts.c`, `voice_mode.c` | ✅ Complete |
| `image_generate`, `video_generate` | `src/tools/image_gen.c`, `video_gen.c` | ✅ Complete |

### Automation

| Tool | File | Status |
|------|------|--------|
| `cronjob` | `src/tools/cronjob.c` | ✅ Complete |
| `kanban_show/list/complete/block/comment/create/unblock/link` | `src/tools/kanban.c` | ✅ Complete |

### Integration

| Tool | File | Status |
|------|------|--------|
| `x_search` | `src/tools/x_search.c` | ✅ Complete |
| `ha_list_entities/get_state/list_services/call_service` | `src/tools/homeassistant.c` | ✅ Complete |
| `discord_send` | `src/tools/discord.c` | ✅ Complete |

### Internal

| Tool | File | Status |
|------|------|--------|
| `execute_code`, `rate_limit`, `url_safety`, `tirith` | Various | ✅ Complete |
| `result_storage`, `tool_config`, `mixture_of_agents` | Various | ✅ Complete |
| `video_analyze`, `path_security`, `xai_http` | Various | ✅ Complete |
| `account_usage`, `ansi_strip`, `transcribe` | Various | ✅ Complete |
| `file_watch`, `file_merge`, `feishu_tools`, `wecom_crypto` | Various | ✅ Complete |

---

## Gateway Platforms (19)

All platforms implement `(init, send, poll, shutdown)` lifecycle. Single-threaded with epoll/kqueue.

| # | Platform | Adapter | Status |
|---|----------|---------|--------|
| 1 | Telegram | `gateway/platforms/telegram.c` | ✅ |
| 2 | Discord | `gateway/platforms/discord.c` | ✅ |
| 3 | Slack | `gateway/platforms/slack.c` | ✅ |
| 4 | Matrix | `gateway/platforms/matrix.c` | ✅ |
| 5 | Mattermost | `gateway/platforms/mattermost.c` | ✅ |
| 6 | WhatsApp | `gateway/platforms/whatsapp.c` | ✅ |
| 7 | Email (IMAP/SMTP) | `gateway/platforms/email.c` | ✅ |
| 8 | Signal | `gateway/platforms/signal.c` | ✅ |
| 9 | SMS (Twilio) | `gateway/platforms/sms.c` | ✅ |
| 10 | Feishu/Lark | `gateway/platforms/feishu.c` | ✅ |
| 11 | WeCom | `gateway/platforms/wecom.c` | ✅ |
| 12 | DingTalk | `gateway/platforms/dingtalk.c` | ✅ |
| 13 | QQ Bot | `gateway/platforms/qqbot.c` | ✅ |
| 14 | BlueBubbles | `gateway/platforms/bluebubbles.c` | ✅ |
| 15 | MSGraph Webhook | `gateway/platforms/msgraph_webhook.c` | ✅ |
| 16 | WeChat Official | `gateway/platforms/weixin.c` | ✅ |
| 17 | YuanBao | `gateway/platforms/yuanbao.c` | ✅ |
| 18 | Webhook | `gateway/platforms/webhook.c` | ✅ |
| 19 | Home Assistant | `gateway/platforms/homeassistant.c` | ✅ |

---

## LLM Providers (10)

Every provider: `(init, chat, stream, count_tokens, build_headers, parse_response)`.

| Provider | Adapter | Type | Status |
|----------|---------|------|--------|
| OpenAI | `provider_openai.c` | OpenAI-compat | ✅ |
| OpenRouter | `provider_openrouter.c` | OpenAI-compat | ✅ |
| DeepSeek | `provider_deepseek.c` | OpenAI-compat | ✅ |
| xAI | `provider_xai.c` | OpenAI-compat | ✅ |
| Anthropic | `provider_anthropic.c` | Native | ✅ |
| Google | `provider_google.c` | Native | ✅ |
| Azure | `provider_azure.c` | Native | ✅ |
| Bedrock | `provider_bedrock.c` | Native | ✅ |
| Custom | `provider_custom.c` | OpenAI-compat | ✅ |
| Copilot | `provider_copilot.c` | OpenAI-compat | ✅ |

---

## Plugins (10 .so)

| Plugin | File | Status |
|--------|------|--------|
| Kanban board | `plugin_kanban.so` | ✅ |
| Honcho memory | `plugin_honcho.so` | ✅ |
| Spotify control | `plugin_spotify.so` | ✅ |
| Disk cleanup | `plugin_disk_cleanup.so` | ✅ |
| File memory | `plugin_file_memory.so` | ✅ |
| Achievements | `plugin_achievements.so` | ✅ |
| Observability | `plugin_observability.so` | ✅ |
| Skills hub | `plugin_skills.so` | ✅ |
| Image generation | `plugin_image_gen.so` | ✅ |
| Google Meet | `plugin_google_meet.so` | ✅ |

---

## Libraries (65 Units)

All compiled directly into the binary. Zero external deps.

| Library | Purpose | Status |
|---------|---------|--------|
| `libjson`, `libyaml`, `libtoml`, `libjson5`, `libcsv` | Parsing | ✅ |
| `libhttp`, `libwebsocket`, `libmcp`, `libmcp_oauth` | Networking | ✅ |
| `libcrypto`, `libhash`, `libbase64`, `libuuid` | Crypto/ID | ✅ |
| `libhtml`, `libregex`, `libprotobuf`, `libdifflib` | Processing | ✅ |
| `libdatetime`, `libcron`, `libglob`, `libtextwrap` | Utilities | ✅ |
| `libpath`, `libansi`, `libsignal`, `libproc` | System | ✅ |
| `libtui`, `libncurses`, `libskin`, `libtemplate` | Display | ✅ |
| `libdb`, `libplugin`, `libcredential`, `libfilestate` | Storage | ✅ |
| `libbrowser`, `libtranscribe`, `libdebug`, `libbinary` | Features | ✅ |
| `libdotenv`, `libosv`, `libwebsite`, `libmsgraph` | Integration | ✅ |
| `libfuzzymatch`, `liberrorclassifier`, `libratelimit` | Quality | ✅ |
| `liblineedit`, `libinterrupt`, `libtoolbackend`, `libtooldispatch` | Core | ✅ |
| `libbudgetconfig`, `libthreatpatterns`, `libcredentialfiles` | Security | ✅ |
| `libskillaudit`, `libskillusage`, `libskillsync`, `libslashconfirm` | Skills | ✅ |
| `libfile_sync`, `libenvpassthrough`, `libfal_common`, `libtooloutput` | Support | ✅ |
| `librateguard`, `libmangateway`, `libschemasanitizer`, `libxai_http` | Infrastructure | ✅ |

---

## CLI Commands (98)

All with tab completion + history. Central command registry with alias resolution.

- **Core:** chat, session, history, model, provider, tools
- **Config:** config show/set/get/edit/migrate, env, profiles
- **Skills:** skills list/view/manage/scan/validate/sync/bundle/search
- **Gateway:** gateway start/stop/status/platforms
- **Cron:** cron list/add/remove/pause/resume/retry
- **Plugins:** plugins list/install/remove
- **MCP:** mcp status, mcp connect/disconnect/servers/tools
- **System:** version, help, doctor, update, logs

---

## Project Structure

```
slermes/
├── src/                      ← 174 .c files
│   ├── agent/                ← Agent loop, LLM client, 10 providers
│   ├── cli/                  ← 98 commands, config, tab complete
│   ├── tools/                ← 85 tool handlers
│   └── gateway/              ← 19 platform adapters, server
├── lib/                      ← 65 library modules
├── tests/                    ← 239 test files (338/0/13)
├── include/                  ← hermes.h + subsystem headers
├── assets/                   ← Logos, mascot, banners
├── extras/                   ← C toolchest (img2svg, palette, asciimg, morph, imggrid)
└── .hermes/                  ← Mind-palace, battleship v34, vault
```

---

## Parity

**338 tests pass, 0 fail, 13 skip. 53 gaps across 8 sectors.**

| Sector | Status | Detail |
|--------|--------|--------|
| S0 Display/Input | ✅ PORTED | D01-D16, T13 — line editing, vi mode, type-ahead |
| S1 Agent Core | ✅ PORTED | L23-L28 — conversation loop, repair, tool dispatch |
| S2 CLI/Config | ✅ PORTED | C01-C37 — all CLI commands, config sections |
| S3 Gateway | ✅ PORTED | G01-G13 — all 19 platforms, server infrastructure |
| S4 TUI | 🔄 ACTIVE | 8 gaps — Ink/React TUI parity |
| S5 Agent Modules | 🔄 ACTIVE | 15 gaps — provider depth, agent runtime |
| S6 Tools | ✅ PORTED | B01-B10 — all 85 tools registered |
| S7 Test Coverage | 🔄 ACTIVE | 18 gaps — edge cases, stress, e2e |
| S8 Providers | ✅ PORTED | R01+R02+R04+R10 — all 10 adapters |
| S9 Ecosystem | 🔄 ACTIVE | 20 gaps — cron, MCP, voice, deploy, ACP |

See [`battleship-v34.md`](.hermes/mind-palace/battleship-v34.md) for the full gap map.
See [`vault/achievements.md`](.hermes/mind-palace/vault/achievements.md) for every resolved gap.

---

## Upstream

This is the C translation inside [waefrebeorn/slermes](https://github.com/waefrebeorn/slermes) — a fork of [NousResearch/hermes-agent](https://github.com/NousResearch/hermes-agent). The Python reference is at the repo root. C work was developed over 277 commits on `c-work` branch, then squashed onto main.

> **Note:** The root `README.md` is the repo entry point. This file is the canonical C docs.
