# The Hermes Translation: What It Means to Rewrite an AI Agent in C

## Why C?

Every large-scale rewrite begins with a question that sounds simple but isn't: *Why?* Why abandon the productivity of Python — 1,791 source files, a vast ecosystem, decades of library maturity — for the unforgiving world of C, where a single misplaced pointer invocation brings down the entire process?

The answer lives in the deployment profile of an AI agent. Hermes sits in a unique position: it mediates between language models and the operating system, handling sensitive configuration, spawning child processes, managing WebSocket connections, and orchestrating tool execution. Python, for all its virtues, introduces a *runtime tax* — a 50MB+ interpreter footprint, unpredictable GC pauses, JIT warmup, and a dependency tree that requires a package manager just to boot. The C port cuts through this: a 9.1MB binary (dynamically linked, libssl+libc only), miniscule startup time, predictable memory layout, and a fraction of Python's surface area.

But the deeper motivation is architectural honesty. An AI agent that can execute arbitrary code, access filesystems, and manage network connections carries a security surface area that Python's dynamic nature papered over. C forces you to confront every allocation, every boundary check, every lifetime. It doesn't let you pretend.

## The Hidden Complexity No One Talks About

The Python codebase was 864,000 lines. It worked. Tests passed. Users were happy. So where was the hidden complexity?

It was in the **config system**. Python's `None`-first initialization pattern — where every config field starts as `None` and gets lazily resolved — masked an astonishing depth of validation logic. The Python type checker (or lack thereof) let fields flow through multiple transformation layers before anyone noticed a schema violation. In C, every field must have a defined type, a known default, and an explicit validation path *before* it touches any logic. What looked like shallow config eventually revealed 98% coverage in the C port — not because the C team was diligent, but because Python's dynamic dispatch was silently absorbing errors that C refused to compile.

Then there were the **providers**. The Python ecosystem treated provider discrepancies as runtime curiosities: OpenAI returns tokens one way, Anthropic another, Ollama differently still. Python's duck typing absorbed these differences with a few `hasattr` guards and `try/except` blocks. C's type system demanded an interface. Every provider had to implement the exact same contract — which meant discovering that the contract didn't actually exist in Python. The C port found *seven* subtle semantic mismatches between providers that had coexisted for months without anyone noticing: streaming chunk formats that differed in their content field names, stop reason enumerations that overlapped but weren't identical, tool call response shapes that varied by provider version.

The **bug list** tells the story best. `temperature=0.0` silently falling back to a default because Python floats treated `0.0` as falsy. A use-after-free in `response_format` that Python's reference counting accidentally papered over. A NULL stream dereference in provider code that only crashed in edge cases where the provider returned empty before sending any content. A heap overflow in the redaction engine — invisible in Python because the string was immutable and the slice happened to land on a GC-safe boundary. Every one of these was a latent time bomb that the type system, the runtime, and the culture of "it works in my environment" had collectively defused but never disarmed.

## The Feel Gap

The hardest thing to translate wasn't code — it was **feel**. Python's `argparse` and `click` pipelines produce interactive CLI experiences with autocomplete, contextual help, and progressive disclosure. Replicating that in C meant building a skin engine, a terminal abstraction layer, and an autocomplete system from scratch. The C port has 74 CLI commands now, with full tab completion and help text — but it took *three* dedicated libraries (`libskin`, `libtemplate`, and `libproc`) to get there.

The skin engine is a microcosm of the entire translation. In Python, templating is a `pip install` away. In C, we wrote a template preprocessor that compiles skin files into C89-compatible format strings. We built a layout system that handles terminal resize events. We implemented a color abstraction that maps named colors to 24-bit ANSI codes. Python's `rich` library provides this in a single `pip install; from rich import *`. C required approximately 2,500 lines of carefully tested code — and we still lack Python's automatic color depth detection.

The same story repeats across the surface area. Python's `readline` module gives you history, tab completion, and input editing for free. C's equivalent required `liblinenoise` integration, a command registry, dynamic completion generation from provider metadata, and a custom keybinding layer for vi-mode support. The result feels *right* — but the effort ratio was probably 50:1.

## Library Porting: Rebuilding the Standard Library

