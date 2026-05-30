# Entry — Slermes C Parity (v338)

Phase 270: S8 R04 Gemini depth — google_extract_multimodal_parts() + google_tool_call_extra_from_part() + google_build_gemini_contents() ported from gemini_native_adapter.py.
Phase 271: S8 R02+R10 depth — batch 10 functions (5 R02 bedrock + 5 R10 model_metadata). bedrock_is_anthropic_model(), bedrock_model_supports_tool_use(), bedrock_resolve_auth_env_var(), bedrock_has_credentials(), bedrock_resolve_region(), model_grok_supports_reasoning_effort(), provider_is_openrouter_base_url(), provider_is_custom_endpoint(), provider_is_known_base_url(), provider_auth_headers(), provider_coerce_reasonable_int(). 104 new assertions. 94 gaps remain.
94 gaps remain (S0+S1+S3+S6+R04+S8 R03+R05-R09 WON'T PORT).
