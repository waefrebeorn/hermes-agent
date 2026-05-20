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

Remotes: origin=NousResearch/hermes-agent  wubu=waefrebeorn/hermes-agent
Branch:  wubu/main (14 unique commits consolidated)
No PRs to upstream until full translation done.

Phase 1: Foundation deps (JSON/YAML/HTTP/crypto/DB/ncurses → C)
Phase 2: Agent core (loop/LLM client/CLI → C)
Phase 3: Tools (terminal/file/web/skills → C)
Phase 4: Gateway (server + platform adapters → C)
Phase 5: Advanced (cron/skills/edge → C)

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
