# Plan — Next Phase (v326)

S0+S1+S3+S6 PORTED. L24+L25+L26+L27+L28 PORTED. F10 PORTED. S8 R01+R10+R04 PARTIAL, R03+R05-R09 WON'T PORT. 95 gaps.

**Next gap targets:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P2 | S0 D09 | Vi mode | o/O open line done (Phase 259). Remaining: % match, . repeat, / search, visual mode, count prefixes |
| P1 | S7 X01-X09 | Test coverage | Continue expanding test files (292 / 1262 parity). Pending: provider depth (R02,R03,R04), gateway edge cases |
| P1 | S8 R03 | Google OAuth | OAuth token exchange — WON'T PORT (cloudcode-only) |
| P1 | S8 R02 | Bedrock depth | Model discovery, region auto-detection — PARTIAL |
| P1 | S8 R04 | Gemini native | Native API format translation — PARTIAL |
| P1 | S4 T01-T18 | TUI backend | JSON-RPC gateway, transport, render, entry |
| P2 | S5 C01-C30 | CLI ecosystem | Setup wizard, doctor, config, profiles |
