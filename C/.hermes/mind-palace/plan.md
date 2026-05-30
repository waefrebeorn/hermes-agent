# Plan — Next Phase (v345)

S0+S1+S3+S6 PORTED. L24+L25+L26+L27+L28 PORTED. F10 PORTED. S8 R01+R10+R04 PORTED, R03+R05-R09 WON'T PORT. 94 gaps.

**Next gap targets:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P1 | S8 R10 | model_metadata depth | 31/43 functions ported (72%). Remaining portable: detect_local_server_type (HTTP probe), _iter_nested_dicts (Python generator, WON'T PORT). Next: try R02 Bedrock or D09 vi mode |
