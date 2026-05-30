(v361)

335/0/15, 289 test files. All pass. 78 gaps. Phase 298: S7 regex test expansion — 15 new edge case assertions (17→32).

Phase 298: S7 regex edge case expansion — extraction (empty string, group bounds, multi-group, invalid pattern), compile/search (empty/case-sensitive/invalid/NULL), replace (empty, single-match, NULL replacement). Bugfix: regex_extract() negative group guard. v361.
Phase 278: S8 R10 depth — provider_add_model_aliases() + provider_get_context_length_from_provider_error(). 19 new assertions (313→332). Suite 334/0/3. v345.
Phase 277: S8 R10 depth — provider_extract_first_int() refactor. Provider metadata 313-test suite (300→313).
Phase 272: S8 R02+R10 depth — batch 5 functions. 54 new assertions. Suite 335/0/0. v339.
Phase 271: S8 R02+R10 depth — batch 10 functions. 104 new assertions. Suite 335/0/0. v338.
Phase 270: S8 R04 Gemini depth — batch 3 functions. 36 new assertions (130→166). Suite 335/0/0. v337.
Phase 269: S8 R04 Gemini depth — tool_choice + thinking_config. 29 new assertions (101→130). Suite 335/0/0. v336.
Coverage: 292/1262 test files (23.1%). 94 structural gaps remain.
