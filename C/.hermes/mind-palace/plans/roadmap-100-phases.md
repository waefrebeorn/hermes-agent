# Hermes C — 100+ Phase Implementation Roadmap (May 21 AM)

## Purpose
Phase-by-phase plan to reach 1:1 parity with Python Hermes Agent. Each phase is a self-contained unit: implement → compile → test → verify.

## Legend
P0=Critical P1=High P2=Medium P3=Low ✅=Done ⚡=In Progress

---

### FOUNDATION (Phase 1-10) — DONE ✅

| Ph | Area | What | Status | LOC |
|----|------|------|--------|-----|
| 1 | Libs | libjson — JSON parser/serializer | ✅ | ~1,200 |
| 2 | Libs | libhttp — HTTP/HTTPS client (SSL, retry, SSE) | ✅ | ~2,500 |
| 3 | Libs | libyaml — YAML subset parser | ✅ | ~800 |
| 4 | Libs | libcrypto — SHA256, base64 | ✅ | ~300 |
| 5 | Libs | libdotenv — .env file reader | ✅ | ~150 |
| 6 | Libs | libproc — process management (signals, wait) | ✅ | ~200 |
| 7 | Libs | libcron — cron expression parser | ✅ | ~250 |
| 8 | Libs | libtemplate — template engine | ✅ | ~300 |
| 9 | Libs | libtui — terminal I/O (colors, progress, boxes) | ✅ | ~400 |
| 10 | Libs | libdb — JSON file session store | ✅ | ~400 |

### AGENT LOOP (Phase 11-20) — DONE ✅

| Ph | Area | What | Status | LOC |
|----|------|------|--------|-----|
| 11 | Agent | Main entry point + arg parsing | ✅ | ~250 |
| 12 | Agent | LLM client (chat completions, non-stream) | ✅ | ~300 |
| 13 | Agent | LLM client (streaming) | ✅ | ~200 |
| 14 | Agent | Context window management | ✅ | ~200 |
| 15 | Agent | Title generation | ✅ | ~50 |
| 16 | Agent | Agent loop (chat → tool call → chat) | ✅ | ~400 |
| 17 | Agent | Message types + memory management | ✅ | ~200 |
| 18 | Agent | CLI orchestrator (hermes_cli_main) | ✅ | ~300 |
| 19 | Agent | Config loading (config.yaml + .env + env vars) | ✅ | ~200 |
| 20 | Agent | Display helpers (banner, colors, streaming output) | ✅ | ~300 |

### TOOLS CORE (Phase 21-30) — DONE ✅

| Ph | Area | What | Status | LOC |
|----|------|------|--------|-----|
| 21 | Tools | Tool registry system | ✅ | ~200 |
| 22 | Tools | terminal — shell command execution | ✅ | ~200 |
| 23 | Tools | file — read/write/search files | ✅ | ~300 |
| 24 | Tools | web — HTTP requests + web search | ✅ | ~200 |
| 25 | Tools | patch — find-and-replace text edits | ✅ | ~200 |
| 26 | Tools | exec_code — Python code execution | ✅ | ~200 |
| 27 | Tools | clarify — ask user questions | ✅ | ~150 |
| 28 | Tools | memory — persistent key-value store | ✅ | ~200 |
| 29 | Tools | todo — task list management | ✅ | ~200 |
| 30 | Tools | process — background process management | ✅ | ~300 |

### TOOLS ADVANCED (Phase 31-40) — DONE ✅

| Ph | Area | What | Status | LOC |
|----|------|------|--------|-----|
| 31 | Tools | send_message — platform message sending | ✅ | ~250 |
| 32 | Tools | cronjob — cron job scheduling | ✅ | ~200 |
| 33 | Tools | skill_mgmt — skill view/manage | ✅ | ~200 |
| 34 | Tools | session_search — FTS session search | ✅ | ~200 |
| 35 | Tools | tts — text-to-speech | ✅ | ~150 |
| 36 | Tools | vision — image analysis | ✅ | ~200 |
| 37 | Tools | delegate — subagent spawning | ✅ | ~200 |
| 38 | Tools | x_search — X/Twitter search | ✅ | ~200 |
| 39 | Tools | skills — skill listing/loading | ✅ | ~150 |
| 40 | Tools | registry + tool_init — tool registration | ✅ | ~200 |

