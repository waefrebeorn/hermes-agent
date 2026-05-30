# Upstream Feature-Gap Methodology

**Context:** May 28 2026. Upstream v0.15.0 (v2026.5.28) — The Velocity Release.
747 PRs, 282K insertions since prior baseline. run_agent.py refactored 16k→3.8k LOC.

## The Pitfall We Just Hit

We were calling C modules "stale" (already ported) based on **file name matching**:

```
Python has conversation_loop.py → C has agent_loop.c → "A01 is STALE"
```

This is WRONG. The correct question is:

```
Python conversation_loop.py has:
  - surrogate sanitization
  - 6+ retry types (codex, anthropic, nous, copilot, thinking_sig, encrypted)
  - background review fork isolation via ContextVars
  - interruptible API calls
  - task-id exposure for cross-agent state registry

C agent_loop.c has:
  - session CRUD
  - snapshot/restore
  - crash recovery
  - auto-prune
  - basic retry/fallback

GAP: C has richer session management but MISSES 6 specific features Python added
```

## Proper Audit Protocol

For each claimed "C equivalent" module:

### Step 1: Get BOTH public APIs

```bash
# Python functions
grep -n '^def ' agent/<module>.py | grep -v '__'

# C functions
grep -nE '^(char |int |bool |void |[a-z]+_)' src/agent/<module>.c | grep -v static
```

### Step 2: Compare feature sets, not names

Key upstream features we may be missing (from v0.15.0 release notes):

| Feature | Where | C Status |
|---------|-------|----------|
| Surrogate sanitization | conversation_loop.py | MISSING |
| 6-type retry tracking | conversation_loop.py | MISSING (C has basic retry) |
| Content-policy immediate fallback | conversation_loop.py | MISSING |
| Background review isolation | agent_loop.c → conversation_loop.py | MISSING (ContextVars) |
| Interruptible API calls | chat_completion_helpers.py | MISSING |
| Concurrent tool execution | tool_executor.py | MISSING (C is serial) |
| Per-message cache markers | prompt_caching.py | MISSING |
| Kanban platform (104 PRs) | plugins/kanban/ | C has kanban tool but simpler |
| Promptware defense | tools/threat_patterns.py | MISSING |
| Bitwarden Secrets Manager | plugins/secrets/ | MISSING |
| env_passthrough GHSA hardening | tools/env_passthrough.py | Partial (just wired config) |
| session_search single-shape API | hermes_state.py → rebuilt | C built for OLD API |
| Plugin surface (TTS, transcription hooks) | plugins/ | MISSING |
| ntfy + Discord/Mattermost as plugins | gateway/platforms/ | C has 19 platforms, missing ntfy |

### Step 3: Check what C HAS that Python doesn't

C may have features Python doesn't (e.g. C's session CRUD, snapshot/restore, crash recovery). These are NOT relevant to "stale" — C's extra features don't make Python's features magically ported.

### Step 4: Classify correctly

| Verdict | Meaning |
|---------|---------|
| **PORTED** | C has >= 80% of Python's feature surface for this module |
| **PARTIAL** | C has 20-80% of features |
| **REAL GAP** | C has < 20% of features or 0% |
| **STALE** | Only use this when the battleship claim is factually wrong (e.g. "file missing" when file exists with all features) |

Don't use "STALE" as a shortcut for avoiding implementation work. A proper claim verification shows the gap, doesn't erase it.

## Key Upstream Changes to Monitor

1. **run_agent.py → 14 agent modules**: The old single-file architecture is gone. Each extracted module may have grown features since extraction.
2. **session_search single-shape**: C was porting the OLD three-param session_search. The new API is discovery/scroll/browse.
3. **Kanban platform**: 104 PRs of maturation. C's kanban tool is simple.
4. **Plugin surface**: `register_tts_provider()`, `register_transcription_provider()`, `register_auxiliary_task()` — new API hooks.
5. **Promptware defense**: `threat_patterns.py` with Brainworm detection. C has nothing equivalent.
6. **env_passthrough**: GHSA-rhgp-j443-p4rf filter applied to config.yaml path. C's is wired but basic.

## Reference Comparison Data (from May 28)

### conversation_loop.py (4606 LOC) vs agent_loop.c (1600 LOC)
Python: run_conversation() with surrogate sanitization, 6 retry types, contextvar isolation, interruptible calls
C: agent_run_conversation() with basic retry/fallback, session CRUD, snapshots, crash recovery
**Gap: 6 specific Python features not ported**

### chat_completion_helpers.py (2468 LOC) vs llm_client.c (1500 LOC)
Python: interruptible_api_call, build_api_kwargs, build_assistant_message, try_activate_fallback, handle_max_iterations, cleanup_task_resources, interruptible_streaming_api_call, anthropic-specific path, bedrock-specific path
C: llm_chat_completion, llm_chat_completion_stream, llm_truncate_context, llm_compress_context, llm_background_review
**Gap: Fallback activation, interruption, provider-specific paths, kwargs builder**

### tool_executor.py vs libtooldispatch
Python: execute_tool_calls_concurrent, execute_tool_calls_sequential (concurrent mode)
C: is_destructive_command, extract_error_preview, extract_file_mutation_targets, is_multimodal_tool_result (helpers only, no dispatch)
**Gap: No concurrent tool execution in C**

### prompt_caching.py vs prompt_caching.c
Python: per-message cache markers, Anthropic-native cache_control, TTL-based
C: cache stats tracking, system prompt caching, basic validity
**Gap: Per-message markers, Anthropic-native format**
