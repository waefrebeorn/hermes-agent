# Plan — Next Phase (v284)

S0+S1+S3+S6 all PORTED. L24+L25+L26+L27 PORTED. 105 gaps remain across 7 sectors.

**Next gap targets:**

| Priority | Sector | Gap | Action |
|----------|--------|-----|--------|
| P2 | S1 L28 | Agent init (PARTIAL) | Compare agent_init.py functions vs C agent_init() + agent_configure_from_config(). Port remaining standalone functions. |
| P1 | S7 X01-X09 | Test coverage | Continue expanding test files (282 / 1262 parity). |
| P1 | S8 R01 | Provider adapters | Anthropic extended thinking, streaming variants |
| P1 | S4 T01-T18 | TUI backend | JSON-RPC gateway, transport, render, entry |
| P2 | S5 C01-C30 | CLI ecosystem | Setup wizard, doctor, config, profiles |
| P1 | S9 P01 | Plugin loading | .so-only → plugin lifecycle |