Python ships with "batteries included." C ships with `<stdlib.h>`. The gap is approximately the size of a small planet.

The C port currently relies on 18 internal libraries (`libjson`, `libyaml`, `libhttp`, `libcrypto`, `libcron`, `libpath`, `libdatetime`, `libdb`, `libmcp`, `libwebsocket`, `libplugin`, `libskin`, `libtemplate`, `libproc`, and others). Every single one was written because Python had something equivalent built-in or easily accessible.

Consider JSON: Python has `json.loads()` and `json.dumps()`. In C, we built `libjson` — a complete JSON parser, serializer, and schema validator. It handles streaming SAX-style parsing, DOM-style tree access, schema validation against a subset of JSON Schema draft-07, and incremental generation. It's 4,200 lines of C and was the *second* thing we built, after the build system.

YAML parsing was worse. Python's `yaml.load()` just works. C has no standard YAML library that compiles cleanly across GCC, Clang, and MSVC without vendoring an entire Python interpreter. So `libyaml` was born — a purpose-built YAML subset parser that handles the 90% of YAML that Hermes actually consumes: configuration files, provider metadata, and skin definitions. The other 10% (multi-document streams, YAML tags, recursive anchors) was deemed out of scope. For now.

The MCP (Model Context Protocol) library deserves special mention. In Python, async HTTP + JSON serialization is a few lines of `aiohttp` and `json.dumps()`. In C, `libmcp` required: a WebSocket handshake implementation, a structured JSON-RPC layer over `libjson`, a message framing protocol, a connection pool with heartbeat management, and a transport abstraction that supports both stdio and TCP. That's roughly one person-month of work for what Python delivered in three `import` statements.

## The Testing Gap

Python upstream: **1,140 test files**, over **17,000 individual tests**, built on `pytest`, with fixtures, parametrization, and mocking built into the framework.

C port so far: **80 test files**, **2,088 assertions**, **116 passing / 0 failing / 0 skipped**.

The disparity is humbling. But it's also misleading. Python's test suite tests Python's *implementation choices* — mock-heavy tests that validate whether a function called another function, fixture trees that exist because Python's GC manages object lifetimes implicitly, parametrization over dozens of provider versions that exist only as remote API endpoints.

C's tests are different by necessity. They test *behavior*. They run against actual binaries. They validate memory safety with AddressSanitizer and LeakSanitizer and UndefinedBehaviorSanitizer baked into the build. They test the config parser against known-bad inputs — because in Python, bad config raises an exception and the world goes on. In C, bad config is a segmentation fault waiting to happen.

Every C test that exists has paid for itself in bugs found. The 2,088 assertions have caught: config validation NULL SEGVs (the config system was the single largest source of crashes in early builds), provider response parsing overflows (when Anthropic changed their streaming format in a minor version), filename encoding edge cases (UTF-8 BOM sequences that Python handles silently), and MCP message framing alignment issues (where 64-bit platform alignment differences surfaced as intermittent failures).

The testing gap will close, but it will never be a direct translation. C's test suite tests different things because C fails in different ways. A 1:1 test count is the wrong goal. A 1:1 *correctness guarantee* is the right one — and that requires different testing strategies entirely: fuzzing, static analysis, formal verification of critical paths, and a testing culture that treats every sanitizer warning as a blocker.

## What 1:1 Parity Really Means

The phrase "1:1 parity" sounds straightforward: the C version should do everything the Python version does. But parity is a relationship, not a property. It depends entirely on *where you set the equivalence point*.

**API parity** is achievable: every CLI command (74), every gateway platform (19), every tool operation (67), every provider interface (9 native, 27 metadata). A user who switches from the Python binary to the C binary should experience no functional regression. All 100% of MCP, 100% of Gateway, 95% of Tools, 98% of Config — these numbers tell the story of a port that has faithfully reproduced the public surface area.

**Behavioral parity** is harder. The C version of `hermes tool call --pipe` must produce identical byte-level output to the Python version — not just semantically equivalent output, but *the same* output, in the same order, with the same timing characteristics. This matters because downstream tools (shell scripts, CI pipelines, editor integrations) may depend on exact output format, stream chunking behavior, or error message text.

