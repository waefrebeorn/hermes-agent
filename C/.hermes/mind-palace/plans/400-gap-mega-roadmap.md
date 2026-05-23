# True 1:1 Parity Gap List (~338 gaps)
**DA v6 verified: 2026-05-27 | Root README + state.md are authoritative**

**Key:** ✅ Done | ⚠️ Partial | ❌ Missing | 🔵 Upstream drift

The full gap list from DA v5 (May 22) is **superseded by DA v6 (May 27)**. 
Every count below is verified from source code — not inherited from prior documents.

---

## A. Config System — Depth gaps: 2 (98% done)

| # | Gap | Status | Notes |
|---|-----|--------|-------|
| A01 | Config profiles | ✅ | Named profiles (dev/prod/test) selectable at runtime |
| A02 | Schema auto-generation | ❌ | `hermes config schema` — JSON Schema from hermes_config_t |
| A03 | Env var expansion | ✅ | `${VAR:-default}` syntax in YAML values |
| A04 | Config includes/imports | ✅ | `!include path.yaml` directive |
| A05 | Config file watching | ❌ | inotify-based hot-reload on YAML change |
| A06 | Config version migration | ❌ | Auto-migrate between config_version N → N+1 |

---

## B. Provider System — 35 gaps (87% done)

### Provider infrastructure — ALL done ✅
B10-B21: credential pool (rotation, weighted selection, multi-key), provider plugins, model catalog, metadata (pricing, context, capabilities), fallback routing — all implemented.

### Provider-specific API features — 7 remaining (18 done)
| # | Gap | Status | Notes |
|---|-----|--------|-------|
| B22 | OpenAI: streaming response depth | ✅ | Token timing, usage metadata |
| B23 | OpenAI: json_mode | ✅ | Force JSON output mode |
| B24 | OpenAI: strict structured outputs | ✅ | response_format with json_schema enforcement |
| B25 | OpenAI: function calling depth | ✅ | Parallel calls, tool_choice enforcement |
| B26 | Anthropic: thinking blocks | ✅ | Extended thinking with budget_tokens |
| B27 | Anthropic: cache control | ✅ | Ephemeral cache breakpoints on messages |
| B28 | Anthropic: batched messages | ❌ | Batch API for async inference |
| B29 | Google: safety_settings | ✅ | Per-category harm block thresholds |
| B30 | Google: generation_config | ✅ | Top_k, candidate_count, stop sequences |
| B31 | Google: system_instruction | ✅ | Structured system prompt format |
| B32 | DeepSeek: FIM mode | ✅ | Fill-in-middle for code completion |
| B33 | DeepSeek: context caching | ✅ | Prefix caching, configurable x-ds-cache-ttl |
| B34 | xAI: encrypted reasoning | ✅ | Thread encrypted_content across turns |
| B35 | xAI: web_search tool | ✅ | Server-side web search via Responses API |
| B36 | xAI: response_format | ❌ | JSON mode for Grok responses |
| B37 | Azure: deployment_id routing | ✅ | Map model names to deployment IDs |
| B38 | Azure: api_version handling | ✅ | Track and manage API version |
| B39 | Bedrock: inferenceProfile | ✅ | Region-based inference profiles |
| B40 | Bedrock: guardrails | ✅ | Content filter configuration |
| B41 | Bedrock: trace | ✅ | Response generation trace |
| B42 | Bedrock: system_prompt format | ❌ | AWS-specific system prompt structure |
| B43-B46 | OpenRouter: provider preferences | ✅ | Order, allow_fallbacks, data_control, ignore |
| B47-B50 | Remaining API quirks | ❌ | Various per-provider edge cases |

---

## C. Gateway — 0 gaps (100% ✅)

All 19 platforms fully wired. Matches/exceeds Python.

---

## D. Plugins — 48 gaps (19% done)

| # | Gap | Status | Notes |
|---|-----|--------|-------|
| D01 | Plugin system: core loader | ✅ | .so discovery, lifecycle, interface |
| D02-D04 | Kanban, Honcho, Spotify | ✅ | Real .so plugins, 90 tests combined |
| D05-D17 | 13 plugin backends | ❌ | memory providers, image_gen, video_gen, achievements, observability, browser, context_engine, model-providers, platforms, google_meet, disk-cleanup, example-dashboard, teams_pipeline |

---

## E. Tools — 24 gaps (95% done)

67 registered ops across 37 handler files. Remaining: 6 CDP/plugin-blocked stubs (browser_cdp, computer_use, feishu doc+drive, video analyze+generate, moa).

---

## F. Agent Loop — 31 gaps (86% done)

G01-G36 implemented. Missing: grace call, prefill messages, stream diagnostic, background review hook, personality switching.

---

## G. CLI — 33 gaps (87% done)

74 commands implemented (exceeds Python's 69). Missing: spinner animation, activity feed, autocomplete depth.

---

## H. Libraries — 11 gaps (41% done)

16 archives. Ported: libpath, libdatetime, libcsv. Missing equivalents: hash, uuid, regex, base64, html, enum, datetime (done), difflib, textwrap, glob depth, socket abstraction, threading wrappers.

---

## I. Tests — 34 gaps (64% done)

81 files, 2,142 assertions, 117/0/0 suite. Target: ~200 files for credible coverage.

---

## J. Error Types — 10 gaps (50% done)

K01-K05 (18 codes) implemented. Missing: K06-K20 (IIOError, ConfigurationError, ConnectionError, AuthenticationError, AuthorizationError, ValidationError, QuotaError, ModelError, ToolError, PluginError, GatewayError, SessionError, SerializationError, InternalError, AbortError).

---

## K. Build/Doc — 1 gap (95% done)

O02 Windows build remains. Everything else: Docker, CI, cross-compile, man page, Doxygen, CHANGELOG, SECURITY, ARCHITECTURE.

---

## Source of Truth

Do NOT use stale gap counts from DA v5 or earlier. The DA v6 document
(`.hermes/mind-palace/plans/devils_advocate_v6.md`) has the only verified numbers.
Root README and `state.md` have the live dashboard.

This file is a summary map. For per-gap detail, read DA v6.
