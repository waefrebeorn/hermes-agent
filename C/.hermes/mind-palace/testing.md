(v377)

325/0/14, 289 test files. All pass. 74 gaps. Phase 313: S5 C17 skills hub CLI — /skills-hub [list|search|show|sync].

Phase 313: S5 C17 skills hub CLI — /skills-hub with 4 subcommands wired to skills_hub.c API. list shows catalog summary + first 50 skills. search finds by query substring (name/title/desc/category/tags/slug). show displays full skill details. sync clears cache and re-fetches. Type renamed from skill_meta_t to hub_skill_meta_t to avoid collision with hermes.h. v377.
Phase 308: S7 enum edge case expansion — 31 new assertions (17→48), full coverage: out-of-range OBO (value==count), large 21-value enum, empty/partial parse, round-trip all values, macro isolation. v372.
Phase 307: S7 difflib edge case expansion — 59 new assertions (22→81), full coverage: ratio swap consistency, single char, unicode, whitespace, long identical, substring, unified_diff NULL/empty/both-NULL, large/zero context, single line, trailing newlines, multiple hunks, simple_diff empty/empty, full add/delete, multiple distant changes, long line change. v371.
Phase 306: S7 glob edge case expansion — 48 new assertions (22→70), full coverage: empty/null patterns, character classes, dotfiles, multi-*, ** at end, **/ alone, cross-/ behavior, regression patterns, glob_find edge cases (empty dir, hidden files, exact path). v370.
Phase 305: S7 template edge case expansion — 26 new assertions (9→35), full coverage: plain text, NULL safety, defaults, for loops, if/else, nested, multi-context. v369.
Phase 304: S7 YAML edge case expansion — 53 new assertions (6→59), full API coverage: lists, map_keys, to_json_string, multi-doc, file, NULL safety, boolean variants, negatives, nesting, comments. v368.
Phase 302: S7 proc edge case expansion — PID 0/invalid/large, NULL safety, load_avg NULL params, vm>=rss, pid==getpid(), consistency, sanity bounds. v366.
Phase 278: S8 R10 depth — provider_add_model_aliases() + provider_get_context_length_from_provider_error(). 19 new assertions (313→332). Suite 334/0/3. v345.
Phase 277: S8 R10 depth — provider_extract_first_int() refactor. Provider metadata 313-test suite (300→313).
Phase 272: S8 R02+R10 depth — batch 5 functions. 54 new assertions. Suite 335/0/0. v339.
Phase 271: S8 R02+R10 depth — batch 10 functions. 104 new assertions. Suite 335/0/0. v338.
Phase 270: S8 R04 Gemini depth — batch 3 functions. 36 new assertions (130→166). Suite 335/0/0. v337.
Phase 269: S8 R04 Gemini depth — tool_choice + thinking_config. 29 new assertions (101→130). Suite 335/0/0. v336.
Coverage: 292/1262 test files (23.1%). 94 structural gaps remain.
