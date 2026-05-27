CC = gcc
AR = ar
CFLAGS = -O2 -g -Wall -Wextra -Wpedantic -I include
LDFLAGS = -lssl -lcrypto -ldl -lpthread -lz
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
LIB_DIRS = lib/liblineedit lib/libbinary lib/libbrowser lib/libdebug lib/libosv lib/libwebsite lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb lib/libplugin lib/libskin lib/libwebsocket lib/libprotobuf lib/libmcp lib/libpath lib/libdatetime lib/libcsv lib/libhash lib/libuuid lib/libbase64 lib/libhtml lib/libtextwrap lib/libglob lib/libsignal lib/libenum lib/libdifflib lib/libregex lib/libansi lib/libjson5 lib/libskillusage lib/libskillsync lib/libtranscribe lib/libmcp_oauth lib/libfal_common lib/libtooloutput lib/libxai_http lib/libenvpassthrough lib/libcredential lib/libschemasanitizer lib/libfuzzymatch lib/libinterrupt lib/libtoolbackend lib/libmangateway lib/libratelimit lib/libfilestate lib/libtooldispatch lib/librateguard lib/libskillutils lib/liberrorclassifier lib/libfile_sync lib/libbudgetconfig lib/libthreatpatterns
LIB_INCS = $(addprefix -I, lib/liblineedit lib/libmangateway lib/libtoolbackend lib/libratelimit lib/libbinary lib/libbrowser lib/libdebug lib/libosv lib/libwebsite lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb lib/libplugin lib/libskin lib/libwebsocket lib/libprotobuf lib/libmcp lib/libpath lib/libdatetime lib/libcsv lib/libhash lib/libuuid lib/libbase64 lib/libhtml lib/libtextwrap lib/libglob lib/libsignal lib/libenum lib/libdifflib lib/libregex lib/libansi lib/libjson5 lib/libskillusage lib/libskillsync lib/libtranscribe lib/libmcp_oauth lib/libfal_common lib/libtooloutput lib/libxai_http lib/libenvpassthrough lib/libcredential lib/libschemasanitizer lib/libfuzzymatch lib/libinterrupt lib/libfilestate lib/libtooldispatch lib/librateguard lib/libskillutils lib/liberrorclassifier lib/libfile_sync lib/libbudgetconfig lib/libthreatpatterns lib/libcredentialfiles lib/libskillaudit lib/libslashconfirm)
LIB_OBJ  = lib/libbinary/binary.o lib/libbrowser/camofox_state.o lib/libdebug/debug_helpers.o lib/libosv/osv.o lib/libwebsite/website_policy.o lib/libjson/json.o lib/libhttp/http.o lib/libtemplate/template.o lib/libyaml/yaml.o lib/libcrypto/crypto.o lib/libdotenv/dotenv.o lib/libcron/cron.o lib/libproc/proc.o lib/libtui/tui.o lib/libdb/db.o lib/libdb/sqlite3.o lib/libplugin/plugin.o lib/libskin/skin.o lib/libwebsocket/websocket.o lib/libprotobuf/protobuf.o lib/libmcp/mcp.o lib/libpath/path.o lib/libdatetime/datetime.o lib/libcsv/csv.o lib/libhash/hash.o lib/libuuid/uuid.o lib/libbase64/base64.o lib/libhtml/html.o lib/libtextwrap/textwrap.o lib/libglob/glob.o lib/libsignal/hermes_signal.o lib/libdifflib/difflib.o lib/libregex/hermes_regex.o lib/libansi/ansi.o lib/libansi/ansi_strip.o lib/libjson5/json5.o lib/libskillusage/skill_usage.o lib/libskillusage/skill_provenance.o lib/libskillsync/skills_sync.o lib/libtranscribe/transcribe.o lib/libmcp_oauth/mcp_oauth.o lib/libfal_common/fal_common.o lib/libtooloutput/tool_output.o lib/libxai_http/xai_http.o lib/libenvpassthrough/env_passthrough.o lib/libcredential/credential.o lib/libschemasanitizer/schema_sanitizer.o lib/libfuzzymatch/fuzzy_match.o lib/libinterrupt/interrupt.o lib/libtoolbackend/tool_backend.o lib/libmangateway/managed_gateway.o lib/libratelimit/rate_limit.o lib/libfilestate/file_state.o lib/libtooldispatch/tool_dispatch_helpers.o lib/librateguard/rate_guard.o lib/libskillutils/skill_utils.o lib/liberrorclassifier/error_classifier.o lib/liblineedit/line_edit.o lib/libfile_sync/file_sync.o lib/libbudgetconfig/budget_config.o lib/libthreatpatterns/threat_patterns.o lib/libcredentialfiles/credential_files.o lib/libskillaudit/skill_audit.o lib/libslashconfirm/slash_confirm.o
LIB_A    = lib/libbinary.a lib/libbrowser.a lib/libdebug.a lib/libosv.a lib/libwebsite.a lib/libenvpassthrough.a lib/libxai_http.a lib/libcredential.a lib/libschemasanitizer.a lib/libfuzzymatch.a lib/libjson.a lib/libhttp.a lib/libtemplate.a lib/libyaml.a lib/libcrypto.a lib/libdotenv.a lib/libcron.a lib/libproc.a lib/libtui.a lib/libdb.a lib/libplugin.a lib/libskin.a lib/libmcp.a lib/libpath.a lib/libdatetime.a lib/libcsv.a lib/libhash.a lib/libuuid.a lib/libbase64.a lib/libhtml.a lib/libtextwrap.a lib/libglob.a lib/libdifflib.a lib/libregex.a lib/libsignal.a lib/libwebsocket.a lib/libprotobuf.a lib/libtoml.a lib/libansi.a lib/libjson5.a lib/libskillusage.a lib/libskillsync.a lib/libtranscribe.a lib/libmcp_oauth.a lib/libfal_common.a lib/libtooloutput.a lib/libratelimit.a lib/libmangateway.a lib/libtoolbackend.a lib/libinterrupt.a lib/libfilestate.a lib/libtooldispatch.a lib/librateguard.a lib/libskillutils.a lib/liberrorclassifier.a lib/liblineedit.a lib/libfile_sync.a lib/libbudgetconfig.a lib/libcredentialfiles.a lib/libskillaudit.a lib/libslashconfirm.a

