(v275)

## Immediate Next

S3+S6 all PORTED. 115 gaps remain across 7 sectors.
Next: S7 test expansion, S0 D09/D16 display depth, or S1 partials (L24-L28).

## Pipeline

S6 B07 depth (terminal):
- terminal_rewrite_sudo() ported ✅ (Phase 196, 24 tests)
- terminal_rewrite_compound_background() ported ✅ (Phase 197, 12 tests)
- _transform_sudo() wired into terminal_handler() ✅ (Phase 198, 7 tests)
- terminal_prompt_for_sudo_password() ported ✅ (Phase 199, 5 tests)
- PTY stdin pipe for sudo password ✅ (Phase 200 — write password to master_fd)

S6 B08 depth (send_message):
- validate_media_path() ported ✅ (Phase 194)
- Media validation test suite (11 tests) ✅ (Phase 195)
- Slack DM resolution (U... → D...) — pending
- Smart message chunking — pending
- Home channel resolution — pending

S3:
- G02 base.py PORTED ✅ (Phase 202)
- G06 wecom_callback PORTED ✅ (Phase 202)

S7 test coverage (278/1262 test files = 22.0%):
- Phases 195-200: 6 test suites added (59 new tests) ✅
- Phase 203-204: 21 new edge case assertions
- Phase 207: 26-test tool_error_sanitize suite
- Phase 208: 35-test tool_coerce suite

## Future

B08 send_message remaining depth, S4 TUI, S5 CLI, S8 providers, S9 plugin system, S10 architecture.
