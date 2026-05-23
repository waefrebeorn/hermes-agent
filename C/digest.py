#!/usr/bin/env python3
"""
digest.py — WuBu Hermes C Translation Digestion Engine

Analyzes Python code changes and maps them to C translation tasks.
Supports upstream tracking for the Super Fork: automatically detect
new Python changes from upstream and flag what C work is needed.

Usage:
  python3 digest.py                           # Diff last commit
  python3 digest.py --diff HEAD~5..HEAD       # Custom range
  python3 digest.py --modules tools/terminal.py tools/file_tools.py  # Manual

  # Super Fork upstream tracking:
  python3 digest.py --upstream                # Fetch origin/main, diff, report
  python3 digest.py --upstream --merge        # Fetch + merge into wubu/main
"""
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

# File mapping: Python module → C file
FILE_MAP = {
    # === Root modules ===
    "run_agent.py":                          ("agent/agent_loop", "Core agent conversation loop"),
    "cli.py":                                ("cli/cli", "CLI orchestrator"),
    "model_tools.py":                        ("tools/registry", "Tool orchestration"),
    "toolsets.py":                           ("tools/registry", "Toolset definitions"),
    "hermes_state.py":                       ("lib/libdb/db", "File-based session store"),
    "hermes_constants.py":                   (".", "Constants (inline in hermes.h)"),
    "hermes_logging.py":                     (".", "Logging (inline or syslog)"),
    "hermes_time.py":                        ("lib/libdatetime/datetime", "Datetime library"),
    "batch_runner.py":                       (".", "Batch processing (not ported)"),
    "mcp_serve.py":                          ("mcp_serve", "MCP server"),
    "utils.py":                              (".", "Utilities (distributed across libs)"),
    "mini_swe_runner.py":                    (".", "SWE-bench runner (not ported)"),
    "hermes_bootstrap.py":                   ("main", "CLI entry"),
    "trajectory_compressor.py":              (".", "Trajectory compression (not ported)"),
    "toolset_distributions.py":              (".", "Toolset distributions (not ported)"),

    # === Agent core ===
    "agent/agent_init.py":                   ("agent/agent_loop", "Agent initialization"),
    "agent/conversation_loop.py":            ("agent/agent_loop", "Conversation loop"),
    "agent/agent_runtime_helpers.py":        (".", "Runtime helpers (not ported)"),
    "agent/async_utils.py":                  (".", "Async patterns (C synchronous)"),
    "agent/chat_completion_helpers.py":      ("agent/llm_client", "LLM client"),
    "agent/tool_dispatch_helpers.py":        ("tools/registry", "Tool dispatch"),
    "agent/tool_executor.py":                ("agent/agent_loop", "Tool execution"),
    "agent/tool_guardrails.py":              (".", "Tool guardrails (not ported)"),
    "agent/tool_result_classification.py":   (".", "Result classification (not ported)"),
    "agent/prompt_builder.py":               ("agent/context", "Prompt building"),
    "agent/prompt_caching.py":               (".", "Prompt caching (not ported)"),
    "agent/system_prompt.py":                ("agent/context", "System prompt"),
    "agent/subdirectory_hints.py":           (".", "Subdirectory detection (not ported)"),
    "agent/shell_hooks.py":                  (".", "Shell hooks (not ported)"),
    "agent/iteration_budget.py":             ("agent/budget_tracker", "Budget tracking"),
    "agent/credential_pool.py":              ("agent/credential_pool", "Credential pool"),
    "agent/credential_sources.py":           ("secrets", "Credential sources"),
    "agent/retry_utils.py":                  ("agent/fallback_routing", "Retry with backoff"),
    "agent/rate_limit_tracker.py":           ("tools/rate_limit", "Rate limiting"),
    "agent/error_classifier.py":             ("hermes_error", "Error classification"),

    # === Provider adapters ===
    "agent/anthropic_adapter.py":            ("agent/provider_anthropic", "Anthropic provider"),
    "agent/bedrock_adapter.py":              ("agent/provider_bedrock", "Bedrock provider"),
    "agent/azure_identity_adapter.py":       ("agent/provider_azure", "Azure provider"),
    "agent/codex_responses_adapter.py":      (".", "Codex Responses (not ported)"),
    "agent/codex_runtime.py":                (".", "Codex runtime (not ported)"),
    "agent/gemini_cloudcode_adapter.py":     (".", "Gemini Cloud Code (not ported)"),
    "agent/gemini_native_adapter.py":        (".", "Gemini native (not ported)"),
    "agent/gemini_schema.py":                (".", "Gemini schema (not ported)"),
    "agent/google_code_assist.py":           (".", "Google Code Assist (not ported)"),
    "agent/openai_adapter.py":               ("agent/provider_openai", "OpenAI provider"),
    "agent/openrouter_adapter.py":           ("agent/provider_openrouter", "OpenRouter provider"),
    "agent/deepseek_adapter.py":             ("agent/provider_deepseek", "DeepSeek provider"),
    "agent/xai_adapter.py":                  ("agent/provider_xai", "xAI provider"),
    "agent/google_adapter.py":               ("agent/provider_google", "Google provider"),
    "agent/google_oauth.py":                 (".", "Google OAuth (not ported)"),
    "agent/custom_adapter.py":               ("agent/provider_custom", "Custom provider"),

    # === Memory & Context ===
    "agent/context_compressor.py":           (".", "Context compression (not ported)"),
    "agent/conversation_compression.py":     (".", "Conv compression (not ported)"),
    "agent/context_engine.py":               (".", "Context engine (not ported)"),
    "agent/context_references.py":           ("agent/context", "Context references"),
    "agent/memory_manager.py":              ("tools/memory", "Memory management"),
    "agent/memory_provider.py":              ("lib/libplugin/plugin", "Memory provider (plugin)"),
    "agent/manual_compression_feedback.py":  (".", "Compression feedback (not ported)"),
    "agent/markdown_tables.py":              (".", "Markdown tables (not ported)"),

    # === Skills ===
    "agent/skill_commands.py":               ("tools/skill_mgmt", "Skill commands"),
    "agent/skill_utils.py":                  ("tools/skills", "Skill utilities"),
    "agent/skill_bundles.py":                (".", "Skill bundles (not ported)"),
    "agent/skill_preprocessing.py":          (".", "Skill preprocessing (not ported)"),
    "agent/curator.py":                      (".", "Curator (not ported)"),
    "agent/curator_backup.py":               (".", "Curator backup (not ported)"),
    "agent/trajectory.py":                   (".", "Trajectory (not ported)"),
    "agent/onboarding.py":                   (".", "Onboarding (not ported)"),

    # === Image, Video, Web Search ===
    "agent/image_gen_provider.py":           ("tools/image_gen", "Image generation"),
    "agent/image_gen_registry.py":           (".", "Image gen registry (not ported)"),
    "agent/image_routing.py":                (".", "Image routing (not ported)"),
    "agent/video_gen_provider.py":           (".", "Video gen (not ported)"),
    "agent/video_gen_registry.py":           (".", "Video gen registry (not ported)"),
    "agent/web_search_provider.py":          ("tools/web", "Web search"),
    "agent/web_search_registry.py":          (".", "Web search registry (not ported)"),

    # === LSP ===
    "agent/lsp/":                            (".", "LSP integration (not ported)"),

    # === Transports ===
    "agent/transports/":                     ("lib/libmcp/mcp", "MCP transport"),
    "agent/auxiliary_client.py":             (".", "Auxiliary client (not ported)"),
    "agent/background_review.py":            (".", "Background review (not ported)"),
    "agent/browser_provider.py":             ("tools/browser", "Browser provider"),
    "agent/browser_registry.py":             (".", "Browser registry (not ported)"),

    # === Display & Security ===
    "agent/display.py":                      ("cli/display", "Terminal display"),
    "agent/file_safety.py":                  ("tools/url_safety", "File path safety"),
    "agent/i18n.py":                         (".", "Internationalization (not ported)"),
    "agent/insights.py":                     (".", "Insights (not ported)"),
    "agent/message_sanitization.py":         ("agent/sanitize", "Message sanitization"),
    "agent/model_metadata.py":               ("agent/provider_metadata", "Provider metadata"),
    "agent/models_dev.py":                   (".", "Model dev tools (not ported)"),
    "agent/plugin_llm.py":                   ("agent/plugin_ext", "Plugin LLM"),
    "agent/redact.py":                       ("agent/redact", "Secret redaction"),
    "agent/stream_diag.py":                  (".", "Stream diagnostics (not ported)"),
    "agent/think_scrubber.py":               (".", "Think scrubber (not ported)"),
    "agent/title_generator.py":              ("agent/title", "Title generation"),
    "agent/usage_pricing.py":                (".", "Usage pricing (not ported)"),

    # === Tools ===
    "tools/terminal_tool.py":                ("tools/terminal", "Terminal execution"),
    "tools/file_tools.py":                   ("tools/file", "File I/O tools"),
    "tools/file_operations.py":              ("tools/file", "File operations"),
    "tools/file_state.py":                   ("tools/file_batch", "File batch state"),
    "tools/web_tools.py":                    ("tools/web", "Web search tools"),
    "tools/skills_tool.py":                  ("tools/skills", "Skill management"),
    "tools/skills_guard.py":                 (".", "Skills security guard (not ported)"),
    "tools/skills_hub.py":                   ("skills_hub", "Skills hub"),
    "tools/skills_sync.py":                  (".", "Skills sync (not ported)"),
    "tools/vision_tools.py":                 ("tools/vision", "Vision analysis"),
    "tools/tts_tool.py":                     ("tools/tts", "Text-to-speech"),
    "tools/memory_tool.py":                  ("tools/memory", "Memory tool"),
    "tools/cronjob_tools.py":                ("tools/cronjob", "Cron job tool"),
    "tools/todo_tool.py":                    ("tools/todo", "Todo tool"),
    "tools/delegate_tool.py":                ("tools/delegate", "Delegate tool"),
    "tools/clarify_tool.py":                 ("tools/clarify", "Clarify tool"),
    "tools/clarify_gateway.py":              ("tools/clarify", "Clarify gateway"),
    "tools/discord_tool.py":                 ("tools/discord", "Discord tool"),
    "tools/send_message_tool.py":            ("tools/send_message", "Send message"),
    "tools/session_search_tool.py":          ("tools/session_search", "Session search"),
    "tools/kanban_tools.py":                 ("plugins/plugin_kanban", "Kanban tool"),
    "tools/browser_tool.py":                 ("tools/browser", "Browser (stub)"),
    "tools/browser_camofox.py":              (".", "Camofox browser (not ported)"),
    "tools/browser_camofox_state.py":        (".", "Camofox state (not ported)"),
    "tools/browser_cdp_tool.py":             (".", "CDP browser (not ported)"),
    "tools/browser_dialog_tool.py":          (".", "Browser dialog (not ported)"),
    "tools/browser_supervisor.py":           (".", "Browser supervisor (not ported)"),
    "tools/computer_use/":                   ("tools/computer_use", "Computer use (stub)"),
    "tools/computer_use_tool.py":            ("tools/computer_use", "Computer use tool (stub)"),
    "tools/code_execution_tool.py":          ("tools/exec_code", "Code execution"),
    "tools/image_generation_tool.py":        ("tools/image_gen", "Image generation"),
    "tools/video_generation_tool.py":        (".", "Video generation (not ported)"),
    "tools/transcription_tools.py":          (".", "Transcription (not ported)"),
    "tools/x_search_tool.py":                ("tools/x_search", "X search"),
    "tools/homeassistant_tool.py":           ("tools/homeassistant", "Home Assistant"),
    "tools/yuanbao_tools.py":                (".", "Yuanbao tool (not ported)"),
    "tools/patch_parser.py":                 ("tools/patch", "Patch parser"),
    "tools/approval.py":                     ("tools/approval", "Approval system"),
    "tools/url_safety.py":                   ("tools/url_safety", "URL safety"),
    "tools/slash_confirm.py":                (".", "Slash confirm (not ported)"),
    "tools/tirith_security.py":              ("tools/tirith", "TIRITH policy"),
    "tools/tool_result_storage.py":          ("tools/result_storage", "Result storage"),
    "tools/tool_backend_helpers.py":         (".", "Tool backend helpers (not ported)"),
    "tools/tool_output_limits.py":           (".", "Tool output limits (not ported)"),
    "tools/fal_common.py":                   (".", "Fal AI common (not ported)"),
    "tools/feishu_doc_tool.py":              (".", "Feishu doc (not ported)"),
    "tools/feishu_drive_tool.py":            (".", "Feishu drive (not ported)"),
    "tools/mcp_tool.py":                     ("tools/mcp_tool", "MCP tool"),
    "tools/mcp_oauth.py":                    (".", "MCP OAuth (not ported)"),
    "tools/mcp_oauth_manager.py":            (".", "MCP OAuth manager (not ported)"),
    "tools/environments/":                   (".", "Terminal environments (not ported)"),
    "tools/voice_mode.py":                   ("tools/voice_mode", "Voice mode"),
    "tools/neutts_synth.py":                 (".", "Neural TTS (not ported)"),
    "tools/mixture_of_agents_tool.py":       (".", "MoA tool (not ported)"),
    "tools/openrouter_client.py":            (".", "OpenRouter client (not ported)"),
    "tools/osv_check.py":                    (".", "OSV check (not ported)"),
    "tools/process_registry.py":             ("tools/process", "Process management"),
    "tools/schema_sanitizer.py":             (".", "Schema sanitizer (not ported)"),
    "tools/skill_provenance.py":             (".", "Skill provenance (not ported)"),
    "tools/skill_usage.py":                  (".", "Skill usage (not ported)"),
    "tools/credential_files.py":             ("secrets", "Credential files"),
    "tools/checkpoint_manager.py":           ("agent/checkpoint", "Checkpoint manager"),
    "tools/budget_config.py":                ("agent/budget_tracker", "Budget config"),
    "tools/binary_extensions.py":            ("lib/libbinary/binary", "Binary extension detection"),
    "tools/ansi_strip.py":                   ("lib/libansi/ansi", "ANSI stripping"),
    "tools/fuzzy_match.py":                  (".", "Fuzzy matching (not ported)"),
    "tools/debug_helpers.py":                (".", "Debug helpers (not ported)"),
    "tools/env_passthrough.py":              (".", "Env passthrough (not ported)"),
    "tools/interrupt.py":                    (".", "Interrupt (not ported)"),
    "tools/lazy_deps.py":                    (".", "Lazy deps (not ported)"),
    "tools/managed_tool_gateway.py":         (".", "Managed gateway (not ported)"),
    "tools/microsoft_graph_auth.py":         (".", "MS Graph auth (not ported)"),
    "tools/microsoft_graph_client.py":       (".", "MS Graph client (not ported)"),
    "tools/path_security.py":                ("tools/url_safety", "Path security"),
    "tools/website_policy.py":               (".", "Website policy (not ported)"),
    "tools/xai_http.py":                     (".", "xAI HTTP (not ported)"),

    # === Gateway ===
    "gateway/run.py":                        ("gateway/server", "Gateway server"),
    "gateway/session.py":                    ("gateway/server", "Gateway session"),
    "gateway/config.py":                     (".", "Gateway config (inline)"),
    "gateway/delivery.py":                   ("gateway/server", "Message delivery"),
    "gateway/hooks.py":                      (".", "Gateway hooks (not ported)"),
    "gateway/builtin_hooks/":                (".", "Built-in hooks (not ported)"),
    "gateway/channel_directory.py":          (".", "Channel directory (not ported)"),
    "gateway/mirror.py":                     (".", "Message mirroring (not ported)"),
    "gateway/pairing.py":                    (".", "Gateway pairing (not ported)"),
    "gateway/restart.py":                    (".", "Gateway restart (not ported)"),
    "gateway/runtime_footer.py":             (".", "Runtime footer (not ported)"),
    "gateway/session_context.py":            (".", "Session context (not ported)"),
    "gateway/shutdown_forensics.py":         (".", "Shutdown forensics (not ported)"),
    "gateway/slash_access.py":               (".", "Slash access (not ported)"),
    "gateway/status.py":                     (".", "Gateway status (not ported)"),
    "gateway/sticker_cache.py":              (".", "Sticker cache (not ported)"),
    "gateway/stream_consumer.py":            (".", "Stream consumer (not ported)"),
    "gateway/display_config.py":             (".", "Display config (not ported)"),
    "gateway/memory_monitor.py":             (".", "Memory monitor (not ported)"),
    "gateway/platform_registry.py":          (".", "Platform registry (not ported)"),
    "gateway/whatsapp_identity.py":          (".", "WhatsApp identity (not ported)"),
    "gateway/platforms/telegram.py":         ("gateway/platforms/telegram", "Telegram"),
    "gateway/platforms/discord.py":          ("gateway/platforms/discord", "Discord"),
    "gateway/platforms/slack.py":            ("gateway/platforms/slack", "Slack"),
    "gateway/platforms/matrix.py":           ("gateway/platforms/matrix", "Matrix"),
    "gateway/platforms/mattermost.py":       ("gateway/platforms/mattermost", "Mattermost"),
    "gateway/platforms/whatsapp.py":         ("gateway/platforms/whatsapp", "WhatsApp"),
    "gateway/platforms/email.py":            ("gateway/platforms/email", "Email"),
    "gateway/platforms/signal.py":           ("gateway/platforms/signal", "Signal"),
    "gateway/platforms/homeassistant.py":    ("gateway/platforms/homeassistant", "HomeAssistant"),
    "gateway/platforms/sms.py":              ("gateway/platforms/sms", "SMS"),
    "gateway/platforms/feishu.py":           ("gateway/platforms/feishu", "Feishu"),
    "gateway/platforms/wecom.py":            ("gateway/platforms/wecom", "WeCom"),
    "gateway/platforms/dingtalk.py":         ("gateway/platforms/dingtalk", "DingTalk"),
    "gateway/platforms/qqbot/":              ("gateway/platforms/qqbot", "QQ Bot"),
    "gateway/platforms/bluebubbles.py":      ("gateway/platforms/bluebubbles", "BlueBubbles"),
    "gateway/platforms/msgraph_webhook.py":  ("gateway/platforms/msgraph_webhook", "MSGraph"),
    "gateway/platforms/weixin.py":           ("gateway/platforms/weixin", "Weixin"),
    "gateway/platforms/yuanbao.py":          ("gateway/platforms/yuanbao", "Yuanbao"),
    "gateway/platforms/webhook.py":          ("gateway/platforms/webhook", "Webhook"),

    # === ACP ===
    "acp_adapter/server.py":                 ("acp/server", "ACP server"),
    "acp_adapter/auth.py":                   (".", "ACP auth (not ported)"),
    "acp_adapter/edit_approval.py":          (".", "ACP edit approval (not ported)"),
    "acp_adapter/entry.py":                  (".", "ACP entry (not ported)"),
    "acp_adapter/events.py":                 (".", "ACP events (not ported)"),
    "acp_adapter/permissions.py":            (".", "ACP permissions (not ported)"),
    "acp_adapter/session.py":                (".", "ACP session (not ported)"),
    "acp_adapter/tools.py":                  (".", "ACP tools (not ported)"),

    # === Cron ===
    "cron/scheduler.py":                     ("cron/scheduler", "Cron scheduler"),
    "cron/jobs.py":                          ("cron/jobs", "Job management"),

    # === CLI ===
    "hermes_cli/main.py":                    ("cli/main", "CLI entry"),
    "hermes_cli/config.py":                  ("cli/config", "Config loading"),
    "hermes_cli/commands.py":                ("cli/commands", "Slash commands"),
    "hermes_cli/skin_engine.py":             ("cli/display", "Skin engine"),
    "hermes_cli/completion.py":              (".", "Tab completion (not ported)"),
    "hermes_cli/banner.py":                  ("cli/display", "Banner display"),
    "hermes_cli/colors.py":                  ("lib/libansi", "ANSI colors"),
    "hermes_cli/cli_output.py":              ("cli/display", "CLI output"),
    "hermes_cli/curses_ui.py":               ("cli/tui_fullscreen", "Curses TUI"),
    "hermes_cli/setup.py":                   (".", "Setup wizard (not ported)"),
    "hermes_cli/auth.py":                    ("secrets", "Auth management"),
    "hermes_cli/providers.py":               (".", "Provider CLI (not ported)"),
    "hermes_cli/models.py":                  (".", "Model CLI (not ported)"),
    "hermes_cli/plugins.py":                 (".", "Plugin CLI (not ported)"),
    "hermes_cli/gateway.py":                 (".", "Gateway CLI (not ported)"),
    "hermes_cli/logs.py":                    (".", "Logs CLI (not ported)"),
    "hermes_cli/debug.py":                   (".", "Debug CLI (not ported)"),
    "hermes_cli/doctor.py":                  (".", "Doctor CLI (not ported)"),
    "hermes_cli/skills_hub.py":              (".", "Skills hub CLI (not ported)"),
    "hermes_cli/secrets_cli.py":             ("secrets", "Secrets CLI"),
    "hermes_cli/tools_config.py":            ("tools/tool_config", "Tools config"),
    "hermes_cli/xai_retirement.py":          ("xai_retirement", "xAI retirement"),
}


