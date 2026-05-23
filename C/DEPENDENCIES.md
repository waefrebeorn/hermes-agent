# Dependency Map — Python → C Equivalents

Every Python dependency Hermes Agent uses, mapped to its C equivalent.

## Direct Dependencies

| Python Package | Role | C Equivalent | Status |
|---------------|------|-------------|--------|
| `openai` / `httpx` / `httpcore` | HTTP client | `libcurl` + `libhttp` | ✅ |
| `pyyaml` / `ruamel.yaml` | YAML config | `libyaml` | ✅ |
| `pydantic` / `pydantic-core` | Validation | Manual struct validation | ✅ |
| `jiter` | JSON iteration | `libjson` | ✅ |
| `jinja2` | Templating | `libtemplate` | ✅ |
| `rich` / `pygments` | Terminal display | `libansi` + `libskin` + ncurses | ✅ |
| `prompt_toolkit` | Input handling | linenoise / libreadline | ✅ |
| `croniter` | Cron parsing | `libcron` | ✅ |
| `psutil` | Process info | /proc filesystem + `libproc` | ✅ |
| `python-dotenv` | .env loading | `libdotenv` | ✅ |
| `cryptography` / `PyJWT` | Crypto | OpenSSL + `libcrypto` | ✅ |
| `sqlite3` | Database | Bundled sqlite3.c + `libdb` | ✅ |
| `cffi` | C FFI bridge | N/A (native C) | N/A |
| `aiohttp` | Async HTTP | Synchronous (thread-per-platform) | ✅ |
| `websockets` | WebSocket | `libwebsocket` | ✅ |
| `protobuf` | Protocol buffers | `libprotobuf` | ✅ |
| `uuid` | UUID generation | `libuuid` | ✅ |
| `hashlib` | Hashing | `libhash` | ✅ |
| `base64` | Base64 encoding | `libbase64` | ✅ |
| `datetime` | Date/time | `libdatetime` | ✅ |
| `pathlib` | Path manipulation | `libpath` | ✅ |
| `csv` | CSV parsing | `libcsv` | ✅ |
| `html` | HTML escaping | `libhtml` | ✅ |
| `textwrap` | Text wrapping | `libtextwrap` | ✅ |
| `glob` | File globbing | `libglob` | ✅ |
| `signal` | Signal handling | `libsignal` | ✅ |
| `enum` | Enumeration | `libenum` | ✅ |
| `difflib` | Diff computation | `libdifflib` | ✅ |
| `re` | Regular expressions | `libregex` (POSIX) | ✅ |
| `json5` | JSON5 parsing | `libjson5` | ✅ |
| `toml` | TOML parsing | `libtoml` | ✅ |
| `argparse` / `click` / `fire` | CLI parsing | getopt + manual | ✅ |
| `readline` | Line editing | linenoise | ✅ |
| `typing` / `dataclasses` | Type system | Manual structs | ✅ |

## Optional / Plugin Dependencies

| Python Package | Role | C Equivalent | Status |
|---------------|------|-------------|--------|
| `numpy` | Numerical ops | N/A (no ML in C port) | N/A |
| `PIL` | Image processing | stb_image | 🔄 |
| `ffmpeg-python` | Media processing | popen(ffmpeg) | ✅ |
| `boto3` | AWS SDK | libcurl + SigV4 in provider | ✅ |
| `requests-aws4auth` | SigV4 signing | Built-in provider_bedrock.c | ✅ |

## Build Dependencies

| Tool | Role | Status |
|------|------|--------|
| `gcc` / `clang` | C compiler | ✅ |
| `make` | Build system | ✅ |
| `libcurl-dev` | HTTP client library | ✅ |
| `libssl-dev` | TLS/crypto library | ✅ |
| `libncurses-dev` | Terminal UI | Optional |
| `doxygen` | API docs | Optional |
| `docker` | Container build | ✅ |
