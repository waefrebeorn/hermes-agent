# Overnight Map — Recent Phases (v380)

| Phase | Change | Before | After |
|-------|--------|--------|-------|
| 316 | S5 C11 auth CLI — /auth [status|providers]. status checks 17 provider env vars, OAuth tokens, .env/config.yaml. providers lists 15 known providers with credential hints. C11 PARTIAL depth. 72 gaps. | `src/cli/commands.c` — cmd_auth() (+110 LOC). Suite 325/0/14. v380. | 72 gaps | 72 gaps |
| 313 | S5 C17 skills hub CLI — /skills-hub [list|search|show|sync] wired to skills_hub.c API. list prints catalog summary + first 50 skills. search finds by query substring (name/title/desc/category/tags/slug). show displays full skill details. sync clears cache and re-fetches. Type renamed hub_skill_meta_t to avoid collision with hermes.h's skill_meta_t. C17 PORTED. 75→74 gaps. | `src/cli/commands.c` — cmd_skills_hub() (95 LOC). `include/hermes_skills_hub.h` — hub_skill_meta_t rename. Suite 325/0/14. v377. |
| 312 | S5 C16 kanban CLI — /kanban list/show/create/complete/block/unblock/link wired to kanban tools via registry_dispatch(). list prints formatted task table with id/status/assignee/title columns. create dispatches kanban_create with assignee=cli. complete/block/unblock/link all call through to backend. C16 PORTED. 76→75 gaps. | `src/cli/commands.c` — cmd_kanban() rewritten (168 LOC). Suite 325/0/14. v376. |
| 311 | S5 C13 gateway CLI — /gateway [status|list|stop|setup|restart] command with 5 subcommands. stop: gw_platform_shutdown_all + session save + exit(0). setup: platform env var readiness check with [ready]/[missing] indicators. restart: save + re-exec. C13 PORTED. 77→76 gaps. | `src/cli/commands.c` — cmd_gateway() + /gateway command registration (87 LOC). Suite 325/0/14. v375. |
| 310 | S5 C15 platform enable/disable CLI — /platform pause/resume now modify config.yaml programmatically via hermes_config_set_platforms(). Finds/creates gateway: section in config.yaml, adds/removes platform from comma-separated gateway.platforms list, writes back to ~/.hermes/config.yaml. Follows migrate_v0_to_v1() read-edit-write pattern. C15 PORTED. 78→77 gaps. | `src/cli/config.c` — hermes_config_set_platforms(). `src/cli/commands.c` — cmd_platform() pause/resume wired. `include/hermes.h` — declaration. Suite 325/0/14. v374. |
| 309 | S7 X09 video_mime edge case expansion — 44 new assertions (14→58). Full coverage: 15 case variants, 7 multi-dot edges, 9 path edge cases, 4 extension lengths, 8 non-video format rejections. | `tests/test_video_mime.c` — 58 assertions (14→58). `test_runner.sh` — count 14→58. Suite 325/0/14. v373. |
| 308 | S7 X09 enum edge case expansion — 31 new assertions (17→48). Full coverage: out-of-range OBO, large 21-value enum, empty/partial parse, round-trip all values, macro isolation, const correctness. | `tests/test_enum.c` — 48 assertions (17→48). Suite 325/0/14. v372. |
| 307 | S7 X09 difflib edge case expansion — 59 new assertions (22→81). Full coverage: ratio swap, single char, unicode, whitespace, large context, zero context, trailing newlines, multiple hunks. | `tests/test_difflib.c` — 81 assertions (22→81). Suite 325/0/14. v371. |
| 306 | S7 X09 glob edge case expansion — 48 new assertions (22→70). Full coverage: patterns, character classes, dotfiles, multi-*, **, cross-/ behavior, regression patterns. | `tests/test_glob.c` — 70 assertions (22→70). Suite 325/0/14. v370. |
| 305 | S7 X09 template edge case expansion — 26 new assertions (9→35). Full coverage: plain text, NULL safety, defaults, for loops, if/else, nested. | `tests/test_template.c` — 35 assertions (9→35). Suite 325/0/14. v369. |
| 304 | S7 X09 YAML edge case expansion — 53 new assertions (6→59). Full API coverage. | `tests/test_yaml.c` — 59 assertions (6→59). Suite 325/0/14. v368. |
| 303 | S7 X09 secrets edge case expansion — 8 new assertions (14→22). Secrets: empty BSM key, BSM with space, special chars, strict mode, multiple refs. | `tests/test_secrets.c` — 22 assertions (14→22). Suite 335/0/15. v367. |
| 302 | S7 X09 proc edge case expansion — PID 0/invalid/large, NULL safety, load_avg NULL params, vm>=rss, pid==getpid(), consistency, sanity bounds. | `tests/test_proc.c` — 36 assertions (15→36). Suite 335/0/15. v366. |
| 295 | S5 stale sweep — C01 setup wizard PORTED (slermes setup exists). /platforms enhanced with config display + -v credential check. S5 19→18 gaps. Total 79→78 gaps. v358. |
| 294 | S5 C14 webhook CLI — /webhook list/add/remove PORTED. S5 20→19 gaps. Total 80→79 gaps. v357. |
| 293 | S5 stale sweep — C04 PORTED, C05 PORTED, C10+C12 WON'T PORT. S5 25→20 gaps. Total 84→80 gaps. v356. |
| 292 | S5 C13 Gateway CLI — gateway status + list + start subcommands. Suite 323/0/16. v356. | 84 gaps | 84 gaps |
| 291 | S7 test_regex.c — 17 assertions (17→294 test files). v355. | 84 gaps | 84 gaps |
| 290 | C11 OAuth status in /secrets. v355. | 84 gaps | 84 gaps |
| 289 | S7 test_trajectory.c — 19 assertions (294 test files). v355. | 84 gaps | 84 gaps |
| 288b | C04/C07/C08/C09 stale cleanup — /model, /profile, /config all exist. v355. | 87 gaps | 84 gaps |
| 288 | S5 C02 doctor — /doctor command. v355. | 88 gaps | 87 gaps |
| 287c | S5 C06 env_loader — load_slermes_env(). Suite 335/0/14. v354. | 89 gaps | 88 gaps |
| 287b | S10 F07 trajectory saving vaulted — trajectory.c already PORTED. v354. | 90 gaps | 89 gaps |
| 287 | S8 R01 endpoint detection — 14 functions PORTED, 69 tests, Bearer auth wiring. Suite 335/0/14. v354. | 92 gaps | 90 gaps |
| 286 | S0 D09 vi count prefixes — digits 1-9 accumulate as repeat count. Multi-digit supported. All commands count-aware. S0 D09 PORTED (all features). 8 new assertions (161→169). Suite 334/0/3. v353. | 92 gaps | 92 gaps |
| 285 | S0 D09 vi visual mode — v/V enter visual mode, x/d/y delete/yank selection, ESC/v exit. Reverse video highlighting. 13 new assertions (148→161). Suite 334/0/3. v352. | 92 gaps | 92 gaps |
| 284 | S0 D09 vi search — / ? forward/backward search, n/N repeat, wrap-around. line_edit_search_internal(). 11 new assertions (137→148). Suite 334/0/4. v351. | 92 gaps | 92 gaps |
| 283 | S8 R02 PORTED — bedrock_convert_messages_to_converse() + bedrock_normalize_converse_response() ported. R02 all 14 portable functions done (100%). Suite 334/0/3. v350. | 93 gaps | 92 gaps |
| 282 | S7 X09 edge case expansion — 23 bedrock utility edge cases. Suite 334/0/3. v349. | 93 gaps | 93 gaps |
| 281 | S8 R10 — provider_query_local_context_length() — multi-endpoint context probing. R10 PORTED (35/43=81%). Suite 334/0/3. v348. | 94 gaps | 93 gaps |
| 280 | S8 R10 depth — provider_query_ollama_api_show() + provider_query_ollama_num_ctx(). 4 new assertions (335→339). Suite 334/0/3. v347. | 94 gaps | 94 gaps |
| 279 | S8 R10 depth — provider_detect_local_server_type() — probes LM Studio/Ollama/llama.cpp/vLLM endpoints via HTTP. 3 new assertions (332→335). Suite 334/0/3. v346. | 94 gaps | 94 gaps |
| 278 | S8 R10 depth — provider_add_model_aliases() + provider_get_context_length_from_provider_error(). 19 new assertions (313→332). Suite 334/0/3. v345. | 94 gaps | 94 gaps |
| 277 | S8 R10 depth — provider_extract_first_int() generic JSON extractor + refactor. 13 new assertions (300→313). Suite 334/0/4. v344. | 94 gaps | 94 gaps |
| 276 | S8 R10 depth — 5 context cache functions (path, load, save, get, invalidate). 15 new assertions (285→300). Suite 335/0/0. v343. | 94 gaps | 94 gaps |
| 275 | S8 R10 depth — CONTEXT_PROBE_TIERS + DEFAULT_FALLBACK_CONTEXT + MINIMUM_CONTEXT_LENGTH + get_next_probe_tier(). 21 new assertions (264→285). Suite 335/0/0. v342. | 94 gaps | 94 gaps |
| 274 | S8 R10 depth — batch 4 token estimation functions. 26 new assertions (238→264). Suite 335/0/0. v341. | 94 gaps | 94 gaps |
| 273 | S8 R02+R10 depth — batch 2 functions (convert_content_to_converse, extract_pricing). ~54 new assertions. Suite 335/0/0. v340. | 94 gaps | 94 gaps |
| 272 | S8 R02+R10 depth — batch 5 functions. 54 new assertions. Suite 335/0/0. v339. | 94 gaps | 94 gaps |
| 271 | S8 R02+R10 depth — batch 10 functions (5 R02 bedrock + 5 R10 model_metadata). 104 new assertions. Suite 335/0/0. v338. | 94 gaps | 94 gaps |
| 270 | S8 R04 Gemini depth — extract + build_contents. 36 new assertions (130→166). Suite 335/0/0. v337. | 94 gaps | 94 gaps |
| 269 | S8 R04 depth — tool_choice + thinking_config. 29 new assertions (101→130). Suite 335/0/0. v336. | 95 gaps | 94 gaps |
| 268 | S8 R04 depth — google_translate_tools_to_gemini(). 15 new assertions (86→101). Suite 335/0/0. v335. | 95 gaps | 95 gaps |
| 267 | S8 R04 depth — google_translate_tool_result(). 13 new assertions (73→86). Suite 335/0/0. v334. | 95 gaps | 95 gaps |
| 266 | S8 R04 depth — google_tool_call_extra_signature() + google_translate_tool_call(). 13 new assertions (60→73). Suite 335/0/0. v333. | 95 gaps | 95 gaps |