# Phase ordering
PHASES = {
    1: "Foundation (deps)",
    2: "Agent Core",
    3: "Tools",
    4: "Gateway",
    5: "Cron + Advanced",
}

PHASE_MAP = {
    "deps/": 1,
    "agent/agent_loop": 2,
    "agent/llm_client": 2,
    "agent/context": 2,
    "cli/main": 2,
    "cli/cli": 2,
    "cli/display": 2,
    "tools/": 3,
    "gateway/": 4,
    "cron/": 5,
    "agent/": 5,
}

HERMES_HOME = Path(__file__).resolve().parent.parent  # C/.. = repo root
C_DIR = HERMES_HOME / "C"
DEP_MD = C_DIR / "DEPENDENCIES.md"
DIGEST_STATE = C_DIR / ".digest_state.json"


def git(*args, cwd=HERMES_HOME, check=True, capture=True, timeout=15):
    """Run git command, return stdout string or CompletedProcess."""
    result = subprocess.run(
        ["git"] + list(args), capture_output=capture, text=True,
        cwd=cwd, timeout=timeout, check=check
    )
    if capture:
        return result.stdout.strip()
    return result


def get_changed_files_since(ref="HEAD~1..HEAD"):
    """Get list of changed Python files since git ref."""
    try:
        result = git("diff", "--name-only", ref, "--", "*.py")
        return [f for f in result.split("\n") if f]
    except Exception:
        return []


