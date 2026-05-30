(v380)

325/0/14, 289 test files. All pass. 72 gaps. Phase 316: S5 C11 auth CLI — /auth [status|providers].

Phase 316: S5 C11 auth CLI — /auth [status|providers]. status checks 17 provider env vars (OPENAI_API_KEY through MISTRAL_API_KEY), OAuth tokens (Claude Code, Bitwarden), .env/config.yaml presence. providers lists 15 known providers with credential hints. C11 PARTIAL depth. v380.
Phase 315: S5 C18 voice CLI — /voice [on|off|tts|status|config|key]. Enhanced CLI with config display, record key formatting, TTS provider status. 6 subcommands. C18 PORTED. v379.
Phase 314: S5 C03 memory CLI — /memory [status|providers|setup]. status: loads config.yaml and displays memory provider, char_limit, user_char_limit, ttl_days, auto_save. providers: lists 5 known memory providers (built-in, honcho, mem0, supermemory, hindsight). setup <provider>: prints manual config.yaml edit instructions. C03 PORTED. v378.
Phase 313: S5 C17 skills hub CLI — /skills-hub with 4 subcommands wired to skills_hub.c API. list shows catalog summary + first 50 skills. search finds by query substring (name/title/desc/category/tags/slug). show displays full skill details. sync clears cache and re-fetches. Type renamed from skill_meta_t to hub_skill_meta_t to avoid collision with hermes.h. v377.
Phase 308: S7 enum edge case expansion — 31 new assertions (17→48), full coverage: out-of-range OBO (value==count), large 21-value enum, empty/partial parse, round-trip all values, macro isolation. v372.
Phase 307: S7 difflib edge case expansion — 59 new assertions (22→81), full coverage: ratio swap consistency, single char, unicode, whitespace, long identical, substring, unified_diff NULL/empty/both-NULL, large/zero context, single line, trailing newlines, multiple hunks, simple_diff empty/empty, full add/delete, multiple distant changes, long line change. v371.
Phase 306: S7 glob edge case expansion — 48 new assertions (22→70), full coverage: empty/null patterns, character classes, dotfiles, multi-*, ** at end, **/ alone, cross-/ behavior, regression patterns, glob_find edge cases (empty dir, hidden files, exact path). v370.
Phase 305: S7 template edge case expansion — 26 new assertions (9→35), full coverage: plain text, NULL safety, defaults, for loops, if/else, nested, multi-context. v369.
Phase 304: S7 YAML edge case expansion — 53 new assertions (6→59), full API coverage: lists, map_keys, to_json_string, multi-doc, file, NULL safety, boolean variants, negatives, nesting, comments. v368.
Phase 302: S7 proc edge case expansion — PID 0/invalid/large, NULL safety, load_avg NULL params, vm>=rss, pid==getpid(), consistency, sanity bounds. v366.
Phase 278: S8 R10 depth — provider_add_model_aliases() + provider_get_context_length_from_provider_error(). 19 new assertions (313→332). Suite 334/0/3. v345.
Coverage: 292/1262 test files (23.1%). 72 structural gaps remain.
