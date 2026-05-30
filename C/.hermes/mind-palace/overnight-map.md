# Overnight Map — Recent Phases (v345)

| Phase | Change | Before | After |
|-------|--------|--------|-------|
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
