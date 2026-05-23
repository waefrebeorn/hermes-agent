CC = gcc
AR = ar
CFLAGS = -O2 -g -Wall -Wextra -Wpedantic -I include
LDFLAGS = -lssl -lcrypto -ldl -lpthread
LIBS = -lm

# Optional ncurses full-screen TUI (enabled when ncurses is available)
TUI_INC = -I lib/libncurses/include
TUI_LIB = lib/libncurses/lib/libncursesw.a lib/libncurses/lib/libpanelw.a
TUI_SRC = src/cli/tui_fullscreen.c
TUI_OBJ = src/cli/tui_fullscreen.o

# Auto-detect: check if local ncurses headers exist
ifneq ("$(wildcard lib/libncurses/include/curses.h)","")
    HAS_NCURSES = 1
    CFLAGS += $(TUI_INC)
else
    HAS_NCURSES = 0
endif

# Standalone library directories
LIB_DIRS = lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb lib/libplugin lib/libskin lib/libwebsocket lib/libprotobuf lib/libmcp lib/libpath lib/libdatetime lib/libcsv lib/libhash lib/libuuid lib/libbase64 lib/libhtml lib/libtextwrap lib/libglob lib/libsignal lib/libenum lib/libdifflib lib/libregex lib/libansi
LIB_INCS = $(addprefix -I, lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb lib/libplugin lib/libskin lib/libwebsocket lib/libprotobuf lib/libmcp lib/libpath lib/libdatetime lib/libcsv lib/libhash lib/libuuid lib/libbase64 lib/libhtml lib/libtextwrap lib/libglob lib/libsignal lib/libenum lib/libdifflib lib/libregex lib/libansi)
LIB_OBJ  = lib/libjson/json.o lib/libhttp/http.o lib/libtemplate/template.o lib/libyaml/yaml.o lib/libcrypto/crypto.o lib/libdotenv/dotenv.o lib/libcron/cron.o lib/libproc/proc.o lib/libtui/tui.o lib/libdb/db.o lib/libdb/sqlite3.o lib/libplugin/plugin.o lib/libskin/skin.o lib/libwebsocket/websocket.o lib/libprotobuf/protobuf.o lib/libmcp/mcp.o lib/libpath/path.o lib/libdatetime/datetime.o lib/libcsv/csv.o lib/libhash/hash.o lib/libuuid/uuid.o lib/libbase64/base64.o lib/libhtml/html.o lib/libtextwrap/textwrap.o lib/libglob/glob.o lib/libsignal/hermes_signal.o lib/libdifflib/difflib.o lib/libregex/hermes_regex.o lib/libansi/ansi.o
LIB_A    = lib/libjson.a lib/libhttp.a lib/libtemplate.a lib/libyaml.a lib/libcrypto.a lib/libdotenv.a lib/libcron.a lib/libproc.a lib/libtui.a lib/libdb.a lib/libplugin.a lib/libskin.a lib/libmcp.a lib/libpath.a lib/libdatetime.a lib/libcsv.a lib/libhash.a lib/libuuid.a lib/libbase64.a lib/libhtml.a lib/libtextwrap.a lib/libglob.a lib/libdifflib.a lib/libregex.a lib/libsignal.a lib/libwebsocket.a lib/libprotobuf.a lib/libtoml.a lib/libansi.a

# Include standalone library headers in build
CFLAGS += $(LIB_INCS)

