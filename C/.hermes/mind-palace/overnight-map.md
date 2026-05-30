# Overnight Map — Recent Phases (v356)

| Phase | Change | Before | After |
|-------|--------|--------|-------|
| 287 | S8 R01 endpoint detection — 14 functions PORTED, 69 tests, Bearer auth wiring. Suite 335/0/14. v354. | 92 gaps | 90 gaps |
| 287b | S10 F07 trajectory saving vaulted — trajectory.c already PORTED. v354. | 90 gaps | 89 gaps |
| 287c | S5 C06 env_loader — load_slermes_env(). Suite 335/0/14. v354. | 89 gaps | 88 gaps |
| 288 | S5 C02 doctor — /doctor command. v355. | 88 gaps | 87 gaps |
| 288b | C04/C07/C08/C09 stale cleanup — /model, /profile, /config all exist. v355. | 87 gaps | 84 gaps |
| 289 | S7 test_trajectory.c — 19 assertions (294 test files). v355. | 84 gaps | 84 gaps |
| 290 | C11 OAuth status in /secrets. v355. | 84 gaps | 84 gaps |
| 291 | S7 test_regex.c — 17 assertions (17→294 test files). v355. | 84 gaps | 84 gaps |
| 292 | S5 C13 Gateway CLI — gateway status + list + start subcommands. Suite 323/0/16. v356. | 84 gaps | 84 gaps |
| 266 | S8 R04 depth — google_tool_call_extra_signature() + google_translate_tool_call(). 13 new assertions (60→73). Suite 335/0/0. v333. | 95 gaps | 95 gaps |
| 267 | S8 R04 depth — google_translate_tool_result(). 13 new assertions (73→86). Suite 335/0/0. v334. | 95 gaps | 95 gaps |
| 268 | S8 R04 depth — google_translate_tools_to_gemini(). 15 new assertions (86→101). Suite 335/0/0. v335. | 95 gaps | 95 gaps |
| 269 | S8 R04 depth — tool_choice + thinking_config. 29 new assertions (101→130). Suite 335/0/0. v336. | 95 gaps | 94 gaps |
| 270 | S8 R04 depth — extract + build_contents. 36 new assertions (130→166). Suite 335/0/0. v337. | 94 gaps | 94 gaps |
| 271 | S8 R02+R10 depth — batch 10 functions (5 R02 bedrock + 5 R10 model_metadata). 104 new assertions. Suite 335/0/0. v338. | 94 gaps | 94 gaps |
| 272 | S8 R02+R10 depth — batch 5 functions (convert_tools, estimate_tokens, resolve_verify, extract context/max_tokens). 54 new assertions. Suite 335/0/0. v339. | 94 gaps | 94 gaps |
| 273 | S8 R02+R10 depth — batch 2 functions (convert_content_to_converse, extract_pricing). ~54 new assertions. Suite 335/0/0. v340. | 94 gaps | 94 gaps |
| 274 | S8 R10 depth — batch 4 token estimation functions (count_image_tokens, message_chars, messages_tokens_rough, request_tokens_rough). 26 new assertions (238→264). Suite 335/0/0. v341. | 94 gaps | 94 gaps |
| 275 | S8 R10 depth — CONTEXT_PROBE_TIERS + DEFAULT_FALLBACK_CONTEXT + MINIMUM_CONTEXT_LENGTH + get_next_probe_tier(). 21 new assertions (264→285). Suite 335/0/0. v342. | 94 gaps | 94 gaps |
| 276 | S8 R10 depth — 5 context cache functions (path, load, save, get, invalidate). 15 new assertions (285→300). Suite 335/0/0. v343. | 94 gaps | 94 gaps |
| 277 | S8 R10 depth — provider_extract_first_int() generic JSON extractor + refactor. 13 new assertions (300→313). Suite 334/0/4. v344. | 94 gaps | 94 gaps |
| 278 | S8 R10 depth — provider_add_model_aliases() + provider_get_context_length_from_provider_error(). 19 new assertions (313→332). Suite 334/0/3. v345. | 94 gaps | 94 gaps |
| 279 | S8 R10 depth — provider_detect_local_server_type() — probes LM Studio/Ollama/llama.cpp/vLLM endpoints via HTTP. 3 new assertions (332→335). Suite 334/0/3. v346. | 94 gaps | 94 gaps |
| 280 | S8 R10 depth — provider_query_ollama_api_show() + provider_query_ollama_num_ctx() — Ollama context query via POST /api/show. 4 new assertions (335→339). Suite 334/0/3. v347. | 94 gaps | 94 gaps |
| 281 | S8 R10 — provider_query_local_context_length() — multi-endpoint context probing. R10 PORTED (35/43=81%). 4 new assertions (339→343). Suite 334/0/3. v348. | 94 gaps | 93 gaps |
| 282 | S7 X09 edge case expansion — 23 bedrock utility edge cases (is_context_overflow, classify_error, extract_provider, get_context_length). Suite 334/0/3. v349. | 93 gaps | 93 gaps |
| 283 | S8 R02 PORTED — bedrock_convert_messages_to_converse() + bedrock_normalize_converse_response() ported. R02 all 14 portable functions done (100%). Suite 334/0/3. v350. | 93 gaps | 92 gaps |
| 284 | S0 D09 vi search — / ? forward/backward search, n/N repeat, wrap-around. line_edit_search_internal(). 11 new assertions (137→148). Suite 334/0/4. v351. | 92 gaps | 92 gaps |
| 285 | S0 D09 vi visual mode — v/V enter visual mode, x/d/y delete/yank selection, ESC/v exit. Reverse video highlighting. 13 new assertions (148→161). Suite 334/0/3. v352. | 92 gaps | 92 gaps |
|| 293 | S5 stale sweep — C04 PORTED, C05 PORTED, C10+C12 WON'T PORT. S5 25→20 gaps. Total 84→80 gaps. v356. |
| 294 | S5 C14 webhook CLI — /webhook list/add/remove PORTED. S5 20→19 gaps. Total 80→79 gaps. v357. |
| 295 | S5 stale sweep — C01 setup wizard PORTED (slermes setup exists). /platforms enhanced with config display + -v credential check. S5 19→18 gaps. Total 79→78 gaps. v358. |
| 298 | S7 X09 regex edge case expansion — 15 new assertions (17→32). Extraction (empty string, group bounds, multi-group, invalid pattern), compile/search (empty/case-sensitive/invalid/NULL), replace (empty, single-match, NULL replacement). Bugfix: regex_extract() negative group guard. Suite 335/0/15. v361. |
