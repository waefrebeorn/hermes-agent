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
- ✅ Fixed streaming config forwarding gap
- 🔄 P0 #3: F08 vision description → Python delegation (partial)
- Next: P0 #3 remaining (F05-F07 stubs), B08-B09 (max_tool_calls, n), or B01-B03 (ACP providers)

## Upstream
- 125 commits since last sync, 52 Python
- 12 new feature gaps identified
- 75% of upstream work is TUI/computer_use/skills — C stubs skip those