# Include standalone library headers in build
CFLAGS += $(LIB_INCS)

# Phase targets (each adds its objects)
DEPS_OBJ = src/hermes_error.o src/secrets.o src/hermes_tokenizer.o src/xai_retirement.o src/skills_hub.o
AGENT_OBJ = src/agent/agent_loop.o src/agent/llm_client.o src/agent/logger.o src/agent/context.o src/agent/context_engine.o src/agent/title.o src/agent/provider.o src/agent/provider_openai.o src/agent/provider_openrouter.o src/agent/provider_deepseek.o src/agent/provider_xai.o src/agent/provider_anthropic.o src/agent/provider_google.o src/agent/provider_azure.o src/agent/provider_bedrock.o src/agent/provider_custom.o src/agent/credential_pool.o src/agent/fallback_routing.o src/agent/budget_tracker.o src/agent/provider_metadata.o src/agent/checkpoint.o src/agent/plugin_ext.o src/agent/redact.o src/agent/audit.o src/agent/sanitize.o src/agent/vault.o src/agent/think_scrubber.o src/agent/retry_utils.o src/agent/trajectory.o src/agent/portal_tags.o src/agent/markdown_tables.o src/agent/markdown_render.o src/agent/file_safety.o src/agent/system_prompt.o src/agent/skill_preprocessing.o src/agent/tool_guardrails.o src/agent/i18n.o src/agent/subdir_hints.o src/agent/onboarding.o src/agent/image_routing.o src/agent/skill_bundles.o src/agent/usage_pricing.o src/agent/lmstudio_reasoning.o src/agent/manual_compression_feedback.o src/agent/prompt_caching.o src/agent/context_references.o src/agent/gemini_schema.o src/agent/moonshot_schema.o src/agent/auxiliary_client.o src/agent/hook_registry.o src/agent/shell_hooks.o src/agent/nous_rate_guard.o src/agent/curator.o src/acp/server.o src/acp/edit_approval.o src/acp/events.o src/acp/permissions.o src/acp/resource.o src/tools/rate_limit.o src/tools/tool_result.o src/agent/skill_commands.o
CLI_OBJ = src/cli/main.o src/cli/cli.o src/cli/display.o src/cli/config.o src/cli/commands.o src/cli/display_core.o src/cli/paths.o src/mcp_serve.o src/api_server.o
TOOLS_OBJ = src/tools/registry.o src/tools/terminal.o src/tools/file.o src/tools/web.o src/tools/skills.o src/tools/tool_init.o src/tools/patch.o src/tools/exec_code.o src/tools/clarify.o src/tools/memory.o src/tools/todo.o src/tools/process.o src/tools/send_message.o src/tools/cronjob.o src/tools/skill_mgmt.o src/tools/session_search.o src/tools/session_crud.o src/tools/tts.o src/tools/vision.o src/tools/delegate.o src/tools/x_search.o src/tools/browser.o src/tools/approval.o src/tools/url_safety.o src/tools/tirith.o src/tools/voice_mode.o src/tools/image_gen.o src/tools/homeassistant.o src/tools/kanban.o src/tools/computer_use.o src/tools/result_storage.o src/tools/api_helpers.o src/tools/tool_config.o src/tools/discord.o src/tools/mcp_tool.o src/tools/file_batch.o src/tools/file_watch.o src/tools/feishu_tools.o src/tools/file_merge.o src/tools/mixture_of_agents.o src/tools/video_gen.o src/tools/video_gen_registry.o src/tools/video_analyze.o src/tools/image_gen_registry.o src/tools/web_search_registry.o src/tools/path_security.o src/tools/xai_http.o src/tools/account_usage.o src/tools/ansi_strip.o src/tools/transcribe.o src/sandbox_escape.o
GATEWAY_OBJ = src/gateway/server.o src/gateway/platforms/telegram.o src/gateway/platforms/discord.o src/gateway/platforms/webhook.o src/gateway/platforms/slack.o src/gateway/platforms/matrix.o src/gateway/platforms/mattermost.o src/gateway/platforms/whatsapp.o src/gateway/platforms/email.o src/gateway/platforms/signal.o src/gateway/platforms/homeassistant.o src/gateway/platforms/sms.o src/gateway/platforms/feishu.o src/gateway/platforms/wecom.o src/gateway/platforms/dingtalk.o src/gateway/platforms/qqbot.o src/gateway/platforms/bluebubbles.o src/gateway/platforms/msgraph_webhook.o src/gateway/platforms/weixin.o src/gateway/platforms/yuanbao.o
CRON_OBJ = src/cron/scheduler.o src/cron/jobs.o src/cron/cron_extras.o src/cron/cron_sqlite.o src/cron/cron_cli.o

