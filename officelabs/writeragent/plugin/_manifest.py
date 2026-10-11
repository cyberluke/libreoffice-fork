"""Auto-generated module manifest. DO NOT EDIT."""

VERSION = '0.8.82'

MODULES = [
    {
        "name": "main",
        "title": "WriterAgent global settings",
        "requires": [],
        "provides_services": [],
        "config": {},
        "config_inline": None,
        "actions": [],
        "action_icons": {}
},
    {
        "name": "core",
        "title": "Core services (config, events, logging)",
        "requires": [],
        "provides_services": [
                "document",
                "config",
                "events",
                "format"
        ],
        "config": {
                "log_level": {
                        "type": "string",
                        "default": "WARN",
                        "widget": "select",
                        "label": "Log Level",
                        "internal": True,
                        "options": [
                                {
                                        "value": "DEBUG",
                                        "label": "Debug"
                                },
                                {
                                        "value": "INFO",
                                        "label": "Info"
                                },
                                {
                                        "value": "WARN",
                                        "label": "Warning"
                                },
                                {
                                        "value": "ERROR",
                                        "label": "Error"
                                }
                        ]
                }
        },
        "config_inline": None,
        "actions": [],
        "action_icons": {}
},
    {
        "name": "agent_backend",
        "title": "Agent Communication Protocol",
        "requires": [
                "config",
                "document"
        ],
        "provides_services": [],
        "config": {
                "backend_id": {
                        "type": "string",
                        "default": "builtin",
                        "widget": "select",
                        "label": "Backend",
                        "options": [
                                {
                                        "value": "builtin",
                                        "label": "Built-in"
                                },
                                {
                                        "value": "hermes",
                                        "label": "Hermes"
                                },
                                {
                                        "value": "claude",
                                        "label": "Claude Code (ACP)"
                                },
                                {
                                        "value": "vibe",
                                        "label": "Mistral Vibe (ACP)"
                                },
                                {
                                        "value": "grok",
                                        "label": "Grok Build (ACP)"
                                },
                                {
                                        "value": "opencode",
                                        "label": "OpenCode (ACP)"
                                }
                        ]
                },
                "path": {
                        "type": "string",
                        "default": "",
                        "widget": "text",
                        "label": "Path / URL",
                        "helper": "Path to backend CLI (e.g. aider) or ACP server URL (e.g. http://localhost:8000 for Hermes). Empty = try default.",
                        "internal": True
                },
                "args": {
                        "type": "string",
                        "default": "",
                        "widget": "text",
                        "label": "Extra arguments",
                        "helper": "Optional arguments for the selected backend (space-separated).",
                        "internal": True
                },
                "acp_agent_name": {
                        "type": "string",
                        "default": "",
                        "widget": "text",
                        "label": "ACP agent name",
                        "helper": "Agent name on the ACP server (e.g. hermes). Empty = auto-discover first agent.",
                        "internal": True
                },
                "prompt_for_permission": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "label": "Prompt for Agent Permissions",
                        "helper": "Ask for confirmation before allowing the agent to use tools (read files, execute commands, etc.)"
                }
        },
        "config_inline": "mcp",
        "actions": [],
        "action_icons": {}
},
    {
        "name": "audio",
        "title": "Speech",
        "requires": [
                "config"
        ],
        "provides_services": [],
        "config": {
                "stt_provider": {
                        "type": "string",
                        "default": "endpoint",
                        "widget": "select",
                        "label": "STT Provider",
                        "helper": "LLM Endpoint uses Audio Model and POST /v1/audio/transcriptions. Local Whisper runs faster-whisper in the Settings â†’ Python venv (not LibreOffice's Python).",
                        "options": [
                                {
                                        "value": "endpoint",
                                        "label": "LLM Endpoint"
                                },
                                {
                                        "value": "local",
                                        "label": "Local Whisper (faster-whisper)"
                                }
                        ]
                },
                "stt_model": {
                        "type": "string",
                        "default": "",
                        "widget": "combo",
                        "label": "Audio Model:",
                        "helper": "Speech-to-text model when STT Provider is LLM Endpoint and the chat model cannot take audio input."
                },
                "stt_local_model": {
                        "type": "string",
                        "default": "base",
                        "widget": "select",
                        "label": "Local Model",
                        "helper": "faster-whisper size. The first transcription downloads weights into the Hugging Face cache (base, the default, is about 150 MB; tiny ~75 MB, small ~500 MB, medium ~1.5 GB).",
                        "options": [
                                {
                                        "value": "tiny",
                                        "label": "tiny (~75 MB)"
                                },
                                {
                                        "value": "base",
                                        "label": "base (~150 MB)"
                                },
                                {
                                        "value": "small",
                                        "label": "small (~500 MB)"
                                },
                                {
                                        "value": "medium",
                                        "label": "medium (~1.5 GB)"
                                }
                        ]
                },
                "tts_enabled": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Enable Speech Output (TTS)",
                        "width": 220,
                        "helper": "Speak assistant responses aloud using text-to-speech."
                },
                "tts_sentence_mode": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "label": "Speak sentence by sentence (Local)",
                        "width": 168,
                        "helper": "Synthesize the next sentences while the current one plays, so a short sentence is not followed by a gap. Stop discards clips that are not playing yet. Turn off to speak the whole reply as one clip."
                },
                "tts_short_answers": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "label": "Keep replies brief",
                        "helper": "Aim for about one paragraph unless the user asks for more. Only applies while speech output (TTS) is on.",
                        "tooltip": True,
                        "width": 160
                },
                "tts_provider": {
                        "type": "string",
                        "default": "system",
                        "widget": "select",
                        "label": "TTS Provider",
                        "helper": "Choose whether to use built-in OS speech, a local neural engine, or your API endpoint.",
                        "options": [
                                {
                                        "value": "system",
                                        "label": "OS Native (say / SAPI / spd-say)"
                                },
                                {
                                        "value": "kokoro",
                                        "label": "Kokoro (Local Neural, ONNX CPU)"
                                },
                                {
                                        "value": "piper",
                                        "label": "Piper (Local Fast Neural, CPU)"
                                },
                                {
                                        "value": "endpoint",
                                        "label": "LLM Endpoint"
                                }
                        ]
                },
                "tts_model": {
                        "type": "string",
                        "default": "",
                        "widget": "combo",
                        "label": "TTS Model",
                        "helper": "Model for speech synthesis (used when TTS Provider is LLM Endpoint)."
                },
                "tts_voice": {
                        "type": "string",
                        "default": "alloy",
                        "widget": "select",
                        "label": "Voice",
                        "helper": "Voice for the selected provider. The list matches the Piper or Kokoro catalog for your locale.",
                        "inline": True,
                        "options_provider": "plugin.audio.tts_voices:settings_voice_options",
                        "options": [
                                {
                                        "value": "alloy",
                                        "label": "alloy (OpenAI Neutral)"
                                },
                                {
                                        "value": "af_sky",
                                        "label": "US Female - Sky"
                                },
                                {
                                        "value": "en_US-lessac-medium",
                                        "label": "US English Female - Lessac"
                                },
                                {
                                        "value": "default",
                                        "label": "default (System Default)"
                                }
                        ]
                },
                "test_voice": {
                        "type": "string",
                        "default": "",
                        "widget": "button",
                        "label": "Test voice",
                        "inline_no_label": True,
                        "settings_persist": False,
                        "x": 260,
                        "width": 88
                },
                "tts_speed": {
                        "type": "string",
                        "default": "1.0x",
                        "widget": "combo",
                        "label": "Speech Speed",
                        "helper": "Playback speed multiplier (1.0x, 1.1x, 1.25x, 1.5x, 1.75x, 2.0x, or custom down to 0.25x).",
                        "options": [
                                {
                                        "value": "1.0x",
                                        "label": "1.0x"
                                },
                                {
                                        "value": "1.1x",
                                        "label": "1.1x"
                                },
                                {
                                        "value": "1.25x",
                                        "label": "1.25x"
                                },
                                {
                                        "value": "1.5x",
                                        "label": "1.5x"
                                },
                                {
                                        "value": "1.75x",
                                        "label": "1.75x"
                                },
                                {
                                        "value": "2.0x",
                                        "label": "2.0x"
                                }
                        ]
                },
                "tts_voice_kokoro": {
                        "type": "string",
                        "default": "af_sky",
                        "internal": True
                },
                "tts_voice_piper": {
                        "type": "string",
                        "default": "en_US-lessac-medium",
                        "internal": True
                },
                "tts_voice_openai": {
                        "type": "string",
                        "default": "alloy",
                        "internal": True
                },
                "tts_voice_openrouter": {
                        "type": "string",
                        "default": "",
                        "internal": True
                },
                "tts_voice_together": {
                        "type": "string",
                        "default": "",
                        "internal": True
                },
                "tts_voice_system": {
                        "type": "string",
                        "default": "default",
                        "internal": True
                }
        },
        "config_inline": None,
        "actions": [],
        "action_icons": {}
},
    {
        "name": "calc",
        "title": "Calc spreadsheet tools",
        "requires": [
                "document",
                "config"
        ],
        "provides_services": [],
        "config": {
                "max_rows_display": {
                        "type": "int",
                        "default": 1000,
                        "min": 100,
                        "max": 100000,
                        "widget": "number",
                        "label": "Max Rows Display",
                        "public": True
                },
                "ods_cache_enabled": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "label": "Cache Excel siblings as ODS",
                        "width": 220,
                        "helper": "Convert sibling .xlsx/.xls to writeragent_ods_cache/ beside the folder so repeated DuckDB SQL reuses the ODS. Native .ods and the live workbook are never cached. Set calc.ods_cache_enabled false in writeragent.json to disable.",
                        "public": True
                }
        },
        "config_inline": "doc",
        "actions": [],
        "action_icons": {}
},
    {
        "name": "mcp",
        "title": "MCP",
        "requires": [
                "config",
                "events"
        ],
        "provides_services": [
                "http_routes"
        ],
        "config": {
                "mcp_enabled": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Enable MCP Server",
                        "width": 168,
                        "helper": "Localhost only, no auth. Clients use http://localhost:<port>/mcp (streamable HTTP).",
                        "public": True
                },
                "mcp_port": {
                        "type": "int",
                        "default": 18765,
                        "min": 1024,
                        "max": 65535,
                        "widget": "number",
                        "label": "MCP Port",
                        "public": True,
                        "inline": True,
                        "x": 110,
                        "width": 70
                },
                "tool_exposure_mode": {
                        "type": "string",
                        "default": "delegate",
                        "widget": "select",
                        "label": "Tool Exposure",
                        "helper": "How specialized tools are surfaced to MCP clients. delegate=reached via the delegate gateway (today's behavior); direct_flat=all MCP-reachable specialized tools listed directly, excluding sidebar-only flows (best for clients with their own tool-search); direct_discovery=small core list plus find_tools (domain catalog, then per-domain tool schemas; best for any client).",
                        "public": True,
                        "label_x": 195,
                        "label_width": 85,
                        "x": 285,
                        "width": 145,
                        "options": [
                                {
                                        "value": "delegate",
                                        "label": "Delegate (default)"
                                },
                                {
                                        "value": "direct_flat",
                                        "label": "Direct â€” list all tools"
                                },
                                {
                                        "value": "direct_discovery",
                                        "label": "Direct â€” discovery (find_tools)"
                                }
                        ]
                },
                "_sep_tunnel": {
                        "widget": "separator",
                        "label": "Public tunnel"
                },
                "tunnel_enabled": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Expose via public tunnel",
                        "helper": "Requires the selected provider binary on PATH (cloudflared, bore, ngrok, or tailscale). MCP has no auth â€” anyone with the public URL can call tools. See MCP Server Status for the public /mcp URL when ready.",
                        "public": True,
                        "inline": True,
                        "width": 170
                },
                "test_tunnel": {
                        "type": "string",
                        "default": "",
                        "widget": "button",
                        "button_text": "Test Tunnel",
                        "settings_persist": False,
                        "inline_no_label": True,
                        "x": 200,
                        "width": 80
                },
                "tunnel_provider": {
                        "type": "string",
                        "default": "cloudflare",
                        "widget": "select",
                        "label": "Provider",
                        "helper": "Cloudflare quick tunnel (default), Bore, Ngrok, or Tailscale Funnel (must already be logged in).",
                        "public": True,
                        "inline": True,
                        "x": 110,
                        "width": 100,
                        "options": [
                                {
                                        "value": "cloudflare",
                                        "label": "Cloudflare"
                                },
                                {
                                        "value": "bore",
                                        "label": "Bore"
                                },
                                {
                                        "value": "ngrok",
                                        "label": "Ngrok"
                                },
                                {
                                        "value": "tailscale",
                                        "label": "Tailscale"
                                }
                        ]
                },
                "tunnel_provider_token": {
                        "type": "string",
                        "default": "",
                        "widget": "password",
                        "label": "Provider config",
                        "helper": "Ngrok authtoken; Cloudflare tunnel token; Bore server or 'server secret'. Empty = provider default / CLI config. Unused for Tailscale.",
                        "public": True,
                        "label_x": 215,
                        "label_width": 85,
                        "x": 302,
                        "width": 128
                },
                "client_config_snippet": {
                        "type": "string",
                        "default": "",
                        "widget": "textarea",
                        "label": "Client config (copy into Claude / Hermes-Agent):",
                        "label_above": True,
                        "label_x": 8,
                        "label_width": 340,
                        "readonly": True,
                        "settings_persist": False,
                        "inline": True,
                        "x": 8,
                        "width": 340,
                        "height": 36
                },
                "copy_config": {
                        "type": "string",
                        "default": "",
                        "widget": "button",
                        "button_text": "Copy Config",
                        "settings_persist": False,
                        "inline_no_label": True,
                        "x": 355,
                        "width": 75,
                        "height": 16
                },
                "cors_allow_private_origins": {
                        "type": "boolean",
                        "default": True,
                        "internal": True
                },
                "cors_allowed_origins": {
                        "type": "list",
                        "default": [],
                        "internal": True
                }
        },
        "config_inline": None,
        "actions": [
                "toggle_server",
                "server_status"
        ],
        "action_icons": {
                "server_status": "stopped"
        }
},
    {
        "name": "chatbot",
        "title": "Sidebar",
        "requires": [
                "document",
                "config",
                "events",
                "http_routes"
        ],
        "provides_services": [],
        "config": {
                "max_tool_rounds": {
                        "type": "int",
                        "default": 15,
                        "min": 1,
                        "max": 200,
                        "widget": "number",
                        "label": "Max Tool Rounds"
                },
                "context_strategy": {
                        "type": "string",
                        "default": "auto",
                        "widget": "select",
                        "label": "Document Context Strategy",
                        "helper": "How much document content to include in LLM context",
                        "options": [
                                {
                                        "value": "auto",
                                        "label": "Auto (by document size)"
                                },
                                {
                                        "value": "full",
                                        "label": "Full document text"
                                },
                                {
                                        "value": "page",
                                        "label": "Pages around cursor"
                                },
                                {
                                        "value": "tree",
                                        "label": "Outline + excerpt"
                                },
                                {
                                        "value": "stats",
                                        "label": "Stats + outline only"
                                }
                        ]
                },
                "extend_selection_max_tokens": {
                        "type": "int",
                        "default": 1000,
                        "min": 10,
                        "max": 4096,
                        "internal": True,
                        "label": "Extend max tokens"
                },
                "edit_selection_max_new_tokens": {
                        "type": "int",
                        "default": 1000,
                        "min": 0,
                        "max": 4096,
                        "internal": True,
                        "label": "Edit extra tokens",
                        "helper": "Extra tokens beyond original text length. 0 = same length as original."
                },
                "web_research_cache_enabled": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Web Research Cache",
                        "helper": "Cache completed web research reports by normalized query (uses web cache database).",
                        "inline": True,
                        "x": 8,
                        "width": 200
                },
                "show_search_thinking": {
                        "type": "boolean",
                        "default": False,
                        "internal": True
                },
                "prompt_for_web_research": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Prompt for Web Research",
                        "helper": "Ask for confirmation before sending a web search query",
                        "x": 220,
                        "width": 200
                },
                "web_research_browser": {
                        "type": "string",
                        "default": "off",
                        "widget": "select",
                        "label": "Web Research Browser",
                        "helper": "Select the browser backend to use for webpage visits during research (Off uses static HTTP requests).",
                        "options": [
                                {
                                        "value": "off",
                                        "label": "Off (Static HTTP)"
                                },
                                {
                                        "value": "firefox",
                                        "label": "Firefox (CDP)"
                                },
                                {
                                        "value": "chromium",
                                        "label": "Chromium (CDP)"
                                },
                                {
                                        "value": "chrome",
                                        "label": "Chrome (CDP)"
                                }
                        ]
                },
                "web_cache_max_mb": {
                        "type": "int",
                        "default": 50,
                        "min": 0,
                        "max": 500,
                        "widget": "number",
                        "label": "Cache max (MB)",
                        "helper": "Max disk size for web search cache (0 to disable)",
                        "inline": True,
                        "x": 110,
                        "width": 100
                },
                "web_cache_validity_days": {
                        "type": "int",
                        "default": 30,
                        "min": 1,
                        "max": 30,
                        "widget": "number",
                        "label": "Cache validity (days)",
                        "helper": "How many days cache entries should be considered valid.",
                        "label_x": 220,
                        "label_width": 100,
                        "x": 322,
                        "width": 100
                },
                "web_research_cache_jaccard_percent": {
                        "type": "int",
                        "default": 60,
                        "min": 0,
                        "max": 100,
                        "widget": "number",
                        "label": "Research Cache Fuzzy Match (%)",
                        "helper": "Minimum similarity (0-100) for a fuzzy cache hit. JSON only (internal); default 60.",
                        "internal": True
                },
                "web_research_cache_embedding_percent": {
                        "type": "int",
                        "default": 75,
                        "min": 0,
                        "max": 100,
                        "widget": "number",
                        "label": "Research Cache Embedding Match (%)",
                        "helper": "Minimum cosine similarity (0-100) for an embedding cache hit. JSON only (internal); default 75.",
                        "internal": True
                },
                "web_research_cache_min_overlap": {
                        "type": "int",
                        "default": 8,
                        "min": 0,
                        "max": 50,
                        "widget": "number",
                        "label": "Research Cache Min Stem Overlap",
                        "helper": "Minimum shared stem count for a fuzzy cache hit (0 disables). JSON only (internal); default 8.",
                        "internal": True
                },
                "deep_research_breadth": {
                        "type": "int",
                        "default": 4,
                        "min": 1,
                        "max": 10,
                        "widget": "number",
                        "label": "Deep Research Breadth",
                        "helper": "Sub-queries per depth level when Deep Research sidebar mode is used (internal).",
                        "internal": True
                },
                "deep_research_depth": {
                        "type": "int",
                        "default": 2,
                        "min": 1,
                        "max": 4,
                        "widget": "number",
                        "label": "Deep Research Depth",
                        "helper": "Legacy alias for max_rounds when deep_research_max_rounds is 0 (internal).",
                        "internal": True
                },
                "deep_research_concurrency": {
                        "type": "int",
                        "default": 2,
                        "min": 1,
                        "max": 5,
                        "widget": "number",
                        "label": "Deep Research Concurrency",
                        "helper": "Parallel sub-query workers in Deep Research sidebar mode (internal).",
                        "internal": True
                },
                "deep_research_max_sub_queries": {
                        "type": "int",
                        "default": 14,
                        "min": 1,
                        "max": 40,
                        "widget": "number",
                        "label": "Deep Research Max Sub-Queries",
                        "helper": "Global cap on shallow sub-agent runs per deep research session (internal).",
                        "internal": True
                },
                "deep_research_max_rounds": {
                        "type": "int",
                        "default": 3,
                        "min": 1,
                        "max": 6,
                        "widget": "number",
                        "label": "Deep Research Max Rounds",
                        "helper": "Adaptive research rounds; deep_research_depth JSON override maps here if max_rounds unset (internal).",
                        "internal": True
                },
                "deep_research_quality_threshold": {
                        "type": "int",
                        "default": 7,
                        "min": 1,
                        "max": 10,
                        "widget": "number",
                        "label": "Deep Research Quality Threshold",
                        "helper": "Stop when LLM coverage score reaches this value (1-10, internal).",
                        "internal": True
                },
                "deep_research_sub_agent_steps": {
                        "type": "int",
                        "default": 0,
                        "min": 0,
                        "max": 50,
                        "widget": "number",
                        "label": "Deep Research Sub-Agent Steps",
                        "helper": "Max ReAct steps per sub-query (0 = 150% of chatbot.max_tool_rounds, internal).",
                        "internal": True
                },
                "rich_text_control_sidebar": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "label": "Rich Text Control Sidebar",
                        "width": 200,
                        "helper": "Formatted chat via RichTextControl. Requires restart."
                },
                "librarian_invoked": {
                        "type": "boolean",
                        "default": False,
                        "internal": True
                },
                "humanizer_enabled": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Humanizer skill (natural prose)",
                        "width": 240,
                        "helper": "When enabled, injects guidance into the system prompt so the model makes generated or revised document text sound more natural and human (removes AI slop patterns). Edit the rules in your LibreOffice profile under writeragent/skills/humanizer/SKILL.md (the file is auto-created on first use). A dedicated Skills tab can be added later."
                },
                "audio_silence_stop_ms": {
                        "type": "int",
                        "default": 3000,
                        "min": 0,
                        "max": 15000,
                        "widget": "number",
                        "label": "Silence before send (ms)",
                        "helper": "Pause after you stop talking, then auto-stop and send (Record). 0 = wait until you click Stop Rec."
                },
                "query_history": {
                        "type": "string",
                        "default": "[]",
                        "internal": True
                }
        },
        "config_inline": None,
        "actions": [
                "extend_selection",
                "edit_selection"
        ],
        "action_icons": {}
},
    {
        "name": "doc",
        "title": "Doc",
        "requires": [
                "document",
                "config",
                "events"
        ],
        "provides_services": [],
        "config": {
                "grammar_proofreader_enabled": {
                        "type": "string",
                        "default": "off",
                        "widget": "select",
                        "label": "Enable grammar checker (Writer)",
                        "helper": "Off = disabled. AI (LLM) = use your configured text model/API. LanguageTool / Vale = local engines via Settings â†’ Python venv. Harper = offline Rust binary auto-downloaded to your profile (no venv required).",
                        "options": [
                                {
                                        "value": "off",
                                        "label": "Off"
                                },
                                {
                                        "value": "llm",
                                        "label": "AI (LLM)"
                                },
                                {
                                        "value": "harper",
                                        "label": "Harper"
                                },
                                {
                                        "value": "languagetool",
                                        "label": "LanguageTool (Local)"
                                },
                                {
                                        "value": "vale",
                                        "label": "Vale (Local Style) (WIP)"
                                }
                        ],
                        "inline": True,
                        "label_x": 8,
                        "label_width": 100,
                        "x": 110,
                        "width": 100
                },
                "grammar_proofreader_model": {
                        "type": "string",
                        "default": "",
                        "widget": "text",
                        "label": "Model (optional)",
                        "helper": "Leave empty to use the same text model as chat.",
                        "label_x": 220,
                        "label_width": 95,
                        "x": 318,
                        "width": 106
                },
                "grammar_proofreader_recheck": {
                        "type": "string",
                        "default": "",
                        "widget": "button",
                        "label": "Recheck this document",
                        "button_text": "Recheck",
                        "helper": "Clears cached grammar results for this document and asks Writer to check again. Use after changing checker or model.",
                        "show_button_label": True,
                        "settings_persist": False,
                        "inline": True,
                        "label_width": 100,
                        "x": 110,
                        "width": 70
                },
                "grammar_proofreader_pause_during_agent": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Pause grammar while chat/agent runs",
                        "helper": "When on, AI grammar skips background requests while sidebar chat or agent runs, to avoid concurrent API calls.",
                        "x": 220,
                        "width": 204
                },
                "grammar_proofreader_batch_sentences": {
                        "type": "int",
                        "default": 1,
                        "min": 1,
                        "max": 8,
                        "widget": "number",
                        "label": "Batch sentences",
                        "helper": "Number of sentences to send per LLM request (1-8). 1 (default) is most reliable; higher values are faster and cheaper but may hit token limits or cause model errors.",
                        "inline": True,
                        "x": 110,
                        "width": 100
                },
                "grammar_proofreader_max_in_flight": {
                        "type": "int",
                        "default": 1,
                        "min": 1,
                        "max": 8,
                        "widget": "number",
                        "label": "Concurrent requests",
                        "helper": "Max simultaneous background grammar API calls (1-8). 1 matches prior behavior. Raise for OpenRouter or other providers that allow parallel requests; use with care alongside sidebar chat.",
                        "label_x": 220,
                        "label_width": 100,
                        "x": 322,
                        "width": 100
                },
                "grammar_proofreader_detect_language": {
                        "type": "string",
                        "default": "off",
                        "widget": "select",
                        "label": "Sentence language detection",
                        "helper": "Verify each complete sentence's language against the document locale. AI uses your grammar/chat API; Local uses langdetect in your Python venv (Settings â†’ Python; no API call). Mismatches update CharLocale and re-check grammar.",
                        "options": [
                                {
                                        "value": "off",
                                        "label": "Off"
                                },
                                {
                                        "value": "llm",
                                        "label": "AI (LLM)"
                                },
                                {
                                        "value": "langdetect",
                                        "label": "Local (langdetect)"
                                }
                        ]
                },
                "chat_enter_key_sends_message": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "label": "Enter key sends sidebar chat message",
                        "width": 300,
                        "helper": "When on, Enter in the sidebar query runs the same action as Send; Shift+Enter inserts a newline. When off, Enter inserts a newline."
                },
                "agent_edit_review_mode": {
                        "type": "string",
                        "default": "off",
                        "widget": "select",
                        "label": "Agent edit review",
                        "helper": "Off = direct edits. Record = tracked changes you accept/reject yourself. Wait = apply_document_content blocks (on chat/MCP worker thread) until you review.",
                        "options": [
                                {
                                        "value": "off",
                                        "label": "Off"
                                },
                                {
                                        "value": "record",
                                        "label": "Record (track changes)"
                                },
                                {
                                        "value": "wait",
                                        "label": "Wait for review"
                                }
                        ]
                },
                "edit_review_timeout": {
                        "type": "int",
                        "default": 900,
                        "min": 0,
                        "widget": "number",
                        "internal": True,
                        "label": "Edit review max wait (seconds)",
                        "helper": "Only used when mode is Wait. Max time apply_document_content blocks before returning with pending changes."
                }
        },
        "config_inline": None,
        "actions": [],
        "action_icons": {}
},
    {
        "name": "draw",
        "title": "Draw and Impress tools",
        "requires": [
                "document",
                "config"
        ],
        "provides_services": [],
        "config": {},
        "config_inline": None,
        "actions": [],
        "action_icons": {}
},
    {
        "name": "embeddings",
        "title": "Vector Search",
        "requires": [
                "config"
        ],
        "provides_services": [],
        "config": {
                "folder_search_mode": {
                        "type": "string",
                        "default": "none",
                        "widget": "select",
                        "label": "Cross-file search",
                        "helper": "Indexed semantic + keyword search for document_research (corpus.db). Requires embeddings venv â€” see Python Test.",
                        "options": [
                                {
                                        "value": "none",
                                        "label": "Off"
                                },
                                {
                                        "value": "hybrid",
                                        "label": "Embeddings + FTS"
                                },
                                {
                                        "value": "llama_index",
                                        "label": "LlamaIndex"
                                },
                                {
                                        "value": "zvec",
                                        "label": "Zvec (experimental)"
                                },
                                {
                                        "value": "lancedb",
                                        "label": "LanceDB (experimental)"
                                }
                        ]
                },
                "embedding_model": {
                        "type": "string",
                        "default": "paraphrase-multilingual-MiniLM-L12-v2",
                        "widget": "text",
                        "label": "Embedding Model",
                        "helper": "HuggingFace model ID for local provider, or endpoint-specific model ID"
                },
                "folder_rerank_enabled": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Enable cross-file rerank",
                        "width": 200,
                        "helper": "Second-stage cross-encoder after hybrid retrieve (Embeddings + FTS and LlamaIndex). Off by default."
                },
                "folder_rerank_model": {
                        "type": "string",
                        "default": "cross-encoder/ms-marco-MiniLM-L-6-v2",
                        "widget": "select",
                        "label": "Rerank model",
                        "helper": "Used only when rerank is enabled. English MiniLM is fast; bge-reranker-v2-m3 is multilingual and downloads ~2.3 GB.",
                        "options": [
                                {
                                        "value": "cross-encoder/ms-marco-MiniLM-L-6-v2",
                                        "label": "cross-encoder/ms-marco-MiniLM-L-6-v2"
                                },
                                {
                                        "value": "BAAI/bge-reranker-v2-m3",
                                        "label": "BAAI/bge-reranker-v2-m3"
                                }
                        ]
                }
        },
        "config_inline": None,
        "actions": [],
        "action_icons": {}
},
    {
        "name": "scripting",
        "title": "Python",
        "requires": [
                "config"
        ],
        "provides_services": [],
        "config": {
                "python_venv_path": {
                        "type": "string",
                        "default": "",
                        "widget": "text",
                        "label": "Python venv path",
                        "helper": "If set, runs =PY() and Run Python Script in that venv (bin/python, Scripts/python.exe, or conda/pyenv-win env-root python.exe). Paste the path without surrounding quotes. If empty, uses this process's sys.executable (basic stdlib); use a venv for numpy/pandas and Vision Helpers (paddleocr, paddlepaddle).",
                        "width": 200,
                        "inline": True
                },
                "test_venv": {
                        "type": "string",
                        "default": "",
                        "widget": "button",
                        "label": "Test",
                        "inline_no_label": True,
                        "settings_persist": False,
                        "x": 314,
                        "width": 48
                },
                "download_audio_binaries": {
                        "type": "string",
                        "default": "",
                        "widget": "button",
                        "label": "Download platform-specific audio and serialization binaries:",
                        "button_text": "Download",
                        "show_button_label": True,
                        "label_width": 280,
                        "settings_persist": False,
                        "x": 297,
                        "width": 65
                },
                "python_exec_timeout": {
                        "type": "int",
                        "default": 10,
                        "min": 1,
                        "max": 600,
                        "widget": "number",
                        "label": "Python script timeout (seconds)",
                        "helper": "Wall-clock limit for Run Python Script and =PYTHON() / =PY()."
                },
                "python_max_data_cells": {
                        "type": "int",
                        "default": 250000,
                        "min": 1000,
                        "max": 2000000,
                        "widget": "number",
                        "label": "Max Calc data cells",
                        "helper": "Maximum cells in =PYTHON() data or Run Python Script data/data_range. Larger values use more RAM and time; UNO range read still dominates.",
                        "public": True
                },
                "python_session_mode": {
                        "type": "string",
                        "default": "isolated",
                        "widget": "select",
                        "label": "Python session mode",
                        "helper": "Isolated: each =PYTHON() cell gets a fresh namespace. Shared: one namespace per workbook (variables carry between cells).",
                        "public": True,
                        "options": [
                                {
                                        "value": "isolated",
                                        "label": "Isolated (default)"
                                },
                                {
                                        "value": "shared",
                                        "label": "Shared kernel"
                                }
                        ]
                },
                "python_geometric_recalc_order": {
                        "type": "bool",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Geometric Recalc Order (Experimental)",
                        "helper": "Ensures PY cells evaluate in sheet order. Most useful with Shared kernel.",
                        "public": True,
                        "width": 250
                },
                "python_auto_spill": {
                        "type": "bool",
                        "default": True,
                        "widget": "checkbox",
                        "label": "Python auto spill in Calc",
                        "helper": "Automatically spill multi-cell array/DataFrame results from =PYTHON() into adjacent cells.",
                        "public": True,
                        "inline": True,
                        "width": 200
                },
                "xl_static_rewrite": {
                        "type": "bool",
                        "default": False,
                        "widget": "checkbox",
                        "label": "Rewrite xl() ranges",
                        "helper": "On Edit Python in Cell save, lift static xl(\"A1:â€¦\") onto =PY data args and rewrite to data / data[i] / .to_pandas().",
                        "public": True,
                        "x": 220,
                        "width": 200
                },
                "force_internal_script_editor": {
                        "type": "bool",
                        "default": False,
                        "internal": True
                },
                "native_run_script_modeless": {
                        "type": "bool",
                        "default": True,
                        "internal": True
                }
        },
        "config_inline": None,
        "actions": [
                "reset_python_session",
                "edit_python_cell"
        ],
        "action_icons": {}
},
    {
        "name": "vision",
        "title": "Vision / OCR",
        "requires": [
                "config"
        ],
        "provides_services": [],
        "config": {
                "images_scale": {
                        "type": "float",
                        "default": 1.0,
                        "min": 0.5,
                        "max": 3.0,
                        "widget": "number",
                        "page": "general",
                        "label": "Image scale",
                        "helper": "Upscale before OCR/layout. Try 2.0 for small screenshots."
                },
                "worker_timeout_sec": {
                        "type": "int",
                        "default": 300,
                        "min": 30,
                        "max": 600,
                        "widget": "number",
                        "page": "general",
                        "label": "Worker timeout (seconds)",
                        "helper": "Host subprocess limit for Vision Helpers (first model download)."
                },
                "artifacts_path": {
                        "type": "string",
                        "default": "",
                        "widget": "text",
                        "page": "general",
                        "label": "Docling artifacts path",
                        "helper": "Optional folder from docling-tools models download (offline use)."
                },
                "text_score": {
                        "type": "float",
                        "default": 0.5,
                        "min": 0.0,
                        "max": 1.0,
                        "widget": "number",
                        "page": "ocr",
                        "label": "OCR confidence threshold",
                        "helper": "RapidOCR minimum text score. 0.0 is a real threshold (not the default). Raise to reduce garbage text."
                },
                "force_full_page_ocr": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "page": "ocr",
                        "label": "OCR entire image",
                        "helper": "Recommended for embedded graphics exported from LibreOffice."
                },
                "table_mode": {
                        "type": "string",
                        "default": "accurate",
                        "widget": "select",
                        "page": "tables",
                        "label": "Table extraction mode",
                        "options": [
                                {
                                        "value": "accurate",
                                        "label": "Accurate (default)"
                                },
                                {
                                        "value": "fast",
                                        "label": "Fast"
                                }
                        ]
                },
                "do_cell_matching": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "page": "tables",
                        "label": "Table cell matching",
                        "helper": "Align detected table cells with content (extract_structure)."
                },
                "create_orphan_clusters": {
                        "type": "boolean",
                        "default": True,
                        "widget": "checkbox",
                        "page": "tables",
                        "label": "Keep orphan text clusters",
                        "helper": "Group unassigned text into clusters for messy layouts."
                },
                "insert_mode": {
                        "type": "string",
                        "default": "html",
                        "widget": "select",
                        "page": "tables",
                        "label": "Document insert mode",
                        "helper": "html = Docling HTML import (default). structured = bbox column layout in Writer; native Calc cells for extract_structure tables.",
                        "options": [
                                {
                                        "value": "html",
                                        "label": "Standard HTML"
                                },
                                {
                                        "value": "structured",
                                        "label": "Structured (layout / cell grid)"
                                }
                        ]
                },
                "layout_model": {
                        "type": "string",
                        "default": "heron",
                        "widget": "select",
                        "page": "advanced",
                        "label": "Layout model",
                        "options": [
                                {
                                        "value": "heron",
                                        "label": "Heron (default)"
                                },
                                {
                                        "value": "egret_large",
                                        "label": "Egret large (slower, complex docs)"
                                }
                        ]
                },
                "do_formula_enrichment": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "page": "advanced",
                        "label": "Extract formulas (LaTeX)",
                        "helper": "Opt-in. Downloads extra VLM models on first use; slower."
                },
                "do_code_enrichment": {
                        "type": "boolean",
                        "default": False,
                        "widget": "checkbox",
                        "page": "advanced",
                        "label": "Code block enrichment",
                        "helper": "Specialized OCR for code/terminal screenshots."
                },
                "document_timeout": {
                        "type": "float",
                        "default": 0,
                        "min": 0,
                        "max": 600,
                        "widget": "number",
                        "page": "advanced",
                        "label": "Docling document timeout (s)",
                        "helper": "Per-conversion timeout inside Docling. 0 = no limit."
                }
        },
        "config_inline": None,
        "actions": [
                "open_settings"
        ],
        "action_icons": {},
        "settings_tab": False,
        "config_dialog": {
                "id": "VisionSettingsDialog",
                "library": "Dialogs",
                "title": "Vision / OCR Settings",
                "width": 350,
                "height": 170,
                "moveable": True,
                "resizeable": True,
                "modeless": True,
                "buttons": [
                        "apply",
                        "ok",
                        "close"
                ]
        }
},
    {
        "name": "writer",
        "title": "Writer",
        "requires": [
                "document",
                "config",
                "format",
                "events"
        ],
        "provides_services": [
                "writer_bookmarks",
                "writer_tree",
                "writer_proximity",
                "writer_index"
        ],
        "config": {},
        "config_inline": None,
        "actions": [
                "review_prev",
                "review_next"
        ],
        "action_icons": {}
},
]