def get_diff(ref="HEAD~1..HEAD"):
    """Get full diff text for analysis."""
    try:
        return git("diff", ref, "--", "*.py")
    except Exception:
        return ""


def extract_functions_from_diff(diff_text):
    """Extract new/modified function signatures from diff."""
    funcs = []
    for line in diff_text.split("\n"):
        m = re.match(r'^\+.*def (\w+)\(([^)]*)\)', line)
        if m:
            funcs.append(f"{m.group(1)}({m.group(2)})")
    return funcs


def extract_classes_from_diff(diff_text):
    """Extract new/modified classes from diff."""
    classes = []
    for line in diff_text.split("\n"):
        m = re.match(r'^\+.*class (\w+)\(?([^)]*)\)?\s*:', line)
        if m:
            classes.append({"name": m.group(1), "bases": m.group(2)})
    return classes


def extract_imports_from_diff(diff_text):
    """Extract new imports added in diff."""
    imports = []
    for line in diff_text.split("\n"):
        m = re.match(r'^\+.*(?:import|from)\s+(\S+)', line)
        if m:
            imports.append(m.group(1))
    return imports


def map_to_c_file(py_file):
    """Map a Python file path to its C translation path."""
    py_file = py_file.replace("\\", "/")

    for pattern, (c_path, desc) in FILE_MAP.items():
        if py_file == pattern or py_file.endswith("/" + pattern):
            return c_path, desc

    stem = Path(py_file).stem
    parent = Path(py_file).parent

    if str(parent) == ".":
        return f"agent/{stem}", f"Module: {stem}"

    first_dir = str(parent).split("/")[0]
    if first_dir in ("tools", "gateway", "cron", "agent", "hermes_cli", "plugins"):
        return f"{first_dir}/{stem}", f"Module: {stem}"

    return f"unknown/{stem}", f"Unmapped: {py_file}"