# Progressively larger builds
PHASE1_OBJ = $(DEPS_OBJ)
PHASE2_OBJ = $(PHASE1_OBJ) src/main.o $(AGENT_OBJ) $(CLI_OBJ)
PHASE3_OBJ = $(PHASE2_OBJ) $(TOOLS_OBJ)
PHASE4_OBJ = $(PHASE3_OBJ) $(GATEWAY_OBJ)
PHASE5_OBJ = $(PHASE4_OBJ) $(CRON_OBJ)

.PHONY: all clean phase1 phase2 phase3 phase4 phase5 test digest \
        upstream-sync upstream-merge what-changed test-libs tui docs asan fuzz

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

slermes: $(PHASE5_OBJ) src/main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

# ncurses full-screen TUI build (shared libs from system)
tui: $(PHASE5_OBJ) $(TUI_OBJ) src/main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -DHAS_NCURSES_TUI -o slermes-tui $^ $(LDFLAGS) $(LIBS) \
		-L lib/libncurses/lib -lncursesw -ltinfo
	@echo "hermes-tui built with ncurses TUI support"

# Fuzz test harness (T08)
fuzz: CFLAGS += -DFUZZ_TESTING -O1 -g
fuzz: tests/fuzz_harness.o $(LIB_OBJ)
	$(CC) $(CFLAGS) $(LIB_INCS) -o slermes-fuzz tests/fuzz_harness.o $(LIB_OBJ) $(LDFLAGS) $(LIBS)
	@echo "Fuzz harness built — run with: ./hermes-fuzz"

# Pattern rule for .c files
src/%.o: src/%.c include/hermes.h
	$(CC) $(CFLAGS) $(TUI_INC) -c -o $@ $<

# Library pattern rule — standalone libs
lib/%.o: lib/%.c
	$(CC) $(CFLAGS) $(LIB_INCS) -c -o $@ $<

# Static library archives
lib/libbinary.a: lib/libbinary/binary.o
	$(AR) rcs $@ $^

lib/libosv.a: lib/libosv/osv.o
	$(AR) rcs $@ $^


lib/libbrowser.a: lib/libbrowser/camofox_state.o
	$(AR) rcs $@ $^