**Performance parity** is a category error. C will be faster. That's not parity; that's superiority. The question is whether the speed difference breaks Python-level assumptions about timing. If the Python version had a 200ms delay between a tool's stdout closing and the next prompt appearing, and the C version has a 2ms delay, downstream tools that rely on that delay for rate-limiting will break. True parity means understanding which performance characteristics are *contractual* and which are *incidental*.

**Safety parity** is perhaps the most interesting category. Python's safety model is "it won't crash because the runtime prevents it." C's safety model is "it won't crash because we prevented it." These are philosophically different approaches. The C port added AddressSanitizer, UndefinedBehaviorSanitizer, MemorySanitizer, and explicit bounds checking on all array accesses. It eliminated entire classes of bugs that Python masked. In this dimension, C parity means *more* safety — but the contract is different: Python promised you wouldn't have to think about memory; C promises that when you do think about it, you'll be right.

**The feel gap may never fully close.** The Python version has a certain *gestalt* — a looseness, an improvisational quality that comes from a codebase that took six years to mature and was written by people who were thinking about agent behavior, not memory layouts. The C version is tighter, more exact, more deliberate. It's a different aesthetic. Some users will prefer the C version's snappiness and reliability. Others will miss the Python version's extensibility and forgiving nature.

## The Verdict

The Hermes translation is not a port. It is a *rediscovery*. The C codebase has 123,065 lines across 310 source files — roughly 14% the size of Python's codebase, doing the same job. Compression that dramatic doesn't come from being clever. It comes from understanding what you actually need versus what you accumulated.

The 158 C-specific commits since the fork start have fixed bugs that existed in Python for months or years — bugs that Python's runtime made invisible but never made harmless. The plugins system sits at 14% completion not because it's hard, but because the C team is asking harder questions: *Do you really need dynamic loading? Is there a safer pattern? Can we do this with static linking and configuration instead?*

1:1 parity is not about compiling the same program in a different language. It's about understanding the *essence* of what a program does — the behaviors it guarantees, the mistakes it prevents, the feel it creates — and reproducing that essence in a form that's truer to the hardware it runs on. The Python version of Hermes was a beautiful, sprawling cathedral of abstractions. The C version is a fortress. Same purpose. Different philosophy. Both are needed.

*The real achievement of the translation isn't that the C version works. It's that we now understand what the Python version was *actually* doing — and it was more than any of us realized.*

## June 2026 Update: The Test Coverage Surge

Two months after the original essay, the numbers tell a different story. The C translation is no longer a hopeful prototype — it has passed the point where "it compiles" is newsworthy. The suite has grown from 117/0/0 to 154/0/0. Library ports are finished at 30 .a archives — every Python stdlib equivalent the port needs has a C counterpart. All 10 plugin backends compile as .so files. The gateway supports 19 platforms (one more than Python). The CLI has ~148 command handlers.

The testing phase has shifted focus from "can we build it" to "can we prove it works." The 116 test files cover: every library (30 archives fully tested), every provider error path (225 assertions across 9 providers), every cron subsystem (scheduler, store, expression parser, chain, template, retry), every gateway escape mode, every tool handler, every plugin.

The bugs being found now are subtle: a NULL guard missing in `cron_job_reset_retry` that only crashes when a chain of specific conditions aligns. A `json_get_str` misuse on a JSON_STRING node that returned empty string instead of the actual value — a bug that existed in the template instantiation code since it was written and would only surface when template substitution was actually used.

The feel gap remains. The Python version's 1,140 test files and 17,000+ tests are not going to be matched in C — they shouldn't be. C tests test different things: memory safety, boundary conditions, null pointer paths, file system persistence, and the exact behavior of 30 library archives. But the gap between "works for the developer" and "works for the user" is closing. The binary runs end-to-end with DeepSeek v4 Flash. Configuration parses correctly. Tool calls execute and return results. The agent loop iterates, retries, falls back.

The translation was always a wager: that the clarity of C would outweigh its cost. That forcing every allocation, every lifetime, and every boundary into the open would produce not just a safer program, but a deeper understanding of what the program actually does. That bet is paying off. The C codebase has 392 commits across 147 modules and 30 libraries. It still serves every person who runs it. And — crucially — it makes the people who maintain it think harder about what "working" really means. That alone made the translation worthwhile.
