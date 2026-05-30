(v258)

315/0/0, 273 test files. All pass. Gap: 273 C vs ~17k Python.
Phase 190: approval check wired into terminal_handler — blocks dangerous commands. Fixed over-broad `> /dev/` pattern (replaced with Python-matching block device patterns). approval API exposed in hermes.h.
