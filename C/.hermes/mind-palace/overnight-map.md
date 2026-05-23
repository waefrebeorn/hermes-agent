# Overnight Map — 2026-05-23 (DA v21)

## Active: 4 library ports done. 189/500 parity.

**Suite: 173/0/0** (137 tests, ~895 assertions)
**Binary: 9.1MB dynamic**
**~401 commits, 0 behind upstream**

## What Was Done (May 23)
- **libdebug**: Port of Python `tools/debug_helpers.py` — debug session logging.
  Per-tool DebugSession with env-var activation, call logging, JSON output.
  3 new files (h+c+test), 24 assertions. Suite: 173/0/0.
- **libwebsite**: Port of Python `tools/website_policy.py` — website access policy.
  URL host extraction, fnmatch glob pattern matching, shared blocklist file loading.
  3 new files (h+c+test), 43 assertions. Suite: 172/0/0.
- **libosv**: Port of Python `tools/osv_check.py` — OSV malware check for MCP packages.
  Infers ecosystem (npx→npm, uvx→PyPI), parses package names, queries OSV API.
  3 new files (h+c+test), 25 assertions. Suite: 171/0/0.
- **libbinary**: Port of Python `tools/binary_extensions.py` — binary extension detection via sorted array + bsearch.
  3 new files (h+c+test), 134 assertions. Suite: 170/0/0.

## P0 Gaps (next session picks first)
1. **D80**: fal_common.py — Fal shared utilities (~300 lines Python)
2. **D81**: skill_manager_tool.py — Skill manager tool (931 lines)
3. **D82**: skill_usage.py — Skill usage tracking + provenance (612 lines)
4. **D83**: transcription_tools.py — Audio transcription (936 lines)
5. **D85**: mcp_oauth.py — MCP OAuth client (649 lines)
6. **D86**: mcp_oauth_manager.py — MCP OAuth manager
7. **T09**: Memory leak detection (valgrind/asan CI pass)
8. **S03**: computer_use Wayland fallback (partial — basic impl exists)
9. **S08-S09**: image_gen local provider + caching