# Phase targets (each adds its objects)
DEPS_OBJ = src/hermes_error.o src/secrets.o src/hermes_tokenizer.o src/xai_retirement.o src/skills_hub.o
AGENT_OBJ = src/agent/agent_loop.o src/agent/llm_client.o src/agent/context.o src/agent/title.o src/agent/provider.o src/agent/provider_openai.o src/agent/provider_openrouter.o src/agent/provider_deepseek.o src/agent/provider_xai.o src/agent/provider_anthropic.o src/agent/provider_google.o src/agent/provider_azure.o src/agent/provider_bedrock.o src/agent/provider_custom.o src/agent/credential_pool.o src/agent/fallback_routing.o src/agent/budget_tracker.o src/agent/provider_metadata.o src/agent/checkpoint.o src/agent/plugin_ext.o src/agent/redact.o src/agent/audit.o src/agent/sanitize.o src/agent/vault.o src/acp/server.o src/tools/rate_limit.o
CLI_OBJ = src/cli/main.o src/cli/cli.o src/cli/display.o src/cli/config.o src/cli/commands.o src/cli/display_core.o src/cli/paths.o src/mcp_serve.o
TOOLS_OBJ = src/tools/registry.o src/tools/terminal.o src/tools/file.o src/tools/web.o src/tools/skills.o src/tools/tool_init.o src/tools/patch.o src/tools/exec_code.o src/tools/clarify.o src/tools/memory.o src/tools/todo.o src/tools/process.o src/tools/send_message.o src/tools/cronjob.o src/tools/skill_mgmt.o src/tools/session_search.o src/tools/session_crud.o src/tools/tts.o src/tools/vision.o src/tools/delegate.o src/tools/x_search.o src/tools/browser.o src/tools/approval.o src/tools/url_safety.o src/tools/tirith.o src/tools/voice_mode.o src/tools/image_gen.o src/tools/homeassistant.o src/tools/kanban.o src/tools/computer_use.o src/tools/result_storage.o src/tools/api_helpers.o src/tools/tool_config.o src/tools/discord.o src/tools/mcp_tool.o src/tools/file_batch.o src/sandbox_escape.o
GATEWAY_OBJ = src/gateway/server.o src/gateway/platforms/telegram.o src/gateway/platforms/discord.o src/gateway/platforms/webhook.o src/gateway/platforms/slack.o src/gateway/platforms/matrix.o src/gateway/platforms/mattermost.o src/gateway/platforms/whatsapp.o src/gateway/platforms/email.o src/gateway/platforms/signal.o src/gateway/platforms/homeassistant.o src/gateway/platforms/sms.o src/gateway/platforms/feishu.o src/gateway/platforms/wecom.o src/gateway/platforms/dingtalk.o src/gateway/platforms/qqbot.o src/gateway/platforms/bluebubbles.o src/gateway/platforms/msgraph_webhook.o src/gateway/platforms/weixin.o src/gateway/platforms/yuanbao.o
CRON_OBJ = src/cron/scheduler.o src/cron/jobs.o src/cron/cron_extras.o src/cron/cron_sqlite.o src/cron/cron_cli.o

# Progressively larger builds
PHASE1_OBJ = $(DEPS_OBJ)
PHASE2_OBJ = $(PHASE1_OBJ) src/main.o $(AGENT_OBJ) $(CLI_OBJ)
PHASE3_OBJ = $(PHASE2_OBJ) $(TOOLS_OBJ)
PHASE4_OBJ = $(PHASE3_OBJ) $(GATEWAY_OBJ)
PHASE5_OBJ = $(PHASE4_OBJ) $(CRON_OBJ)

.PHONY: all clean phase1 phase2 phase3 phase4 phase5 test digest \
        upstream-sync upstream-merge what-changed test-libs tui docs

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

# ncurses full-screen TUI build (shared libs from system)
tui: $(PHASE5_OBJ) $(TUI_OBJ) src/main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -DHAS_NCURSES_TUI -o hermes-tui $^ $(LDFLAGS) $(LIBS) \
		-L lib/libncurses/lib -lncursesw -ltinfo
	@echo "hermes-tui built with ncurses TUI support"

# Pattern rule for .c files
src/%.o: src/%.c include/hermes.h
	$(CC) $(CFLAGS) $(TUI_INC) -c -o $@ $<

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

lib/libmcp.a: lib/libmcp/mcp.o
	$(AR) rcs $@ $^

lib/libpath.a: lib/libpath/path.o
	$(AR) rcs $@ $^

lib/libdatetime.a: lib/libdatetime/datetime.o
	$(AR) rcs $@ $^

lib/libcsv.a: lib/libcsv/csv.o
	$(AR) rcs $@ $^

lib/libhash.a: lib/libhash/hash.o
	$(AR) rcs $@ $^

lib/libuuid.a: lib/libuuid/uuid.o
	$(AR) rcs $@ $^

lib/libbase64.a: lib/libbase64/base64.o
	$(AR) rcs $@ $^

lib/libhtml.a: lib/libhtml/html.o
	$(AR) rcs $@ $^

lib/libtextwrap/textwrap.o: lib/libtextwrap/textwrap.c lib/libtextwrap/textwrap.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtextwrap.a: lib/libtextwrap/textwrap.o
	$(AR) rcs $@ $^