### GATEWAY CORE (Phase 41-50) — DONE ✅

| Ph | Area | What | Status | LOC |
|----|------|------|--------|-----|
| 41 | Gateway | Gateway server (multi-platform dispatcher) | ✅ | ~400 |
| 42 | Gateway | Telegram platform (long-polling) | ✅ | ~200 |
| 43 | Gateway | Discord platform (REST polling) | ✅ | ~250 |
| 44 | Gateway | Webhook HTTP API server | ✅ | ~500 |
| 45 | Gateway | Slack platform (REST polling) | ✅ | ~250 |
| 46 | Gateway | Matrix platform (REST polling) | ✅ | ~300 |
| 47 | Gateway | Mattermost platform (REST polling) | ✅ | ~250 |
| 48 | Gateway | WhatsApp platform (basic) | ✅ | ~300 |
| 49 | Gateway | Plugin system (dlopen loader) | ✅ | ~300 |
| 50 | Gateway | Skin engine (JSON theming) + cron scheduler + ncurses TUI | ✅ | ~600 |

---

### PHASE 51-60: CLI PARITY [P0] — 16 commands → 50+

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 51 | CLI | /new — new session | ✅ 50 | Session DB |
| 52 | CLI | /reset — reset context | ✅ 50 | Agent loop |
| 53 | CLI | /undo — undo last turn | ✅ 100 | Message history |
| 54 | CLI | /retry — retry last turn | 100 | Agent loop |
| 55 | CLI | /history — show full history | 100 | Session DB |
| 56 | CLI | /topic — set conversation topic | 50 | Context |
| 57 | CLI | /compress — compress context | 200 | LLM client |
| 58 | CLI | /branch — branch conversation | 100 | Session DB |
| 59 | CLI | /snapshot — save checkpoint | 100 | Session DB |
| 60 | CLI | /handoff — handoff to another agent | 200 | Delegate |

### PHASE 61-70: CLI CONFIG COMMANDS [P0]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 61 | CLI | /config — show/edit config | 200 | Config |
| 62 | CLI | /model — full model management | 100 | LLM client |
| 63 | CLI | /personality — set system prompt | 100 | Context |
| 64 | CLI | /tools — list/enable/disable tools | 100 | Tool registry |
| 65 | CLI | /toolsets — manage tool sets | 200 | Tool registry |
| 66 | CLI | /skills — skill management UI | 100 | Skills |
| 67 | CLI | /plugins — plugin management | 100 | Plugin system |
| 68 | CLI | /cron — cron job management UI | 100 | Cron |
| 69 | CLI | /verbose — toggle verbose mode | 50 | Display |
| 70 | CLI | /yolo — toggle YOLO mode | 50 | Agent config |

### PHASE 71-80: CLI SUBCOMMANDS [P0]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 71 | CLI | hermes doctor — system health check | 300 | All subsystems |
| 72 | CLI | hermes status — runtime status | 200 | Agent state |
| 73 | CLI | hermes logs — log viewer | 300 | Logging |
| 74 | CLI | hermes setup — interactive setup wizard | 500 | Config |
| 75 | CLI | hermes profiles — profile management | 300 | Config |
| 76 | CLI | hermes config — CLI config editor | 400 | Config |
| 77 | CLI | hermes models — model/provider management | 400 | Provider system |
| 78 | CLI | hermes tools — tool listing CLI | 200 | Tool registry |
| 79 | CLI | hermes backup — backup/restore | 300 | Session DB, config |
| 80 | CLI | hermes update — self-update | 200 | Git |

