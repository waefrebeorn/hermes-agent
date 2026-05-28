══════════════════════════════════════════════════════
PERPETUAL GOAL — TWO PROJECTS
══════════════════════════════════════════════════════

P0: WuBu Hermes C Translation  (until complete)
P1: ByTropix GPU Inference      (DA review + maintenance)

══════════════════════════════════════════════════════
PROJECT A: WuBu Hermes — C Translation
══════════════════════════════════════════════════════

/home/wubu/hermes-agent-dev/                         -- dev clone (Python + C)
/home/wubu/hermes-agent-dev/C/                       -- C translation directory
/home/wubu/hermes-agent-dev/C/DEPENDENCIES.md         -- full Python->C dep map
/home/wubu/hermes-agent-dev/C/digest.py               -- digestion automation
/home/wubu/hermes-agent-dev/C/digestion.md            -- digestion workflow
/home/wubu/hermes-agent-dev/C/Makefile                -- 5-phase build system
/home/wubu/hermes-agent-dev/C/include/hermes.h        -- master header
/home/wubu/hermes-agent-dev/C/src/main.c              -- entry point stub
/home/wubu/.hermes/hermes-agent/                      -- production (for reference only)

Remotes: origin=NousResearch/hermes-agent  wubu=waefrebeorn/slermes
Branch:  main (276 C commits, 7583 upstream commits behind)
No PRs to upstream until full parity + upstream sync done.

Phase 1-5: Complete (stub-hunt done). Current phase: REAL PARITY + UPSTREAM SYNC
33 gaps across 5 sectors (S0-S4). S4 = Upstream Drift (NEW, 7 gaps, 7583 commits behind).
See battleship-v27.md for the active gap map.

Every pull: python3 C/digest.py → pick next undone item → implement → build → commit

══════════════════════════════════════════════════════
PROJECT B: ByTropix GPU Inference Engine
══════════════════════════════════════════════════════

/home/wubu/bytropix/                                  -- source repo
/home/wubu/bytropix/src/                              -- CUDA kernels + gguf reader
/home/wubu/bytropix/tools/                            -- infer_text.c + scripts
/home/wubu/bytropix/include/                          -- headers
/home/wubu/bytropix/.hermes/mind-palace/              -- prestige docs
/home/wubu/llama.cpp/build/bin/llama-cli              -- reference binary
/models/Qwen3.6-35B-A3B-UD-IQ2_M.gguf                -- test model
/tmp/ref_last_hidden.bin                              -- reference hidden dumps
/tmp/dump_layers/                                     -- layer dumps (DUMP_LAYER_DIR)

Known fixed: Q5_K denormals (bf573b8), GQA interleave (cdccde2), MoE IQ3_XXS (9093c61)
Active issue: GPU SSM state sync (hybrid workaround)
Dead code: F32 cuBLAS SSM path (VRAM pressure)

══════════════════════════════════════════════════════
you are an ai. no time goals. yolo mode. work until done. no status summaries. no excuses. no choices about what to do next.

read state.md. read goal-mantra.md. read plan.md. read prestige_prompt.md. read overnight-map.md.
write a goal-mantra.md for hermes-dev if one doesn't exist.

always pick from P0 first (Hermes C translation). only work P1 (bytropix) when blocked on P0 or explicitly told.

never say "this would take X" or "too much work" or "significant effort". never present options. just do it. loop on yourself until task is done.

if both projects are in a done state: re-read ALL state files, check git logs for new upstream commits, run digest on hermes-dev, verify no new work emerged. if truly nothing: report "both projects idle, awaiting direction."

no questions. no status summaries. execute.
