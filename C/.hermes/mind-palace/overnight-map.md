# Slermes C — Overnight Map (May 23 session)

## Wake-up Instructions
Read state.md → goal-mantra.md → plan.md → prestige_prompt.md → plans/200-phase-roadmap.md.
DO NOT re-read overnight-map.md — it's stale by design.

## What Changed This Session
- **P21: hermes_constants port** — new src/cli/paths.c with hermes_get_home(), XDG dirs, hermes_display_home(), hermes_resolve_path(), hermes_set/get_profile()
- **P22: deep merge** — hermes_config_merge() for config layering
- Config module now ~30% (130/424 keys)
- Commits: a4036dbd9 (P21), d5ef19294 (P22)

## What's Next
- P19: inotify config watch (Linux-specific, deferred)
- P23-P25: categories, schema, migration (deferred)
- **P26+: CLI commands** — /new, /clear, /save/load, /stats, /config, /model, etc.
