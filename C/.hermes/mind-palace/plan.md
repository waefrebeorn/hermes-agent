(v280)

## Immediate Next

S0+S3+S6 all PORTED. 110 gaps remain across 7 sectors.
Next: S7 test expansion, S1 partials (L24/L26-L28), or B08 send_message remaining depth.

## Pipeline

S1 L25 depth (agent_runtime_helpers):
- hermes_repair_message_sequence() ported ✅ (Phase 211, 17 tests)
- hermes_sanitize_tool_call_arguments() ported ✅ (Phase 212, 22 tests)
- repair_tool_call() ported ✅ (Phase 213, 11 tests)

S0 D09 depth (line_edit):
- Emacs keybindings PORTED ✅ (Phase 209, 77 tests)
- Vi mode remains — pending (P2)

S6 B08 depth (send_message):
- validate_media_path() ported ✅ (Phase 194)
- Media validation test suite (11 tests) ✅ (Phase 195)
- Slack DM resolution (U... → D...) — pending
- Smart message chunking — pending
- Home channel resolution — pending

S7 test coverage (280/1262 test files = 22.2%):
- Phase 213: 11-test registry_repair_tool_name suite
- Suite now 323/0/0

## Future

S1 partials depth (L24 checkpoint, L26 chat completion helpers, L27 prompt builder, L28 agent init), B08 send_message remaining depth, S4 TUI, S5 CLI, S8 providers, S9 plugin system, S10 architecture.
