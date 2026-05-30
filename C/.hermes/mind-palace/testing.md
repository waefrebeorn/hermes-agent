(v353)

334/0/3, 292 test files. All pass. Gap: 292 C vs ~1262 Python (23.1% parity).

Phase 281: S8 R10 — provider_query_local_context_length() — multi-endpoint context probing. R10 PORTED (35/43=81%). 4 new assertions (339→343). Gap count 94→93. Suite 334/0/3. v348.
Phase 283: S8 R02 — bedrock_convert_messages_to_converse() + bedrock_normalize_converse_response() ported. R02 PORTED (14/14=100%). 84 new assertions (228 total). Suite 334/0/3. v350. S8 2→1 gap. Total 93→92 gaps.
Phase 280: S8 R10 depth — provider_query_ollama_api_show() + provider_query_ollama_num_ctx(). 4 new assertions (335→339). Suite 334/0/3. v347.
Phase 279: S8 R10 depth — provider_detect_local_server_type() — HTTP-based local inference endpoint detection. 3 new assertions (332→335). Suite 334/0/3. v346.
Phase 278: S8 R10 depth — provider_add_model_aliases() + provider_get_context_length_from_provider_error(). 19 new assertions (313→332). Suite 334/0/3. v345.
Phase 277: S8 R10 depth — provider_extract_first_int() refactor. Provider metadata 313-test suite (300→313).
Phase 272: S8 R02+R10 depth — batch 5 functions. 54 new assertions. Suite 335/0/0. v339.
Phase 271: S8 R02+R10 depth — batch 10 functions. 104 new assertions. Suite 335/0/0. v338.
Phase 270: S8 R04 Gemini depth — batch 3 functions. 36 new assertions (130→166). Suite 335/0/0. v337.
Phase 269: S8 R04 Gemini depth — tool_choice + thinking_config. 29 new assertions (101→130). Suite 335/0/0. v336.
Coverage: 292/1262 test files (23.1%). 94 structural gaps remain.