### PHASE 81-90: BROWSER AUTOMATION [P0]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 81 | Tools | browser_navigate — navigate to URL | ✅ 300 | HTTP client |
| 82 | Tools | browser_snapshot — capture page | ✅ 400 | HTTP, HTML parse |
| 83 | Tools | browser_click — click element | 200 | Browser state |
| 84 | Tools | browser_type — type text | 200 | Browser state |
| 85 | Tools | browser_scroll — scroll page | 150 | Browser state |
| 86 | Tools | browser_back/forward — history nav | ✅ 100 | Browser state |
| 87 | Tools | browser_get_images — extract images | 200 | HTML parse |
| 88 | Tools | browser_vision — Vision on page | 200 | Vision tool |
| 89 | Tools | browser_console — JS console | 300 | CDP/JS eval |
| 90 | Tools | browser_cdp — Chrome DevTools Protocol | 500 | WebSocket |

### PHASE 91-100: SECURITY [P0]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 91 | Tools | dangerous command approval — approve/deny dangerous cmds | 500 | Gateway |
| 92 | Tools | path traversal protection — validate file paths | 200 | File tools |
| 93 | Tools | URL safety validation — SSRF protection | 300 | Web tool |
| 94 | Tools | Tirith security scanning — pre-exec security scan | 500 | Terminal tool |
| 95 | Tools | credential file management — secure key storage | 300 | Config |
| 96 | Tools | schema sanitization — validate tool call args | 300 | Tool registry |
| 97 | Tools | budget config — token/iteration budgets | 200 | Agent loop |
| 98 | Gateway | /approve, /deny slash commands | 200 | Gateway |
| 99 | Gateway | gateway authorization (allowed users, tokens) | 300 | Gateway |
| 100 | Security | OSV vulnerability checking | 200 | Web tool |

### PHASE 101-110: PROVIDER SUPPORT [P0]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 101 | Agent | Abstract provider interface | 300 | LLM client |
| 102 | Agent | OpenAI provider adapter | 200 | Provider interface |
| 103 | Agent | Anthropic provider adapter | 300 | Provider interface |
| 104 | Agent | DeepSeek provider adapter | 100 | Provider interface |
| 105 | Agent | OpenRouter provider adapter | 100 | Provider interface |
| 106 | Agent | Google/Gemini provider adapter | 200 | Provider interface |
| 107 | Agent | AWS Bedrock provider adapter | 300 | Provider interface |
| 108 | Agent | Azure OpenAI provider adapter | 200 | Provider interface |
| 109 | Agent | Provider config (model list, pricing, features) | 500 | Config |
| 110 | Agent | Provider auto-detection + fallback chains | 300 | Provider interface |

### PHASE 111-120: GATEWAY PLATFORMS [P1]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 111 | Gateway | Signal platform | 300 | Gateway |
| 112 | Gateway | Email (IMAP/SMTP) platform | 400 | Gateway |
| 113 | Gateway | SMS (Twilio) platform | 300 | Gateway |
| 114 | Gateway | HomeAssistant platform | 400 | Gateway |
| 115 | Gateway | DingTalk platform | 300 | Gateway |
| 116 | Gateway | WeCom platform | 400 | Gateway |
| 117 | Gateway | Feishu/Lark platform | 400 | Gateway |
| 118 | Gateway | BlueBubbles (iMessage) platform | 300 | Gateway |
| 119 | Gateway | Yuanbao platform | 500 | Gateway |
| 120 | Gateway | QQ Bot platform | 300 | Gateway |

### PHASE 121-130: GATEWAY ADVANCED [P1]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 121 | Gateway | Multi-platform simultaneous operation | 500 | Gateway platforms |
| 122 | Gateway | Gateway session lifecycle (topics, threads) | 500 | Session DB |
| 123 | Gateway | Streaming responses to platforms | 300 | LLM streaming |
| 124 | Gateway | Gateway health monitoring + auto-recovery | 300 | Gateway |
| 125 | Gateway | Platform pause/resume | 200 | Gateway |
| 126 | Gateway | Gateway hooks system | 300 | Plugin system |
| 127 | Gateway | Gateway plugin platforms (IRC, Line, etc.) | 500 | Plugin system |
| 128 | Gateway | Gateway webhook auth (signature verification) | 200 | Gateway |
| 129 | Gateway | Gateway rate limiting per platform | 300 | Gateway |
| 130 | Gateway | Gateway admin UI (web dashboard) | 1,000 | Web server |

