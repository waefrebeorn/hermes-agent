
# DA Audit — 2026-05-21

## Verdict

**Phase 5: P0 build exists. Not P0 complete.**

No runtime verification vs Python ground truth. No test suite pass. No DA approvals without evidence.

## Dimension 1: Config Keys

| Metric | C | Python | Coverage |
|--------|---|---|----------|
| Leaf keys parsed | 206 | 322 | **64%** |
| Top-level sections | ~35 | ~35 | ~100% |
| v1 nested format | Dotted paths parsed | Full nested YAML | ~70% |
| Env var fallback | 35 vars | 20+ vars | **~100%** |
| Profile support | 2-profile (default/env) | Multi-profile native | ~60% |

Config missing in C: `credential_pool_strategies`, `toolsets` (array), `approval.*` subkeys, `kanban.*`, `stt.*`, `tts.*`, `vision.*`, `auxiliary.*`, `gateway.*` timeouts, `orchestrator.*`, `guardrail.*`, `context.*`, `discord.*`, `telegram.*` platform-specific keys, `model_catalog.*`, `curator.*`.

## Dimension 2: Tools

| Metric | C | Python | Coverage |
|--------|---|---|----------|
| Registered | 74 | ~80 | **~92%** |
| Expected (verify cmd) | 54 all found | — | **100%** |
| Unique toolsets | 4 | 8+ | 50% |
| Descriptive | 74/74 have descriptions | Varies | 100% |

Missing from C: several MCP resource-related tools, kanban sub-tools, environment sub-tools (docker/ssh), some plugin lifecycle management.

## Dimension 3: CLI Commands

| Metric | C | Python | Coverage |
|--------|---|---|----------|
| Slash commands | 72 | 85 | **85%** |
| Help text | All have descriptions | All | 100% |
| Aliases | Yes (75% have /short) | Yes | 90% |
| Argument parsing | Basic | Full argparse | 50% |

Missing: `hermes status`, `hermes logs`, `hermes doctor`, `hermes dashboard`, `hermes completion`, `hermes setup`, `hermes update` — these are Python subcommands not exposed as slash commands.

## Dimension 4: Providers

| Metric | C | Python | Coverage |
|--------|---|---|----------|
| Implemented | 3 (openai, anthropic, google) | 29 plugins | **10%** |
| Credential pool | Yes | Yes | 80% |
| Fallback routing | Yes | Yes | 80% |
| Budget tracking | Yes | Yes | 80% |
| Streaming | OpenAI only | All 3 | 33% |

Missing: deepseek, xai, gemini (native), openrouter, groq, together, fireworks, perplexity, bedrock, azure, ollama, etc.

## Dimension 5: Gateway Platforms

| Metric | C | Python | Coverage |
|--------|---|---|----------|
| Platform files | 19 | ~20 | **95%** |
| Session binding | Yes | Yes | 100% |
| Rate limiting | Yes | Yes | 100% |
| Connection pool | Yes | Yes | 100% |
| Platform auth | 19/19 basic | Full OAuth flows | ~70% |

Full OAuth (Discord bot token flow, Telegram webhook verification, etc.) not implemented in C.

## Dimension 6: MCP

| Metric | C | Python | Coverage |
|--------|---|---|----------|
| Core protocol | Yes (lib/libmcp/mcp.c) | Full framework | **70%** |
| Stdio transport | Yes | Yes | 100% |
| SSE transport | Yes | Yes | 100% |
| Auth/OAuth | Basic | Full | 50% |
| Resource listing | Yes | Yes | 100% |
| Prompt templates | Yes | Yes | 100% |
| Tools discovery | Yes | Yes | 100% |
| Tool calling | Yes | Yes | 100% |

C MCP missing: streaming responses, progress notifications, logging, sampling, roots.

## Dimension 7: Plugins

| Metric | C | Python | Coverage |
|--------|---|---|----------|
| Core system | Yes | Yes | **100%** |
| Discovery | Yes | Yes | 100% |
| Lifecycle | Yes (init/free) | Full lifecycle | 70% |
| Example plugins | 3 | 12+ | 25% |
| Plugin APIs | Basic .so | Rich plugin API | 30% |

## Dimension 8: Code Size

| Metric | C | Python | Ratio |
|--------|---|---|-------|
| Source files | 121 | ~900 | 13% |
| Non-test LOC | ~53K | ~431K | **12.3%** |
| Test LOC | 1.3K | ~430K | **<1%** |
| Binary | 2.9MB static | N/A | N/A |

## Dimension 9: Tests

| Metric | C | Python | Coverage |
|--------|---|---|----------|
| Test files | 12 | ~900 | **<1%** |
| Test LOC | 1,328 | ~430K | **<1%** |
| Runtime tests | Manual | Pytest suite | **0%** |
| CI/CD | None | GitHub Actions | **0%** |

## Dimension 10: Runtime Stability

| Metric | Status |
|--------|--------|
| Compiles | ✅ `make -j` passes with zero errors |
| --help | ✅ Usage text, exit 0 |
| --version | ✅ v0.14.1-wubu, exit 0 |
| Interactive CLI | ✅ All 72 commands dispatch without crash |
| One-shot mode | ✅ Fails gracefully (no API key) |
| Session DB | ✅ 231 sessions loaded from SQLite |
| Gateway start | ✅ Graceful failure without API tokens |
| Cron commands | ✅ Scheduler status reported |
| Plugin listing | ✅ 0 loaded (expected — no plugins installed) |
| Tool listing | ✅ 74 registered, all described |
| Config loading | ✅ Flat keys OK; v1 nested model/provider not mapped |
| Session search | Implemented but not tested |
| Memory system | Implemented but not tested |
| Delegation | Implemented but not tested |
| Background processes | Implemented but not tested |
| Browser tools | Implemented but not tested |
| MCP tools | Implemented but not tested |

## Overall Score by Weight

Weighted by user priorities: config size, tool parity, runtime stability.

- **Config**: 64% ✓
- **Tools**: 92% ✓
- **Commands**: 85% ✓
- **Providers**: 31% ✗
- **Gateway**: 95% ✓
- **MCP**: 70% ≃
- **Plugins**: 25% ✗
- **Tests**: 0% ✗
- **Runtime**: 60% ≃ (no Python parity)

**Weighted: ~45%** (up from 8% in DA v3)
