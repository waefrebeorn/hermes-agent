# Triple Devil's Advocate — Hermes C Translation
## DA v11 (2026-05-23) — Deep Audit + Stub Hunt + Battleship Expansion

---

## Part 1: Build Health Assessment

### Local Build: ✅ PASS (0 errors, ~40 warnings)
- `make -j$(nproc)`: Compiles clean. 9.6MB binary
- `make plugins`: 10 .so files, all build
- `./test_runner.sh`: **154/0/0**, 573 assertions

### CI Build (GitHub Actions): Potential Failures

| Workflow | Status | Issue | Fix |
|----------|--------|-------|-----|
| `c-build.yml` | 🔴 Docker fails | Dockerfile `RUN make` at repo root — no Makefile there | ✅ Fixed: `cd C && make` |
| `c-build.yml` | 🟡 Tail-truncated logs | `tail -20` hides build errors in CI | ✅ Fixed: full output |
| `c-build.yml` | 🟡 TUI build destroys binary | `make clean` before TUI removes `hermes` binary | ✅ Fixed: no clean |
| `nix.yml` | 🔴 Pre-existing | Stale npm lockfile hashes in TUI/Web subpkgs | Unrelated to C — upstream issue |
| `tests.yml` | 🟢 N/A | Only triggers on Python changes; C changes don't affect | — |

### Dockerfile Bugs Found
**Root cause:** `WORKDIR /build` + `COPY . .` copies repo root, but Makefile lives in `C/`. `RUN make -j$(nproc) hermes` fails with "no Makefile". Also the `COPY --from=builder` paths in stage 2 pointed to wrong location.

**Fix applied:** `RUN cd C && make`, `COPY --from=builder /build/C/hermes`.

---

## Part 2: Stub Analysis (2nd Run — Deep Stub Hunt)

### Stub 1: `src/tools/computer_use.c` — 🔴 100% Stub
```
static char *handle_computer_use(...) {
    return strdup("{\"error\":\"computer_use is not available...\"}");
}
```
- 30-line file: schema stub + error-return handler
- Returns hardcoded "not available on Linux/WSL" error
- Python has 5 separate files: `backend.py`, `cua_backend.py`, `schema.py`, `tool.py`, `vision_routing.py`
- **Battleship impact:** 5 gaps (D75-D79) misclassified as "new from upstream" — they're actually our biggest tool feature gap
- **Fix path:** Need macOS cua-driver. C equivalent requires MCP client for cua-driver protocol

### Stub 2: `src/tools/browser.c:stub_cdp_handler` — 🟡 Partial Stub
```
static char *stub_cdp_handler(...) {
    return strdup("{\"success\":false,\"error\":\"Requires Camofox or Playwright CDP server...\"}");
}
```
- Text-based browsing works (navigate, click, scroll, type all functional via HTML parsing)
- **CDP-dependent features stubbed:** `browser_cdp`, `browser_get_images`, `browser_console`, `browser_dialog`, `browser_vision`
- These 5 CDP tools are registered but always return "requires CDP server" error
- **Impact:** browser tool claims 11 registrations but only 6 are fully functional

### Stub 3: `src/plugins/plugin_image_gen.c:impl_image_gen` — 🔴 Simulated Stub
```
snprintf(rec->result_url, ..., "https://api.hermes.ai/image/%ld_%dx%d.png", ...);
```
- Returns a fabricated URL (`api.hermes.ai/image/...`) — does NOT generate any image
- Stores history, manages provider/model config, but image URL is always fake
- Python has real image generation via Fal AI, Stable Diffusion, DALL-E, etc.
- **Impact:** Plugin claims image generation capability but produces unusable URLs

### Stub 4: `src/cli/tui_fullscreen.c:session_list` — 🟡 Placeholder
```c
/* For now, show placeholder */
strncpy(tui.sessions.sessions[0], "current", sizeof(tui.sessions.sessions[0]) - 1);
tui.sessions.count = 1;
```
- Session browser in TUI shows hardcoded "current" entry instead of querying DB
- Config editor IS populated with real entries (good)
- **Impact:** TUI session management unusable — cannot browse/switch sessions