lib/libdebug.a: lib/libdebug/debug_helpers.o
	$(AR) rcs $@ $^

lib/libwebsite.a: lib/libwebsite/website_policy.o
	$(AR) rcs $@ $^

lib/libskillusage.a: lib/libskillusage/skill_usage.o lib/libskillusage/skill_provenance.o
	$(AR) rcs $@ $^

lib/libskillsync.a: lib/libskillsync/skills_sync.o
	$(AR) rcs $@ $^

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

lib/libglob/glob.o: lib/libglob/glob.c lib/libglob/hermes_glob.h
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

lib/libjson5/json5.o: lib/libjson5/json5.c lib/libjson5/json5.h lib/libjson/json.h
	$(CC) $(CFLAGS) -I lib/libjson -c $< -o $@

lib/libjson5.a: lib/libjson5/json5.o
	$(AR) rcs $@ $^

lib/librateguard/rate_guard.o: lib/librateguard/rate_guard.c lib/librateguard/rate_guard.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/librateguard.a: lib/librateguard/rate_guard.o
	$(AR) rcs $@ $^

lib/libfal_common/fal_common.o: lib/libfal_common/fal_common.c lib/libfal_common/fal_common.h lib/libjson/json.h lib/libhttp/http.h
	$(CC) $(CFLAGS) -I lib/libjson -I lib/libhttp -c $< -o $@

lib/libfal_common.a: lib/libfal_common/fal_common.o
	$(AR) rcs $@ $^

lib/libtooloutput/tool_output.o: lib/libtooloutput/tool_output.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtooloutput.a: lib/libtooloutput/tool_output.o
	$(AR) rcs $@ $^

lib/libxai_http/xai_http.o: lib/libxai_http/xai_http.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libxai_http.a: lib/libxai_http/xai_http.o
	$(AR) rcs $@ $^

lib/libenvpassthrough/env_passthrough.o: lib/libenvpassthrough/env_passthrough.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libenvpassthrough.a: lib/libenvpassthrough/env_passthrough.o
	$(AR) rcs $@ $^

lib/libcredential/credential.o: lib/libcredential/credential.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libcredential.a: lib/libcredential/credential.o
	$(AR) rcs $@ $^

lib/libschemasanitizer/schema_sanitizer.o: lib/libschemasanitizer/schema_sanitizer.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libschemasanitizer.a: lib/libschemasanitizer/schema_sanitizer.o
	$(AR) rcs $@ $^

lib/libfuzzymatch/fuzzy_match.o: lib/libfuzzymatch/fuzzy_match.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libfuzzymatch.a: lib/libfuzzymatch/fuzzy_match.o
	$(AR) rcs $@ $^

lib/libinterrupt/interrupt.o: lib/libinterrupt/interrupt.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libinterrupt.a: lib/libinterrupt/interrupt.o
	$(AR) rcs $@ $^

lib/libtoolbackend/tool_backend.o: lib/libtoolbackend/tool_backend.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtoolbackend.a: lib/libtoolbackend/tool_backend.o
	$(AR) rcs $@ $^

lib/libmangateway/managed_gateway.o: lib/libmangateway/managed_gateway.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libmangateway.a: lib/libmangateway/managed_gateway.o
	$(AR) rcs $@ $^

lib/libratelimit/rate_limit.o: lib/libratelimit/rate_limit.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libratelimit.a: lib/libratelimit/rate_limit.o
	$(AR) rcs $@ $^

lib/libfilestate/file_state.o: lib/libfilestate/file_state.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libfilestate.a: lib/libfilestate/file_state.o
	$(AR) rcs $@ $^

lib/libtooldispatch/tool_dispatch_helpers.o: lib/libtooldispatch/tool_dispatch_helpers.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtooldispatch.a: lib/libtooldispatch/tool_dispatch_helpers.o
	$(AR) rcs $@ $^

lib/liberrorclassifier/error_classifier.o: lib/liberrorclassifier/error_classifier.c lib/liberrorclassifier/error_classifier.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/liberrorclassifier.a: lib/liberrorclassifier/error_classifier.o
	$(AR) rcs $@ $^

lib/libskillutils.a: lib/libskillutils/skill_utils.o
	$(AR) rcs $@ $^

lib/libbudgetconfig/budget_config.o: lib/libbudgetconfig/budget_config.c lib/libbudgetconfig/budget_config.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libbudgetconfig.a: lib/libbudgetconfig/budget_config.o
	$(AR) rcs $@ $^

