# Hermes C — Multi-stage Docker build
# Produces minimal (~20MB) static-linked binary
#
# Build:  docker build -t hermes-c -f Dockerfile ..
# Run:    docker run --rm -it hermes-c --help
# Shell:  docker run --rm -it --entrypoint sh hermes-c

# === Stage 1: Builder ===
FROM gcc:13-bookworm AS builder

# Install build deps
RUN apt-get update -qq && apt-get install -y -qq \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

# Build default (no TUI — saves ~800KB)
RUN make -j$(nproc) hermes

# Strip debug symbols, reduce size
RUN cp hermes hermes-full && \
    strip --strip-all hermes

# === Stage 2: Runtime ===
FROM debian:bookworm-slim

# Runtime deps: only libssl (needed by curl calls from plugins)
RUN apt-get update -qq && apt-get install -y -qq \
    libssl3 \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Hermes config home
ENV SLERMES_HOME=/etc/hermes

# Copy binary (stripped) and any needed data
COPY --from=builder /build/hermes /usr/local/bin/hermes
COPY --from=builder /build/hermes-full /usr/local/bin/hermes-full

# Default entry
ENTRYPOINT ["/usr/local/bin/hermes"]
CMD ["--help"]
