#!/bin/bash
# gh-wrapper: transparently authenticates gh CLI via the credential server.
# Extracts the token from git credential helper and passes it as GH_TOKEN.
# This way the token never persists in env — it's fetched per-invocation.
export GH_TOKEN=$(printf "protocol=https\nhost=github.com\n\n" | git credential fill 2>/dev/null | grep "^password=" | cut -d= -f2)
exec /usr/bin/gh-real "$@"
