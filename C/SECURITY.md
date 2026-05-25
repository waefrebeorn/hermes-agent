# Security Policy for Hermes Agent (C Port)

## Supported Versions

| Version | Supported |
|---------|-----------|
| latest | ✅ Active development |

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
- Confirmation within **3 business days**
- Fix and public disclosure timeline depends on severity — coordinated disclosure preferred

## Known Security Features

### File & Path Security
- **`secure_parent_dir()`**: Enforces 0700 permissions on `~/.slermes/` at startup
- **`url_safety.c`**: URL validation, path traversal detection, shell injection guards
- **`sandbox_escape.c`**: 48 escape patterns detected across 7 categories (path traversal, shell injection, fork bombs, reverse shells, etc.)
- **Redact system**: Automatic API key redaction in logs and output (`hermes_redact()`)
- **TIRITH policy engine**: 4 rule types (file_path, network, command, env_var) with 15 built-in defaults

### Credential Protection
- **`.env` file**: Strict parsing, 0600 permissions enforced
- **Config.yaml**: 0600 permissions on credential-containing config files
- **Vault encryption**: AES-256-GCM at rest for stored credentials
- **Redaction**: Keys and tokens automatically redacted from conversation output
- **Secrets manager**: Bitwarden Secrets Manager integration (optional)

### Execution Safety
- **Approval system**: Tool operations can require user approval before execution
- **Sandbox detection**: Detects bwrap/firejail/docker environments and adjusts behavior
- **Command allowlist**: Configurable command allowlist for terminal execution

## Security Gaps (Known)

| Gap | Description | Priority |
|-----|-------------|----------|
| O02 | Windows build not tested | P2 |
| Skills guard | Skills guardrails not ported from Python | P1 |
| Rate limiting | Gateway-level rate limiting incomplete | P1 |
| Audit coverage | Some execution paths lack audit events | P2 |
