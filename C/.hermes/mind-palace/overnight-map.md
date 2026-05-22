# Slermes C — Overnight Map (May 21+ session — 5 commits)

## What Changed This Session
- **P15: Config validation** — extended to all 14 config groups. Type/range/enum checks for browser, memory, compression, cron, notification, plugin, MCP + existing groups.
- **P19: Config hot-reload** — SIGHUP handler. `hermes_config_setup_reload()` + `hermes_config_check_reload()` wired into CLI startup + main loop.
- **P22: Config merge logic** — full field-level merge for ALL config groups (browser viewport/js, memory sub-fields, cron, notification, plugin, security/session/MCP expanded).
- **P168: File sandbox** — `sandbox_init()` call added to `tools_init_all()`. HOME/tmp/SLERMES_HOME directories allowed. Symlink attack protection active.
- **YAML parser gap-fill** — ~16 struct fields now populated from config.yaml: plugin.dirs/enabled, session auto_save_interval/compress/store_trajectories, MCP max_tools/credential_store, memory ttl_days/auto_save/compression_enabled/search_limit/auto_save_interval/dedup/storage_type/storage_path, browser.javascript, cron.notify_on_failure, notification.sound.
- DA audit updated: P15/P19/P22/P13/P168 all ✅ now. Config group ~85%.

## What's Next (Provider Expansion)
- **Active phase: P71-P85 (Providers)** — 26 providers missing (10%, 3/29). Biggest gap by count.
- Config P1-P25 all 25 phases now structurally complete (~85%, ~150 leaf keys remain).
- Priorities: OpenRouter, Groq, Together, Bedrock, Azure, xAI native implementations.
- Provider pattern: provider_openai.c / provider_anthropic.c / provider_google.c implement provider_ops_t vtable.
- P72 (Provider plugin .so system) also ❌.

## Pending
- Providers: 3/29. 26 missing. Most critical: OpenRouter, Groq, Together.
- Tools: 14 missing (feishu doc+drive (5), MoA (1), video analyze+gen (2), yuanbao (6)).
- MCP: namespace/filter/sampling/roots (P65, P66, P68, P70).
- TUI: 6/12 phases (P193-P200).
- Tests: <1% — critical gap but deferred until features stabilize.
