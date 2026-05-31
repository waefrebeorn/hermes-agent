# State — Slermes C (v416)
328/0/12. Phase 359: Nous Portal OAuth device code login flow.
  oauth_device_code_request() — RFC 8628 device authorization request
  oauth_device_code_poll() — poll token endpoint with grant_type=device_code
  oauth_device_code_free() — free device code response
  nous_device_code_login() — full device code flow orchestration
  oauth_refresh_token() — OAuth token refresh via grant_type=refresh_token
  /auth login nous runs interactive device code flow instead of instructions
  Fixed stale "use Python CLI" reference on line 4999 (commands.c)
67 gaps.

|Phase|Description|
|-----|-----------|
|- Phase 245:|
