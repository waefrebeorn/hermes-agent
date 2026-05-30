# Testing — Slermes C (v383)

325/0/14, 289 test files. All pass. 68 gaps. Phase 319: X09 approval edge case expansion — 56 new assertions (19→75).

Phase 318: T13 model picker — interactive model selection in TUI via /model. 16 models listed, arrow keys/Enter/ESC/q navigation. Suite 325/0/15. v382.
Phase 319: X09 approval edge case expansion — 56 new assertions (19→75). Covers approval_is_terminal_dangerous (25 patterns), approval_normalize_command (ANSI/edge cases), approval_is_path_dangerous (12 paths), approval_is_path_traversal (7 patterns). Suite 325/0/14. v383.
Phase 316: S5 C11 auth CLI — /auth [status|providers]. status checks 17 provider env vars, OAuth tokens, .env/config.yaml presence. providers lists 15 known providers with credential hints. C11 PARTIAL depth. v380.
Phase 315: S5 C18 voice CLI — /voice [on|off|tts|status|config|key]. Enhanced CLI with config display, record key formatting, TTS provider status. 6 subcommands. C18 PORTED. v379.
Phase 314: S5 C03 memory CLI — /memory [status|providers|setup]. C03 PORTED. v378.
Phase 313: S5 C17 skills hub CLI — /skills-hub [list|search|show|sync]. v377.
Coverage: 292/1262 test files (23.1%). 72 structural gaps remain.