def get_phase(c_path):
    """Determine translation phase for a C path."""
    for prefix, phase in sorted(PHASE_MAP.items(), key=lambda x: -len(x[0])):
        if c_path.startswith(prefix) or c_path == prefix.rstrip("/"):
            return phase
    return 5


def load_translation_status():
    """Load current translation status from DEPENDENCIES.md."""
    if not DEP_MD.exists():
        return {}
    status = {}
    content = DEP_MD.read_text()
    for line in content.split("\n"):
        parts = [p.strip() for p in line.split("|")]
        if len(parts) >= 5:
            name = parts[1].strip("`")
            c_eq = parts[3].strip("`")
            stat = parts[4].strip()
            if name and stat in ("🔲", "🔄", "✅"):
                status[name] = {"c_equiv": c_eq, "status": stat}
    return status


def generate_report(changed_files, funcs, classes, imports):
    """Generate a structured report of what needs translation."""
    report = {
        "summary": {
            "python_files_changed": len(changed_files),
            "functions_detected": len(funcs),
            "classes_detected": len(classes),
            "new_imports": len(imports),
        },
        "changed_files": [],
        "work_items": [],
    }

    for py_file in changed_files:
        c_path, desc = map_to_c_file(py_file)
        phase = get_phase(c_path)

        entry = {
            "python": py_file,
            "c_file": f"src/{c_path}.c",
            "c_header": f"include/{c_path}.h" if c_path != "." else "include/hermes.h",
            "description": desc,
            "phase": phase,
            "phase_name": PHASES.get(phase, "Unknown"),
        }
        report["changed_files"].append(entry)

        c_exists = (C_DIR / "src" / f"{c_path}.c").exists()

        item = {
            "c_file": entry["c_file"],
            "status": "✅ up to date" if (c_exists and not funcs) else "🔄 needs update" if c_exists else "🔲 not started",
            "phase": entry["phase_name"],
            "action": "Implement" if not c_exists else "Update for new functions",
            "functions": funcs[:5],
            "classes": [c["name"] for c in classes[:3]],
        }
        report["work_items"].append(item)

    report["work_items"].sort(key=lambda x: x.get("phase_num", 99) or
                              [k for k, v in PHASES.items() if v == x["phase"]][0])

    return report