CONFIG_DEFAULTS = {
    "core.log_level": "WARN",
    "log_level": "WARN",
    "agent_backend.backend_id": "builtin",
    "backend_id": "builtin",
    "agent_backend.path": "",
    "path": "",
    "agent_backend.args": "",
    "args": "",
    "agent_backend.acp_agent_name": "",
    "acp_agent_name": "",
    "agent_backend.prompt_for_permission": True,
    "prompt_for_permission": True,
    "audio.stt_provider": "endpoint",
    "stt_provider": "endpoint",
    "audio.stt_model": "",
    "stt_model": "",
    "audio.stt_local_model": "base",
    "stt_local_model": "base",
    "audio.tts_enabled": False,
    "tts_enabled": False,
    "audio.tts_sentence_mode": True,
    "tts_sentence_mode": True,
    "audio.tts_short_answers": True,
    "tts_short_answers": True,
    "audio.tts_provider": "system",
    "tts_provider": "system",
    "audio.tts_model": "",
    "tts_model": "",
    "audio.tts_voice": "alloy",
    "tts_voice": "alloy",
    "audio.test_voice": "",
    "test_voice": "",
    "audio.tts_speed": "1.0x",
    "tts_speed": "1.0x",
    "audio.tts_voice_kokoro": "af_sky",
    "tts_voice_kokoro": "af_sky",
    "audio.tts_voice_piper": "en_US-lessac-medium",
    "tts_voice_piper": "en_US-lessac-medium",
    "audio.tts_voice_openai": "alloy",
    "tts_voice_openai": "alloy",
    "audio.tts_voice_openrouter": "",
    "tts_voice_openrouter": "",
    "audio.tts_voice_together": "",
    "tts_voice_together": "",
    "audio.tts_voice_system": "default",
    "tts_voice_system": "default",
    "calc.max_rows_display": 1000,
    "max_rows_display": 1000,
    "calc.ods_cache_enabled": True,
    "ods_cache_enabled": True,
    "mcp.mcp_enabled": False,
    "mcp_enabled": False,
    "mcp.mcp_port": 18765,
    "mcp_port": 18765,
    "mcp.tool_exposure_mode": "delegate",
    "tool_exposure_mode": "delegate",
    "mcp.tunnel_enabled": False,
    "tunnel_enabled": False,
    "mcp.test_tunnel": "",
    "test_tunnel": "",
    "mcp.tunnel_provider": "cloudflare",
    "tunnel_provider": "cloudflare",
    "mcp.tunnel_provider_token": "",
    "tunnel_provider_token": "",
    "mcp.client_config_snippet": "",
    "client_config_snippet": "",
    "mcp.copy_config": "",
    "copy_config": "",
    "mcp.cors_allow_private_origins": True,
    "cors_allow_private_origins": True,
    "mcp.cors_allowed_origins": [],
    "cors_allowed_origins": [],
    "chatbot.max_tool_rounds": 15,
    "max_tool_rounds": 15,
    "chatbot.context_strategy": "auto",
    "context_strategy": "auto",
    "chatbot.extend_selection_max_tokens": 1000,
    "extend_selection_max_tokens": 1000,
    "chatbot.edit_selection_max_new_tokens": 1000,
    "edit_selection_max_new_tokens": 1000,
    "chatbot.web_research_cache_enabled": False,
    "web_research_cache_enabled": False,
    "chatbot.show_search_thinking": False,
    "show_search_thinking": False,
    "chatbot.prompt_for_web_research": False,
    "prompt_for_web_research": False,
    "chatbot.web_research_browser": "off",
    "web_research_browser": "off",
    "chatbot.web_cache_max_mb": 50,
    "web_cache_max_mb": 50,
    "chatbot.web_cache_validity_days": 30,
    "web_cache_validity_days": 30,
    "chatbot.web_research_cache_jaccard_percent": 60,
    "web_research_cache_jaccard_percent": 60,
    "chatbot.web_research_cache_embedding_percent": 75,
    "web_research_cache_embedding_percent": 75,
    "chatbot.web_research_cache_min_overlap": 8,
    "web_research_cache_min_overlap": 8,
    "chatbot.deep_research_breadth": 4,
    "deep_research_breadth": 4,
    "chatbot.deep_research_depth": 2,
    "deep_research_depth": 2,
    "chatbot.deep_research_concurrency": 2,
    "deep_research_concurrency": 2,
    "chatbot.deep_research_max_sub_queries": 14,
    "deep_research_max_sub_queries": 14,
    "chatbot.deep_research_max_rounds": 3,
    "deep_research_max_rounds": 3,
    "chatbot.deep_research_quality_threshold": 7,
    "deep_research_quality_threshold": 7,
    "chatbot.deep_research_sub_agent_steps": 0,
    "deep_research_sub_agent_steps": 0,
    "chatbot.rich_text_control_sidebar": True,
    "rich_text_control_sidebar": True,
    "chatbot.librarian_invoked": False,
    "librarian_invoked": False,
    "chatbot.humanizer_enabled": False,
    "humanizer_enabled": False,
    "chatbot.audio_silence_stop_ms": 3000,
    "audio_silence_stop_ms": 3000,
    "chatbot.query_history": "[]",
    "query_history": "[]",
    "doc.grammar_proofreader_enabled": "off",
    "grammar_proofreader_enabled": "off",
    "doc.grammar_proofreader_model": "",
    "grammar_proofreader_model": "",
    "doc.grammar_proofreader_recheck": "",
    "grammar_proofreader_recheck": "",
    "doc.grammar_proofreader_pause_during_agent": False,
    "grammar_proofreader_pause_during_agent": False,
    "doc.grammar_proofreader_batch_sentences": 1,
    "grammar_proofreader_batch_sentences": 1,
    "doc.grammar_proofreader_max_in_flight": 1,
    "grammar_proofreader_max_in_flight": 1,
    "doc.grammar_proofreader_detect_language": "off",
    "grammar_proofreader_detect_language": "off",
    "doc.chat_enter_key_sends_message": True,
    "chat_enter_key_sends_message": True,
    "doc.agent_edit_review_mode": "off",
    "agent_edit_review_mode": "off",
    "doc.edit_review_timeout": 900,
    "edit_review_timeout": 900,
    "embeddings.folder_search_mode": "none",
    "folder_search_mode": "none",
    "embeddings.embedding_model": "paraphrase-multilingual-MiniLM-L12-v2",
    "embedding_model": "paraphrase-multilingual-MiniLM-L12-v2",
    "embeddings.folder_rerank_enabled": False,
    "folder_rerank_enabled": False,
    "embeddings.folder_rerank_model": "cross-encoder/ms-marco-MiniLM-L-6-v2",
    "folder_rerank_model": "cross-encoder/ms-marco-MiniLM-L-6-v2",
    "scripting.python_venv_path": "",
    "python_venv_path": "",
    "scripting.test_venv": "",
    "test_venv": "",
    "scripting.download_audio_binaries": "",
    "download_audio_binaries": "",
    "scripting.python_exec_timeout": 10,
    "python_exec_timeout": 10,
    "scripting.python_max_data_cells": 250000,
    "python_max_data_cells": 250000,
    "scripting.python_session_mode": "isolated",
    "python_session_mode": "isolated",
    "scripting.python_geometric_recalc_order": False,
    "python_geometric_recalc_order": False,
    "scripting.python_auto_spill": True,
    "python_auto_spill": True,
    "scripting.xl_static_rewrite": False,
    "xl_static_rewrite": False,
    "scripting.force_internal_script_editor": False,
    "force_internal_script_editor": False,
    "scripting.native_run_script_modeless": True,
    "native_run_script_modeless": True,
    "vision.images_scale": 1.0,
    "images_scale": 1.0,
    "vision.worker_timeout_sec": 300,
    "worker_timeout_sec": 300,
    "vision.artifacts_path": "",
    "artifacts_path": "",
    "vision.text_score": 0.5,
    "text_score": 0.5,
    "vision.force_full_page_ocr": True,
    "force_full_page_ocr": True,
    "vision.table_mode": "accurate",
    "table_mode": "accurate",
    "vision.do_cell_matching": True,
    "do_cell_matching": True,
    "vision.create_orphan_clusters": True,
    "create_orphan_clusters": True,
    "vision.insert_mode": "html",
    "insert_mode": "html",
    "vision.layout_model": "heron",
    "layout_model": "heron",
    "vision.do_formula_enrichment": False,
    "do_formula_enrichment": False,
    "vision.do_code_enrichment": False,
    "do_code_enrichment": False,
    "vision.document_timeout": 0,
    "document_timeout": 0
}

