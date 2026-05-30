# Entry — Slermes C Parity (v284)

Phase 218: L24 checkpoint/snapshot reclassified PORTED. C has agent_snapshot_take/restore per tool iteration (agent_loop.c:1625/1650), plus checkpoint.c with 10 functions (save, restore, list, autosave, diff, branch-restore). Python's equivalent is undo_last() array truncation — C has more features. 105 gaps remain. S0+S1+S3+S6 PORTED.