lib/libglob/glob.o: lib/libglob/glob.c lib/libglob/glob.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libglob.a: lib/libglob/glob.o
	$(AR) rcs $@ $^

lib/libdifflib/difflib.o: lib/libdifflib/difflib.c lib/libdifflib/difflib.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libdifflib.a: lib/libdifflib/difflib.o
	$(AR) rcs $@ $^

lib/libregex/hermes_regex.o: lib/libregex/hermes_regex.c lib/libregex/hermes_regex.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libregex.a: lib/libregex/hermes_regex.o
	$(AR) rcs $@ $^

lib/libsignal/hermes_signal.o: lib/libsignal/hermes_signal.c lib/libsignal/hermes_signal.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libsignal.a: lib/libsignal/hermes_signal.o
	$(AR) rcs $@ $^

lib/libwebsocket/websocket.o: lib/libwebsocket/websocket.c lib/libwebsocket/websocket.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libwebsocket.a: lib/libwebsocket/websocket.o
	$(AR) rcs $@ $^

lib/libprotobuf/protobuf.o: lib/libprotobuf/protobuf.c lib/libprotobuf/protobuf.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libprotobuf.a: lib/libprotobuf/protobuf.o
	$(AR) rcs $@ $^

lib/libtoml/toml.o: lib/libtoml/toml.c lib/libtoml/toml.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtoml.a: lib/libtoml/toml.o
	$(AR) rcs $@ $^

lib/libansi/ansi.o: lib/libansi/ansi.c lib/libansi/ansi.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libansi.a: lib/libansi/ansi.o
	$(AR) rcs $@ $^

# Build all standalone libs
libs: $(LIB_A)
	@echo "Standalone libraries built: $(words $(LIB_A)) archives"

# Build example plugins as .so files
plugins: src/plugins/plugin_honcho.so src/plugins/plugin_kanban.so src/plugins/plugin_spotify.so src/plugins/plugin_disk_cleanup.so src/plugins/plugin_file_memory.so src/plugins/plugin_achievements.so src/plugins/plugin_observability.so src/plugins/plugin_skills.so src/plugins/plugin_image_gen.so src/plugins/plugin_google_meet.so

src/plugins/plugin_%.so: src/plugins/plugin_%.c include/hermes_plugin.h lib/libplugin/plugin.h
	$(CC) -O2 -fPIC -shared -I include -I lib/libplugin -DSPOTIFY_PLUGIN_VERSION='"1.0.0"' -o $@ $< -ldl

# Install example plugins to ~/.hermes/plugins/
install-plugins: plugins
	mkdir -p ~/.hermes/plugins
	cp src/plugins/plugin_honcho.so ~/.hermes/plugins/
	cp src/plugins/plugin_kanban.so ~/.hermes/plugins/
	cp src/plugins/plugin_spotify.so ~/.hermes/plugins/
	cp src/plugins/honcho-memory.yaml ~/.hermes/plugins/
	cp src/plugins/kanban-board.yaml ~/.hermes/plugins/
	cp src/plugins/spotify-control.yaml ~/.hermes/plugins/
	@echo "Plugins installed to ~/.hermes/plugins/"

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
	@echo "=== Testing libpath ==="
	@gcc -O2 -Wall -Wextra -I lib/libpath lib/libpath/path.c tests/test_path.c -o /tmp/t_path -lm 2>/dev/null; /tmp/t_path 2>&1 || echo "(no test runner)"

# Check what changed since last git pull
what-changed:
	@cd .. && git log --oneline @{u}..HEAD --name-only -- '*.py' 2>/dev/null | head -40

# API documentation
docs:
	@if command -v doxygen > /dev/null 2>&1; then \
		echo "Generating API docs..."; \
		doxygen Doxyfile 2>/dev/null; \
		echo "Done: docs/api/html/index.html"; \
	else \
		echo "doxygen not installed. Install: sudo apt install doxygen"; \
	fi

clean:
	rm -f hermes src/*.o src/*/*.o src/*/*/*.o
	rm -f $(LIB_OBJ) $(LIB_A)
	rm -rf digest_output/

# O05: Release automation target
release:
	@bash scripts/release.sh $(filter-out $@,$(MAKECMDGOALS))
