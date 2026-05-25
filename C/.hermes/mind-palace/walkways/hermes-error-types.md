# Error Types — K01-K05

**Status: 5/5 (100%) — ✅ completed 2026-05-22**

- K01: ValueError — `hermes_error_t` with `HERMES_ERR_VALUE` type
- K02: TypeError — `HERMES_ERR_TYPE`
- K03: RuntimeError — `HERMES_ERR_RUNTIME`  
- K04: OSError — `HERMES_ERR_OS` with errno capture + strerror
- K05: TimeoutError — `HERMES_ERR_TIMEOUT`

**File:** `include/hermes_error.h`, `src/hermes_error.c`
**Tests:** `tests/test_hermes_error.c` — 11 tests
**API:** Macros `hermes_err_value()`, `hermes_err_type()`, `hermes_err_runtime()`, `hermes_err_os()`, `hermes_err_timeout()`, `hermes_errno()`. Thread-local `hermes_set_last_error()` / `hermes_get_last_error()`.
