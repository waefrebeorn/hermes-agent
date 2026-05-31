# `strtoul(ptr, (char**)&ptr, 10)` Pitfall with `const char *ptr`

**Discovered:** Phase 320 (v384) — libproc proc_read_stat bug

## The Bug

```c
const char *ptr = buf;

// THIS IS BROKEN on GCC:
// strtoul writes through endptr, but (char**)&ptr cast
// from const char** doesn't propagate back to ptr
p->utime = strtoul(ptr, (char **)&ptr, 10);

// ptr is NOT updated — still points before the number!
long vsize = strtol(ptr, (char **)&ptr, 10); // reads SAME field again!
```

The `(char**)&ptr` pattern relies on writing a `char*` through a pointer-to-pointer cast. On this GCC version, the type-qualifier mismatch (`const char**` → `char**`) causes the compiler to skip the endptr write-back. The `ptr` variable is not updated, causing repeated reads of the same field.

## The Fix

```c
const char *ptr = buf;

// CORRECT: use a local char* end pointer
{
    char *end = NULL;
    p->utime = strtoul(ptr, &end, 10);
    if (end) ptr = end;
}

// ptr is now correctly advanced past the number
```

## Scope

- Affects ALL `strto*` functions: `strtoul`, `strtol`, `strtoull`, `strtoll`, `strtod`, `strtof`
- Wherever `strto*(ptr, (char**)&ptr, ...)` is used with a `const char *ptr`
- Workaround: use local `char *end` variable + `if (end) ptr = end;`
- Also ensure space-skipping (`while (*ptr == ' ') ptr++;`) is called after each strto* call so field-counting for-loops don't waste their first iteration on spaces

## Verification

Tested on:
- GCC (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0 — BROKEN with `(char**)&ptr`
- Same code with `char *end` + `ptr = end` — WORKS correctly on the same GCC