CONFIG_SCHEMAS = {
    "core.log_level": {
        "type": "string",
        "default": "WARN",
        "widget": "select",
        "label": "Log Level",
        "internal": True,
        "options": [
            {
                "value": "DEBUG",
                "label": "Debug"
            },
            {
                "value": "INFO",
                "label": "Info"
            },
            {
                "value": "WARN",
                "label": "Warning"
            },
            {
                "value": "ERROR",
                "label": "Error"
            }
        ]
    },
    "log_level": {
        "type": "string",
        "default": "WARN",
        "widget": "select",
        "label": "Log Level",
        "internal": True,
        "options": [
            {
                "value": "DEBUG",
                "label": "Debug"
            },
            {
                "value": "INFO",
                "label": "Info"
            },
            {
                "value": "WARN",
                "label": "Warning"
            },
            {
                "value": "ERROR",
                "label": "Error"
            }
        ]
    },
    "agent_backend.backend_id": {
        "type": "string",
        "default": "builtin",
        "widget": "select",
        "label": "Backend",
        "options": [
            {
                "value": "builtin",
                "label": "Built-in"
            },
            {
                "value": "hermes",
                "label": "Hermes"
            },
            {
                "value": "claude",
                "label": "Claude Code (ACP)"
            },
            {
                "value": "vibe",
                "label": "Mistral Vibe (ACP)"
            },
            {
                "value": "grok",
                "label": "Grok Build (ACP)"
            },
            {
                "value": "opencode",
                "label": "OpenCode (ACP)"
            }
        ]
    },
    "backend_id": {
        "type": "string",
        "default": "builtin",
        "widget": "select",
        "label": "Backend",
        "options": [
            {
                "value": "builtin",
                "label": "Built-in"
            },
            {
                "value": "hermes",
                "label": "Hermes"
            },
            {
                "value": "claude",
                "label": "Claude Code (ACP)"
            },
            {
                "value": "vibe",
                "label": "Mistral Vibe (ACP)"
            },
            {
                "value": "grok",
                "label": "Grok Build (ACP)"
            },
            {
                "value": "opencode",
                "label": "OpenCode (ACP)"
            }
        ]
    },
    "agent_backend.path": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "Path / URL",
        "helper": "Path to backend CLI (e.g. aider) or ACP server URL (e.g. http://localhost:8000 for Hermes). Empty = try default.",
        "internal": True
    },
    "path": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "Path / URL",
        "helper": "Path to backend CLI (e.g. aider) or ACP server URL (e.g. http://localhost:8000 for Hermes). Empty = try default.",
        "internal": True
    },
    "agent_backend.args": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "Extra arguments",
        "helper": "Optional arguments for the selected backend (space-separated).",
        "internal": True
    },
    "args": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "Extra arguments",
        "helper": "Optional arguments for the selected backend (space-separated).",
        "internal": True
    },
    "agent_backend.acp_agent_name": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "ACP agent name",
        "helper": "Agent name on the ACP server (e.g. hermes). Empty = auto-discover first agent.",
        "internal": True
    },
    "acp_agent_name": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "ACP agent name",
        "helper": "Agent name on the ACP server (e.g. hermes). Empty = auto-discover first agent.",
        "internal": True
    },
    "agent_backend.prompt_for_permission": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Prompt for Agent Permissions",
        "helper": "Ask for confirmation before allowing the agent to use tools (read files, execute commands, etc.)"
    },
    "prompt_for_permission": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Prompt for Agent Permissions",
        "helper": "Ask for confirmation before allowing the agent to use tools (read files, execute commands, etc.)"
    },
    "audio.stt_provider": {
        "type": "string",
        "default": "endpoint",
        "widget": "select",
        "label": "STT Provider",
        "helper": "LLM Endpoint uses Audio Model and POST /v1/audio/transcriptions. Local Whisper runs faster-whisper in the Settings â†’ Python venv (not LibreOffice's Python).",
        "options": [
            {
                "value": "endpoint",
                "label": "LLM Endpoint"
            },
            {
                "value": "local",
                "label": "Local Whisper (faster-whisper)"
            }
        ]
    },
    "stt_provider": {
        "type": "string",
        "default": "endpoint",
        "widget": "select",
        "label": "STT Provider",
        "helper": "LLM Endpoint uses Audio Model and POST /v1/audio/transcriptions. Local Whisper runs faster-whisper in the Settings â†’ Python venv (not LibreOffice's Python).",
        "options": [
            {
                "value": "endpoint",
                "label": "LLM Endpoint"
            },
            {
                "value": "local",
                "label": "Local Whisper (faster-whisper)"
            }
        ]
    },
    "audio.stt_model": {
        "type": "string",
        "default": "",
        "widget": "combo",
        "label": "Audio Model:",
        "helper": "Speech-to-text model when STT Provider is LLM Endpoint and the chat model cannot take audio input."
    },
    "stt_model": {
        "type": "string",
        "default": "",
        "widget": "combo",
        "label": "Audio Model:",
        "helper": "Speech-to-text model when STT Provider is LLM Endpoint and the chat model cannot take audio input."
    },
    "audio.stt_local_model": {
        "type": "string",
        "default": "base",
        "widget": "select",
        "label": "Local Model",
        "helper": "faster-whisper size. The first transcription downloads weights into the Hugging Face cache (base, the default, is about 150 MB; tiny ~75 MB, small ~500 MB, medium ~1.5 GB).",
        "options": [
            {
                "value": "tiny",
                "label": "tiny (~75 MB)"
            },
            {
                "value": "base",
                "label": "base (~150 MB)"
            },
            {
                "value": "small",
                "label": "small (~500 MB)"
            },
            {
                "value": "medium",
                "label": "medium (~1.5 GB)"
            }
        ]
    },
    "stt_local_model": {
        "type": "string",
        "default": "base",
        "widget": "select",
        "label": "Local Model",
        "helper": "faster-whisper size. The first transcription downloads weights into the Hugging Face cache (base, the default, is about 150 MB; tiny ~75 MB, small ~500 MB, medium ~1.5 GB).",
        "options": [
            {
                "value": "tiny",
                "label": "tiny (~75 MB)"
            },
            {
                "value": "base",
                "label": "base (~150 MB)"
            },
            {
                "value": "small",
                "label": "small (~500 MB)"
            },
            {
                "value": "medium",
                "label": "medium (~1.5 GB)"
            }
        ]
    },
    "audio.tts_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Enable Speech Output (TTS)",
        "width": 220,
        "helper": "Speak assistant responses aloud using text-to-speech."
    },
    "tts_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Enable Speech Output (TTS)",
        "width": 220,
        "helper": "Speak assistant responses aloud using text-to-speech."
    },
    "audio.tts_sentence_mode": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Speak sentence by sentence (Local)",
        "width": 168,
        "helper": "Synthesize the next sentences while the current one plays, so a short sentence is not followed by a gap. Stop discards clips that are not playing yet. Turn off to speak the whole reply as one clip."
    },
    "tts_sentence_mode": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Speak sentence by sentence (Local)",
        "width": 168,
        "helper": "Synthesize the next sentences while the current one plays, so a short sentence is not followed by a gap. Stop discards clips that are not playing yet. Turn off to speak the whole reply as one clip."
    },
    "audio.tts_short_answers": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Keep replies brief",
        "helper": "Aim for about one paragraph unless the user asks for more. Only applies while speech output (TTS) is on.",
        "tooltip": True,
        "width": 160
    },
    "tts_short_answers": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Keep replies brief",
        "helper": "Aim for about one paragraph unless the user asks for more. Only applies while speech output (TTS) is on.",
        "tooltip": True,
        "width": 160
    },
    "audio.tts_provider": {
        "type": "string",
        "default": "system",
        "widget": "select",
        "label": "TTS Provider",
        "helper": "Choose whether to use built-in OS speech, a local neural engine, or your API endpoint.",
        "options": [
            {
                "value": "system",
                "label": "OS Native (say / SAPI / spd-say)"
            },
            {
                "value": "kokoro",
                "label": "Kokoro (Local Neural, ONNX CPU)"
            },
            {
                "value": "piper",
                "label": "Piper (Local Fast Neural, CPU)"
            },
            {
                "value": "endpoint",
                "label": "LLM Endpoint"
            }
        ]
    },
    "tts_provider": {
        "type": "string",
        "default": "system",
        "widget": "select",
        "label": "TTS Provider",
        "helper": "Choose whether to use built-in OS speech, a local neural engine, or your API endpoint.",
        "options": [
            {
                "value": "system",
                "label": "OS Native (say / SAPI / spd-say)"
            },
            {
                "value": "kokoro",
                "label": "Kokoro (Local Neural, ONNX CPU)"
            },
            {
                "value": "piper",
                "label": "Piper (Local Fast Neural, CPU)"
            },
            {
                "value": "endpoint",
                "label": "LLM Endpoint"
            }
        ]
    },
    "audio.tts_model": {
        "type": "string",
        "default": "",
        "widget": "combo",
        "label": "TTS Model",
        "helper": "Model for speech synthesis (used when TTS Provider is LLM Endpoint)."
    },
    "tts_model": {
        "type": "string",
        "default": "",
        "widget": "combo",
        "label": "TTS Model",
        "helper": "Model for speech synthesis (used when TTS Provider is LLM Endpoint)."
    },
    "audio.tts_voice": {
        "type": "string",
        "default": "alloy",
        "widget": "select",
        "label": "Voice",
        "helper": "Voice for the selected provider. The list matches the Piper or Kokoro catalog for your locale.",
        "inline": True,
        "options_provider": "plugin.audio.tts_voices:settings_voice_options",
        "options": [
            {
                "value": "alloy",
                "label": "alloy (OpenAI Neutral)"
            },
            {
                "value": "af_sky",
                "label": "US Female - Sky"
            },
            {
                "value": "en_US-lessac-medium",
                "label": "US English Female - Lessac"
            },
            {
                "value": "default",
                "label": "default (System Default)"
            }
        ]
    },
    "tts_voice": {
        "type": "string",
        "default": "alloy",
        "widget": "select",
        "label": "Voice",
        "helper": "Voice for the selected provider. The list matches the Piper or Kokoro catalog for your locale.",
        "inline": True,
        "options_provider": "plugin.audio.tts_voices:settings_voice_options",
        "options": [
            {
                "value": "alloy",
                "label": "alloy (OpenAI Neutral)"
            },
            {
                "value": "af_sky",
                "label": "US Female - Sky"
            },
            {
                "value": "en_US-lessac-medium",
                "label": "US English Female - Lessac"
            },
            {
                "value": "default",
                "label": "default (System Default)"
            }
        ]
    },
    "audio.test_voice": {
        "type": "string",
        "default": "",
        "widget": "button",
        "label": "Test voice",
        "inline_no_label": True,
        "settings_persist": False,
        "x": 260,
        "width": 88
    },
    "test_voice": {
        "type": "string",
        "default": "",
        "widget": "button",
        "label": "Test voice",
        "inline_no_label": True,
        "settings_persist": False,
        "x": 260,
        "width": 88
    },
    "audio.tts_speed": {
        "type": "string",
        "default": "1.0x",
        "widget": "combo",
        "label": "Speech Speed",
        "helper": "Playback speed multiplier (1.0x, 1.1x, 1.25x, 1.5x, 1.75x, 2.0x, or custom down to 0.25x).",
        "options": [
            {
                "value": "1.0x",
                "label": "1.0x"
            },
            {
                "value": "1.1x",
                "label": "1.1x"
            },
            {
                "value": "1.25x",
                "label": "1.25x"
            },
            {
                "value": "1.5x",
                "label": "1.5x"
            },
            {
                "value": "1.75x",
                "label": "1.75x"
            },
            {
                "value": "2.0x",
                "label": "2.0x"
            }
        ]
    },
    "tts_speed": {
        "type": "string",
        "default": "1.0x",
        "widget": "combo",
        "label": "Speech Speed",
        "helper": "Playback speed multiplier (1.0x, 1.1x, 1.25x, 1.5x, 1.75x, 2.0x, or custom down to 0.25x).",
        "options": [
            {
                "value": "1.0x",
                "label": "1.0x"
            },
            {
                "value": "1.1x",
                "label": "1.1x"
            },
            {
                "value": "1.25x",
                "label": "1.25x"
            },
            {
                "value": "1.5x",
                "label": "1.5x"
            },
            {
                "value": "1.75x",
                "label": "1.75x"
            },
            {
                "value": "2.0x",
                "label": "2.0x"
            }
        ]
    },
    "audio.tts_voice_kokoro": {
        "type": "string",
        "default": "af_sky",
        "internal": True
    },
    "tts_voice_kokoro": {
        "type": "string",
        "default": "af_sky",
        "internal": True
    },
    "audio.tts_voice_piper": {
        "type": "string",
        "default": "en_US-lessac-medium",
        "internal": True
    },
    "tts_voice_piper": {
        "type": "string",
        "default": "en_US-lessac-medium",
        "internal": True
    },
    "audio.tts_voice_openai": {
        "type": "string",
        "default": "alloy",
        "internal": True
    },
    "tts_voice_openai": {
        "type": "string",
        "default": "alloy",
        "internal": True
    },
    "audio.tts_voice_openrouter": {
        "type": "string",
        "default": "",
        "internal": True
    },
    "tts_voice_openrouter": {
        "type": "string",
        "default": "",
        "internal": True
    },
    "audio.tts_voice_together": {
        "type": "string",
        "default": "",
        "internal": True
    },
    "tts_voice_together": {
        "type": "string",
        "default": "",
        "internal": True
    },
    "audio.tts_voice_system": {
        "type": "string",
        "default": "default",
        "internal": True
    },
    "tts_voice_system": {
        "type": "string",
        "default": "default",
        "internal": True
    },
    "calc.max_rows_display": {
        "type": "int",
        "default": 1000,
        "min": 100,
        "max": 100000,
        "widget": "number",
        "label": "Max Rows Display",
        "public": True
    },
    "max_rows_display": {
        "type": "int",
        "default": 1000,
        "min": 100,
        "max": 100000,
        "widget": "number",
        "label": "Max Rows Display",
        "public": True
    },
    "calc.ods_cache_enabled": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Cache Excel siblings as ODS",
        "width": 220,
        "helper": "Convert sibling .xlsx/.xls to writeragent_ods_cache/ beside the folder so repeated DuckDB SQL reuses the ODS. Native .ods and the live workbook are never cached. Set calc.ods_cache_enabled false in writeragent.json to disable.",
        "public": True
    },
    "ods_cache_enabled": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Cache Excel siblings as ODS",
        "width": 220,
        "helper": "Convert sibling .xlsx/.xls to writeragent_ods_cache/ beside the folder so repeated DuckDB SQL reuses the ODS. Native .ods and the live workbook are never cached. Set calc.ods_cache_enabled false in writeragent.json to disable.",
        "public": True
    },
    "mcp.mcp_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Enable MCP Server",
        "width": 168,
        "helper": "Localhost only, no auth. Clients use http://localhost:<port>/mcp (streamable HTTP).",
        "public": True
    },
    "mcp_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Enable MCP Server",
        "width": 168,
        "helper": "Localhost only, no auth. Clients use http://localhost:<port>/mcp (streamable HTTP).",
        "public": True
    },
    "mcp.mcp_port": {
        "type": "int",
        "default": 18765,
        "min": 1024,
        "max": 65535,
        "widget": "number",
        "label": "MCP Port",
        "public": True,
        "inline": True,
        "x": 110,
        "width": 70
    },
    "mcp_port": {
        "type": "int",
        "default": 18765,
        "min": 1024,
        "max": 65535,
        "widget": "number",
        "label": "MCP Port",
        "public": True,
        "inline": True,
        "x": 110,
        "width": 70
    },
    "mcp.tool_exposure_mode": {
        "type": "string",
        "default": "delegate",
        "widget": "select",
        "label": "Tool Exposure",
        "helper": "How specialized tools are surfaced to MCP clients. delegate=reached via the delegate gateway (today's behavior); direct_flat=all MCP-reachable specialized tools listed directly, excluding sidebar-only flows (best for clients with their own tool-search); direct_discovery=small core list plus find_tools (domain catalog, then per-domain tool schemas; best for any client).",
        "public": True,
        "label_x": 195,
        "label_width": 85,
        "x": 285,
        "width": 145,
        "options": [
            {
                "value": "delegate",
                "label": "Delegate (default)"
            },
            {
                "value": "direct_flat",
                "label": "Direct â€” list all tools"
            },
            {
                "value": "direct_discovery",
                "label": "Direct â€” discovery (find_tools)"
            }
        ]
    },
    "tool_exposure_mode": {
        "type": "string",
        "default": "delegate",
        "widget": "select",
        "label": "Tool Exposure",
        "helper": "How specialized tools are surfaced to MCP clients. delegate=reached via the delegate gateway (today's behavior); direct_flat=all MCP-reachable specialized tools listed directly, excluding sidebar-only flows (best for clients with their own tool-search); direct_discovery=small core list plus find_tools (domain catalog, then per-domain tool schemas; best for any client).",
        "public": True,
        "label_x": 195,
        "label_width": 85,
        "x": 285,
        "width": 145,
        "options": [
            {
                "value": "delegate",
                "label": "Delegate (default)"
            },
            {
                "value": "direct_flat",
                "label": "Direct â€” list all tools"
            },
            {
                "value": "direct_discovery",
                "label": "Direct â€” discovery (find_tools)"
            }
        ]
    },
    "mcp._sep_tunnel": {
        "widget": "separator",
        "label": "Public tunnel"
    },
    "_sep_tunnel": {
        "widget": "separator",
        "label": "Public tunnel"
    },
    "mcp.tunnel_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Expose via public tunnel",
        "helper": "Requires the selected provider binary on PATH (cloudflared, bore, ngrok, or tailscale). MCP has no auth â€” anyone with the public URL can call tools. See MCP Server Status for the public /mcp URL when ready.",
        "public": True,
        "inline": True,
        "width": 170
    },
    "tunnel_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Expose via public tunnel",
        "helper": "Requires the selected provider binary on PATH (cloudflared, bore, ngrok, or tailscale). MCP has no auth â€” anyone with the public URL can call tools. See MCP Server Status for the public /mcp URL when ready.",
        "public": True,
        "inline": True,
        "width": 170
    },
    "mcp.test_tunnel": {
        "type": "string",
        "default": "",
        "widget": "button",
        "button_text": "Test Tunnel",
        "settings_persist": False,
        "inline_no_label": True,
        "x": 200,
        "width": 80
    },
    "test_tunnel": {
        "type": "string",
        "default": "",
        "widget": "button",
        "button_text": "Test Tunnel",
        "settings_persist": False,
        "inline_no_label": True,
        "x": 200,
        "width": 80
    },
    "mcp.tunnel_provider": {
        "type": "string",
        "default": "cloudflare",
        "widget": "select",
        "label": "Provider",
        "helper": "Cloudflare quick tunnel (default), Bore, Ngrok, or Tailscale Funnel (must already be logged in).",
        "public": True,
        "inline": True,
        "x": 110,
        "width": 100,
        "options": [
            {
                "value": "cloudflare",
                "label": "Cloudflare"
            },
            {
                "value": "bore",
                "label": "Bore"
            },
            {
                "value": "ngrok",
                "label": "Ngrok"
            },
            {
                "value": "tailscale",
                "label": "Tailscale"
            }
        ]
    },
    "tunnel_provider": {
        "type": "string",
        "default": "cloudflare",
        "widget": "select",
        "label": "Provider",
        "helper": "Cloudflare quick tunnel (default), Bore, Ngrok, or Tailscale Funnel (must already be logged in).",
        "public": True,
        "inline": True,
        "x": 110,
        "width": 100,
        "options": [
            {
                "value": "cloudflare",
                "label": "Cloudflare"
            },
            {
                "value": "bore",
                "label": "Bore"
            },
            {
                "value": "ngrok",
                "label": "Ngrok"
            },
            {
                "value": "tailscale",
                "label": "Tailscale"
            }
        ]
    },
    "mcp.tunnel_provider_token": {
        "type": "string",
        "default": "",
        "widget": "password",
        "label": "Provider config",
        "helper": "Ngrok authtoken; Cloudflare tunnel token; Bore server or 'server secret'. Empty = provider default / CLI config. Unused for Tailscale.",
        "public": True,
        "label_x": 215,
        "label_width": 85,
        "x": 302,
        "width": 128
    },
    "tunnel_provider_token": {
        "type": "string",
        "default": "",
        "widget": "password",
        "label": "Provider config",
        "helper": "Ngrok authtoken; Cloudflare tunnel token; Bore server or 'server secret'. Empty = provider default / CLI config. Unused for Tailscale.",
        "public": True,
        "label_x": 215,
        "label_width": 85,
        "x": 302,
        "width": 128
    },
    "mcp.client_config_snippet": {
        "type": "string",
        "default": "",
        "widget": "textarea",
        "label": "Client config (copy into Claude / Hermes-Agent):",
        "label_above": True,
        "label_x": 8,
        "label_width": 340,
        "readonly": True,
        "settings_persist": False,
        "inline": True,
        "x": 8,
        "width": 340,
        "height": 36
    },
    "client_config_snippet": {
        "type": "string",
        "default": "",
        "widget": "textarea",
        "label": "Client config (copy into Claude / Hermes-Agent):",
        "label_above": True,
        "label_x": 8,
        "label_width": 340,
        "readonly": True,
        "settings_persist": False,
        "inline": True,
        "x": 8,
        "width": 340,
        "height": 36
    },
    "mcp.copy_config": {
        "type": "string",
        "default": "",
        "widget": "button",
        "button_text": "Copy Config",
        "settings_persist": False,
        "inline_no_label": True,
        "x": 355,
        "width": 75,
        "height": 16
    },
    "copy_config": {
        "type": "string",
        "default": "",
        "widget": "button",
        "button_text": "Copy Config",
        "settings_persist": False,
        "inline_no_label": True,
        "x": 355,
        "width": 75,
        "height": 16
    },
    "mcp.cors_allow_private_origins": {
        "type": "boolean",
        "default": True,
        "internal": True
    },
    "cors_allow_private_origins": {
        "type": "boolean",
        "default": True,
        "internal": True
    },
    "mcp.cors_allowed_origins": {
        "type": "list",
        "default": [],
        "internal": True
    },
    "cors_allowed_origins": {
        "type": "list",
        "default": [],
        "internal": True
    },
    "chatbot.max_tool_rounds": {
        "type": "int",
        "default": 15,
        "min": 1,
        "max": 200,
        "widget": "number",
        "label": "Max Tool Rounds"
    },
    "max_tool_rounds": {
        "type": "int",
        "default": 15,
        "min": 1,
        "max": 200,
        "widget": "number",
        "label": "Max Tool Rounds"
    },
    "chatbot.context_strategy": {
        "type": "string",
        "default": "auto",
        "widget": "select",
        "label": "Document Context Strategy",
        "helper": "How much document content to include in LLM context",
        "options": [
            {
                "value": "auto",
                "label": "Auto (by document size)"
            },
            {
                "value": "full",
                "label": "Full document text"
            },
            {
                "value": "page",
                "label": "Pages around cursor"
            },
            {
                "value": "tree",
                "label": "Outline + excerpt"
            },
            {
                "value": "stats",
                "label": "Stats + outline only"
            }
        ]
    },
    "context_strategy": {
        "type": "string",
        "default": "auto",
        "widget": "select",
        "label": "Document Context Strategy",
        "helper": "How much document content to include in LLM context",
        "options": [
            {
                "value": "auto",
                "label": "Auto (by document size)"
            },
            {
                "value": "full",
                "label": "Full document text"
            },
            {
                "value": "page",
                "label": "Pages around cursor"
            },
            {
                "value": "tree",
                "label": "Outline + excerpt"
            },
            {
                "value": "stats",
                "label": "Stats + outline only"
            }
        ]
    },
    "chatbot.extend_selection_max_tokens": {
        "type": "int",
        "default": 1000,
        "min": 10,
        "max": 4096,
        "internal": True,
        "label": "Extend max tokens"
    },
    "extend_selection_max_tokens": {
        "type": "int",
        "default": 1000,
        "min": 10,
        "max": 4096,
        "internal": True,
        "label": "Extend max tokens"
    },
    "chatbot.edit_selection_max_new_tokens": {
        "type": "int",
        "default": 1000,
        "min": 0,
        "max": 4096,
        "internal": True,
        "label": "Edit extra tokens",
        "helper": "Extra tokens beyond original text length. 0 = same length as original."
    },
    "edit_selection_max_new_tokens": {
        "type": "int",
        "default": 1000,
        "min": 0,
        "max": 4096,
        "internal": True,
        "label": "Edit extra tokens",
        "helper": "Extra tokens beyond original text length. 0 = same length as original."
    },
    "chatbot.web_research_cache_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Web Research Cache",
        "helper": "Cache completed web research reports by normalized query (uses web cache database).",
        "inline": True,
        "x": 8,
        "width": 200
    },
    "web_research_cache_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Web Research Cache",
        "helper": "Cache completed web research reports by normalized query (uses web cache database).",
        "inline": True,
        "x": 8,
        "width": 200
    },
    "chatbot.show_search_thinking": {
        "type": "boolean",
        "default": False,
        "internal": True
    },
    "show_search_thinking": {
        "type": "boolean",
        "default": False,
        "internal": True
    },
    "chatbot.prompt_for_web_research": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Prompt for Web Research",
        "helper": "Ask for confirmation before sending a web search query",
        "x": 220,
        "width": 200
    },
    "prompt_for_web_research": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Prompt for Web Research",
        "helper": "Ask for confirmation before sending a web search query",
        "x": 220,
        "width": 200
    },
    "chatbot.web_research_browser": {
        "type": "string",
        "default": "off",
        "widget": "select",
        "label": "Web Research Browser",
        "helper": "Select the browser backend to use for webpage visits during research (Off uses static HTTP requests).",
        "options": [
            {
                "value": "off",
                "label": "Off (Static HTTP)"
            },
            {
                "value": "firefox",
                "label": "Firefox (CDP)"
            },
            {
                "value": "chromium",
                "label": "Chromium (CDP)"
            },
            {
                "value": "chrome",
                "label": "Chrome (CDP)"
            }
        ]
    },
    "web_research_browser": {
        "type": "string",
        "default": "off",
        "widget": "select",
        "label": "Web Research Browser",
        "helper": "Select the browser backend to use for webpage visits during research (Off uses static HTTP requests).",
        "options": [
            {
                "value": "off",
                "label": "Off (Static HTTP)"
            },
            {
                "value": "firefox",
                "label": "Firefox (CDP)"
            },
            {
                "value": "chromium",
                "label": "Chromium (CDP)"
            },
            {
                "value": "chrome",
                "label": "Chrome (CDP)"
            }
        ]
    },
    "chatbot.web_cache_max_mb": {
        "type": "int",
        "default": 50,
        "min": 0,
        "max": 500,
        "widget": "number",
        "label": "Cache max (MB)",
        "helper": "Max disk size for web search cache (0 to disable)",
        "inline": True,
        "x": 110,
        "width": 100
    },
    "web_cache_max_mb": {
        "type": "int",
        "default": 50,
        "min": 0,
        "max": 500,
        "widget": "number",
        "label": "Cache max (MB)",
        "helper": "Max disk size for web search cache (0 to disable)",
        "inline": True,
        "x": 110,
        "width": 100
    },
    "chatbot.web_cache_validity_days": {
        "type": "int",
        "default": 30,
        "min": 1,
        "max": 30,
        "widget": "number",
        "label": "Cache validity (days)",
        "helper": "How many days cache entries should be considered valid.",
        "label_x": 220,
        "label_width": 100,
        "x": 322,
        "width": 100
    },
    "web_cache_validity_days": {
        "type": "int",
        "default": 30,
        "min": 1,
        "max": 30,
        "widget": "number",
        "label": "Cache validity (days)",
        "helper": "How many days cache entries should be considered valid.",
        "label_x": 220,
        "label_width": 100,
        "x": 322,
        "width": 100
    },
    "chatbot.web_research_cache_jaccard_percent": {
        "type": "int",
        "default": 60,
        "min": 0,
        "max": 100,
        "widget": "number",
        "label": "Research Cache Fuzzy Match (%)",
        "helper": "Minimum similarity (0-100) for a fuzzy cache hit. JSON only (internal); default 60.",
        "internal": True
    },
    "web_research_cache_jaccard_percent": {
        "type": "int",
        "default": 60,
        "min": 0,
        "max": 100,
        "widget": "number",
        "label": "Research Cache Fuzzy Match (%)",
        "helper": "Minimum similarity (0-100) for a fuzzy cache hit. JSON only (internal); default 60.",
        "internal": True
    },
    "chatbot.web_research_cache_embedding_percent": {
        "type": "int",
        "default": 75,
        "min": 0,
        "max": 100,
        "widget": "number",
        "label": "Research Cache Embedding Match (%)",
        "helper": "Minimum cosine similarity (0-100) for an embedding cache hit. JSON only (internal); default 75.",
        "internal": True
    },
    "web_research_cache_embedding_percent": {
        "type": "int",
        "default": 75,
        "min": 0,
        "max": 100,
        "widget": "number",
        "label": "Research Cache Embedding Match (%)",
        "helper": "Minimum cosine similarity (0-100) for an embedding cache hit. JSON only (internal); default 75.",
        "internal": True
    },
    "chatbot.web_research_cache_min_overlap": {
        "type": "int",
        "default": 8,
        "min": 0,
        "max": 50,
        "widget": "number",
        "label": "Research Cache Min Stem Overlap",
        "helper": "Minimum shared stem count for a fuzzy cache hit (0 disables). JSON only (internal); default 8.",
        "internal": True
    },
    "web_research_cache_min_overlap": {
        "type": "int",
        "default": 8,
        "min": 0,
        "max": 50,
        "widget": "number",
        "label": "Research Cache Min Stem Overlap",
        "helper": "Minimum shared stem count for a fuzzy cache hit (0 disables). JSON only (internal); default 8.",
        "internal": True
    },
    "chatbot.deep_research_breadth": {
        "type": "int",
        "default": 4,
        "min": 1,
        "max": 10,
        "widget": "number",
        "label": "Deep Research Breadth",
        "helper": "Sub-queries per depth level when Deep Research sidebar mode is used (internal).",
        "internal": True
    },
    "deep_research_breadth": {
        "type": "int",
        "default": 4,
        "min": 1,
        "max": 10,
        "widget": "number",
        "label": "Deep Research Breadth",
        "helper": "Sub-queries per depth level when Deep Research sidebar mode is used (internal).",
        "internal": True
    },
    "chatbot.deep_research_depth": {
        "type": "int",
        "default": 2,
        "min": 1,
        "max": 4,
        "widget": "number",
        "label": "Deep Research Depth",
        "helper": "Legacy alias for max_rounds when deep_research_max_rounds is 0 (internal).",
        "internal": True
    },
    "deep_research_depth": {
        "type": "int",
        "default": 2,
        "min": 1,
        "max": 4,
        "widget": "number",
        "label": "Deep Research Depth",
        "helper": "Legacy alias for max_rounds when deep_research_max_rounds is 0 (internal).",
        "internal": True
    },
    "chatbot.deep_research_concurrency": {
        "type": "int",
        "default": 2,
        "min": 1,
        "max": 5,
        "widget": "number",
        "label": "Deep Research Concurrency",
        "helper": "Parallel sub-query workers in Deep Research sidebar mode (internal).",
        "internal": True
    },
    "deep_research_concurrency": {
        "type": "int",
        "default": 2,
        "min": 1,
        "max": 5,
        "widget": "number",
        "label": "Deep Research Concurrency",
        "helper": "Parallel sub-query workers in Deep Research sidebar mode (internal).",
        "internal": True
    },
    "chatbot.deep_research_max_sub_queries": {
        "type": "int",
        "default": 14,
        "min": 1,
        "max": 40,
        "widget": "number",
        "label": "Deep Research Max Sub-Queries",
        "helper": "Global cap on shallow sub-agent runs per deep research session (internal).",
        "internal": True
    },
    "deep_research_max_sub_queries": {
        "type": "int",
        "default": 14,
        "min": 1,
        "max": 40,
        "widget": "number",
        "label": "Deep Research Max Sub-Queries",
        "helper": "Global cap on shallow sub-agent runs per deep research session (internal).",
        "internal": True
    },
    "chatbot.deep_research_max_rounds": {
        "type": "int",
        "default": 3,
        "min": 1,
        "max": 6,
        "widget": "number",
        "label": "Deep Research Max Rounds",
        "helper": "Adaptive research rounds; deep_research_depth JSON override maps here if max_rounds unset (internal).",
        "internal": True
    },
    "deep_research_max_rounds": {
        "type": "int",
        "default": 3,
        "min": 1,
        "max": 6,
        "widget": "number",
        "label": "Deep Research Max Rounds",
        "helper": "Adaptive research rounds; deep_research_depth JSON override maps here if max_rounds unset (internal).",
        "internal": True
    },
    "chatbot.deep_research_quality_threshold": {
        "type": "int",
        "default": 7,
        "min": 1,
        "max": 10,
        "widget": "number",
        "label": "Deep Research Quality Threshold",
        "helper": "Stop when LLM coverage score reaches this value (1-10, internal).",
        "internal": True
    },
    "deep_research_quality_threshold": {
        "type": "int",
        "default": 7,
        "min": 1,
        "max": 10,
        "widget": "number",
        "label": "Deep Research Quality Threshold",
        "helper": "Stop when LLM coverage score reaches this value (1-10, internal).",
        "internal": True
    },
    "chatbot.deep_research_sub_agent_steps": {
        "type": "int",
        "default": 0,
        "min": 0,
        "max": 50,
        "widget": "number",
        "label": "Deep Research Sub-Agent Steps",
        "helper": "Max ReAct steps per sub-query (0 = 150% of chatbot.max_tool_rounds, internal).",
        "internal": True
    },
    "deep_research_sub_agent_steps": {
        "type": "int",
        "default": 0,
        "min": 0,
        "max": 50,
        "widget": "number",
        "label": "Deep Research Sub-Agent Steps",
        "helper": "Max ReAct steps per sub-query (0 = 150% of chatbot.max_tool_rounds, internal).",
        "internal": True
    },
    "chatbot.rich_text_control_sidebar": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Rich Text Control Sidebar",
        "width": 200,
        "helper": "Formatted chat via RichTextControl. Requires restart."
    },
    "rich_text_control_sidebar": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Rich Text Control Sidebar",
        "width": 200,
        "helper": "Formatted chat via RichTextControl. Requires restart."
    },
    "chatbot.librarian_invoked": {
        "type": "boolean",
        "default": False,
        "internal": True
    },
    "librarian_invoked": {
        "type": "boolean",
        "default": False,
        "internal": True
    },
    "chatbot.humanizer_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Humanizer skill (natural prose)",
        "width": 240,
        "helper": "When enabled, injects guidance into the system prompt so the model makes generated or revised document text sound more natural and human (removes AI slop patterns). Edit the rules in your LibreOffice profile under writeragent/skills/humanizer/SKILL.md (the file is auto-created on first use). A dedicated Skills tab can be added later."
    },
    "humanizer_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Humanizer skill (natural prose)",
        "width": 240,
        "helper": "When enabled, injects guidance into the system prompt so the model makes generated or revised document text sound more natural and human (removes AI slop patterns). Edit the rules in your LibreOffice profile under writeragent/skills/humanizer/SKILL.md (the file is auto-created on first use). A dedicated Skills tab can be added later."
    },
    "chatbot.audio_silence_stop_ms": {
        "type": "int",
        "default": 3000,
        "min": 0,
        "max": 15000,
        "widget": "number",
        "label": "Silence before send (ms)",
        "helper": "Pause after you stop talking, then auto-stop and send (Record). 0 = wait until you click Stop Rec."
    },
    "audio_silence_stop_ms": {
        "type": "int",
        "default": 3000,
        "min": 0,
        "max": 15000,
        "widget": "number",
        "label": "Silence before send (ms)",
        "helper": "Pause after you stop talking, then auto-stop and send (Record). 0 = wait until you click Stop Rec."
    },
    "chatbot.query_history": {
        "type": "string",
        "default": "[]",
        "internal": True
    },
    "query_history": {
        "type": "string",
        "default": "[]",
        "internal": True
    },
    "doc.grammar_proofreader_enabled": {
        "type": "string",
        "default": "off",
        "widget": "select",
        "label": "Enable grammar checker (Writer)",
        "helper": "Off = disabled. AI (LLM) = use your configured text model/API. LanguageTool / Vale = local engines via Settings â†’ Python venv. Harper = offline Rust binary auto-downloaded to your profile (no venv required).",
        "options": [
            {
                "value": "off",
                "label": "Off"
            },
            {
                "value": "llm",
                "label": "AI (LLM)"
            },
            {
                "value": "harper",
                "label": "Harper"
            },
            {
                "value": "languagetool",
                "label": "LanguageTool (Local)"
            },
            {
                "value": "vale",
                "label": "Vale (Local Style) (WIP)"
            }
        ],
        "inline": True,
        "label_x": 8,
        "label_width": 100,
        "x": 110,
        "width": 100
    },
    "grammar_proofreader_enabled": {
        "type": "string",
        "default": "off",
        "widget": "select",
        "label": "Enable grammar checker (Writer)",
        "helper": "Off = disabled. AI (LLM) = use your configured text model/API. LanguageTool / Vale = local engines via Settings â†’ Python venv. Harper = offline Rust binary auto-downloaded to your profile (no venv required).",
        "options": [
            {
                "value": "off",
                "label": "Off"
            },
            {
                "value": "llm",
                "label": "AI (LLM)"
            },
            {
                "value": "harper",
                "label": "Harper"
            },
            {
                "value": "languagetool",
                "label": "LanguageTool (Local)"
            },
            {
                "value": "vale",
                "label": "Vale (Local Style) (WIP)"
            }
        ],
        "inline": True,
        "label_x": 8,
        "label_width": 100,
        "x": 110,
        "width": 100
    },
    "doc.grammar_proofreader_model": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "Model (optional)",
        "helper": "Leave empty to use the same text model as chat.",
        "label_x": 220,
        "label_width": 95,
        "x": 318,
        "width": 106
    },
    "grammar_proofreader_model": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "Model (optional)",
        "helper": "Leave empty to use the same text model as chat.",
        "label_x": 220,
        "label_width": 95,
        "x": 318,
        "width": 106
    },
    "doc.grammar_proofreader_recheck": {
        "type": "string",
        "default": "",
        "widget": "button",
        "label": "Recheck this document",
        "button_text": "Recheck",
        "helper": "Clears cached grammar results for this document and asks Writer to check again. Use after changing checker or model.",
        "show_button_label": True,
        "settings_persist": False,
        "inline": True,
        "label_width": 100,
        "x": 110,
        "width": 70
    },
    "grammar_proofreader_recheck": {
        "type": "string",
        "default": "",
        "widget": "button",
        "label": "Recheck this document",
        "button_text": "Recheck",
        "helper": "Clears cached grammar results for this document and asks Writer to check again. Use after changing checker or model.",
        "show_button_label": True,
        "settings_persist": False,
        "inline": True,
        "label_width": 100,
        "x": 110,
        "width": 70
    },
    "doc.grammar_proofreader_pause_during_agent": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Pause grammar while chat/agent runs",
        "helper": "When on, AI grammar skips background requests while sidebar chat or agent runs, to avoid concurrent API calls.",
        "x": 220,
        "width": 204
    },
    "grammar_proofreader_pause_during_agent": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Pause grammar while chat/agent runs",
        "helper": "When on, AI grammar skips background requests while sidebar chat or agent runs, to avoid concurrent API calls.",
        "x": 220,
        "width": 204
    },
    "doc.grammar_proofreader_batch_sentences": {
        "type": "int",
        "default": 1,
        "min": 1,
        "max": 8,
        "widget": "number",
        "label": "Batch sentences",
        "helper": "Number of sentences to send per LLM request (1-8). 1 (default) is most reliable; higher values are faster and cheaper but may hit token limits or cause model errors.",
        "inline": True,
        "x": 110,
        "width": 100
    },
    "grammar_proofreader_batch_sentences": {
        "type": "int",
        "default": 1,
        "min": 1,
        "max": 8,
        "widget": "number",
        "label": "Batch sentences",
        "helper": "Number of sentences to send per LLM request (1-8). 1 (default) is most reliable; higher values are faster and cheaper but may hit token limits or cause model errors.",
        "inline": True,
        "x": 110,
        "width": 100
    },
    "doc.grammar_proofreader_max_in_flight": {
        "type": "int",
        "default": 1,
        "min": 1,
        "max": 8,
        "widget": "number",
        "label": "Concurrent requests",
        "helper": "Max simultaneous background grammar API calls (1-8). 1 matches prior behavior. Raise for OpenRouter or other providers that allow parallel requests; use with care alongside sidebar chat.",
        "label_x": 220,
        "label_width": 100,
        "x": 322,
        "width": 100
    },
    "grammar_proofreader_max_in_flight": {
        "type": "int",
        "default": 1,
        "min": 1,
        "max": 8,
        "widget": "number",
        "label": "Concurrent requests",
        "helper": "Max simultaneous background grammar API calls (1-8). 1 matches prior behavior. Raise for OpenRouter or other providers that allow parallel requests; use with care alongside sidebar chat.",
        "label_x": 220,
        "label_width": 100,
        "x": 322,
        "width": 100
    },
    "doc.grammar_proofreader_detect_language": {
        "type": "string",
        "default": "off",
        "widget": "select",
        "label": "Sentence language detection",
        "helper": "Verify each complete sentence's language against the document locale. AI uses your grammar/chat API; Local uses langdetect in your Python venv (Settings â†’ Python; no API call). Mismatches update CharLocale and re-check grammar.",
        "options": [
            {
                "value": "off",
                "label": "Off"
            },
            {
                "value": "llm",
                "label": "AI (LLM)"
            },
            {
                "value": "langdetect",
                "label": "Local (langdetect)"
            }
        ]
    },
    "grammar_proofreader_detect_language": {
        "type": "string",
        "default": "off",
        "widget": "select",
        "label": "Sentence language detection",
        "helper": "Verify each complete sentence's language against the document locale. AI uses your grammar/chat API; Local uses langdetect in your Python venv (Settings â†’ Python; no API call). Mismatches update CharLocale and re-check grammar.",
        "options": [
            {
                "value": "off",
                "label": "Off"
            },
            {
                "value": "llm",
                "label": "AI (LLM)"
            },
            {
                "value": "langdetect",
                "label": "Local (langdetect)"
            }
        ]
    },
    "doc.chat_enter_key_sends_message": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Enter key sends sidebar chat message",
        "width": 300,
        "helper": "When on, Enter in the sidebar query runs the same action as Send; Shift+Enter inserts a newline. When off, Enter inserts a newline."
    },
    "chat_enter_key_sends_message": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "label": "Enter key sends sidebar chat message",
        "width": 300,
        "helper": "When on, Enter in the sidebar query runs the same action as Send; Shift+Enter inserts a newline. When off, Enter inserts a newline."
    },
    "doc.agent_edit_review_mode": {
        "type": "string",
        "default": "off",
        "widget": "select",
        "label": "Agent edit review",
        "helper": "Off = direct edits. Record = tracked changes you accept/reject yourself. Wait = apply_document_content blocks (on chat/MCP worker thread) until you review.",
        "options": [
            {
                "value": "off",
                "label": "Off"
            },
            {
                "value": "record",
                "label": "Record (track changes)"
            },
            {
                "value": "wait",
                "label": "Wait for review"
            }
        ]
    },
    "agent_edit_review_mode": {
        "type": "string",
        "default": "off",
        "widget": "select",
        "label": "Agent edit review",
        "helper": "Off = direct edits. Record = tracked changes you accept/reject yourself. Wait = apply_document_content blocks (on chat/MCP worker thread) until you review.",
        "options": [
            {
                "value": "off",
                "label": "Off"
            },
            {
                "value": "record",
                "label": "Record (track changes)"
            },
            {
                "value": "wait",
                "label": "Wait for review"
            }
        ]
    },
    "doc.edit_review_timeout": {
        "type": "int",
        "default": 900,
        "min": 0,
        "widget": "number",
        "internal": True,
        "label": "Edit review max wait (seconds)",
        "helper": "Only used when mode is Wait. Max time apply_document_content blocks before returning with pending changes."
    },
    "edit_review_timeout": {
        "type": "int",
        "default": 900,
        "min": 0,
        "widget": "number",
        "internal": True,
        "label": "Edit review max wait (seconds)",
        "helper": "Only used when mode is Wait. Max time apply_document_content blocks before returning with pending changes."
    },
    "embeddings.folder_search_mode": {
        "type": "string",
        "default": "none",
        "widget": "select",
        "label": "Cross-file search",
        "helper": "Indexed semantic + keyword search for document_research (corpus.db). Requires embeddings venv â€” see Python Test.",
        "options": [
            {
                "value": "none",
                "label": "Off"
            },
            {
                "value": "hybrid",
                "label": "Embeddings + FTS"
            },
            {
                "value": "llama_index",
                "label": "LlamaIndex"
            },
            {
                "value": "zvec",
                "label": "Zvec (experimental)"
            },
            {
                "value": "lancedb",
                "label": "LanceDB (experimental)"
            }
        ]
    },
    "folder_search_mode": {
        "type": "string",
        "default": "none",
        "widget": "select",
        "label": "Cross-file search",
        "helper": "Indexed semantic + keyword search for document_research (corpus.db). Requires embeddings venv â€” see Python Test.",
        "options": [
            {
                "value": "none",
                "label": "Off"
            },
            {
                "value": "hybrid",
                "label": "Embeddings + FTS"
            },
            {
                "value": "llama_index",
                "label": "LlamaIndex"
            },
            {
                "value": "zvec",
                "label": "Zvec (experimental)"
            },
            {
                "value": "lancedb",
                "label": "LanceDB (experimental)"
            }
        ]
    },
    "embeddings.embedding_model": {
        "type": "string",
        "default": "paraphrase-multilingual-MiniLM-L12-v2",
        "widget": "text",
        "label": "Embedding Model",
        "helper": "HuggingFace model ID for local provider, or endpoint-specific model ID"
    },
    "embedding_model": {
        "type": "string",
        "default": "paraphrase-multilingual-MiniLM-L12-v2",
        "widget": "text",
        "label": "Embedding Model",
        "helper": "HuggingFace model ID for local provider, or endpoint-specific model ID"
    },
    "embeddings.folder_rerank_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Enable cross-file rerank",
        "width": 200,
        "helper": "Second-stage cross-encoder after hybrid retrieve (Embeddings + FTS and LlamaIndex). Off by default."
    },
    "folder_rerank_enabled": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "label": "Enable cross-file rerank",
        "width": 200,
        "helper": "Second-stage cross-encoder after hybrid retrieve (Embeddings + FTS and LlamaIndex). Off by default."
    },
    "embeddings.folder_rerank_model": {
        "type": "string",
        "default": "cross-encoder/ms-marco-MiniLM-L-6-v2",
        "widget": "select",
        "label": "Rerank model",
        "helper": "Used only when rerank is enabled. English MiniLM is fast; bge-reranker-v2-m3 is multilingual and downloads ~2.3 GB.",
        "options": [
            {
                "value": "cross-encoder/ms-marco-MiniLM-L-6-v2",
                "label": "cross-encoder/ms-marco-MiniLM-L-6-v2"
            },
            {
                "value": "BAAI/bge-reranker-v2-m3",
                "label": "BAAI/bge-reranker-v2-m3"
            }
        ]
    },
    "folder_rerank_model": {
        "type": "string",
        "default": "cross-encoder/ms-marco-MiniLM-L-6-v2",
        "widget": "select",
        "label": "Rerank model",
        "helper": "Used only when rerank is enabled. English MiniLM is fast; bge-reranker-v2-m3 is multilingual and downloads ~2.3 GB.",
        "options": [
            {
                "value": "cross-encoder/ms-marco-MiniLM-L-6-v2",
                "label": "cross-encoder/ms-marco-MiniLM-L-6-v2"
            },
            {
                "value": "BAAI/bge-reranker-v2-m3",
                "label": "BAAI/bge-reranker-v2-m3"
            }
        ]
    },
    "scripting.python_venv_path": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "Python venv path",
        "helper": "If set, runs =PY() and Run Python Script in that venv (bin/python, Scripts/python.exe, or conda/pyenv-win env-root python.exe). Paste the path without surrounding quotes. If empty, uses this process's sys.executable (basic stdlib); use a venv for numpy/pandas and Vision Helpers (paddleocr, paddlepaddle).",
        "width": 200,
        "inline": True
    },
    "python_venv_path": {
        "type": "string",
        "default": "",
        "widget": "text",
        "label": "Python venv path",
        "helper": "If set, runs =PY() and Run Python Script in that venv (bin/python, Scripts/python.exe, or conda/pyenv-win env-root python.exe). Paste the path without surrounding quotes. If empty, uses this process's sys.executable (basic stdlib); use a venv for numpy/pandas and Vision Helpers (paddleocr, paddlepaddle).",
        "width": 200,
        "inline": True
    },
    "scripting.test_venv": {
        "type": "string",
        "default": "",
        "widget": "button",
        "label": "Test",
        "inline_no_label": True,
        "settings_persist": False,
        "x": 314,
        "width": 48
    },
    "test_venv": {
        "type": "string",
        "default": "",
        "widget": "button",
        "label": "Test",
        "inline_no_label": True,
        "settings_persist": False,
        "x": 314,
        "width": 48
    },
    "scripting.download_audio_binaries": {
        "type": "string",
        "default": "",
        "widget": "button",
        "label": "Download platform-specific audio and serialization binaries:",
        "button_text": "Download",
        "show_button_label": True,
        "label_width": 280,
        "settings_persist": False,
        "x": 297,
        "width": 65
    },
    "download_audio_binaries": {
        "type": "string",
        "default": "",
        "widget": "button",
        "label": "Download platform-specific audio and serialization binaries:",
        "button_text": "Download",
        "show_button_label": True,
        "label_width": 280,
        "settings_persist": False,
        "x": 297,
        "width": 65
    },
    "scripting.python_exec_timeout": {
        "type": "int",
        "default": 10,
        "min": 1,
        "max": 600,
        "widget": "number",
        "label": "Python script timeout (seconds)",
        "helper": "Wall-clock limit for Run Python Script and =PYTHON() / =PY()."
    },
    "python_exec_timeout": {
        "type": "int",
        "default": 10,
        "min": 1,
        "max": 600,
        "widget": "number",
        "label": "Python script timeout (seconds)",
        "helper": "Wall-clock limit for Run Python Script and =PYTHON() / =PY()."
    },
    "scripting.python_max_data_cells": {
        "type": "int",
        "default": 250000,
        "min": 1000,
        "max": 2000000,
        "widget": "number",
        "label": "Max Calc data cells",
        "helper": "Maximum cells in =PYTHON() data or Run Python Script data/data_range. Larger values use more RAM and time; UNO range read still dominates.",
        "public": True
    },
    "python_max_data_cells": {
        "type": "int",
        "default": 250000,
        "min": 1000,
        "max": 2000000,
        "widget": "number",
        "label": "Max Calc data cells",
        "helper": "Maximum cells in =PYTHON() data or Run Python Script data/data_range. Larger values use more RAM and time; UNO range read still dominates.",
        "public": True
    },
    "scripting.python_session_mode": {
        "type": "string",
        "default": "isolated",
        "widget": "select",
        "label": "Python session mode",
        "helper": "Isolated: each =PYTHON() cell gets a fresh namespace. Shared: one namespace per workbook (variables carry between cells).",
        "public": True,
        "options": [
            {
                "value": "isolated",
                "label": "Isolated (default)"
            },
            {
                "value": "shared",
                "label": "Shared kernel"
            }
        ]
    },
    "python_session_mode": {
        "type": "string",
        "default": "isolated",
        "widget": "select",
        "label": "Python session mode",
        "helper": "Isolated: each =PYTHON() cell gets a fresh namespace. Shared: one namespace per workbook (variables carry between cells).",
        "public": True,
        "options": [
            {
                "value": "isolated",
                "label": "Isolated (default)"
            },
            {
                "value": "shared",
                "label": "Shared kernel"
            }
        ]
    },
    "scripting.python_geometric_recalc_order": {
        "type": "bool",
        "default": False,
        "widget": "checkbox",
        "label": "Geometric Recalc Order (Experimental)",
        "helper": "Ensures PY cells evaluate in sheet order. Most useful with Shared kernel.",
        "public": True,
        "width": 250
    },
    "python_geometric_recalc_order": {
        "type": "bool",
        "default": False,
        "widget": "checkbox",
        "label": "Geometric Recalc Order (Experimental)",
        "helper": "Ensures PY cells evaluate in sheet order. Most useful with Shared kernel.",
        "public": True,
        "width": 250
    },
    "scripting.python_auto_spill": {
        "type": "bool",
        "default": True,
        "widget": "checkbox",
        "label": "Python auto spill in Calc",
        "helper": "Automatically spill multi-cell array/DataFrame results from =PYTHON() into adjacent cells.",
        "public": True,
        "inline": True,
        "width": 200
    },
    "python_auto_spill": {
        "type": "bool",
        "default": True,
        "widget": "checkbox",
        "label": "Python auto spill in Calc",
        "helper": "Automatically spill multi-cell array/DataFrame results from =PYTHON() into adjacent cells.",
        "public": True,
        "inline": True,
        "width": 200
    },
    "scripting.xl_static_rewrite": {
        "type": "bool",
        "default": False,
        "widget": "checkbox",
        "label": "Rewrite xl() ranges",
        "helper": "On Edit Python in Cell save, lift static xl(\"A1:â€¦\") onto =PY data args and rewrite to data / data[i] / .to_pandas().",
        "public": True,
        "x": 220,
        "width": 200
    },
    "xl_static_rewrite": {
        "type": "bool",
        "default": False,
        "widget": "checkbox",
        "label": "Rewrite xl() ranges",
        "helper": "On Edit Python in Cell save, lift static xl(\"A1:â€¦\") onto =PY data args and rewrite to data / data[i] / .to_pandas().",
        "public": True,
        "x": 220,
        "width": 200
    },
    "scripting.force_internal_script_editor": {
        "type": "bool",
        "default": False,
        "internal": True
    },
    "force_internal_script_editor": {
        "type": "bool",
        "default": False,
        "internal": True
    },
    "scripting.native_run_script_modeless": {
        "type": "bool",
        "default": True,
        "internal": True
    },
    "native_run_script_modeless": {
        "type": "bool",
        "default": True,
        "internal": True
    },
    "vision.images_scale": {
        "type": "float",
        "default": 1.0,
        "min": 0.5,
        "max": 3.0,
        "widget": "number",
        "page": "general",
        "label": "Image scale",
        "helper": "Upscale before OCR/layout. Try 2.0 for small screenshots."
    },
    "images_scale": {
        "type": "float",
        "default": 1.0,
        "min": 0.5,
        "max": 3.0,
        "widget": "number",
        "page": "general",
        "label": "Image scale",
        "helper": "Upscale before OCR/layout. Try 2.0 for small screenshots."
    },
    "vision.worker_timeout_sec": {
        "type": "int",
        "default": 300,
        "min": 30,
        "max": 600,
        "widget": "number",
        "page": "general",
        "label": "Worker timeout (seconds)",
        "helper": "Host subprocess limit for Vision Helpers (first model download)."
    },
    "worker_timeout_sec": {
        "type": "int",
        "default": 300,
        "min": 30,
        "max": 600,
        "widget": "number",
        "page": "general",
        "label": "Worker timeout (seconds)",
        "helper": "Host subprocess limit for Vision Helpers (first model download)."
    },
    "vision.artifacts_path": {
        "type": "string",
        "default": "",
        "widget": "text",
        "page": "general",
        "label": "Docling artifacts path",
        "helper": "Optional folder from docling-tools models download (offline use)."
    },
    "artifacts_path": {
        "type": "string",
        "default": "",
        "widget": "text",
        "page": "general",
        "label": "Docling artifacts path",
        "helper": "Optional folder from docling-tools models download (offline use)."
    },
    "vision.text_score": {
        "type": "float",
        "default": 0.5,
        "min": 0.0,
        "max": 1.0,
        "widget": "number",
        "page": "ocr",
        "label": "OCR confidence threshold",
        "helper": "RapidOCR minimum text score. 0.0 is a real threshold (not the default). Raise to reduce garbage text."
    },
    "text_score": {
        "type": "float",
        "default": 0.5,
        "min": 0.0,
        "max": 1.0,
        "widget": "number",
        "page": "ocr",
        "label": "OCR confidence threshold",
        "helper": "RapidOCR minimum text score. 0.0 is a real threshold (not the default). Raise to reduce garbage text."
    },
    "vision.force_full_page_ocr": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "page": "ocr",
        "label": "OCR entire image",
        "helper": "Recommended for embedded graphics exported from LibreOffice."
    },
    "force_full_page_ocr": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "page": "ocr",
        "label": "OCR entire image",
        "helper": "Recommended for embedded graphics exported from LibreOffice."
    },
    "vision.table_mode": {
        "type": "string",
        "default": "accurate",
        "widget": "select",
        "page": "tables",
        "label": "Table extraction mode",
        "options": [
            {
                "value": "accurate",
                "label": "Accurate (default)"
            },
            {
                "value": "fast",
                "label": "Fast"
            }
        ]
    },
    "table_mode": {
        "type": "string",
        "default": "accurate",
        "widget": "select",
        "page": "tables",
        "label": "Table extraction mode",
        "options": [
            {
                "value": "accurate",
                "label": "Accurate (default)"
            },
            {
                "value": "fast",
                "label": "Fast"
            }
        ]
    },
    "vision.do_cell_matching": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "page": "tables",
        "label": "Table cell matching",
        "helper": "Align detected table cells with content (extract_structure)."
    },
    "do_cell_matching": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "page": "tables",
        "label": "Table cell matching",
        "helper": "Align detected table cells with content (extract_structure)."
    },
    "vision.create_orphan_clusters": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "page": "tables",
        "label": "Keep orphan text clusters",
        "helper": "Group unassigned text into clusters for messy layouts."
    },
    "create_orphan_clusters": {
        "type": "boolean",
        "default": True,
        "widget": "checkbox",
        "page": "tables",
        "label": "Keep orphan text clusters",
        "helper": "Group unassigned text into clusters for messy layouts."
    },
    "vision.insert_mode": {
        "type": "string",
        "default": "html",
        "widget": "select",
        "page": "tables",
        "label": "Document insert mode",
        "helper": "html = Docling HTML import (default). structured = bbox column layout in Writer; native Calc cells for extract_structure tables.",
        "options": [
            {
                "value": "html",
                "label": "Standard HTML"
            },
            {
                "value": "structured",
                "label": "Structured (layout / cell grid)"
            }
        ]
    },
    "insert_mode": {
        "type": "string",
        "default": "html",
        "widget": "select",
        "page": "tables",
        "label": "Document insert mode",
        "helper": "html = Docling HTML import (default). structured = bbox column layout in Writer; native Calc cells for extract_structure tables.",
        "options": [
            {
                "value": "html",
                "label": "Standard HTML"
            },
            {
                "value": "structured",
                "label": "Structured (layout / cell grid)"
            }
        ]
    },
    "vision.layout_model": {
        "type": "string",
        "default": "heron",
        "widget": "select",
        "page": "advanced",
        "label": "Layout model",
        "options": [
            {
                "value": "heron",
                "label": "Heron (default)"
            },
            {
                "value": "egret_large",
                "label": "Egret large (slower, complex docs)"
            }
        ]
    },
    "layout_model": {
        "type": "string",
        "default": "heron",
        "widget": "select",
        "page": "advanced",
        "label": "Layout model",
        "options": [
            {
                "value": "heron",
                "label": "Heron (default)"
            },
            {
                "value": "egret_large",
                "label": "Egret large (slower, complex docs)"
            }
        ]
    },
    "vision.do_formula_enrichment": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "page": "advanced",
        "label": "Extract formulas (LaTeX)",
        "helper": "Opt-in. Downloads extra VLM models on first use; slower."
    },
    "do_formula_enrichment": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "page": "advanced",
        "label": "Extract formulas (LaTeX)",
        "helper": "Opt-in. Downloads extra VLM models on first use; slower."
    },
    "vision.do_code_enrichment": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "page": "advanced",
        "label": "Code block enrichment",
        "helper": "Specialized OCR for code/terminal screenshots."
    },
    "do_code_enrichment": {
        "type": "boolean",
        "default": False,
        "widget": "checkbox",
        "page": "advanced",
        "label": "Code block enrichment",
        "helper": "Specialized OCR for code/terminal screenshots."
    },
    "vision.document_timeout": {
        "type": "float",
        "default": 0,
        "min": 0,
        "max": 600,
        "widget": "number",
        "page": "advanced",
        "label": "Docling document timeout (s)",
        "helper": "Per-conversion timeout inside Docling. 0 = no limit."
    },
    "document_timeout": {
        "type": "float",
        "default": 0,
        "min": 0,
        "max": 600,
        "widget": "number",
        "page": "advanced",
        "label": "Docling document timeout (s)",
        "helper": "Per-conversion timeout inside Docling. 0 = no limit."
    }
}

