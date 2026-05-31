# State — Slermes C (v453)
338/?/13. Phase 397: Azure Provider Test Expansion — S7 X03 EXPANDED.
   test_provider_azure.c — 40 new assertions (54→94 total).
   4 new test functions + 1 bug fix (missing finish_reason extraction in parse_response).
   New coverage:
   - test_url_edge_cases — proxy URL, custom model, empty base (3)
   - test_header_edge_cases — empty key, NULL key, long key (3)
   - test_parse_response_edge_cases — empty choices, no choices, null content,
     length finish with finish_reason, no usage (6)
   - test_parse_stream_edge_depth — empty delta, empty chunk, length finish,
     whitespace delta (4)
   Bug fix: azure_parse_response — extract finish_reason from choice level
     (same pattern as xAI fix in Phase 396).
53 gaps. S7 X03 EXPANDED (Azure provider tests).
S7: 18 clusters (X03 improved, X04 improved, X10 improved, X08 improved, X07 improved, X06 improved, X09 ported).