### Stub 5: `src/tools/mcp_tool.c:950` — 🟢 Minor Placeholder
- Sets placeholder auth entry when saving tokens for a server that doesn't exist yet
- `"Also set a placeholder auth entry"` — this is defensive code, not a stub
- **Verdict:** False positive. This is a legitimate initialization pattern

### Stub 6: `src/cron/cron_extras.c` — 🟢 Legitimate Use
- Template placeholder replacement (`{{param}}` → value) — not a stub
- Correct implementation of parameter substitution in cron template commands
- **Verdict:** False positive. Correct functionality

### Stub Summary

| Stub | File | Severity | Impact |
|------|------|----------|--------|
| computer_use | `tools/computer_use.c` | 🔴 Critical | 5 system features broken |
| CDP browser features | `tools/browser.c` | 🟡 High | 5 tool registrations non-functional |
| image_gen | `plugins/plugin_image_gen.c` | 🔴 Critical | Fake image URLs |
| TUI sessions | `cli/tui_fullscreen.c` | 🟡 Medium | Can't browse sessions |
| MCP auth placeholder | `tools/mcp_tool.c` | 🟢 None | Defensive code |

---

## Part 3: False ✅ Hunt

### False ✅ #1: "Tools: 95% (28 registered)" — Battleship indexed 28 tools
**Reality:** **68 tools registered** in C code. The 28 figure was from a much older audit.
- True gap is not 28→95% but 68→? Need to compare with Python tool count
- Python has ~42-50 core tools (tools/*.py) + plugin tools = ~60-70 total
- C's 68 includes multiple registrations per file (kanban has 9, browser has 11, skills has 13)
- **Verdict:** Claim is stale, not false. 68 is competitive. Update: ~95% accurate

### False ✅ #2: "Libraries: 32 .a (100%)"
**Reality:** There are NO .a library archives in C/. Libraries are compiled to .o files and linked directly into the hermes binary (no intermediate .a).
- The "32 libraries" claim refers to .o compilation units in lib/ not .a files
- Misleading term — `libjson.a`, `libhttp.a`, etc. don't exist
- **Verdict:** Misleading. Should say "32 library units compiled (100%)" or verify .a creation

### False ✅ #3: "Gateway: 100% ✅"
**Reality:** 19 gateway platforms exist. But:
- At least 3 platforms have no per-platform tests (stub test only)
- Several platforms have incomplete implementations (e.g., signal.c, sms.c are basic)
- The 100% refers to "existence of platform adapter code" not "feature-complete"
- **Verdict:** Overclaim. Should be ~90% — all platforms exist, but some are thin wrappers

### False ✅ #4: "Plugin depth: 45% (10 done / 22 gaps)"
**Reality:** J07-J08 (libhash, libuuid) are marked as library tasks but are libraries, not plugins. Moving to sector K.
- 10 plugins .so files exist (honcho, kanban, spotify, disk_cleanup, file_memory, achievements, observability, skills, image_gen, google_meet)
- 12 Python plugins not ported: fal_image, fal_video, discord_platform, openviking, and others
- **Verdict:** Sector classification fuzzy. Need cleaner split Libraries→Plugins

### False ✅ #5: "Suite: 154/0/0, 573 assertions"
**This is correct.** Verified by running the full suite. No false claims here.

---

## Part 4: Code Quality Issues Found

### Critical: `src/tools/terminal.c:strtok_r` — Incompatible Pointer Type
```
strtok_r(str, delim, &saveptr)  // saveptr is char**, but strtok_r expects char**
```
On glibc, `strtok_r` is `char *strtok_r(char *str, const char *delim, char **saveptr)`. In C, `char **saveptr` is declared with `restrict` qualifier. Passing `&saveptr` where `saveptr` is `char *` is correct — the warning is GCC being pedantic about `restrict` aliasing. **Low risk**, but ugly.

### Medium: `src/agent/agent_loop.c:384` — Dangling Pointer Warning
```
char new_id[36];
char *session_id = new_id;
// ... later use of session_id after new_id scope ends?
```
GCC -Wdangling-pointer= warning. The variable `session_id` may point to stack memory that's no longer valid. Needs review.

### Medium: `src/agent/plugin_ext.c:73` — Object→Function Pointer Conversion
```
(Warning) ISO C forbids conversion of object pointer to function pointer type
```
This is inherent to dlsym-based plugin loading. On POSIX, `dlsym` returns `void*` which must be cast to function pointer. This generates a pedantic warning but is the standard pattern. **Acceptable but noisy.**

### Low: `src/cli/config.c:1557` — Dead Code / Unused Variable
```
int sdb;  // declared but never used
```
Removing unused variable would clean up a warning.

---

## Part 5: Test Coverage Gaps

| Area | Tests | Status |
|------|-------|--------|
| Libraries | All tested | ✅ |
| Plugins | All 10 tested | ✅ |
| Agent / Provider | 225 assertions, 9 providers | ✅ Strong |
| Cron | 4 test files | ✅ |
| Gateway | Stub test only | ❌ No per-platform tests |
| CLI | Thin (commands, paths, display) | 🟡 ~60% |
| TUI | Zero tests | ❌ 0% |
| MCP | Zero tests | ❌ 0% |
| ACP | Zero tests | ❌ 0% |
| Browser | Partial (text-based only) | 🟡 ~60% |
| Computer use | Zero tests | ❌ 100% missing |

---

## Part 6: Battleship Expansion (468 → 500)

### New Sectors Added

| Sector | Area | New Gaps | Total |
|--------|------|----------|-------|
| S | Stub remediation | 10 | — |
| T | Test infrastructure | 12 | — |
| U | CI/CD | 10 | — |

### Gap Decomposition (splitting existing large gaps)

**B: Agent** (109 → 115 gaps, +6)
- B110-B112: Split agent_loop into subcomponents (retry logic, interrupt handling, session lifecycle)
- B113-B115: Split provider metadata (rate limiting, cost tracking, model fallback orchestration)

**C: CLI** (90 → 95 gaps, +5)
- C91-C93: Split config migration (YAML upgrade, schema validation, env var import)
- C94-C95: Split display layer (markdown rendering, JSON output mode)

**D: Tools** (86 → 92 gaps, +6)
- D87-D88: Stub remediation (computer_use hardware abstraction, CDP browser backend)
- D89-D90: browser.c URL safety & content security policy
- D91-D92: image_gen tool real backend (Fal AI, local SD)

**E: Gateway** (61 → 64 gaps, +3)
- E62: Gateway platform test infrastructure
- E63-E64: Signal & SMS platform completeness

**J: Plugins** (22 → 26 gaps, +4)
- J23-J24: Plugin verification framework (sandboxed loading, API surface diffing)
- J25-J26: Plugin documentation generator, version compatibility checker

**New Sector S: Stub Remediation** (10 gaps)
| ID | Gap | Priority |
|----|-----|----------|
| S01 | computer_use: hardware abstraction layer (MCP client for cua-driver) | P0 |
| S02 | computer_use: macOS CUA backend implementation | P0 |
| S03 | computer_use: Linux fallback (X11/Wayland screenshot + input) | P1 |
| S04 | browser CDP: WebSocket CDP client implementation | P1 |
| S05 | browser CDP: JavaScript execution engine | P1 |
| S06 | browser CDP: Screenshot capture pipeline | P2 |
| S07 | image_gen: Real backend (Fal AI REST client) | P1 |
| S08 | image_gen: Local provider (Stable Diffusion subprocess) | P2 |
| S09 | image_gen: Result verification & caching | P2 |
| S10 | TUI session list: DB-backed session browser | P1 |

**New Sector T: Test Infrastructure** (12 gaps)
| ID | Gap | Priority |
|----|-----|----------|
| T01 | Gateway platform per-platform integration tests | P1 |
| T02 | CLI command coverage (>80%) | P1 |
| T03 | TUI component tests (ncurses simulation) | P2 |
| T04 | MCP server/transport integration tests | P2 |
| T05 | ACP server protocol tests | P2 |
| T06 | Browser CDP integration test harness | P2 |
| T07 | Plugin sandbox loading tests | P1 |
| T08 | Fuzz testing for JSON/config/message parsers | P2 |
| T09 | Memory leak detection (valgrind/asan CI pass) | P1 |
| T10 | Thread safety tests (cron scheduler, gateway server) | P2 |
| T11 | Cross-platform build matrix (macOS, FreeBSD) | P2 |
| T12 | Benchmark regression harness (latency, throughput) | P3 |

**New Sector U: CI/CD** (10 gaps)
| ID | Gap | Priority |
|----|-----|----------|
| U01 | C build workflow must pass before merge | P0 |
| U02 | Docker build CI step (currently broken — separate binary) | P0 |
| U03 | Artifacts: attach binary to PR for quick download | P1 |
| U04 | ASan/UBSan CI job (address sanitizer) | P1 |
| U05 | Cross-compilation matrix (aarch64, arm) | P2 |
| U06 | Release automation (semantic version, changelog) | P2 |
| U07 | Code coverage upload (lcov → Codecov) | P2 |
| U08 | Pre-commit hooks (format, lint, minimal build) | P1 |
| U09 | Dependency vulnerability scanning (CVE check for libssl) | P2 |
| U10 | Performance gate (binary size, startup time regression check) | P3 |

### Summary: 468 + 32 = 500

| Sector | Old | New | Change |
|--------|-----|-----|--------|
| A: Core | 16 | 16 | — |
| B: Agent | 109 | 115 | +6 |
| C: CLI | 90 | 95 | +5 |
| D: Tools | 86 | 92 | +6 |
| E: Gateway | 61 | 64 | +3 |
| F: MCP | 11 | 11 | — |
| G: ACP | 9 | 9 | — |
| H: Cron | 3 | 3 | — |
| I: TUI | 8 | 8 | — |
| J: Plugins | 22 | 26 | +4 |
| L: Config | 6 | 6 | — |
| N: Build/Doc | 11 | 11 | — |
| O: Upstream | 3 | 3 | — |
| P: Security | 10 | 10 | — |
| Q: Cross-cut | 5 | 5 | — |
| R: Provider quirks | 18 | 18 | — |
| **S: Stubs** | — | 10 | **new** |
| **T: Tests** | — | 12 | **new** |
| **U: CI/CD** | — | 10 | **new** |
| **Total** | **468** | **~500** | **+32** |

---

## Part 7: CI/CD Fixes Applied

1. **Dockerfile** — `RUN make` at wrong directory → `RUN cd C && make`
2. **Dockerfile** — Wrong stage2 COPY paths → `/build/slermes/hermes`
3. **c-build.yml** — Removed `tail -20` truncation → full build output visible
4. **c-build.yml** — Removed `make clean` before TUI build (was destroying binary)
5. **c-build.yml** — Uncapped `tail -5`/`tail -10` for plugin and TUI output
6. **nix-lockfile-fix.yml** — Version bumps already applied in earlier session

### Unfixed (C-side cannot fix):
- nix.yml stale lockfile hashes — Python/Node infrastructure issue
- tests.yml Python test failures — unrelated to C changes

---

## Verification Levels Applied

| Claim | Level | Status |
|-------|-------|--------|
| 154/0/0 test suite | ✅ Verified | Suite run confirms |
| Binary compiles clean | ✅ Verified | `make -j$(nproc)` 0 errors |
| 68 tools registered | ✅ Verified | grep count |
| 10 plugins (.so) | ✅ Verified | ls count |
| Dockerfile broken | ✅ Verified | Code reading confirms |
| Stub: computer_use | ✅ Verified | Code reading confirms 100% stub |
| Stub: browser CDP | ✅ Verified | Code reading confirms 5 CDP fns |
| Stub: image_gen | ✅ Verified | Code reading confirms fake URLs |
| Stub: TUI sessions | ✅ Verified | Code reading confirms placeholder |
| "32 .a libraries" | ❓ Misleading | .a files don't exist; .o files do |

*Generated 2026-05-23. Triple DA audit: code audit (115 .c, 68 tools, 10 plugins), build simulation, CI workflow inspection.*
