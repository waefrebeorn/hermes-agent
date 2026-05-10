FROM python:3.12-slim AS base

# System dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    git curl wget jq build-essential gcc g++ make \
    openssh-client ca-certificates gnupg \
    && rm -rf /var/lib/apt/lists/*

# Install uv
RUN curl -LsSf https://astral.sh/uv/install.sh | sh
ENV PATH="/root/.local/bin:$PATH"

# Install Node.js 20
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

# Install gh CLI
RUN curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
    | dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg \
    && chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
    | tee /etc/apt/sources.list.d/github-cli.list > /dev/null \
    && apt-get update && apt-get install -y gh \
    && rm -rf /var/lib/apt/lists/*

# Install gosu for privilege dropping in entrypoint
RUN apt-get update && apt-get install -y --no-install-recommends gosu \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user (no sudo access)
RUN useradd -m -u 1000 -s /bin/bash agent

# Create workspace root
RUN mkdir -p /workspaces && chown agent:agent /workspaces

# Create directory for hermes-agent clone (populated externally or at first boot)
RUN mkdir -p /opt/hermes-agent && chown agent:agent /opt/hermes-agent

# Create secrets directory (credential server reads from here)
RUN mkdir -p /run/secrets

# Git config for the agent user — set at SYSTEM level (/etc/gitconfig)
# because /home is mounted as tmpfs at runtime, wiping per-user configs.
RUN git config --system user.name "daimon[bot]" \
    && git config --system user.email "daimon[bot]@nousresearch.com" \
    && git config --system credential.helper daimon \
    && git config --system init.defaultBranch main

# Build credential server
COPY credential-server.c /tmp/credential-server.c
RUN gcc -O2 -Wall -o /usr/local/bin/credential-server /tmp/credential-server.c \
    && rm /tmp/credential-server.c

# Install credential helper client
COPY git-credential-daimon /usr/local/bin/git-credential-daimon
RUN chmod 755 /usr/local/bin/git-credential-daimon

# gh CLI wrapper — fetches token from credential server per-invocation
# so the token never persists in environment variables.
COPY gh-wrapper.sh /usr/local/bin/gh-wrapper
RUN chmod 755 /usr/local/bin/gh-wrapper \
    && mv /usr/bin/gh /usr/bin/gh-real \
    && ln -s /usr/local/bin/gh-wrapper /usr/bin/gh

# Default working directory
WORKDIR /workspaces

COPY entrypoint.sh /entrypoint.sh
RUN chmod 755 /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["sleep", "infinity"]