DOTTED_FALLBACKS = {
    "log_level": [
        "core.log_level"
    ],
    "backend_id": [
        "agent_backend.backend_id"
    ],
    "path": [
        "agent_backend.path"
    ],
    "args": [
        "agent_backend.args"
    ],
    "acp_agent_name": [
        "agent_backend.acp_agent_name"
    ],
    "prompt_for_permission": [
        "agent_backend.prompt_for_permission"
    ],
    "stt_provider": [
        "audio.stt_provider"
    ],
    "stt_model": [
        "audio.stt_model"
    ],
    "stt_local_model": [
        "audio.stt_local_model"
    ],
    "tts_enabled": [
        "audio.tts_enabled"
    ],
    "tts_sentence_mode": [
        "audio.tts_sentence_mode"
    ],
    "tts_short_answers": [
        "audio.tts_short_answers"
    ],
    "tts_provider": [
        "audio.tts_provider"
    ],
    "tts_model": [
        "audio.tts_model"
    ],
    "tts_voice": [
        "audio.tts_voice"
    ],
    "test_voice": [
        "audio.test_voice"
    ],
    "tts_speed": [
        "audio.tts_speed"
    ],
    "tts_voice_kokoro": [
        "audio.tts_voice_kokoro"
    ],
    "tts_voice_piper": [
        "audio.tts_voice_piper"
    ],
    "tts_voice_openai": [
        "audio.tts_voice_openai"
    ],
    "tts_voice_openrouter": [
        "audio.tts_voice_openrouter"
    ],
    "tts_voice_together": [
        "audio.tts_voice_together"
    ],
    "tts_voice_system": [
        "audio.tts_voice_system"
    ],
    "max_rows_display": [
        "calc.max_rows_display"
    ],
    "ods_cache_enabled": [
        "calc.ods_cache_enabled"
    ],
    "mcp_enabled": [
        "mcp.mcp_enabled"
    ],
    "mcp_port": [
        "mcp.mcp_port"
    ],
    "tool_exposure_mode": [
        "mcp.tool_exposure_mode"
    ],
    "_sep_tunnel": [
        "mcp._sep_tunnel"
    ],
    "tunnel_enabled": [
        "mcp.tunnel_enabled"
    ],
    "test_tunnel": [
        "mcp.test_tunnel"
    ],
    "tunnel_provider": [
        "mcp.tunnel_provider"
    ],
    "tunnel_provider_token": [
        "mcp.tunnel_provider_token"
    ],
    "client_config_snippet": [
        "mcp.client_config_snippet"
    ],
    "copy_config": [
        "mcp.copy_config"
    ],
    "cors_allow_private_origins": [
        "mcp.cors_allow_private_origins"
    ],
    "cors_allowed_origins": [
        "mcp.cors_allowed_origins"
    ],
    "max_tool_rounds": [
        "chatbot.max_tool_rounds"
    ],
    "context_strategy": [
        "chatbot.context_strategy"
    ],
    "extend_selection_max_tokens": [
        "chatbot.extend_selection_max_tokens"
    ],
    "edit_selection_max_new_tokens": [
        "chatbot.edit_selection_max_new_tokens"
    ],
    "web_research_cache_enabled": [
        "chatbot.web_research_cache_enabled"
    ],
    "show_search_thinking": [
        "chatbot.show_search_thinking"
    ],
    "prompt_for_web_research": [
        "chatbot.prompt_for_web_research"
    ],
    "web_research_browser": [
        "chatbot.web_research_browser"
    ],
    "web_cache_max_mb": [
        "chatbot.web_cache_max_mb"
    ],
    "web_cache_validity_days": [
        "chatbot.web_cache_validity_days"
    ],
    "web_research_cache_jaccard_percent": [
        "chatbot.web_research_cache_jaccard_percent"
    ],
    "web_research_cache_embedding_percent": [
        "chatbot.web_research_cache_embedding_percent"
    ],
    "web_research_cache_min_overlap": [
        "chatbot.web_research_cache_min_overlap"
    ],
    "deep_research_breadth": [
        "chatbot.deep_research_breadth"
    ],
    "deep_research_depth": [
        "chatbot.deep_research_depth"
    ],
    "deep_research_concurrency": [
        "chatbot.deep_research_concurrency"
    ],
    "deep_research_max_sub_queries": [
        "chatbot.deep_research_max_sub_queries"
    ],
    "deep_research_max_rounds": [
        "chatbot.deep_research_max_rounds"
    ],
    "deep_research_quality_threshold": [
        "chatbot.deep_research_quality_threshold"
    ],
    "deep_research_sub_agent_steps": [
        "chatbot.deep_research_sub_agent_steps"
    ],
    "rich_text_control_sidebar": [
        "chatbot.rich_text_control_sidebar"
    ],
    "librarian_invoked": [
        "chatbot.librarian_invoked"
    ],
    "humanizer_enabled": [
        "chatbot.humanizer_enabled"
    ],
    "audio_silence_stop_ms": [
        "chatbot.audio_silence_stop_ms"
    ],
    "query_history": [
        "chatbot.query_history"
    ],
    "grammar_proofreader_enabled": [
        "doc.grammar_proofreader_enabled"
    ],
    "grammar_proofreader_model": [
        "doc.grammar_proofreader_model"
    ],
    "grammar_proofreader_recheck": [
        "doc.grammar_proofreader_recheck"
    ],
    "grammar_proofreader_pause_during_agent": [
        "doc.grammar_proofreader_pause_during_agent"
    ],
    "grammar_proofreader_batch_sentences": [
        "doc.grammar_proofreader_batch_sentences"
    ],
    "grammar_proofreader_max_in_flight": [
        "doc.grammar_proofreader_max_in_flight"
    ],
    "grammar_proofreader_detect_language": [
        "doc.grammar_proofreader_detect_language"
    ],
    "chat_enter_key_sends_message": [
        "doc.chat_enter_key_sends_message"
    ],
    "agent_edit_review_mode": [
        "doc.agent_edit_review_mode"
    ],
    "edit_review_timeout": [
        "doc.edit_review_timeout"
    ],
    "folder_search_mode": [
        "embeddings.folder_search_mode"
    ],
    "embedding_model": [
        "embeddings.embedding_model"
    ],
    "folder_rerank_enabled": [
        "embeddings.folder_rerank_enabled"
    ],
    "folder_rerank_model": [
        "embeddings.folder_rerank_model"
    ],
    "python_venv_path": [
        "scripting.python_venv_path"
    ],
    "test_venv": [
        "scripting.test_venv"
    ],
    "download_audio_binaries": [
        "scripting.download_audio_binaries"
    ],
    "python_exec_timeout": [
        "scripting.python_exec_timeout"
    ],
    "python_max_data_cells": [
        "scripting.python_max_data_cells"
    ],
    "python_session_mode": [
        "scripting.python_session_mode"
    ],
    "python_geometric_recalc_order": [
        "scripting.python_geometric_recalc_order"
    ],
    "python_auto_spill": [
        "scripting.python_auto_spill"
    ],
    "xl_static_rewrite": [
        "scripting.xl_static_rewrite"
    ],
    "force_internal_script_editor": [
        "scripting.force_internal_script_editor"
    ],
    "native_run_script_modeless": [
        "scripting.native_run_script_modeless"
    ],
    "images_scale": [
        "vision.images_scale"
    ],
    "worker_timeout_sec": [
        "vision.worker_timeout_sec"
    ],
    "artifacts_path": [
        "vision.artifacts_path"
    ],
    "text_score": [
        "vision.text_score"
    ],
    "force_full_page_ocr": [
        "vision.force_full_page_ocr"
    ],
    "table_mode": [
        "vision.table_mode"
    ],
    "do_cell_matching": [
        "vision.do_cell_matching"
    ],
    "create_orphan_clusters": [
        "vision.create_orphan_clusters"
    ],
    "insert_mode": [
        "vision.insert_mode"
    ],
    "layout_model": [
        "vision.layout_model"
    ],
    "do_formula_enrichment": [
        "vision.do_formula_enrichment"
    ],
    "do_code_enrichment": [
        "vision.do_code_enrichment"
    ],
    "document_timeout": [
        "vision.document_timeout"
    ]
}