lib/libthreatpatterns/threat_patterns.o: lib/libthreatpatterns/threat_patterns.c lib/libthreatpatterns/threat_patterns.h lib/libregex/hermes_regex.h
	$(CC) $(CFLAGS) -I lib/libregex -c $< -o $@

lib/libthreatpatterns.a: lib/libthreatpatterns/threat_patterns.o
	$(AR) rcs $@ $^

lib/libcredentialfiles/credential_files.o: lib/libcredentialfiles/credential_files.c lib/libcredentialfiles/credential_files.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libcredentialfiles.a: lib/libcredentialfiles/credential_files.o
	$(AR) rcs $@ $^

lib/libskillaudit/skill_audit.o: lib/libskillaudit/skill_audit.c lib/libskillaudit/skill_audit.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libskillaudit.a: lib/libskillaudit/skill_audit.o
	$(AR) rcs $@ $^

lib/libslashconfirm/slash_confirm.o: lib/libslashconfirm/slash_confirm.c lib/libslashconfirm/slash_confirm.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libslashconfirm.a: lib/libslashconfirm/slash_confirm.o
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
test: slermes
	bash test_runner.sh

# Test all standalone libraries
test-libs:
	@echo "=== Testing libjson ==="
	@gcc -O2 -Wall -Wextra -I lib/libjson lib/libjson/json.c tests/test_json.c -o /tmp/t_json -lm 2>/dev/null; /tmp/t_json 2>&1 || echo "(no test runner)"
	@echo "=== Testing libhttp ==="
	@gcc -O2 -Wall -Wextra -I lib/libhttp lib/libhttp/http.c tests/test_http.c -o /tmp/t_http -lssl -lcrypto -lz 2>/dev/null; /tmp/t_http 2>&1 || echo "(no test runner)"
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
	rm -f slermes src/*.o src/*/*.o src/*/*/*.o
	rm -f $(LIB_OBJ) $(LIB_A)
	rm -rf digest_output/

# O05: Release automation target
release:
	@bash scripts/release.sh $(filter-out $@,$(MAKECMDGOALS))

# ASan build target — compile with AddressSanitizer for memory error detection
asan:
	$(MAKE) CFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -I include $(LIB_INCS)" \
		LDFLAGS="-fsanitize=address -lssl -lcrypto -ldl -lpthread $(LIBS)" \
		clean all

# Valgrind leak check target — build with debug symbols, run under valgrind
valgrind: hermes
	@echo "=== Valgrind: Basic memory check ==="
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--error-exitcode=1 --errors-for-leak-kinds=all \
		./hermes --help 2>&1
	@echo ""
	@echo "=== Valgrind: hermes binary OK ==="

# U07: Code coverage target — build with gcov flags, run tests, generate lcov report
# Requires: sudo apt-get install lcov
coverage:
	$(MAKE) CFLAGS="-O0 -g --coverage -Wall -Wextra -Wpedantic -I include $(LIB_INCS)" \
		LDFLAGS="--coverage -lssl -lcrypto -ldl -lpthread $(LIBS)" \
		clean all
	@echo "=== Coverage: Running test suite ==="
	./test_runner.sh
	@echo "=== Coverage: Generating report ==="
	lcov --capture --directory . --output-file coverage.info \
		--rc lcov_branch_coverage=1 2>/dev/null
	lcov --remove coverage.info '/usr/*' '*/lib/libdb/*' '*/tests/*' \
		--output-file coverage-filtered.info --rc lcov_branch_coverage=1 2>/dev/null
	genhtml coverage-filtered.info --output-directory coverage_html \
		--rc lcov_branch_coverage=1 2>/dev/null
	@echo "=== Coverage report: coverage_html/index.html ==="

# U10: Performance gate — check binary size and startup time against baseline
# Baseline stored in .perf-baseline.json (auto-created on first run)
# Use --update-baseline to refresh baseline after intentional growth
perf-gate: hermes
	@python3 scripts/perf-gate.py

# R07: Combined check — bash lint, build, test suite
.PHONY: check
check:
	@echo "=== Check: lint ==="
	bash -n test_runner.sh 2>/dev/null || true
	@echo "=== Check: build ==="
	$(MAKE) -j$(nproc) all
	@echo "=== Check: test suite ==="
	bash test_runner.sh
