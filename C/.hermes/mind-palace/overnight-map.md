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
| **P1** | Tool sub-features (36) | 6 |

## Build & Test

```bash
make -j$(nproc) && bash test_runner.sh --verbose
# Expect: 58/59 pass (pre-existing plugin failure)
```

## Known Issues
- temperature=0.0 silently dropped — fix first
- Plugins at 8% is the biggest structural gap (45 backends to port)
- 25 provider-specific API quirks entirely missed before this audit
- Browser/memory/kanban tool handlers verified 1:1 ✅

## Upstream
- 125 commits since last sync, 52 Python
- 12 new feature gaps identified
- 75% of upstream work is TUI/computer_use/skills — C stubs skip those
