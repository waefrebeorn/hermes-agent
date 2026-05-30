# Bug Bounty — Hermes C Translation

**Single source of truth for ALL bugs found.** Updated DA v15 (2026-05-24).

## Active Bugs

### CDP Dead Code — browser_cdp tool wired to stub handler
| Field | Value |
|-------|-------|
| **Severity** | 🟡 High |
| **Location** | src/tools/browser.c:1172 (stub), :1471+ (registration) |
| **Impact** | 13 browser tools registered, `browser_cdp` returns "CDP not available" |
| **Root cause** | Tool registration still points to `stub_cdp_handler`. Real CDP WebSocket client code exists at line 1193+ but was never wired to the registration. |
| **Fix** | Rewire `browser_cdp` tool to `cdp_command_handler` instead of `stub_cdp_handler` |
| **Found** | DA v14 |
| **Status** | 🔴 Open |

### Suite Timeout — 173 test files can't complete in 120s
| Field | Value |
|-------|-------|
| **Severity** | 🟡 High |
| **Location** | test_runner.sh |
| **Impact** | Full test suite can't be verified in a single run |
| **Root cause** | Tests run sequentially. Some tests (process, DB) take seconds each. |
| **Fix** | Parallelize test execution or increase timeout |
| **Found** | DA v15 |
| **Status** | 🔴 Open |

### CLI — 197 of 237 cmd_ functions are printf stubs
| Field | Value |
|-------|-------|
| **Severity** | 🟡 High |
| **Location** | src/cli/commands.c |
| **Impact** | CLI parity is 9%. Most commands print "not available" or "---" |
| **Root cause** | Scaffolding created all cmd_ function signatures, only ~40 have real implementations |
| **Fix** | Each stub needs real implementation |
| **Found** | DA v15 |
| **Status** | 🔴 Open |

## Fixed Bugs (Archived)

### YAML Parser — empty inline value blocked multi-line list detection
| Field | Value |
|-------|-------|
| **Severity** | 🟡 High |
| **Location** | src/yaml_parser.c:parse_inline() |
| **Impact** | Skills: block with empty inline value prevented `- skill` list from being parsed |
| **Fix** | Changed `YVAL_MAP` → `YVAL_STRING` for empty inline values |
| **Date** | 2026-05-23 |

### Display Test Compilation — missing include paths
| Field | Value |
|-------|-------|
| **Severity** | 🔵 Medium |
| **Location** | test_runner.sh |
| **Impact** | test_display.c SKIP because libans + libjson not linked |
| **Fix** | Added `-I lib/libansi -I lib/libjson` and linked ansi.c, json.c |
| **Date** | 2026-05-23 |

### Process Tests — stale processes.json contamination
| Field | Value |
|-------|-------|
| **Severity** | 🔵 Medium |
| **Location** | test_runner.sh |
| **Impact** | process test failed because SLERMES_HOME pointed to real config dir with stale processes.json |
| **Fix** | Set SLERMES_HOME=/tmp/hermes_test_proc in test_runner.sh |
| **Date** | 2026-05-23 |

### C JSON Use-After-Free (9 providers)
| Field | Value |
|-------|-------|
| **Severity** | 🔴 Critical |
| **Location** | `json_object_set` → stores pointer, not copy |
| **Impact** | All 9 providers had UAF in response_format handler. `json_free(rf)` freed node still referenced by root |
| **Fix** | `json_object_set(root, ..., json_copy(rf))` then `json_free(rf)` |
| **Date** | 2026-05-22 |

### Tool Dispatch — paths_overlap root-path bug
| Field | Value |
|-------|-------|
| **Severity** | 🔵 Medium |
| **Impact** | Root path "/" didn't overlap `/usr` correctly |
| **Fix** | Added explicit root-path check |
| **Date** | 2026-05-23 |

### Dockerfile — wrong working directory
| Field | Value |
|-------|-------|
| **Severity** | 🟡 High |
| **Impact** | Docker build would fail — `RUN make` in wrong CWD |
| **Fix** | Changed to `cd C && make`, corrected COPY paths |
| **Date** | 2026-05-22 |