def print_report(report, title="Digestion Report"):
    """Print human-readable report."""
    s = report["summary"]
    print(f"\n{'═'*60}")
    print(f"  {title}")
    print(f"  {s['python_files_changed']} Python files changed")
    print(f"  Functions: {s['functions_detected']}  Classes: {s['classes_detected']}  Imports: {s['new_imports']}")
    print(f"{'═'*60}")

    if not report["changed_files"]:
        print("\n  No Python files changed since last digest.\n")
        return

    print(f"\n{'─'*60}")
    print(f"  Translation Work Items (by phase):")
    print(f"{'─'*60}")

    current_phase = None
    for item in report["work_items"]:
        if item["phase"] != current_phase:
            current_phase = item["phase"]
            pnum = [k for k, v in PHASES.items() if v == current_phase][0]
            print(f"\n  ▸ Phase {pnum}: {current_phase}")
            print(f"    {'─'*50}")

        status_icon = {"✅ up to date": "✅", "🔄 needs update": "🔄", "🔲 not started": "🔲"}
        icon = status_icon.get(item["status"], "❓")
        print(f"    {icon} {item['c_file']}")
        print(f"       {item['action']}")
        if item.get("functions"):
            print(f"       fn: {', '.join(item['functions'][:3])}")
        if item.get("classes"):
            print(f"       cls: {', '.join(item['classes'][:3])}")

    print()


