# Overnight Map — Recent Phases (v381)

| Phase | Change | Before | After |
|-------|--------|--------|-------|
| 317 | S7 X10 fuzz tests expanded — 14 test cases (5→19): JSON (5), YAML (2), template (2), regex (2), HTML (1), path (1), JSON round-trip property (1). 6 parser families. Stale claim corrected (tests existed, now expanded). | `tests/test_fuzz.c` — 14 new tests. `test_runner.sh` — deps updated. Suite 325/0/14. v381. | 72 gaps | 72 gaps |
| 316 | S5 C11 auth CLI — /auth [status|providers]. status checks 17 provider env vars, OAuth tokens, .env/config.yaml. providers lists 15 known providers with credential hints. C11 PARTIAL depth. | `src/cli/commands.c` — cmd_auth() (+110 LOC). Suite 325/0/14. v380. | 72 gaps | 72 gaps |