### PHASE 131-140: TOOLS [P1]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 131 | Tools | kanban board create/read/update/delete | 500 | DB |
| 132 | Tools | kanban lane/card management | 500 | Kanban core |
| 133 | Tools | kanban assign/prioritize | 300 | Kanban core |
| 134 | Tools | kanban workflow automation | 500 | Kanban core |
| 135 | Tools | kanban dashboard/report | 300 | Kanban core |
| 136 | Tools | spotify play/search/queue | 500 | HTTP client |
| 137 | Tools | spotify playlist/library mgmt | 300 | Spotify |
| 138 | Tools | transcription (Whisper) | 500 | Audio I/O |
| 139 | Tools | image generation (multi-provider) | 500 | HTTP client |
| 140 | Tools | MCP client integration | 500 | Tool registry |

### PHASE 141-150: ADVANCED FEATURES [P1-P2]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 141 | Tools | Skills hub/sync (community skills) | 500 | Skills |
| 142 | Tools | Skills guard/provenance (security audit) | 400 | Skills |
| 143 | Tools | File checkpoint/rollback manager | 800 | File tools |
| 144 | Tools | Tool output limits + truncation | 300 | Tool registry |
| 145 | Tools | Tool result storage | 300 | DB |
| 146 | Tools | Terminal backends (docker, ssh, modal) | 1,500 | Terminal tool |
| 147 | Agent | Context compression (summary-based) | 500 | LLM client |
| 148 | Agent | Checkpoint/resume across restarts | 500 | Session DB |
| 149 | Agent | Iteration budget + budget grace call | 200 | Agent loop |
| 150 | Agent | Service tier framework | 300 | Provider |

### PHASE 151-160: TESTING & QA [P2]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 151 | Tests | Test harness expansion (21→100+ tests) | 2,000 | All features |
| 152 | Tests | API mock testing (no live key needed) | 1,000 | HTTP client |
| 153 | Tests | Integration test suite (live API) | 1,000 | All features |
| 154 | Tests | CI pipeline (GitHub Actions) | 200 | Tests |
| 155 | Tests | Fuzz testing for parsers | 500 | libjson, libyaml |
| 156 | Tests | Memory leak detection (valgrind) | 100 | All features |
| 157 | Tests | Performance benchmarks | 500 | All features |
| 158 | Tests | Security penetration tests | 500 | Security tools |
| 159 | Tests | Cross-platform build tests (Linux, macOS, WSL) | 200 | Makefile |
| 160 | Tests | Golden output regression tests | 500 | All features |

### PHASE 161-170: DEVELOPER EXPERIENCE [P2-P3]

| Ph | Area | What | Est LOC | Depends On |
|----|------|------|---------|------------|
| 161 | Docs | Docusaurus documentation site | 5,000 | All features |
| 162 | Docs | API reference (auto-generated) | 500 | All features |
| 163 | Docs | CLI man page | 500 | CLI |
| 164 | Packaging | Nix flake | 100 | Build |
| 165 | Packaging | Docker image | 50 | Build |
| 166 | Packaging | APT/Homebrew packages | 100 | Build |
| 167 | Tooling | Digestion automation (digest.py) | 500 | Git |
| 168 | Tooling | Code generation from Python stubs | 1,000 | Template engine |
| 169 | Tooling | Hot-reload development mode | 500 | Plugin system |
| 170 | Tooling | Telemetry/monitoring | 500 | All features |

## Summary: 100+ phases, ~55,000+ new LOC, ~165 gaps closed
P0 phases: 30 (51-80) — CLI, Browser, Security, Providers
P1 phases: 40 (81-120) — Gateways, Tools, Kanban, Spotify, MCP
P2 phases: 30 (121-150) — Advanced features, Testing, CI
P3 phases: 10 (151-160) — DX, Docs, Packaging