# ─── Super Fork: Upstream Tracking ─────────────────────────────────────

def load_digest_state():
    """Load last sync state."""
    if DIGEST_STATE.exists():
        return json.loads(DIGEST_STATE.read_text())
    return {"last_sync_commit": None, "last_sync_time": None, "upstream_head": None}


def save_digest_state(state):
    """Save sync state."""
    DIGEST_STATE.write_text(json.dumps(state, indent=2))
    print(f"  [digest] state saved to {DIGEST_STATE}")


def upstream_sync(do_merge=False):
    """
    Super Fork upstream sync.
    1. Fetch origin/main (upstream)
    2. Find Python files changed since last sync
    3. Diff them, map to C work items
    4. Optionally merge upstream into current branch
    """
    state = load_digest_state()
    print(f"\n  [upstream] fetching origin/main...")
    git("fetch", "origin", "main", "--no-tags")

    # Get upstream head
    up_head = git("rev-parse", "origin/main")
    print(f"  [upstream] origin/main at {up_head[:12]}")

    last_sync = state.get("last_sync_commit")
    if last_sync:
        print(f"  [upstream] last sync was {last_sync[:12]}")
    else:
        # First sync: track all Python files
        print(f"  [upstream] first sync — full scan")
        last_sync = git("merge-base", "origin/main", "HEAD")

    # Diff Python files since last sync
    diff_range = f"{last_sync}..origin/main"
    print(f"  [upstream] diff range: {diff_range}")

    try:
        changed = get_changed_files_since(diff_range)
    except Exception as e:
        print(f"  [upstream] diff failed: {e}")
        changed = []

    # Check if there's any difference
    ahead = git("rev-list", "--count", f"{last_sync}..origin/main")
    print(f"  [upstream] {ahead.strip()} new commits on upstream")

    if not changed:
        print(f"  [upstream] no Python file changes since last sync\n")
        # Still update state so we track position
        state["last_sync_commit"] = up_head
        state["last_sync_time"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        state["upstream_head"] = up_head
        save_digest_state(state)
        print(f"\n{'='*60}")
        print(f"  ✅ Super Fork: No new C work needed — upstream Python unchanged")
        print(f"{'='*60}\n")
        return

    # Analyze diff
    diff_text = get_diff(diff_range)
    funcs = extract_functions_from_diff(diff_text)
    classes = extract_classes_from_diff(diff_text)
    imports = extract_imports_from_diff(diff_text)

    report = generate_report(changed, funcs, classes, imports)

    # Also list changed Python files nicely
    print(f"\n  [upstream] Changed Python files ({len(changed)}):")
    for f in changed:
        print(f"    • {f}")

    print_report(report, title="Super Fork: Upstream Change Digest")

    # Save sync state
    state["last_sync_commit"] = up_head
    state["last_sync_time"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    state["upstream_head"] = up_head
    state["last_changed_python"] = changed
    state["last_new_funcs"] = funcs
    state["last_new_classes"] = [c["name"] for c in classes]
    save_digest_state(state)

    # Generate JSON report for programmatic use
    report_path = C_DIR / "digest_output" / "upstream_report.json"
    report_path.parent.mkdir(exist_ok=True)
    report_path.write_text(json.dumps({
        "timestamp": state["last_sync_time"],
        "upstream_head": up_head,
        "changed_files": changed,
        "functions": funcs,
        "classes": [c["name"] for c in classes],
        "work_items": report["work_items"],
    }, indent=2))
    print(f"\n  [digest] JSON report saved to {report_path}")

    # Optionally merge upstream into current branch
    if do_merge:
        print(f"\n  [upstream] merging origin/main into current branch...")
        # Stash any local changes first
        git("stash", check=False, capture=False)
        try:
            git("merge", "origin/main", "-m", "chore: sync with upstream (auto-merge via digest.py)")
            print(f"  [upstream] merged successfully")
        except Exception as e:
            print(f"  [upstream] merge failed: {e}")
            print(f"  [upstream] run manually: git merge origin/main")
        git("stash", "pop", check=False, capture=False)

    return changed, funcs, classes, report


# ─── Stub Generation ────────────────────────────────────────────────────

def generate_stubs(report):
    """Generate C header stubs for unimplemented files."""
    stub_dir = C_DIR / "digest_output"
    stub_dir.mkdir(exist_ok=True)
    count = 0
    for item in report["work_items"]:
        if item["status"].startswith("🔲"):
            c_path = item["c_file"]
            stub_name = Path(c_path).name
            stub_file = stub_dir / stub_name
            if not stub_file.exists():
                guard = Path(c_path).stem.upper().replace("-", "_")
                stub_file.write_text(f"""#ifndef {guard}_H
#define {guard}_H

/* Auto-generated stub from Super Fork digestion.
 * Python: {item.get('python_file', 'unknown')}
 * Phase: {item.get('phase', 'unknown')}
 * Upstream trigger: origin/main
 */

// TODO: implement

#endif
""")
                count += 1
    print(f"  [digest] {count} stubs generated in {stub_dir}")
    return count


# ─── Main ───────────────────────────────────────────────────────────────

def main():
    import argparse
    parser = argparse.ArgumentParser(
        description="WuBu Hermes C Translation Digestion Engine — Super Fork",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 digest.py                              # Diff last commit
  python3 digest.py --diff HEAD~5..HEAD          # Custom range
  python3 digest.py --upstream                   # Fetch origin/main, diff, report
  python3 digest.py --upstream --merge           # Fetch + merge upstream
  python3 digest.py --upstream --generate-stubs  # Fetch + diff + stub new files
        """)
    parser.add_argument("--diff", default="HEAD~1..HEAD",
                        help="Git diff range to analyze")
    parser.add_argument("--modules", nargs="+",
                        help="Specific modules to digest")
    parser.add_argument("--format", choices=["summary", "json", "detailed"],
                        default="summary")
    parser.add_argument("--generate-stubs", action="store_true",
                        help="Generate C stub headers for unimplemented files")
    parser.add_argument("--upstream", action="store_true",
                        help="Super Fork mode: fetch origin/main, diff since last sync, report C work needed")
    parser.add_argument("--merge", action="store_true",
                        help="With --upstream: also merge upstream into current branch")
    args = parser.parse_args()

    if args.upstream:
        result = upstream_sync(do_merge=args.merge)
        if result:
            changed, funcs, classes, report = result
            if args.generate_stubs and report.get("work_items"):
                generate_stubs(report)
            if args.format == "json":
                print(json.dumps(report, indent=2))
        return

    if args.modules:
        changed_files = args.modules
        diff_text = ""
    else:
        changed_files = get_changed_files_since(args.diff)
        diff_text = get_diff(args.diff)

    funcs = extract_functions_from_diff(diff_text)
    classes = extract_classes_from_diff(diff_text)
    imports = extract_imports_from_diff(diff_text)

    report = generate_report(changed_files, funcs, classes, imports)

    if args.format == "json":
        print(json.dumps(report, indent=2))
    else:
        print_report(report)

    if args.generate_stubs:
        generate_stubs(report)


if __name__ == "__main__":
    main()
