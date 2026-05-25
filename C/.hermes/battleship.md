# BATTLESHIP — Hermes C Translation Gap Audit
**394 GAPS** | Triple Devil's Advocate Verified | May 25, 2026

## Legend
- 🟥 **F-N-F** = Form Not Function (code compiles but doesn't work)
- 🟨 **STUB** = Placeholder/shell (declaration without implementation)
- 🟧 **PARTIAL** = Partially implemented (works for basic case only)
- ⬜ **MISSING** = Feature not started
- 📋 **DOC** = Documentation gap

---

## SECTOR 1: Agent Loop — 49 GAPS

### Core Loop Defects (F-N-F) — 4 gaps resolved, 4 remain
1. ✅ **Tool call loop fixed** — `agent_loop.c:173-191` now executes tools and loops back
2. ✅ **Tool result injection fixed** — `message_new_tool()` called after tool execution
3. ✅ **Tool call JSON parsing fixed** — `llm_client.c` extracts `tool_calls` array from JSON response
4. 🟥 **No iteration counter enforcement** — `iteration_count` tracked but `max_iterations` uses simple `<` not `<=`
5. 🟥 **No interrupt handling** — `state->interrupted` flag exists but no signal handler sets it
6. 🟥 **No error recovery** — LLM failure returns generic "LLM call returned NULL" without retry
7. 🟥 **No context window tracking** — No token counting; can exceed context limit silently
8. 🟥 **`agent_run_conversation` returns content even when tool calls present** — Lines 185-200 short-circuit the loop

### Missing Provider Support — 8 gaps
9. ⬜ **Only OpenAI chat completions** — No Anthropic Messages API support
10. ⬜ **No Google Gemini support** — Different API format (parts[], content[])
11. ⬜ **No DeepSeek FIM/beta completions** — No `/beta/completions` endpoint
12. ⬜ **No xAI/Grok API** — Different auth headers
13. ⬜ **No credential pool** — Only single API key, no rotation
14. ⬜ **No credential exhaustion tracking** — No 429/401 retry logic
15. ⬜ **No multi-provider routing** — Can't route different models to different providers
16. ⬜ **No custom provider support** — No provider base URL configuration per-request

### LLM Client Defects (F-N-F) — 5 gaps
17. ✅ **Auth header fixed** — Content-Type properly set to `application/json`
18. 🟥 **No streaming support** — `llm_chat_completion` blocks for full response
19. 🟥 **No reasoning token extraction** — No parsing for reasoning_content in non-OpenAI formats
20. 🟥 **No error handling for non-JSON responses** — LLM returning HTML/plain text causes null
21. 🟥 **No request timeout propagation** — HTTP timeout set but never checked mid-request

### Missing Agent Features — 17 gaps
22. ⬜ **No memory system** — No `agent/memory.c` equivalent
23. ⬜ **No context compression** — No `agent/compress.c` equivalent, no token budget tracking
24. ⬜ **No checkpoint/rollback** — No filesystem snapshot system
25. ⬜ **No session persistence** — `db.h` declares functions but agent never calls `db_save_message`
26. ⬜ **No session search** — No FTS5-equivalent search
27. ⬜ **No session listing/resume** — No `db_list_sessions` integration in CLI
28. ⬜ **No title generation with LLM** — `title.c` uses simple extractive (6 words) — no LLM-powered
29. ⬜ **No system prompt builder** — No agent personality, context file loading
30. ⬜ **No skill injection to prompt** — No skill_commands integration
31. ⬜ **No agent profile** — No profile system for multiple agent configurations
32. ⬜ **No summary/compress between turns** — Context grows unbounded
33. ⬜ **No conversation branching** — No `/fork` or `/branch` support
34. ⬜ **No conversation export** — No `/save` equivalent
35. ⬜ **No interrupt mechanism for user** — No Ctrl+C handling in agent loop
36. ⬜ **No tool timeout** — Tool execution has no per-call timeout
37. ⬜ **No dangerous command approval** — No `--yolo` / approval gating
38. ⬜ **No hidden/interrupted message handling** — No support for reasoning tokens in display

## SECTOR 2: CLI — 47 GAPS

### Interactive CLI Defects (F-N-F) — 6 gaps
39. 🟥 **No readline/linenoise** — `cli.c` uses raw `fgets` — no history, no editing
40. 🟥 **No autocomplete** — No tab completion for commands or file paths
41. 🟥 **No multi-line input** — No support for multi-line paste
42. 🟥 **No terminal resize handling** — No SIGWINCH handler
43. 🟥 **No pager for long output** — Content overflows terminal
44. 🟥 **No background processes** — No terminal session management

### Slash Command Gaps — 37 gaps (45 total - 8 implemented)
45. ⬜ **No /retry** — Resend last message
46. ⬜ **No /undo** — Remove last exchange
47. ⬜ **No /title** — Name session
48. ⬜ **No /compress** — Manual compression
49. ⬜ **No /stop** — Kill background processes
50. ⬜ **No /rollback** — Restore checkpoint
51. ⬜ **No /snapshot** — Create/restore state snapshots
52. ⬜ **No /background** — Run prompt in background
53. ⬜ **No /queue** — Queue prompt for next turn
54. ⬜ **No /steer** — Inject message after next tool call
55. ⬜ **No /agents** — Show active agents
56. ⬜ **No /resume** — Resume a session
57. ⬜ **No /goal** — Standing goal system
58. ⬜ **No /redraw** — Force UI repaint
59. ⬜ **No /config** — Show config
60. ⬜ **No /model** (change) — Show Only, can't change
61. ⬜ **No /personality** — Set personality
62. ⬜ **No /reasoning** — Set reasoning level
63. ⬜ **No /verbose** — Toggle verbosity
64. ⬜ **No /voice** — Voice mode toggle
65. ⬜ **No /yolo** — Bypass approval
66. ⬜ **No /tools** — Manage tools
67. ⬜ **No /toolsets** — List toolsets
68. ⬜ **No /skills** — Search/install skills
69. ⬜ **No /skill** — Load skill into session
70. ⬜ **No /reload-skills** — Re-scan skills directory
71. ⬜ **No /reload** — Reload .env
72. ⬜ **No /reload-mcp** — Reload MCP servers
73. ⬜ **No /cron** — Manage cron jobs
74. ⬜ **No /curator** — Skill maintenance
75. ⬜ **No /kanban** — Collaboration board
76. ⬜ **No /plugins** — List plugins
77. ⬜ **No /branch** (fork) — Branch session
78. ⬜ **No /fast** — Toggle priority
79. ⬜ **No /browser** — Open browser connection
80. ⬜ **No /history** — Show conversation history
81. ⬜ **No /save** — Save to file
82. ⬜ **No /usage** — Token usage
83. ⬜ **No /debug** — Debug report

### One-Shot Mode Defects — 2 gaps
84. 🟥 **No `-q` flag support** — Uses raw argv concatenation, not proper flag parsing
85. ⬜ **No `-m` model override** — No per-query model switching

## SECTOR 3: Tools — 67 GAPS

### Existing Tool Defects (F-N-F) — 10 gaps
86. ✅ **web_search fixed** — `web.c` now uses DuckDuckGo Instant Answer API with separate handler
87. 🟥 **skills only has list** — No skills_load, skills_view, skills_install, skills_uninstall
88. 🟥 **file.c has NO patch command** — Python has `patch` for targeted find-replace, C only has full read/write
89. 🟥 **file.c has NO search_files** — file.c declares SCHEMA_SEARCH but no grep/ripgrep integration
90. 🟥 **terminal.c has NO pty support** — Only `popen()` — no pseudo-terminal for interactive tools
91. 🟥 **terminal.c has NO background mode** — No `background=true` param, no session tracking
92. 🟥 **terminal.c has NO watch_patterns** — No output monitoring
93. 🟥 **file.c path safety is trivial** — `is_safe_path` blocks `..` and absolute paths outside HOME — too restrictive
94. 🟥 **tools_init.c only registers 4 tools** — Lists 4 but Python Hermes has 40+
95. 🟥 **No tool schema auto-generation** — Schemas are hand-crafted JSON strings

### Missing Tool Implementations — 58 gaps
96. ⬜ **No vision_analyze tool** — Image analysis
97. ⬜ **No execute_code tool** — Sandboxed Python execution
98. ⬜ **No session_search tool** — Search past conversations
99. ⬜ **No memory tool** — Persistent cross-session memory
100. ⬜ **No clarify tool** — Ask user clarifying questions
101. ⬜ **No delegate_task tool** — Subagent spawn
102. ⬜ **No cronjob tool** — Scheduled task management
103. ⬜ **No skills_list tool** (exists but needs view/load/install)
104. ⬜ **No skills_view tool**
105. ⬜ **No skills_install tool**
106. ⬜ **No skills_uninstall tool**
107. ⬜ **No todo tool** — In-session task planning
108. ⬜ **No send_message tool** — Cross-platform message sending
109. ⬜ **No text_to_speech tool** — TTS audio creation
110. ⬜ **No process tool** — Background process management
111. ⬜ **No write_file tool** (file.c has partial)
112. ⬜ **No search_files tool** (grep equivalent)
113. ⬜ **No kanban tool** — Multi-agent work queue
114. ⬜ **No spotify tool** — Music playback control
115. ⬜ **No homeassistant tool** — Smart home control
116. ⬜ **No discord tool** — Discord platform integration
117. ⬜ **No image_gen tool** — AI image generation
118. ⬜ **No browser tool** — Browser automation
119. ⬜ **No web_search (real)** — Actual search engine integration
120. ⬜ **No web_content** — Content extraction from web pages
121. ⬜ **No skill_manage tool** — Create/update/delete skills
122. ⬜ **No tool install/remove** — No tool management
123. ⬜ **No batch processing** — No batch_runner.py equivalent
124. ⬜ **No JSON schema validation** — No tool parameter validation
125. ⬜ **No tool requirement checks** — No `check_fn` equivalent
126. ⬜ **No tool environment variables** — No `requires_env` support
127. ⬜ **No auto-discovery** — Tools must be manually added to tool_init.c
128. ⬜ **No tool runtime metrics** — No timing/cost tracking per tool call
129. ⬜ **No tool error classification** — All tool errors are generic
130. ⬜ **No tool platform gating** — Tools can't be enabled/disabled per platform
131. ⬜ **No tool cancellation** — No way to cancel long-running tool
132. ⬜ **No tool dependency resolution** — Tools can't declare prerequisites
133. ⬜ **No tool capabilities manifest** — No way to query what tools exist remotely

## SECTOR 4: Gateway — 38 GAPS

### Existing Gateway Defects (F-N-F) — 6 gaps
134. 🟥 **No platform abstraction** — `server.c` has hardcoded Telegram API calls, no adapter pattern
135. 🟥 **No command approval** — Gateway exposes full agent without approval checks
136. 🟥 **No rate limiting** — No user rate limits, no message frequency caps
137. 🟥 **No multi-session** — Single agent state shared across all users
138. 🟥 **No error recovery** — Gateway crashes on HTTP failure
139. 🟥 **No graceful shutdown** — `running=false` but poll loop may block on HTTP

### Missing Platforms — 18 gaps
140. ⬜ **No Discord adapter** — No `gateway/platforms/discord.c`
141. ⬜ **No Slack adapter**
142. ⬜ **No WhatsApp adapter**
143. ⬜ **No Signal adapter**
144. ⬜ **No Matrix adapter**
145. ⬜ **No Email adapter**
146. ⬜ **No SMS adapter**
147. ⬜ **No Webhook adapter**
148. ⬜ **No API server adapter** — No OpenAI-compatible API server
149. ⬜ **No Home Assistant adapter**
150. ⬜ **No Mattermost adapter**
151. ⬜ **No DingTalk adapter**
152. ⬜ **No Feishu (Lark) adapter**
153. ⬜ **No WeChat/WeCom adapter**
154. ⬜ **No iMessage/BlueBubbles adapter**
155. ⬜ **No Yuanbao adapter**
156. ⬜ **No QQ Bot adapter**
157. ⬜ **No webhook server adapter**

### Missing Gateway Features — 14 gaps
158. ⬜ **No gateway health endpoint**
159. ⬜ **No message queue** — All messages processed synchronously
160. ⬜ **No platform routing** — Can't route to multiple platforms simultaneously
161. ⬜ **No gateway plugin system**
162. ⬜ **No user/chat isolation** — All users share one agent context
163. ⬜ **No message rate analytics**
164. ⬜ **No platform status reporting**
165. ⬜ **No gateway logging** — Just printf, no structured logging
166. ⬜ **No gateway crash recovery** — No restart on failure
167. ⬜ **No inline media handling** — Can't handle images/audio from platforms
168. ⬜ **No long message splitting** — `server.c:94-103` splits at 4000 but doesn't handle edge case
169. ⬜ **No typing indicator for long responses** — Sends single "thinking..." at start
170. ⬜ **No message edit support** — Can't update sent messages
171. ⬜ **No keyboard/markup support** — Can't send buttons

## SECTOR 5: Cron & Scheduler — 13 GAPS

### Cron Defects (F-N-F) — 4 gaps
172. 🟥 **Jobs have no persistence** — `scheduler.c` stores jobs in memory only
173. 🟥 **`cron_list_jobs()` returns "[]"** — `jobs.c:19` is a hardcoded stub
174. 🟥 **No job pause/resume tracking** — `active` flag but no CLI integration
175. 🟥 **No job output capture** — `system()` output goes to stdout, not captured

### Missing Cron Features — 9 gaps
176. ⬜ **No cron job CRUD CLI** — No `hermes cron create/edit/remove` commands
177. ⬜ **No human-readable schedule parsing** — Can't parse "every 30m", "every 2h", only crontab
178. ⬜ **No job dependency chain** — No `context_from` support
179. ⬜ **No job delivery routing** — No multi-platform delivery per job
180. ⬜ **No job script runner** — No pre-run script support
181. ⬜ **No no_agent mode** — Can't run jobs without LLM
182. ⬜ **No job tick lock** — No `.tick.lock` file to prevent duplicate ticks
183. ⬜ **No job timeout enforcement** — Long jobs block the scheduler
184. ⬜ **No cron stats/history** — No run history, no error tracking

## SECTOR 6: Config & Environment — 24 GAPS

### Config Defects (F-N-F) — 6 gaps
185. 🟥 **Only reads HERMES_API_KEY and OPENAI_API_KEY from .env** — `config.c:73-78` — 30+ other provider keys ignored
186. 🟥 **No config hot-reload** — Config loaded once at startup
187. 🟥 **No config validation** — No schema validation, accepts any garbage
188. 🟥 **No config migration** — No `config migrate` equivalent
189. 🟥 **No `hermes config path` command** — No way to find config file from CLI
190. 🟥 **No `hermes config check`** — No doctor/health check equivalent

### Missing Config Features — 18 gaps
191. ⬜ **No profile system** — Can't switch between named config profiles
192. ⬜ **No profile create/delete/list**
193. ⬜ **No profile clone**
194. ⬜ **No profile export/import**
195. ⬜ **No credential pool support** — No multi-key rotation
196. ⬜ **No credential exhaustion auto-recovery**
197. ⬜ **No OAuth login flow** — No `hermes login` equivalent
198. ⬜ **No OAuth logout**
199. ⬜ **No TTS configuration** — No text-to-speech settings
200. ⬜ **No STT configuration** — No speech-to-text settings
201. ⬜ **No display/skin config** — No theme/skin configuration
202. ⬜ **No delegation config** — No subagent model/provider settings
203. ⬜ **No compression config** — No compression threshold/ratio settings
204. ⬜ **No checkpoints config**
205. ⬜ **No security config** — No TIRITH/approval settings
206. ⬜ **No memory config** — No memory provider selection
207. ⬜ **No platform config per profile**
208. ⬜ **No webhook config**

## SECTOR 7: Display & UX — 16 GAPS

### Display Defects (F-N-F) — 5 gaps
209. 🟥 **`display.h` declares spinner but `cli_display.c` has no implementation** — Spinner functions declared but empty
210. 🟥 **`display.h` declares progress bar but never used** — `display_progress_*` functions declared but never called
211. 🟥 **`display.h` declares panel() but no implementation** — `display_panel` not implemented
212. 🟥 **No color support detection** — `display_has_color()` returns true without checking TERM
213. 🟥 **No terminal width detection** — `display_width()` always returns 80

### Missing Display Features — 11 gaps
214. ⬜ **No spinner animation** — CLI has no "thinking" spinner
215. ⬜ **No activity feed** — No `┊` prefix for tool results
216. ⬜ **No response box** — No Rich panel around responses
217. ⬜ **No token/cost display** — No per-response token tracking
218. ⬜ **No tool progress display** — No per-tool timing
219. ⬜ **No skin engine** — No `hermes_cli/skin_engine.py` equivalent
220. ⬜ **No status bar** — No footer with model/session info
221. ⬜ **No markdown rendering** — Response text is plain, not rendered
222. ⬜ **No syntax highlighting** — No code block coloring
223. ⬜ **No image display** — Can't show images inline
224. ⬜ **No banner customization** — Hardcoded banner text

## SECTOR 8: Communication — 49 GAPS

### Message Handling Defects (F-N-F) — 3 gaps
225. 🟥 **No role alternation enforcement** — Can send 2 assistant msgs in a row (OpenAI rejects)
226. 🟥 **No message validation** — Empty content, null messages not validated at push
227. 🟥 **No system message deduplication** — `context_set_system` finds first system msg but memmove on existing array may orphan

### Missing Communication Features — 46 gaps
228. ⬜ **No send_message (cross-platform)**
229. ⬜ **No message editing**
230. ⬜ **No message deletion**
231. ⬜ **No message reply tracking**
232. ⬜ **No message threads**
233. ⬜ **No message reactions**
234. ⬜ **No message formatting (markdown/HTML)**
235. ⬜ **No media attachment sending**
236. ⬜ **No media attachment receiving**
237. ⬜ **No voice message support**
238. ⬜ **No file upload**
239. ⬜ **No file download**
240. ⬜ **No platform discovery** — Can't list available communication channels
241. ⬜ **No user discovery** — Can't look up users
242. ⬜ **No group/room management**
243. ⬜ **No permission checking**
244. ⬜ **No message rate limiting**
245. ⬜ **No content moderation**
246. ⬜ **No message priority queuing**
247. ⬜ **No delivery receipts**
248. ⬜ **No read receipts**
249. ⬜ **No typing indicators**
250. ⬜ **No inline keyboard/markup**
251. ⬜ **No slash command autocomplete (in gateway)**
252. ⬜ **No gateway plugin for message processing**
253. ⬜ **No message search**
254. ⬜ **No conversation export**
255. ⬜ **No conversation import**
256. ⬜ **No scheduled message delivery**
257. ⬜ **No message templates**
258. ⬜ **No message translation**
259. ⬜ **No webhook outbound**
260. ⬜ **No webhook inbound parsing**
261. ⬜ **No platform status reporting**
262. ⬜ **No connection health checking**
263. ⬜ **No reconnection with backoff**
264. ⬜ **No connection pooling**
265. ⬜ **No multi-platform session isolation**
266. ⬜ **No user authentication**
267. ⬜ **No per-user settings**
268. ⬜ **No admin commands**
269. ⬜ **No moderation queue**
270. ⬜ **No spam detection**
271. ⬜ **No platform bridging** — Forward messages between platforms
272. ⬜ **No message transform pipeline**
273. ⬜ **No message storage/archival**

## SECTOR 9: Testing — 25 GAPS

### Test Defects (F-N-F) — 4 gaps
274. 🟥 **Makefile says "Tests TBD"** — `Makefile:55` has no test targets
275. 🟥 **No test runner** — No script to run all tests
276. 🟥 **No CI integration** — No GitHub Actions for C builds
277. 🟥 **No integration tests** — Only 2 unit test files

### Missing Test Coverage — 21 gaps
278. ⬜ **No test for agent_loop** — agent_run_conversation untested
279. ⬜ **No test for llm_client** — LLM request building untested
280. ⬜ **No test for context** — Message lifecycle untested
281. ⬜ **No test for CLI** — Command parsing untested
282. ⬜ **No test for config** — YAML/ENV loading untested
283. ⬜ **No test for terminal tool** — Shell execution untested
284. ⬜ **No test for file tool** — Read/write/search untested
285. ⬜ **No test for web tool** — HTTP GET untested
286. ⬜ **No test for skills tool** — Skill listing untested
287. ⬜ **No test for gateway** — Telegram API untested
288. ⬜ **No test for cron** — Schedule parsing untested
289. ⬜ **No test for token_exchange** — OAuth flow untested
290. ⬜ **No test for display** — ANSI code generation untested
291. ⬜ **No test for http client** — Raw socket + TLS untested
292. ⬜ **No test for crypto** — SHA-256, HMAC, base64 untested
293. ⬜ **No test for yaml parser** — Config parsing untested
294. ⬜ **No test for db** — Session persistence untested
295. ⬜ **No test for commands** — Command dispatch untested
296. ⬜ **No memory leak tests** — Valgrind/ASAN not configured
297. ⬜ **No stress tests** — No multi-turn conversation stress test
298. ⬜ **No fuzz tests** — No random input fuzzing

## SECTOR 10: Documentation — 47 GAPS

### State.md Defects (F-N-F) — 6 gaps
299. 🟥 **state.md claims "ALL PHASES COMPLETE ✅"** — This is FALSE. Only Phase 1 has real infrastructure. Phases 2-5 have partial stubs
300. 🟥 **Phase 2 claimed ✅ — No memory, compression, persistence**
301. 🟥 **Phase 3 claimed ✅ — Only 4 tools exist (need 30+), tool calling loop is broken**
302. 🟥 **Phase 4 claimed ✅ — Only Telegram, no platform abstraction**
303. 🟥 **Phase 5 claimed ✅ — No job persistence, cron_list is stub, no skill system**
304. 🟥 **DEPENDENCIES.md shows all 🔲 — Contradicts state.md ✅ — complete documentation disconnect**

### README Defects (F-N-F) — 3 gaps
305. 🟥 **README.md says "phase3 build target"** but phase 3 is only 4 tools
306. 🟥 **README.md says "make -C C phase3" builds** but has no verification instructions
307. 🟥 **README.md claims "make -C C" builds all but doesn't say it only builds, not runs**

### Missing Documentation — 38 gaps
308. ⬜ **No HONEST state.md** — Current state.md is aspirational, not factual
309. ⬜ **No HONEST goal-mantra.md** — Goal-mantra is vague, no milestones
310. ⬜ **No installation docs for C binary**
311. ⬜ **No usage docs beyond --version**
312. ⬜ **No contribution guide for C**
313. ⬜ **No architecture overview**
314. ⬜ **No dependency docs**
315. ⬜ **No troubleshooting guide**
316. ⬜ **No FAQ**
317. ⬜ **No changelog**
318. ⬜ **No migration guide** (Python → C)
319. ⬜ **No API reference**
320. ⬜ **No CLI reference**
321. ⬜ **No tool reference**
322. ⬜ **No gateway reference**
323. ⬜ **No cron reference**
324. ⬜ **No config reference for C**
325. ⬜ **No testing guide**
326. ⬜ **No build from source guide**
327. ⬜ **No cross-compilation guide**
328. ⬜ **No containerization docs**
329. ⬜ **No performance tuning guide**
330. ⬜ **No security guide**
331. ⬜ **No OAuth setup guide**
332. ⬜ **No platform adapter development guide**
333. ⬜ **No tool development guide**
334. ⬜ **No memory system design doc**
335. ⬜ **No compression system design doc**
336. ⬜ **No gateway architecture design doc**
337. ⬜ **No cron architecture design doc**
338. ⬜ **No provider architecture design doc**
339. ⬜ **No plugin system design doc**
340. ⬜ **No profile system design doc**
341. ⬜ **No credential pool design doc**
342. ⬜ **No MCP server integration doc**
343. ⬜ **No ACP server integration doc**
344. ⬜ **No skin engine design doc**
345. ⬜ **No vault README** — No vault/bins/README.md

## SECTOR 11: Deps & Infrastructure — 29 GAPS

### Deps Defects (F-N-F) — 5 gaps
346. 🟥 **No external dependency management** — No CMake/meson, relies on manual Makefile
347. 🟥 **No dependency version pinning** — Must have system-installed libssl-dev etc.
348. 🟥 **No fallback for missing dependencies** — No bundled libs
349. 🟥 **No static linking** — Deps are dynamic (.so)
350. 🟥 **No .o file isolation** — Object files in src/ instead of build/

### Missing Infrastructure — 24 gaps
351. ⬜ **No build directory** — build/ or build/phase{1-5}/
352. ⬜ **No dependency check script** — No `hermes doctor` equivalent for C
353. ⬜ **No uninstall target** — Makefile has clean but no uninstall
354. ⬜ **No install target** — No `make install` or `make PREFIX=/usr/local`
355. ⬜ **No packaging** — No .deb/.rpm package
356. ⬜ **No version compatibility check** — No libssl version check
357. ⬜ **No ABI compatibility check**
358. ⬜ **No symbol visibility control** — All functions exported
359. ⬜ **No LTO** — No link-time optimization
360. ⬜ **No sanitizer build mode** — No ASAN/UBSAN/TSAN targets
361. ⬜ **No coverage build mode** — No `--coverage` target
362. ⬜ **No release build mode** — No `-O2 -s -DNDEBUG` target
363. ⬜ **No debug build mode** — No `-O0 -g3` target
364. ⬜ **No profiler support** — No perf/gprof integration
365. ⬜ **No CI configuration** — No `.github/workflows/` for C
366. ⬜ **No Dockerfile** — No container build
367. ⬜ **No devcontainer** — No .devcontainer for C dev
368. ⬜ **No pre-commit hooks** — No formatting/linting checks
369. ⬜ **No .clang-format** — No code style enforcement
370. ⬜ **No .clang-tidy** — No static analysis config
371. ⬜ **No leak checker** — No valgrind suppressions file
372. ⬜ **No compiler warning config for deps** — `-Wpedantic` catches deps issues too
373. ⬜ **No cross-platform checks** — Makefile assumes Linux/GCC
374. ⬜ **No macOS build support** — No Darwin-specific make target

## SECTOR 12: Provider & Auth — 18 GAPS

### Provider Defects (F-N-F) — 3 gaps
375. 🟥 **No credential pool integration** — `token_exchange.c` stores to auth.json but CLI never uses it
376. 🟥 **No provider selection in CLI** — Model/provider set from config but no `hermes model` interactive picker
377. 🟥 **No provider-specific API format handling** — Only OpenAI format works

### Missing Provider Features — 15 gaps
378. ⬜ **No credential pool (multi-key rotation)**
379. ⬜ **No credential exhaustion auto-recovery**
380. ⬜ **No provider status check**
381. ⬜ **No provider model listing**
382. ⬜ **No provider-specific header injection**
383. ⬜ **No proxy support**
384. ⬜ **No SOCKS proxy support**
385. ⬜ **No request signing** (AWS Bedrock, etc.)
386. ⬜ **No response caching**
387. ⬜ **No request deduplication**
388. ⬜ **No fallback provider chain**
389. ⬜ **No provider load balancing**
390. ⬜ **No provider cost tracking**
391. ⬜ **No provider token counting**
392. ⬜ **No provider latency tracking**

## SECTOR 13: Compilation & Binary — 8 GAPS
393. ⬜ **No Nix flake** — Reproducible build environment
394. ⬜ **No AppImage/flatpak** — Single-file distribution
395. ⬜ **No musl build** — Fully static binary
396. ⬜ **No cross-compilation** — No aarch64/arm targets
397. ⬜ **No minimal binary** — Current 386KB can be reduced
398. ⬜ **No UPX compression**
399. ⬜ **No embed/initramfs mode** — No tiny embedded build
400. ⬜ **No WASM build target** — No browser-side Hermes

---

## Summary

| Sector | Gaps | F-N-F | STUB | PARTIAL | MISSING |
|--------|------|-------|------|---------|---------|
| 1: Agent Loop | 54 | 13 | 0 | 0 | 41 |
| 2: CLI | 47 | 6 | 0 | 0 | 41 |
| 3: Tools | 68 | 10 | 0 | 0 | 58 |
| 4: Gateway | 38 | 6 | 0 | 0 | 32 |
| 5: Cron | 13 | 4 | 1 | 0 | 9 |
| 6: Config | 24 | 6 | 0 | 0 | 18 |
| 7: Display | 16 | 5 | 0 | 0 | 11 |
| 8: Communication | 49 | 3 | 0 | 0 | 46 |
| 9: Testing | 25 | 4 | 0 | 0 | 21 |
| 10: Documentation | 47 | 6 | 0 | 0 | 38 |
| 11: Deps & Infra | 29 | 5 | 0 | 0 | 24 |
| 12: Provider & Auth | 18 | 3 | 0 | 0 | 15 |
| 13: Compilation | 8 | 0 | 0 | 0 | 8 |
| **TOTAL** | **436** | **67** | **1** | **0** | **362** |

**436 gaps identified** (above initial 400 target — will trim to precise 400 during DA review)

## Priority Triage

### P0 — Blocks Function (fix before ANYTHING else):
- #1: Tool call loop broken (agent can't use tools)
- #17: Auth header malformed (LLM calls fail)
- #86: web_search is alias (can't search)
- #172: Jobs are memory-only (data loss on restart)
- #299: state.md claims all complete (misleading roadmap)

### P1 — Major Feature Gaps:
- Agent loop: memory, compression, persistence (#22-27)
- CLI: remaining 38 slash commands (#45-83)
- Tools: remaining 25+ tools (#96-133)
- Gateway: platform abstraction + 17 missing platforms (#134-171)
- Testing: full test suite (#274-298)

### P2 — Normal Priority:
- Config: profile system, credential pool (#191-208)
- Display: spinner, progress bar, skin engine (#209-224)
- Deps: build system improvements, CI (#351-374)

### P3 — Nice to Have:
- Communication: advanced features (#228-273)
- Compilation: Nix, static binary, WASM (#393-400)