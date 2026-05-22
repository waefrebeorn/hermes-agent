# Security Policy for Hermes Agent (C Port)

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| latest  | ✅ Active development |

We do not maintain older releases. Always run the latest from `main`.

## Reporting a Vulnerability

**Do not open a public GitHub issue.** Send details to:

- **Email:** waefrebeorn@proton.me (PGP key available on keys.openpgp.org)
- **Signal:** Contact @waefrebeorn.03 via Signal

Include:
1. Description of the vulnerability
2. Steps to reproduce (config snippets, input payloads, commands)
3. Impact assessment (what an attacker could achieve)
4. Any proposed fix or mitigation

You should receive an acknowledgment within 72 hours. If not, follow up.

### Disclosure Timeline

- We aim to confirm the report within **3 business days**
- A fix is typically provided within **7–14 days** depending on severity
- Public disclosure happens **30 days after the fix is released**, or earlier if a
  viable patch is already deployed upstream

## Security Features

### API Key Protection

- API keys are accepted via `~/.slermes/.env` only (file mode 0600 enforced at
  startup via `secure_parent_dir()`)
- `hermes_redact` strips secrets from logs, session output, and error messages
  before persistence or transmission. Patterns covered: `sk-*`, `ghp_*`,
  `Bearer <token>`, JWT tokens, custom patterns
- `url_safety.c` validates destination hosts before sending API keys — keys are
  never forwarded to unrecognised endpoints
- `provider_url_is_trusted()` checks the provider's base_url against known
  hosts before injecting the key into HTTP headers

### Command Execution Isolation

- `exec_code` tool supports `bwrap`-based sandboxing (namespace + seccomp) on
  Linux hosts. Controlled by the `sandbox: true` flag per invocation
- `terminal` tool enforces per-command environment isolation — `env` arg
  creates a clean environment, preventing variable leakage between commands
- File sandbox: `sandbox_init()` + `sandbox_add_path()` + realpath resolution
  prevents path traversal attacks. Symlink attacks are blocked via realpath
  canonicalisation

### Configuration Hardening

- `secure_parent_dir()` creates `~/.slermes/` with mode 0700 on first run,
  rejecting world-readable or root-owned config directories
- `.env` files are parsed by `libdotenv` with strict formatting — no shell
  injection via env values
- `HERMES_HOME` override is validated before loading any config file

### Audit Logging

- All tool executions and agent decisions are logged to `~/.slermes/logs/`
- Log rotation is configurable via `logging.max_size` and `logging.max_files`
- Sensitive parameters are automatically redacted before log entry

### Credential Rotation

- The credential pool (`credential_pool.c`) supports multi-key rotation across
  providers — keys are cycled on failure or configurable interval
- Weighted selection distributes load across available keys

## Known Security Gaps

These are tracked as gaps in the parity roadmap and are being addressed:

1. **Credential encryption at rest** (O11) — API keys in session DB are
   currently stored as plaintext. Planned: AES-GCM encryption with key derived
   from a local master secret.
2. **Audit log rotation** (O12) — Auto-rotation, compression, and expiry of
   audit logs is not yet implemented.
3. **Sandbox escape detection** (O14) — No active monitoring for container
   breakout attempts.
4. **File permission hardening** (O15) — Not all files created by the binary
   use 0600/0700 masks.

## Third-Party Dependencies

| Library | Source | Version | Notes |
|---------|--------|---------|-------|
| libjson | bundled (lib/libjson) | vendored | C89 JSON parser |
| libhttp | bundled (lib/libhttp) | vendored | Sync HTTP via libcurl |
| libyaml | bundled (lib/libyaml) | vendored | Minimal YAML subset |
| libcrypto | bundled (lib/libcrypto) | vendored | OpenSSL wrapper |
| OpenSSL | system | ≥ 1.1.1 | TLS, HMAC, key derivation |
| libcurl | system | ≥ 7.68 | HTTP transport |
| sqlite3 | bundled (lib/libdb) | vendored | Session DB, cron persistence |

Bundled libraries are vendored for portability and audited on import. System
dependencies (OpenSSL, libcurl) should be kept up-to-date via your OS package
manager.

## Supply Chain

- Releases are built from tagged commits on `main`
- CI runs on every push: build + smoke test + full test suite + ASan check
- Docker images are built from the multi-stage `Dockerfile` — no intermediate
  artifacts included in the runtime image
- Binary checksums are published with each release
