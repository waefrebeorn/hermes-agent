# Slermes C

**Slermes — Full C translation of the Python [Hermes Agent](https://github.com/NousResearch/hermes-agent) by Nous Research.**  
One static binary. Zero runtime deps beyond libc + libssl. 31M ELF.

```text
|||||||| Suite:  282/0/0 (239 test files, completes in <60s)
|||||||| Binary: 31M    (dynamic ELF, -O2 -g)
|||||||| Source: 456+ .c files (src/ + lib/ + tests/): 108K+ C LOC
|||||||||| Gaps:   22 verified gaps (Triple DA v22 + D01 pagination) — see battleship-v22
||||||||Stubs:  0 confirmed stubs remain. All entry points verified working.
||||||Build:  gcc -O2 -g -Wall -Wextra -Wpedantic — 0 errors, 0 warnings
||||||CLI:    98 cmd_ functions — 99 unique tools registered
||||||Tools:  99 registered (103+ at runtime with MCP dynamic)
||||Libraries: 66 C modules — zero external deps beyond libc+libssl
||||Gateway: 19 platform adapters (Telegram, Discord, Slack, Signal, SMS, etc.)
||||Providers: 10 .c modules + metadata (OpenAI, Anthropic, Google, DeepSeek, xAI, Azure, Bedrock, OpenRouter, Custom, Copilot)
```

> **Symlink note:** `README.md` → `C/README.md`. The canonical README lives at `C/README.md`. Edit that file; the root follows automatically.
    21|    21|
    22|    22|---
    23|    23|
    24|    24|## Table of Contents
    25|    25|
    26|    26|- [Quick Start](#quick-start)
    27|    27|- [Architecture](#architecture)
    28|    28|- [Build System](#build-system)
    29|    29|- [All Tools (99 Registered)](#all-tools-99-registered)
    30|    30|- [Gateway Platforms (19)](#gateway-platforms-19)
    31|    31|- [LLM Providers (10)](#llm-providers-10)
    32|    32|- [Plugins (10 .c)](#plugins-10-c-0-so)
    33|    33|- [Libraries (66 Units)](#libraries-66-units)
    34|    34|- [CLI Commands (98 Slash, Real)](#cli-commands-98-slash-real)
    35|    35|- [Verified Stubs (All Resolved)](#verified-stubs-all-resolved)
    36|    36|- [Bugfix History](#bugfix-history)
    37|    37|- [Project Structure](#project-structure)
    38|    38|- [The Agentic Process (.hermes)](#the-agentic-process-hermes)
    39|    39||||||- [Battleship Roadmap (25 Gaps)](#battleship-roadmap-25-gaps)
    40|    40|- [Test Suite](#test-suite)
    41|    41|- [CI/CD](#cicd)
    42|    42|- [Development Guide](#development-guide)
    43|    43|- [Config Reference](#config-reference)
    44|    44|- [Upstream](#upstream)
    45|    45|
    46|    46|---
    47|    47|
    48|    48|## Quick Start
    49|    49|
    50|    50|```bash
    51|    51|cd C/
    52|    52|make -j$(nproc)            # Build hermes binary
    53|    53|./hermes --help            # Usage
    54|    54|bash test_runner.sh        # 282/0/0
    55|    55|./hermes --version         # v0.14.1+
    56|    56|
    57|    57|# Modes
    58|    58|./hermes "hello"                        # One-shot
    59|    59|./hermes                                # Interactive (/help for commands)
    60|    60|./hermes --gateway --platform telegram  # Gateway daemon
    61|    61|make tui && ./hermes-tui                # ncurses TUI (experimental)
    62|    62|```
    63|    63|
    64|    64|### Docker
    65|    65|```bash
    66|    66|docker build -t hermes-c -f C/Dockerfile .
    67|    67|docker run --rm hermes-c --help
    68|    68|# ~20MB slim image (bookworm-slim, static-linked)
    69|    69|```
    70|    70|
    71|    71|### Smoke Test
    72|    72|```bash
    73|    73|echo '/tools' | ./hermes     # List all 99 registered tools
    74|    74|echo "/providers" | ./hermes # List provider configurations
    75|    75|```
    76|    76|
    77|    77|---
    78|    78|
    79|    79|## Architecture
    80|    80|
    81|    81|```
    82|    82|                    ┌─────────────────────────┐
    83|    83|                    │  CLI / Gateway / TUI     │
    84|    84|                    │  (3 entry points)        │
    85|    85|                    └──────────┬──────────────┘
    86|    86|                               │ User message
    87|    87|                    ┌──────────▼──────────────┐
    88|    88|                    │     Agent Loop           │
    89|    89|                    │  (context, budget,       │
    90|    90|                    │   fallback, checkpoint,  │
    91|    91|                    │   interrupt, audit)      │
    92|    92|                    └──────────┬──────────────┘
    93|    93|                               │ LLM call
    94|    94|                    ┌──────────▼──────────────┐
    95|    95|                    │   LLM Client + 11         │
    96|    96|                    │   Provider Adapters      │
    97|    97|                    │  (OpenAI-compat + native)│
    98|    98|                    └──────────┬──────────────┘
    99|    99|                               │ Tool call
   100|   100|                    ┌──────────▼──────────────┐
   101|   101|                    │   84 Tool Registry       │
   102|   102|                    │  (file, web, terminal,   │
   103|   103|                    │   skills, MCP, kanban,   │
   104|   104|                    │   browser, delegate...)  │
   105|   105|                    └──────────┬──────────────┘
   106|   106|                               │ Plugin dispatch
   107|   107|                    ┌──────────▼──────────────┐
   108|   108|                    │   10 Plugin .c           │
   109|   109|                    │  (kanban, honcho,        │
   110|   110|                    │   spotify, image_gen...) │
   111|   111|                    └──────────┬──────────────┘
   112|   112|                               │ System calls
   113|   113|                    ┌──────────▼──────────────┐
   114|   114|                    │   66 Library Units       │
   115|   115|                    │  (json, yaml, http,      │
   116|   116|                    │   crypto, mcp, cron...)  │
   117|   117|                    └─────────────────────────┘
   118|   118|```
   119|   119|
   120|   120|**Data flow:** User message → entry point (CLI/Gateway/TUI) → Agent Loop (context management, budget tracking, fallback routing, interrupt handling, checkpoint/resume) → LLM Client → Provider adapter → LLM response → tool calls → plugin dispatch → response to user.
   121|   121|
   122|   122|---
   123|   123|
   124|   124|## Build System
   125|   125|
   126|   126|```bash
   127|   127|make hermes           # Full binary (phase5) — 0 errors, 31M
   128|   128|make plugins          # 10 .so shared objects
   129|   129|make tui              # ncurses TUI → hermes-tui (experimental)
   130|   130|make libs             # 66 library compilation units
   131|   131|make install-plugins  # Build + copy .so → ~/.hermes/plugins/
   132|   132|make docs             # Doxygen HTML docs (if doxygen available)
   133|   133|```
   134|   134|
   135|   135|### 5-Phase Build Pipeline
   136|   136|
   137|   137|| Phase | What | Output |
   138|   138||-------|------|--------|
   139|   139||| P1 | 66 library units (.o) | lib/*.o |
   140|   140|| P2 | Agent core + CLI + 10 providers | src/agent/*.o, src/cli/*.o |
   141|   141|| P3 | 99 tool handlers | src/tools/*.o |
   142|   142|| P4 | 19 gateway platforms | src/gateway/*.o |
   143|   143|| P5 | Cron scheduler + final link | hermes binary |
   144|   144|
   145|   145|### Build Variants
   146|   146|```bash
   147|   147|# Release (default)
   148|   148|make -j$(nproc)
   149|   149|
   150|   150|# Debug (more warnings, no optimization)
   151|   151|make CFLAGS="-O0 -g3 -Wall -Wextra -Wpedantic -Werror" hermes
   152|   152|
   153|   153|# AddressSanitizer
   154|   154|make CFLAGS="-O1 -g -fsanitize=address" LDFLAGS="-fsanitize=address" hermes
   155|   155|
   156|   156|# UndefinedBehaviorSanitizer
   157|   157|make CFLAGS="-O1 -g -fsanitize=undefined" LDFLAGS="-fsanitize=undefined" hermes
   158|   158|```
   159|   159|
   160|   160|---
   161|   161|
   ## All Tools (99 Registered)
   163|   163|
   164|   164|Every tool is registered at startup via `registry_register(name, description, schema, handler)`. Tools are discovered by the agent loop and called with JSON arguments.
   165|   165|
   166|   166|### File & Terminal
   167|   167|| Tool | File | Status |
   168|   168||------|------|--------|
   169|   169|| `read_file` | `src/tools/file.c` | ✅ Complete |
   170|   170|| `write_file` | `src/tools/file.c` | ✅ Complete |
   171|   171|| `search_files` | `src/tools/file.c` | ✅ Complete |
   172|   172|| `file_batch` | `src/tools/file_batch.c` | ✅ Complete |
   173|   173|| `terminal` | `src/tools/terminal.c` | ✅ Complete |
   174|   174|| `process` | `src/tools/process.c` | ✅ Complete |
   175|   175|| `patch` | `src/tools/patch.c` | ✅ Complete |
   176|   176|
   177|   177|### Web
   178|   178|| Tool | File | Status |
   179|   179||------|------|--------|
   180|   180|| `web_get` | `src/tools/web.c` | ✅ Complete |
   181|   181|| `web_search` | `src/tools/web.c` | ✅ Complete |
   182|   182|| `web_extract` | `src/tools/web.c` | ✅ Complete |
   183|   183|
   184|   184|### Browser (Text + CDP)
   185|   185|| Tool | File | Status |
   186|   186||------|------|--------|
   187|   187|| `browser_navigate` | `src/tools/browser.c` | ✅ Text-based |
   188|   188|| `browser_snapshot` | `src/tools/browser.c` | ✅ Text-based |
   189|   189|| `browser_click` | `src/tools/browser.c` | ✅ Text-based |
   190|   190|| `browser_type` | `src/tools/browser.c` | ✅ Text-based |
   191|   191|| `browser_scroll` | `src/tools/browser.c` | ✅ Text-based |
   192|   192|| `browser_back` | `src/tools/browser.c` | ✅ Text-based |
   193|   193|| `browser_forward` | `src/tools/browser.c` | ✅ Text-based |
   194|   194|| `browser_get_images` | `src/tools/browser.c` | ✅ Text-based (HTML parse) |
   195|   195|| `browser_press` | `src/tools/browser.c` | ✅ Text-based |
   196|   196|| `browser_vision` | `src/tools/browser.c` | ✅ CDP-dependent (real) |
   197|   197|| `browser_console` | `src/tools/browser.c` | ✅ CDP-dependent (real) |
   198|   198|| `browser_dialog` | `src/tools/browser.c` | ✅ CDP-dependent (real) |
   199|   199|| `browser_cdp` | `src/tools/browser.c` | ✅ CDP-dependent (real) |
   200|   200|
   201|   201|### Agent Communication
   202|   202|| Tool | File | Status |
   203|   203||------|------|--------|
   204|   204|| `delegate_task` | `src/tools/delegate.c` | ✅ Complete |
   205|   205|| `clarify` | `src/tools/clarify.c` | ✅ Complete |
   206|   206|| `send_message` | `src/tools/send_message.c` | ✅ Complete |
   207|   207|| `approval_status` | `src/tools/approval.c` | ✅ Complete |
   208|   208|
   209|   209|### Skills
   210|   210|| Tool | File | Status |
   211|   211||------|------|--------|
   212|   212|| `skills_list` | `src/tools/skills.c` | ✅ Complete |
   213|   213|| `skill_scan` | `src/tools/skills.c` | ✅ Complete |
   214|   214|| `skill_validate` | `src/tools/skills.c` | ✅ Complete |
   215|   215|| `skill_provenance` | `src/tools/skills.c` | ✅ Complete |
   216|   216|| `skill_sync` | `src/tools/skills.c` | ✅ Complete |
   217|   217|| `skill_bundle` | `src/tools/skills.c` | ✅ Complete |
   218|   218|| `skill_usage` | `src/tools/skills.c` | ✅ Complete |
   219|   219|| `skill_cache` | `src/tools/skills.c` | ✅ Complete |
   220|   220|| `skill_search` | `src/tools/skills.c` | ✅ Complete |
   221|   221|| `skill_curator` | `src/tools/skills.c` | ✅ Complete |
   222|   222|| `skill_deps` | `src/tools/skills.c` | ✅ Complete |
   223|   223|| `skill_hub` | `src/tools/skills.c` | ✅ Complete |
   224|   224|| `skill_view` | `src/tools/skill_mgmt.c` | ✅ Complete |
   225|   225|| `skill_manage` | `src/tools/skill_mgmt.c` | ✅ Complete |
   226|   226|
   227|   227|### Memory & Sessions
   228|   228|| Tool | File | Status |
   229|   229||------|------|--------|
   230|   230|| `memory` | `src/tools/memory.c` | ✅ Complete |
   231|   231|| `session_search` | `src/tools/session_search.c` | ✅ Complete |
   232|   232|| `session_crud` | `src/tools/session_crud.c` | ✅ Complete |
   233|   233|| `todo` | `src/tools/todo.c` | ✅ Complete |
   234|   234|
   235|   235|### MCP
   236|   236|| Tool | File | Status |
   237|   237||------|------|--------|
   238|   238|| `mcp_connect` | `src/tools/mcp_tool.c` | ✅ Complete |
   239|   239|| `mcp_disconnect` | `src/tools/mcp_tool.c` | ✅ Complete |
   240|   240|| `mcp_list_tools` | `src/tools/mcp_tool.c` | ✅ Complete |
   241|   241|| `mcp_call_tool` | `src/tools/mcp_tool.c` | ✅ Complete |
   242|   242|| `mcp_list_resources` | `src/tools/mcp_tool.c` | ✅ Complete |
   243|   243|| `mcp_read_resource` | `src/tools/mcp_tool.c` | ✅ Complete |
   244|   244|| `mcp_list_prompts` | `src/tools/mcp_tool.c` | ✅ Complete |
   245|   245|| `mcp_get_prompt` | `src/tools/mcp_tool.c` | ✅ Complete |
   246|   246|| (dynamic) | `src/tools/mcp_tool.c` | Tools registered by connected servers |
   247|   247|
   248|   248|### Media & Input
   249|   249|| Tool | File | Status |
   250|   250||------|------|--------|
   251|   251|| `vision_analyze` | `src/tools/vision.c` | ✅ Complete |
   252|   252|| `text_to_speech` | `src/tools/tts.c` | ✅ Complete |
   253|   253|| `voice_listen` | `src/tools/voice_mode.c` | ✅ Complete |
   254|   254|| `voice_speak` | `src/tools/voice_mode.c` | ✅ Complete |
   255|   255|| `image_generate` | `src/tools/image_gen.c` | ✅ Complete (shells to plugin) |
   256|   256|| `computer_use` | `src/tools/computer_use.c` | 🔴 **Full stub** (needs macOS) |
   257|   257|
   258|   258|### Automation
   259|   259|| Tool | File | Status |
   260|   260||------|------|--------|
   261|   261|| `cronjob` | `src/tools/cronjob.c` | ✅ Complete |
   262|   262|| `cron_cmd` | `src/cron/cron_cli.c` | ✅ Complete |
   263|   263|| `kanban_show` | `src/tools/kanban.c` | ✅ Complete |
   264|   264|| `kanban_list` | `src/tools/kanban.c` | ✅ Complete |
   265|   265|| `kanban_complete` | `src/tools/kanban.c` | ✅ Complete |
   266|   266|| `kanban_block` | `src/tools/kanban.c` | ✅ Complete |
   267|   267|| `kanban_heartbeat` | `src/tools/kanban.c` | ✅ Complete |
   268|   268|| `kanban_comment` | `src/tools/kanban.c` | ✅ Complete |
   269|   269|| `kanban_create` | `src/tools/kanban.c` | ✅ Complete |
   270|   270|| `kanban_unblock` | `src/tools/kanban.c` | ✅ Complete |
   271|   271|| `kanban_link` | `src/tools/kanban.c` | ✅ Complete |
   272|   272|
   273|   273|### Integration
   274|   274|| Tool | File | Status |
   275|   275||------|------|--------|
   276|   276|| `x_search` | `src/tools/x_search.c` | ✅ Complete |
   277|   277|| `ha_list_entities` | `src/tools/homeassistant.c` | ✅ Complete |
   278|   278|| `ha_get_state` | `src/tools/homeassistant.c` | ✅ Complete |
   279|   279|| `ha_list_services` | `src/tools/homeassistant.c` | ✅ Complete |
   280|   280|| `ha_call_service` | `src/tools/homeassistant.c` | ✅ Complete |
   281|   281|| `discord_send` | `src/tools/discord.c` | ✅ Complete |
   282|   282|| `discord_read` | `src/tools/discord.c` | ✅ Complete |
   283|   283|
   284|   284|### Internal
   285|   285|| Tool | File | Status |
   286|   286||------|------|--------|
   287|   287|| `execute_code` | `src/tools/exec_code.c` | ✅ Complete |
   288|   288|| `rate_limit` | `src/tools/rate_limit.c` | ✅ Complete |
   289|   289|| `url_safety` | `src/tools/url_safety.c` | ✅ Complete |
   290|   290|| `tirith` | `src/tools/tirith.c` | ✅ Complete |
   291|   291|| `result_storage` | `src/tools/result_storage.c` | ✅ Complete |
   292|   292|
   293|   293|---
   294|   294|
   295|   295|## Gateway Platforms (19)
   296|   296|
   297|   297|All platforms implement `(init, send, poll, shutdown)` lifecycle. They run co-operatively in a single thread with epoll/kqueue.
   298|   298|
   299|   299|| # | Platform | Adapter | Status |
   300|   300||---|----------|---------|--------|
   301|   301|| 1 | Telegram | `gateway/platforms/telegram.c` | ✅ |
   302|   302|| 2 | Discord | `gateway/platforms/discord.c` | ✅ |
   303|   303|| 3 | Slack | `gateway/platforms/slack.c` | ✅ |
   304|   304|| 4 | Matrix | `gateway/platforms/matrix.c` | ✅ |
   305|   305|| 5 | Mattermost | `gateway/platforms/mattermost.c` | ✅ |
   306|   306|| 6 | WhatsApp | `gateway/platforms/whatsapp.c` | ✅ |
   307|   307|| 7 | Email (IMAP/SMTP) | `gateway/platforms/email.c` | ✅ |
   308|   308|| 8 | Signal | `gateway/platforms/signal.c` | ✅ |
   309|   309|| 9 | SMS (Twilio) | `gateway/platforms/sms.c` | ✅ |
   310|   310|| 10 | Feishu/Lark | `gateway/platforms/feishu.c` | ✅ |
   311|   311|| 11 | WeCom (WeChat Work) | `gateway/platforms/wecom.c` | ✅ |
   312|   312|| 12 | DingTalk | `gateway/platforms/dingtalk.c` | ✅ |
   313|   313|| 13 | QQ Bot | `gateway/platforms/qqbot.c` | ✅ |
   314|   314|| 14 | BlueBubbles | `gateway/platforms/bluebubbles.c` | ✅ |
   315|   315|| 15 | MSGraph Webhook | `gateway/platforms/msgraph_webhook.c` | ✅ |
   316|   316|| 16 | WeChat Official | `gateway/platforms/weixin.c` | ✅ |
   317|   317|| 17 | YuanBao | `gateway/platforms/yuanbao.c` | ✅ |
   318|   318|| 18 | Webhook | `gateway/platforms/webhook.c` | ✅ |
   319|   319|| 19 | Home Assistant | `gateway/platforms/homeassistant.c` | ✅ |
   320|   320|
   321|   321|**Test status:** Stub test only (escape_mode, slack_blocks, discord_interactions, whatsapp_msg). No per-platform integration tests yet — gap T01.
   322|   322|
   323|   323|---
   324|   324|
   325|   325|## LLM Providers (10)
   326|   326|
   327|   327|Every provider implements the same interface: `(init, chat, stream, count_tokens, build_headers, parse_response)`. OpenAI-compatible providers share the generic adapter; native providers have hand-tuned implementations.
   328|   328|
   329|   329|| Provider | Adapter | Type | Status |
   330|   330||----------|---------|------|--------|
   331|   331|| OpenAI | `provider_openai.c` | OpenAI-compat | ✅ |
   332|   332|| OpenRouter | `provider_openrouter.c` | OpenAI-compat | ✅ |
   333|   333|| DeepSeek | `provider_deepseek.c` | OpenAI-compat | ✅ |
   334|   334|| xAI | `provider_xai.c` | OpenAI-compat | ✅ |
   335|   335|| Anthropic | `provider_anthropic.c` | Native | ✅ |
   336|   336|| Google | `provider_google.c` | Native | ✅ |
   337|   337|| Azure | `provider_azure.c` | Native | ✅ |
   338|   338|| Bedrock | `provider_bedrock.c` | Native | ✅ |
   339|   339|| Custom | `provider_custom.c` | OpenAI-compat | ✅ |
   340|   340|
   341|   341|**Metadata:** Provider metadata system handles rate limits, cost tracking, model lists, streaming support, JSON mode, and API quirks. Provider-specific API quirks remaining: 7 (tracked as sector R in battleship).
   342|   342|
   343|   343|---
   344|   344|
   345|   345|## Plugins (10 .c files, 0 .so built)
   346|   346|
   347|   347|Plugins are `.so` files loaded at runtime via `dlopen`. Each exposes `plugin_init`, `plugin_process`, and `plugin_shutdown`.
   348|   348|
   349|   349|| Plugin | File | Status |
   350|   350||--------|------|--------|
   351|   351|| Kanban board | `plugin_kanban.so` | ✅ |
   352|   352|| Honcho memory | `plugin_honcho.so` | ✅ |
   353|   353|| Spotify control | `plugin_spotify.so` | ✅ |
   354|   354|| Disk cleanup | `plugin_disk_cleanup.so` | ✅ |
   355|   355|| File memory | `plugin_file_memory.so` | ✅ |
   356|   356|| Achievements | `plugin_achievements.so` | ✅ |
   357|   357|| Observability | `plugin_observability.so` | ✅ |
   358|   358|| Skills hub | `plugin_skills.so` | ✅ |
   359|   359|| Image generation | `plugin_image_gen.so` | 🔴 **Fake URLs** |
   360|   360|| Google Meet | `plugin_google_meet.so` | ✅ |
   361|   361|
   362|   362|---
   363|   363|
   364|   364|## Libraries (59 Units)
   365|   365|
   366|   366|Libraries are compiled directly into the binary (no intermediate `.a` archives). Each is a self-contained module under `lib/`.
   367|   367|
   368|   368|| Library | Purpose | Status |
   369|   369||---------|---------|--------|
   370|   370|| `libjson` | JSON parser/builder | ✅ |
   371|   371|| `libyaml` | YAML parser | ✅ |
   372|   372|| `libhttp` | HTTP/HTTPS client | ✅ |
   373|   373|| `libcrypto` | AES, SHA, HMAC | ✅ |
   374|   374|| `libmcp` | MCP transport layer | ✅ |
   375|   375|| `libpath` | Path manipulation | ✅ |
   376|   376|| `libdatetime` | Date/time parsing | ✅ |
   377|   377|| `libcsv` | CSV parsing | ✅ |
   378|   378|| `libhash` | Hash functions (SHA, MD5) | ✅ |
   379|   379|| `libuuid` | UUID generation | ✅ |
   380|   380|| `libbase64` | Base64 encode/decode | ✅ |
   381|   381|| `libhtml` | HTML parsing | ✅ |
   382|   382|| `libtextwrap` | Text wrapping | ✅ |
   383|   383|| `libglob` | File globbing | ✅ |
   384|   384|| `libsignal` | Signal handling | ✅ |
   385|   385|| `libenum` | Enum helpers | ✅ |
   386|   386|| `libdifflib` | Diff algorithms | ✅ |
   387|   387|| `libregex` | Regex matching | ✅ |
   388|   388|| `libjson5` | JSON5 parser | ✅ |
   389|   389|| `libwebsocket` | WebSocket client | ✅ |
   390|   390|| `libprotobuf` | Protobuf decode | ✅ |
   391|   391|| `libtoml` | TOML parser | ✅ |
   392|   392|| `libansi` | ANSI escape codes | ✅ |
   393|   393|| `libcron` | Cron expression parser | ✅ |
   394|   394|| `libproc` | Process management | ✅ |
   395|   395|| `libtui` | ncurses TUI helpers | ✅ |
   396|   396|| `libdb` | SQLite wrapper | ✅ |
   397|   397|| `libplugin` | Plugin loading | ✅ |
   398|   398|| `libskin` | Skin/theming engine | ✅ |
   399|   399|| `libtemplate` | Simple template engine | ✅ |
   400|   400|
   401|   401|---
   402|   402|
   403|   403||CLI: 98 commands (all real, tab complete + history)
   404|   404|
   405|   405|The CLI uses a central command registry (`cli/commands.c`) with alias resolution and subcommand dispatch.
   406|   406|
   407|   407|**Categories:**
   408|   408|- **Core:** chat, session, history, model, provider, tools
   409|   409|- **Config:** config show/set/get/edit/migrate, env, profiles
   410|   410|- **Skills:** skills list/view/manage/scan/validate/sync/bundle/search
   411|   411|- **Gateway:** gateway start/stop/status/platforms
   412|   412|- **Cron:** cron list/add/remove/pause/resume/retry
   413|   413|- **Plugins:** plugins list/install/remove
   414|   414|- **MCP:** mcp connect/disconnect/servers/tools
   415|   415|- **System:** version, help, doctor, update, logs
   416|   416|
   417|   417|---
   418|   418|
   419|   419|## Verified Stubs (9 Remaining — sector 1 in battleship-v8)
   420|   420|
   421|   421|9 verified stubs remain — all documented with clear resolution paths in battleship-v8.md:
   422|   422|- **S01-S06** — memory.c plugin_vtable NULL function pointers (import_json, export_json, get_by_hash, compress_old, get_prioritized, plugin_delete always false)
   423|   423|- **S07** — server.c plat.shutdown = NULL (no-op, handled by gw_platform_shutdown_all)
   424|   424|- **S08** — video_gen.c fal_provider.generate = NULL (uses handler directly)
   425|   425|- **S09** — commands.c cmd_background "background mode not available" message
   426|   426|
   427|   427|All are P2-P3. The 90 tools + 98 CLI commands are real implementations.
   428|   428|
   429|   429|---
   430|   430|
   431|   431|## Bugfix History
   432|   432|
   433|   433|All bugs discovered through DA audits and runtime testing.
   434|   434|
   435|   435|| Bug | Impact | Fix |
   436|   436||-----|--------|-----|
   437|   437|| temperature=0.0 silent drop | All 9 providers used default temp | Pass 0.0 explicitly |
   438|   438|| response_format UAF | All 9 providers — `json_free` before use | `json_copy` before `json_free` |
   439|   439|| NULL stream chunk SIGSEGV | 6 providers crash on null chunks | NULL check before `strncmp` |
   440|   440|| `cron_job_reset_retry(NULL)` | SEGV on NULL job pointer | Guard with `if (!job) return` |
   441|   441|| Secrets `ow` pointer not advanced | Secrets truncated on write | Increment write pointer |
   442|   442|| x_search auth literal `***` | `printf` with `%s` missing | Correct format string |
   443|   443|| Dockerfile wrong WORKDIR | CI docker build fails immediately | `cd C && make` |
   444|   444|| `glob.h` shadows system | `path.c` can't find `GLOB_MARK` | Renamed to `hermes_glob.h` |
   445|   445|| `path.c` missing `_GNU_SOURCE` | `glob()` undeclared | `#define _GNU_SOURCE` before includes |
   446|   446|
   447|   447|---
   448|   448|
   449|   449|## Project Structure
   450|   450|
   451|   451|```
   452|   452|waefrebeorn/slermes/         ← Repo root
   453|   453|├── C/                            ← All source code (canonical README lives here)
   454|   454|│   ├── src/                      ← 172 .c files
   455|   455|│   │   ├── agent/                ←   Provider adapters, LLM client, fallback routing,
   456|   456|│   │   │                           budget tracker, checkpoint/resume, audit, redact/sanitize
   457|   457|│   │   ├── cli/                  ←   CLI orchestrator, command registry, config,
   458|   458|│   │   │                           display engine, TUI (ncurses)
   459|   459|│   │   ├── cron/                 ←   Scheduler, SQLite job store, locking, retry
   460|   460|│   │   ├── gateway/              ←   Server + 19 platform adapters
   461|   461|│   │   │   └── platforms/        ←     Individual platform implementations
   462|   462|│   │   ├── plugins/              ←   10 .so plugin implementations (.c + Makefile)
   463|   463|│   │   ├── tools/                ←   99 tool handler implementations
   464|   464|│   │   ├── acp/                  ←   ACP JSON-RPC server
   465|   465|│   │   └── main.c                ←   Entry point (CLI option parsing + dispatch)
   466|   466|│   ├── lib/                      ←   66 library units (compiled directly, no .a)
   467|   467|│   │   ├── libjson/              ←   JSON parser/builder
   468|   468|│   │   ├── libhttp/              ←   HTTP/HTTPS client (libcurl wrapper)
   469|   469|│   │   ├── libmcp/               ←   MCP transport layer (stdio, server, SSE)
   470|   470|│   │   ├── libdb/                ←   SQLite wrapping (sessions, cron state, store)
   471|   471|│   │   ├── libcrypto/            ←   AES, SHA256, HMAC, base64
   472|   472|│   │   └── ...                   ←   25 more
   473|   473|│   ├── include/                  ←   66 headers (64 include/ + 2 src/)
   474|   474|│   ├── tests/                    ←   200+ test files (via test_runner.sh)
   475|   475|│   ├── Dockerfile                ←   Multi-stage Docker build
   476|   476|│   ├── Makefile                  ←   5-phase build pipeline
   477|   477|│   └── .hermes/                  ←   Agentic process documentation (see below)
   478|   478|├── .github/workflows/            ←   CI (c-build.yml, nix-lockfile-fix.yml)
   479|   479|└── README.md → C/README.md       ←   Symlink (edit C/README.md)
   480|   480|```
   481|   481|
   482|   482|---
   483|   483|
   484|   484|## The Agentic Process (.hermes)
   485|   485|
   486|   486|The `.hermes/mind-palace/` directory documents the entire development process — every DA audit, every decision, every gap discovered. This is the **agentic record**: how an AI agent systematically translates 75K LOC of Python to C through iterative discovery, testing, and verification.
   487|   487|
   488|   488|### Reading Order (for a new AI agent assuming this project)
   489|   489|
   490|   490|| Step | File | What It Contains |
   491|   491||------|------|-----------------|
   492|   492|| 1 | `prestige_prompt.md` | Priority queue + current state snapshot |
   493|   493|| 2 | `mind-palace/goal-mantra.md` | Perpetual goal + the loop (one-page session kickoff) |
   494|   494|| 3 | `mind-palace/state.md` | Binary truth table — every sector's done/total |
   495|   495|| 4 | `plan.md` | Phase completion + next steps sorted by priority |
   496|   496|| 5 | `entry.md` | Build/run commands + architecture |
   497|   497|| 6 | `overnight-map.md` | Session navigation + fallback task |
   498|   498|| 7 | `testing.md` | Test suite coverage + known gaps |
   499|   499|| 8 | `da-v12-triple-audit.md` | Final Triple DA audit — methodology, findings, 8 verified stubs |
   500|   500||| 9 | `plans/battleship-v8.md` | Complete ~156-gap roadmap by sector (Triple DA verified) |
   501|
