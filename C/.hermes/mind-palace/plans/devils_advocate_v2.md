# Hermes C — Devil's Advocate v2 (May 21 AM)

## 4-Phase DA: Full Sweep of C Translation

### PHASE 1: CLAIMS

#### Claim C1: "C codebase has 1:1 parity with itself (C/ == slermes/)"
**Source:** parity_loop.sh
**Trust:** HIGH
**Verify:** `bash parity_loop.sh` — 0 issues. All 88 source files present, normalized diff shows only 2 intentional diffs (config.c SLERMES_API_KEY fallback + llm_client.c whitespace). Zero hermes symbols in slermes binary. Zero .hermes paths in slermes source.
**Re-check:** Still tight.

#### Claim C2: "21 tests pass"
**Source:** bash test_runner.sh
**Trust:** HIGH
**Verify:** 10 lib + 2 plugin + 9 integration = 21/21 PASS, 0 FAIL, 0 SKIP.
**Re-check:** Still tight.

#### Claim C3: "LLM client makes live API calls"
**Source:** slermes "Say exactly: Hello from Slermes C" 
**Trust:** HIGH
**Verify:** Output: "Hello from Slermes C" — 1603ms round trip to DeepSeek API. Also verified via curl independently.
**Re-check:** Still tight.

#### Claim C4: "5 bugs fixed in this session"
**Source:** git log, manual grep
**Trust:** HIGH
**Verify:** B1: `grep "...ype" llm_client.c` → 0 matches. B2: Chunked TE code present in http.c. B3: "default" skip in config.c. B4: SLERMES_API_KEY in .env parser. B5: SLERMES_WEBHOOK_PORT in server.c.
**Re-check:** All verified present.

#### Claim C5: "C codebase has ~165 feature gaps vs Python hermes"
**Source:** GAP_ANALYSIS.md
**Trust:** HIGH
**Verify:** Count confirmed: 12 browser tools + 15 gateway platforms + 5 security tools + 40+ CLI commands + 30+ providers + 10+ tool categories = ~165.
**Re-check:** Accurate.

### PHASE 2: VERIFICATION — Cross-Reference

**DA-1: Code vs Docs**
| Walkway Claim | Code Reality | Status |
|--------------|-------------|--------|
| "17 tools" (prestige v1) | 21 tools | ⚠️ STALE — v1 doc, fixed in v2 |
| "17 tools" (state v1) | 21 tools | ⚠️ STALE — v1 doc, fixed in v2 |
| "703KB binary" | 809KB | ⚠️ STALE — code grew |
| "No API key" | DeepSeek key configured | ⚠️ STALE — now has API access |
| "0 compiler warnings" | 8 pre-existing warnings | ⚠️ STALE — warnings exist (format-truncation, unused params) |

**DA-2: Python Feature Gap — Verified Subset**
| Feature | Python LOC | C LOC | Gap Ratio |
|---------|-----------|--------|-----------|
| CLI commands | ~11k (cli.py) | ~1.5k (cli.c + commands.c) | 7:1 |
| Tools | 40+ tools | 21 tools | 2:1 |
| Gateway platforms | 20+ platforms | 7 platforms | 3:1 |
| Security tools | 5 tools (~3k LOC) | 0 tools | ∞ |
| Provider support | 30+ providers | 1 hardcoded | 30:1 |
| Browser automation | 12 tools | 0 tools | ∞ |
| Test infrastructure | ~17k tests | 21 tests | 800:1 |

**DA-3: Cold Gap Prioritization**
| Priority | Gap | Why | Lines of C needed (est) |
|----------|-----|-----|------------------------|
| P0 | CLI parity (50+ commands) | Blocks daily use | ~5,000 |
| P0 | Browser automation (12 tools) | No web interaction at all | ~3,000 |
| P0 | Security (dangerous cmd approval) | Unsafe without it | ~1,500 |
| P0 | Provider profiles (30+) | Only DeepSeek works | ~2,000 |
| P1 | Gateway platforms (15+) | Can't reach users on most platforms | ~200 each |
| P1 | Multi-platform gateway | Can only run 1 platform at a time | ~500 |
| P1 | Skills hub/sync | No community skill sharing | ~1,000 |
| P1 | Terminal backends (docker, ssh) | Only local terminal | ~2,000 |

### PHASE 3: RISK ASSESSMENT

| Risk | Assessment | Mitigation |
|------|------------|------------|
| **Confirmation bias** | Tests only verify compilation + 21 smoke tests. No integration test against live API in CI. | Add API smoke test to test_runner.sh |
| **Survivorship bias** | Only tested DeepSeek. Other providers (OpenAI, Anthropic) would need different auth headers. | Abstract provider interface |
| **Phantom PASS** | 21 tests exit 0 but don't verify output correctness (e.g., "JSON parse error" was still exit 0) | Add output content checks |
| **Scope creep** | 165+ gaps is overwhelming. Must resist trying to do all at once. | Strict P0-first ordering. Only P0 until all P0 done. |
| **State staleness** | C codebase drifts from Python upstream. Python gets new features daily. | `python3 C/digest.py` after every git pull |

### PHASE 4: MITIGATION PLAN

| Risk | Monitoring | Revert Threshold |
|------|------------|-----------------|
| Scope creep | Track only P0 items. P1+ go on backlog | No P1 work until all P0 green |
| Phantom PASS | Add output content verification to tests | Any test that passes on garbage |
| Staleness | Run digest.py after every pull | Any Phase gap > 2 versions |
| Security | No dangerous command approval = no gateway production use | Block production gateway until P0 security done |
