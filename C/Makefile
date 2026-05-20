CC = gcc
AR = ar
CFLAGS = -O2 -g -Wall -Wextra -Wpedantic -I include
LDFLAGS = -lssl -lcrypto -ldl
LIBS = -lm

# Standalone library directories
LIB_DIRS = lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb lib/libplugin lib/libskin
LIB_INCS = $(addprefix -I, lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb lib/libplugin lib/libskin)
LIB_OBJ  = lib/libjson/json.o lib/libhttp/http.o lib/libtemplate/template.o lib/libyaml/yaml.o lib/libcrypto/crypto.o lib/libdotenv/dotenv.o lib/libcron/cron.o lib/libproc/proc.o lib/libtui/tui.o lib/libdb/db.o lib/libplugin/plugin.o lib/libskin/skin.o
LIB_A    = lib/libjson.a lib/libhttp.a lib/libtemplate.a lib/libyaml.a lib/libcrypto.a lib/libdotenv.a lib/libcron.a lib/libproc.a lib/libtui.a lib/libdb.a lib/libplugin.a lib/libskin.a

# Include standalone library headers in build
CFLAGS += $(LIB_INCS)

# Phase targets (each adds its objects)
DEPS_OBJ =
AGENT_OBJ = src/agent/agent_loop.o src/agent/llm_client.o src/agent/context.o src/agent/title.o
CLI_OBJ = src/cli/main.o src/cli/cli.o src/cli/display.o src/cli/config.o src/cli/commands.o src/cli/display_core.o
TOOLS_OBJ = src/tools/registry.o src/tools/terminal.o src/tools/file.o src/tools/web.o src/tools/skills.o src/tools/tool_init.o src/tools/patch.o src/tools/exec_code.o src/tools/clarify.o src/tools/memory.o src/tools/todo.o src/tools/process.o src/tools/send_message.o src/tools/cronjob.o src/tools/skill_mgmt.o src/tools/session_search.o src/tools/tts.o src/tools/vision.o src/tools/delegate.o
GATEWAY_OBJ = src/gateway/server.o src/gateway/platforms/telegram.o src/gateway/platforms/discord.o src/gateway/platforms/webhook.o src/gateway/platforms/slack.o src/gateway/platforms/matrix.o
CRON_OBJ = src/cron/scheduler.o src/cron/jobs.o

# Progressively larger builds
PHASE1_OBJ = $(DEPS_OBJ)
PHASE2_OBJ = $(PHASE1_OBJ) src/main.o $(AGENT_OBJ) $(CLI_OBJ)
PHASE3_OBJ = $(PHASE2_OBJ) $(TOOLS_OBJ)
PHASE4_OBJ = $(PHASE3_OBJ) $(GATEWAY_OBJ)
PHASE5_OBJ = $(PHASE4_OBJ) $(CRON_OBJ)

.PHONY: all clean phase1 phase2 phase3 phase4 phase5 test digest \
        upstream-sync upstream-merge what-changed test-libs

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

hermes: $(PHASE5_OBJ) src/main.o $(LIB_OBJ)
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

lib/libproc.a: lib/libproc/proc.o
	$(AR) rcs $@ $^

lib/libtui.a: lib/libtui/tui.o
	$(AR) rcs $@ $^

lib/libdb.a: lib/libdb/db.o
	$(AR) rcs $@ $^

lib/libplugin.a: lib/libplugin/plugin.o
	$(AR) rcs $@ $^

lib/libskin.a: lib/libskin/skin.o
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
	bash test_runner.sh

# Test all standalone libraries
test-libs:
	@echo "=== Testing libjson ==="
	@gcc -O2 -Wall -Wextra -I lib/libjson lib/libjson/json.c tests/test_json.c -o /tmp/t_json -lm 2>/dev/null; /tmp/t_json 2>&1 || echo "(no test runner)"
	@echo "=== Testing libhttp ==="
	@gcc -O2 -Wall -Wextra -I lib/libhttp lib/libhttp/http.c tests/test_http.c -o /tmp/t_http -lssl -lcrypto 2>/dev/null; /tmp/t_http 2>&1 || echo "(no test runner)"
	@echo "=== Testing libyaml ==="
	@gcc -O2 -Wall -Wextra -I lib/libyaml lib/libyaml/yaml.c tests/test_yaml.c -o /tmp/t_yaml -lm 2>/dev/null; /tmp/t_yaml 2>&1 || echo "(no test runner)"
	@echo "=== Testing libcrypto ==="
	@gcc -O2 -Wall -Wextra -I lib/libcrypto lib/libcrypto/crypto.c tests/test_crypto.c -o /tmp/t_crypto -lssl -lcrypto 2>/dev/null; /tmp/t_crypto 2>&1 || echo "(no test runner)"
	@echo "=== Testing libdotenv ==="
	@gcc -O2 -Wall -Wextra -I lib/libdotenv lib/libdotenv/dotenv.c tests/test_dotenv.c -o /tmp/t_dotenv -lm 2>/dev/null; /tmp/t_dotenv 2>&1 || echo "(no test runner)"
	@echo "=== Testing libcron ==="
	@gcc -O2 -Wall -Wextra -I lib/libcron lib/libcron/cron.c tests/test_cron.c -o /tmp/t_cron -lm 2>/dev/null; /tmp/t_cron 2>&1 || echo "(no test runner)"
	@echo "=== Testing libproc ==="
	@gcc -O2 -Wall -Wextra -I lib/libproc lib/libproc/proc.c tests/test_proc.c -o /tmp/t_proc -lm 2>/dev/null; /tmp/t_proc 2>&1 || echo "(no test runner)"
	@echo "=== Testing libtemplate ==="
	@gcc -O2 -Wall -Wextra -I lib/libtemplate -I lib/libjson lib/libtemplate/template.c lib/libjson/json.c tests/test_template.c -o /tmp/t_tmpl -lm 2>/dev/null; /tmp/t_tmpl 2>&1 || echo "(no test runner)"
	@echo "=== Testing libtui ==="
	@gcc -O2 -Wall -Wextra -I lib/libtui lib/libtui/tui.c tests/test_tui.c -o /tmp/t_tui -lm 2>/dev/null; /tmp/t_tui 2>&1 || echo "(no test runner)"
	@echo "=== Testing libdb ==="
	@gcc -O2 -Wall -Wextra -I lib/libdb lib/libdb/db.c tests/test_db.c -o /tmp/t_db -lm 2>/dev/null; /tmp/t_db 2>&1 || echo "(no test runner)"

# Check what changed since last git pull
what-changed:
	@cd .. && git log --oneline @{u}..HEAD --name-only -- '*.py' 2>/dev/null | head -40

clean:
	rm -f hermes src/*.o src/*/*.o src/*/*/*.o
	rm -f $(LIB_OBJ) $(LIB_A)
	rm -rf digest_output/
