CC = gcc
AR = ar
CFLAGS = -O2 -g -Wall -Wextra -Wpedantic -I include
LDFLAGS = -lssl -lcrypto
LIBS = -lm

# Standalone library directories
LIB_DIRS = lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron
LIB_INCS = $(addprefix -I, lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron)
LIB_OBJ  = lib/libjson/json.o lib/libhttp/http.o lib/libtemplate/template.o lib/libyaml/yaml.o lib/libcrypto/crypto.o lib/libdotenv/dotenv.o lib/libcron/cron.o
LIB_A    = lib/libjson.a lib/libhttp.a lib/libtemplate.a lib/libyaml.a lib/libcrypto.a lib/libdotenv.a lib/libcron.a

# Phase targets (each adds its objects)
DEPS_OBJ = src/deps/json.o src/deps/yaml.o src/deps/http.o src/deps/db.o src/deps/crypto.o src/deps/cli_display.o
AGENT_OBJ = src/agent/agent_loop.o src/agent/llm_client.o src/agent/context.o src/agent/title.o
CLI_OBJ = src/cli/main.o src/cli/cli.o src/cli/display.o src/cli/config.o src/cli/commands.o
TOOLS_OBJ = src/tools/registry.o src/tools/terminal.o src/tools/file.o src/tools/web.o src/tools/skills.o src/tools/tool_init.o
GATEWAY_OBJ = src/gateway/server.o src/gateway/platforms/telegram.o
CRON_OBJ = src/cron/scheduler.o src/cron/jobs.o

# Progressively larger builds
PHASE1_OBJ = $(DEPS_OBJ)
PHASE2_OBJ = $(PHASE1_OBJ) src/main.o $(AGENT_OBJ) $(CLI_OBJ)
PHASE3_OBJ = $(PHASE2_OBJ) $(TOOLS_OBJ)
PHASE4_OBJ = $(PHASE3_OBJ) $(GATEWAY_OBJ)
PHASE5_OBJ = $(PHASE4_OBJ) $(CRON_OBJ)

.PHONY: all clean phase1 phase2 phase3 phase4 phase5 test digest \
        upstream-sync upstream-merge what-changed

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

phase5: hermes
	@echo "Phase 5 complete: hermes binary built"

hermes: $(PHASE5_OBJ) src/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

# Pattern rule for .c files
src/%.o: src/%.c include/hermes.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Library pattern rule — standalone libs
lib/%.o: lib/%.c
	$(CC) $(CFLAGS) $(LIB_INCS) -c -o $@ $<

# Static library archives
lib/libjson.a: lib/libjson/json.o
	$(AR) rcs $@ $^

lib/libhttp.a: lib/libhttp/http.o
	$(AR) rcs $@ $^

lib/libtemplate.a: lib/libtemplate/template.o
	$(AR) rcs $@ $^

lib/libyaml.a: lib/libyaml/yaml.o
	$(AR) rcs $@ $^

lib/libcrypto.a: lib/libcrypto/crypto.o
	$(AR) rcs $@ $^

lib/libdotenv.a: lib/libdotenv/dotenv.o
	$(AR) rcs $@ $^

lib/libcron.a: lib/libcron/cron.o
	$(AR) rcs $@ $^

# Build all standalone libs
libs: $(LIB_A)
	@echo "Standalone libraries built: $(words $(LIB_A)) archives"

# Digestion — standard local diff
digest:
	python3 digest.py

# ─── Super Fork: Upstream Tracking ─────────────────────────────────
# Fetch origin/main (upstream), diff Python since last sync, report C work needed
upstream-sync:
	python3 digest.py --upstream

# Fetch + merge upstream into current branch + generate stubs
upstream-merge:
	python3 digest.py --upstream --merge --generate-stubs

# Full workflow: sync, build, report
sync-all: upstream-merge phase5
	@echo "Super Fork sync complete."

# Test
test: hermes
	@echo "Tests TBD"

# Check what changed since last git pull
what-changed:
	@cd .. && git log --oneline @{u}..HEAD --name-only -- '*.py' 2>/dev/null | head -40

clean:
	rm -f hermes src/*.o src/*/*.o src/*/*/*.o
	rm -f $(LIB_OBJ) $(LIB_A)
	rm -rf digest_output/
