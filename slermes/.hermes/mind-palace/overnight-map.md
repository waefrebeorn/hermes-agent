# Overnight Map — Recent Phases (v385)

| Phase | Change | What | Before | After |
|-------|--------|------|--------|-------|
| 319 | X09 | Approval edge case expansion — 56 new assertions (19→75). Covers approval_is_terminal_dangerous (25 patterns across 6 categories), approval_normalize_command (ANSI stripping, null byte edge cases), approval_is_path_dangerous (12 system paths), approval_is_path_traversal (6 encodings). | `tests/test_approval.c` — 3 test functions + fwd decls. Suite 325/0/14. v383. | 68 gaps | 68 gaps |
| 317 | S7 X10 fuzz tests expanded — 14 test cases (5→19): JSON (5), YAML (2), template (2), regex (2), HTML (1), path (1), JSON round-trip property (1). 6 parser families. Stale claim corrected (tests existed, now expanded). | `tests/test_fuzz.c` — 14 new tests. `test_runner.sh` — deps updated. Suite 325/0/14. v381. | 72 gaps | 72 gaps |
| 316 | S5 C11 auth CLI — /auth [status|providers]. status checks 17 provider env vars, OAuth tokens, .env/config.yaml. providers lists 15 known providers with credential hints. C11 PARTIAL depth. | `src/cli/commands.c` — cmd_auth() (+110 LOC). Suite 325/0/14. v380. | 72 gaps | 72 gaps |
