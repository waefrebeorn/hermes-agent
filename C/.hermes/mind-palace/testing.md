(v381)

325/0/14, 289 test files. All pass. 72 gaps. Phase 317: S7 X10 fuzz tests — 14 test cases across 6 parser families.

Phase 317: S7 X10 fuzz tests expanded — 14 new test cases (5→19): JSON (5 tests), YAML (2: malformed, large sequence), template (2: malformed, deep nested), regex (2: malformed, large), HTML (1: strip tags fuzz), path (1: traversal fuzz), JSON property round-trip (1: serialize/parse symmetry). 6 parser families. Covers more crash surfaces. v381.
Phase 316: S5 C11 auth CLI — /auth [status|providers]. status checks 17 provider env vars, OAuth tokens, .env/config.yaml presence. providers lists 15 known providers with credential hints. C11 PARTIAL depth. v380.
Phase 315: S5 C18 voice CLI — /voice [on|off|tts|status|config|key]. Enhanced CLI with config display, record key formatting, TTS provider status. 6 subcommands. C18 PORTED. v379.
Phase 314: S5 C03 memory CLI — /memory [status|providers|setup]. C03 PORTED. v378.
Phase 313: S5 C17 skills hub CLI — /skills-hub [list|search|show|sync]. v377.
Coverage: 292/1262 test files (23.1%). 72 structural gaps remain.
