# Overnight Map (v6) — 2026-05-22

```
~50% complete  •  ~400 gaps remaining  •  125 commits behind upstream
```

## Active Workstreams

| Stream | What | Sessions |
|--------|------|----------|
| **P0a** | Fix temperature=0.0 bug (s/>= 0.0f) | 10 min |
| **P0b** | B04-B05: response_format + metadata | 1 |
| **P0c** | F01-F08: 8 tool stubs → real | 3 |
| **P0d** | B01-B03: 3 ACP providers | 4 |
| **P1** | Gateway depth (34 gaps) | 8 |
| **P1** | Agent state + session (32) | 3 |
| **P1** | CLI feature depth (34) | 4 |
| **P1** | Tool sub-features (31) | 5 |
| **P2** | Plugin depth (51) | 15 |

## Build & Test

```bash
make -j$(nproc) && bash test_runner.sh --verbose
# Expect: 58/59 pass (pre-existing plugin failure)
```

## Known Issues
- Plugins at 8% is the biggest structural gap (45 backends to port)
- 25 provider-specific API quirks entirely missed before this audit
- F05-F07: computer_use (WSL), memory_sqlite (needs libsqlite3), memory_plugin (needs plugin system) still stubs
- Browser/memory/kanban tool handlers verified 1:1 ✅
- **Fixed:** temperature=0.0 dropped (s/> 0.0f/>= 0.0f/ × 9 providers)
- **Fixed:** streaming path missing config forwarding (16 params now forwarded)

### Session 2026-05-22
- ✅ P0 #1: temp=0.0 fix
- ✅ P0 #2: response_format + metadata 
- ✅ P0 #5: tool_choice + parallel_tool_calls
- ✅ P0 #6: max_tool_calls + n (choices)
- ✅ Fixed streaming config forwarding gap
- ✅ **18/18 LLM params fully wired**
- ✅ P0 #3: F08 vision description — real Python Hermes CLI delegation
- ✅ E01-E05: Telegram sendPhoto/Document/Voice/Video/Animation
- ✅ E14: Telegram forwardMessage
- ✅ E15: Telegram pinChatMessage + unpinChatMessage
- ✅ F41: Image format validation (extension check + 50MB limit)
- ✅ B01: ACP server Sessions 2-4 — full lifecycle + auth + streaming
- ✅ B02-B03: Covered by B01 ACP server infrastructure
- ✅ E16: Telegram message reactions (setMessageReaction API)
- ✅ **G15+G16: enabled_toolsets + disabled_toolsets** — toolset field in tool_t, registry_register_ex, registry_filter_by_toolset, tools.enabled_toolsets/disabled_toolsets YAML keys, agent state wiring, toolset labels for 34 key tools
- ✅ **G17: system_message override** — per-conversation override field in agent_state_t, wired in agent_run_conversation before each LLM call
- ✅ **G19: thread/user/chat IDs** — routing metadata fields in agent_state_t
- ✅ **G01-G03: session token tracking** — session_total/input/output_tokens counters, updated after each LLM response
- ✅ **G04-G12: deep token tracking** — reasoning/cache/cost counters, user/tool turn counts, last_activity_ts, pending_steer, interrupt_message on fatal tool errors; /status shows all fields
- Next: G13-G14 tool_choice/parallel_tool_calls state, G18 conversation_history injection, G20 model_family, or gateway E06-E12 interactive sends, or upstream sync L01-L12

### Session 2026-05-22 (Part 2)
- ✅ **G21-G36 (16 gaps):** Compression depth, iteration budget, checkpoint depth, prefill variants, steer queue, typed interrupt + partial results
- ◀ **Agent loop now at ~85% parity (G01-G36 all filled)** — 32/32 agent state gaps closed

### Session 2026-05-22 (Part 3)
- ✅ **E27-E34 (8 gaps):** Gateway infrastructure — keepalive per-platform, message dedup (64-entry TTL ring buffer), batch aggregation (2s coalesce), markdown stripping (code/bold/italic/headers), per-platform cooldown, reconnect backoff (exponential 2^attempt, ±10% jitter, 60s cap), proxy per-platform, group observe prefix
- ◀ **Gateway now at ~48% parity** (30/63 gaps closed)
- ◀ Committed: `d5e5109db`

### Session 2026-05-22 (Part 6+)
- ✅ **F11: Docker execution backend** — temp script approach, config-driven (docker_image, volumes, env forwarding, host user mapping, extra args), per-call image override
- ✅ **F34-F36: Process sub-features** — signal sending by name/number, env overrides on start, per-process timeout with auto-kill
- ✅ **F26+F28+F29: Cron job enhancements** — schedule validation (cron_parse at add-time), per-job notifications (notify_on_complete/notify_on_failure), retry with exponential backoff (max_retries + backoff_sec config)
- ✅ **F09+F10+F12** — PTY mode, env isolation, timeout propagation (marked in roadmap)
- ✅ **F16-F20** — Web search backends (searxng, google, brave, tavily, firecrawl) marked in roadmap
- ✅ **F30-F33** — Memory tool ops (save/search/delete/list) marked in roadmap
- ◀ **Tools now at ~90% parity** (31 remaining gaps)
- ◀ Roadmap gap count: ~387 total (~13 closed)
- ◀ Commit: `76bf19925 (F11 Docker execution backend)`

## Upstream
- 125 commits since last sync, 52 Python
- 12 new feature gaps identified
- 75% of upstream work is TUI/computer_use/skills — C stubs skip those
