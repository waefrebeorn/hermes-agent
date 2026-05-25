CC = gcc
CFLAGS = -O2 -g -Wall -Wextra -Wpedantic -I include
LDFLAGS = -lssl -lcrypto
LIBS = -lm

# Phase targets (each adds its objects)
DEPS_OBJ = src/deps/json.o src/deps/yaml.o src/deps/http.o src/deps/db.o src/deps/crypto.o src/deps/cli_display.o
AGENT_OBJ = src/agent/agent_loop.o src/agent/llm_client.o src/agent/context.o src/agent/title.o
CLI_OBJ = src/cli/main.o src/cli/cli.o src/cli/display.o src/cli/config.o src/cli/commands.o
TOOLS_OBJ = src/tools/registry.o src/tools/terminal.o src/tools/file.o src/tools/web.o src/tools/skills.o src/tools/tool_init.o
PROVIDER_OBJ = src/provider/token_exchange.o
GATEWAY_OBJ = src/gateway/server.o src/gateway/platforms/telegram.o
CRON_OBJ = src/cron/scheduler.o src/cron/jobs.o

# Progressively larger builds
PHASE1_OBJ = $(DEPS_OBJ)
PHASE2_OBJ = $(PHASE1_OBJ) src/main.o $(AGENT_OBJ) $(CLI_OBJ)
PHASE3_OBJ = $(PHASE2_OBJ) $(TOOLS_OBJ)
PHASE4_OBJ = $(PHASE3_OBJ) $(GATEWAY_OBJ)
PHASE5_OBJ = $(PHASE4_OBJ) $(PROVIDER_OBJ) $(CRON_OBJ)

.PHONY: all clean phase1 phase2 phase3 phase4 phase5 test digest

all: phase5

# Phase targets
phase1: $(PHASE1_OBJ)
	@echo "Phase 1 complete: $(words $(PHASE1_OBJ)) objects"

phase2: $(PHASE2_OBJ)
	@echo "Phase 2 complete: $(words $(PHASE2_OBJ)) objects"

phase3: $(PHASE3_OBJ)
	@echo "Phase 3 complete: $(words $(PHASE3_OBJ)) objects"

phase4: $(PHASE4_OBJ)
	@echo "Phase 4 complete: $(words $(PHASE4_OBJ)) objects"

phase5: slermes
	@echo "Phase 5 complete: slermes binary built"

slermes: $(PHASE5_OBJ) src/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

# Pattern rule for .c files
src/%.o: src/%.c include/hermes.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Digestion
digest:
	python3 digest.py

# Test
test: slermes
	@echo "Tests TBD"

# Check what changed since last git pull
what-changed:
	@cd .. && git log --oneline @{u}..HEAD --name-only -- '*.py' 2>/dev/null | head -40

# Installation paths
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1
COMPLETION_DIR ?= $(PREFIX)/share/bash-completion/completions
ZSH_COMPLETION_DIR ?= $(PREFIX)/share/zsh/site-functions

# U02: make install
install: slermes slermes.1
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 slermes $(DESTDIR)$(BINDIR)/slermes
	install -d $(DESTDIR)$(MANDIR)
	install -m 644 slermes.1 $(DESTDIR)$(MANDIR)/slermes.1
	install -d $(DESTDIR)$(COMPLETION_DIR)
	install -m 644 contrib/completions/slermes.bash $(DESTDIR)$(COMPLETION_DIR)/slermes
	install -d $(DESTDIR)$(ZSH_COMPLETION_DIR)
	install -m 644 contrib/completions/_slermes $(DESTDIR)$(ZSH_COMPLETION_DIR)/_slermes
	@echo "slermes installed to $(DESTDIR)$(BINDIR)/"

# U12: make uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/slermes $(DESTDIR)$(MANDIR)/slermes.1
	rm -f $(DESTDIR)$(COMPLETION_DIR)/slermes $(DESTDIR)$(ZSH_COMPLETION_DIR)/_slermes

clean:
	rm -f slermes src/*.o src/*/*.o src/*/*/*.o
	rm -rf digest_output/
