# agent_provider.sh — provider plumbing shared by every agent call site (sourced, never run).
#
# User: harness/agent_run.sh (one `docker run --rm` per call), which gets these pieces:
#   ap_resolve MODEL          provider (claude|codex|kimi|glm|grok), agent (claude-code|codex), gateway route, GLM-under-Codex
#   vendor_env PROVIDER MODEL VENV: base URL, key and model pins for Anthropic-compatible vendors (Kimi, GLM, Grok)
#   ap_budget_args            BUDGETARG: --max-budget-usd only when BUDGET is a number (BUDGET=none disables it)
#   ap_meter_begin / ap_meter_env   the per-call OUTPUT-token cap (token_meter.py) for Claude Code agents: METER_*, MENV
#   ap_codex_home             a per-call CODEX_HOME: the ChatGPT login (auth.json + config.toml) or, for GLM_AGENT=codex,
#                             the gateway provider config.toml + an env file holding the key (never argv)
#   ap_codex_watch            the Codex output-cap / spend watcher; the caller says how to stop the agent
#   ap_codex_finish / ap_finish     the files kept beside the transcript: .rc, .meter.json, .claude_session.tgz,
#                             .codex_rollout.jsonl, .codex_cost.json, .effort_mismatch
# Provider is inferred from the model name: gpt-*/o<N>/codex -> codex; kimi-* -> Kimi (Moonshot) and glm-*/zai-* -> GLM
# (z.ai), grok-* -> Grok, all through claude-code against an Anthropic-compatible endpoint; anything else -> claude.
# Grok, and GLM when GLM_VIA=gateway, go through an Anthropic-compatible model gateway (GATEWAY_BASE_URL, key in
# GATEWAY_KEY_FILE). GLM_AGENT=codex (with GLM_VIA=gateway) runs GLM under Codex against the gateway's Responses API.

_AP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
agent_provider () { case "${1:-}" in gpt-*|gpt[0-9]*|o[0-9]*|codex*|GPT-*) echo codex;; kimi-*|Kimi-*) echo kimi;;
  glm-*|glm[0-9]*|GLM-*|zai-*) echo glm;; grok-*|Grok-*) echo grok;; *) echo claude;; esac; }

# ap_resolve MODEL -> AP_PROV, AP_AGENT (claude-code|codex), AP_GWCODEX (GLM under Codex), AP_GATEWAY (gateway route)
ap_resolve () {
  AP_PROV="$(agent_provider "$1")"; AP_GWCODEX=0; AP_GATEWAY=0; AP_AGENT=claude-code
  [ "$AP_PROV" = glm ] && [ "${GLM_AGENT:-}" = codex ] && [ "${GLM_VIA:-}" = gateway ] && AP_GWCODEX=1
  { [ "$AP_PROV" = grok ] || { [ "$AP_PROV" = glm ] && [ "${GLM_VIA:-}" = gateway ]; }; } && AP_GATEWAY=1
  { [ "$AP_PROV" = codex ] || [ "$AP_GWCODEX" = 1 ]; } && AP_AGENT=codex
  return 0
}

# ap_key_var PROVIDER -> the variable naming the key file of a vendor provider
ap_key_var () {
  if [ "$1" = grok ] || { [ "$1" = glm ] && [ "${GLM_VIA:-}" = gateway ]; }; then echo GATEWAY_KEY_FILE
  elif [ "$1" = glm ]; then echo ZAI_KEY_FILE; else echo KIMI_KEY_FILE; fi; }

# Anthropic-compatible vendors run claude-code with the vendor's base URL and key. The key file holds the
# raw key (never baked or committed); the vendor host must be in the egress allow-list (egress_up.sh).
# AP_DRY=1: never read a key; a non-empty key file yields the placeholder <redacted:FILE> (dry runs print argv).
_ap_key () {   # _ap_key FILE [FALLBACK_FILE] -> the key (or its placeholder under AP_DRY=1)
  local f
  for f in "$@"; do
    [ -n "$f" ] && [ -s "$f" ] || continue
    if [ "${AP_DRY:-0}" = 1 ]; then echo "<redacted:$f>"; else cat "$f"; fi
    return 0
  done
}
vendor_env () {   # vendor_env PROVIDER MODEL -> fills the VENV array, or returns 1 if the key is missing
  local key base
  case "$1" in
    kimi) key="$(_ap_key "${KIMI_KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/kimi.key}" 2>/dev/null)"
          base="${KIMI_BASE_URL:-https://api.moonshot.ai/anthropic}"; METER_UP=kimi ;;
    glm)  key="$(_ap_key "${ZAI_KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/zai.key}" 2>/dev/null)"
          base="${ZAI_BASE_URL:-}"; METER_UP=zai ;;
  esac
  if [ "$1" = grok ] || { [ "$1" = glm ] && [ "${GLM_VIA:-}" = gateway ]; }; then
    key="$(_ap_key "${GATEWAY_KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/gateway.key}" 2>/dev/null | tr -d '\r\n')"
    base="${GATEWAY_BASE_URL:-}"; METER_UP=gateway
  fi
  [ -n "$key" ] || return 1
  [ -n "${METER_URL:-}" ] && base="$METER_URL/$METER_UP"   # output-token cap: route through the meter
  # endpoints without a public default (RUNNING.md): ZAI_BASE_URL for glm-*, GATEWAY_BASE_URL for grok-* / GLM_VIA=gateway
  [ -n "$base" ] || { echo "[agent_provider] no endpoint for $1: set ZAI_BASE_URL or GATEWAY_BASE_URL (RUNNING.md)" >&2; return 1; }
  VENV=(-e ANTHROPIC_BASE_URL="$base" -e ANTHROPIC_AUTH_TOKEN="$key" -e ANTHROPIC_MODEL="$2"
        -e ANTHROPIC_DEFAULT_OPUS_MODEL="$2" -e ANTHROPIC_DEFAULT_SONNET_MODEL="$2" -e ANTHROPIC_DEFAULT_HAIKU_MODEL="$2"
        -e CLAUDE_CODE_SUBAGENT_MODEL="$2" -e API_TIMEOUT_MS="${VENDOR_API_TIMEOUT_MS:-${GLM_API_TIMEOUT_MS:-3000000}}")
  # GLM-5.x thinks by default and z.ai documents no thinking cap; effort comes from --effort like every Claude Code
  # agent. GLM_MAX_THINKING=<n> caps thinking tokens (an earlier GLM run streamed unbounded thinking).
  [ "$1" = glm ] && [ -n "${GLM_MAX_THINKING:-}" ] && VENV+=(-e MAX_THINKING_TOKENS="$GLM_MAX_THINKING")
  # GLM_EXTRA_BODY=<json> is merged into every request body by Claude Code (CLAUDE_CODE_EXTRA_BODY), e.g. vendor fields.
  [ "$1" = glm ] && [ -n "${GLM_EXTRA_BODY:-}" ] && VENV+=(-e CLAUDE_CODE_EXTRA_BODY="$GLM_EXTRA_BODY")
  return 0
}

# Spend cap: --max-budget-usd only when BUDGET is a number (BUDGET=none disables it). -> BUDGETARG
ap_budget_args () { BUDGETARG=(); case "${BUDGET:-none}" in none|0|"") ;; *) BUDGETARG=(--max-budget-usd "$BUDGET");; esac; }

# Per-call OUTPUT-token cap (OUTPUT_TOKEN_CAP, e.g. 250000): Claude Code agents reach their provider through
# token_meter.py, which counts the provider's own output tokens and refuses further requests at the cap; Codex is
# stopped by a watcher on its session rollout. The call's count is kept beside the transcript as .meter.json.
# ap_meter_begin PROV (PROV = codex for every Codex call) -> METER_ID, METER_HOST, METER_URL (empty: no meter)
ap_meter_begin () {
  METER_URL="" METER_ID="" METER_HOST=""
  if [ -n "${OUTPUT_TOKEN_CAP:-}" ] && [ "$1" != codex ]; then
    METER_ID="$(date +%s%N)_$$_$RANDOM"; METER_HOST="${METER:-${EGRESS_PROXY:-cyberdual_proxy}_meter}"; METER_URL="http://$METER_HOST:8899/m/$METER_ID"
    [ "${AP_DRY:-0}" = 1 ] && return 0
    printf '{"call_id":"%s","cap":%s,"output_tokens":0,"input_tokens":0,"requests":0,"capped":false}\n' "$METER_ID" "$OUTPUT_TOKEN_CAP" \
      > "${METER_DIR:-/tmp/cyberdual_meter}/$METER_ID.json"
  fi
}
# ap_meter_env PROV -> MENV (Claude: the meter replaces the Anthropic base URL; vendors: vendor_env already routes
# the base URL through the meter)
ap_meter_env () {
  MENV=(); [ -n "${METER_URL:-}" ] || return 0
  if [ "$1" = claude ]; then
    MENV=(-e ANTHROPIC_BASE_URL="$METER_URL/anthropic" -e NO_PROXY="$METER_HOST" -e ANTHROPIC_CUSTOM_HEADERS="x-cyberdual-output-cap: $OUTPUT_TOKEN_CAP")
  else
    MENV=(-e NO_PROXY="$METER_HOST" -e ANTHROPIC_CUSTOM_HEADERS="x-cyberdual-output-cap: $OUTPUT_TOKEN_CAP")
  fi
}

# ap_codex_model MODEL -> the Codex model name (the literal "codex" means CODEX_MODEL)
ap_codex_model () { local m="$1"; [ "$m" = codex ] && m="${CODEX_MODEL:-gpt-6.1-sol}"; echo "$m"; }

# ap_codex_home CHOME CMODEL GWCODEX ENVFILE -> fills CHOME (a per-call CODEX_HOME) and the GWENV array.
# Gateway provider (GLM_AGENT=codex, no ChatGPT login): config.toml names the gateway's Responses endpoint and the
# key reaches the container through ENVFILE (--env-file), never argv. High retry limits: the gateway admits at most
# 6 concurrent GLM requests and answers 429 above that. Otherwise: a writable copy of the host login + config.
ap_codex_home () {
  local chome="$1" cmodel="$2" gw="$3" envf="$4"
  GWENV=()
  if [ "$gw" = 1 ]; then
    [ -n "${GATEWAY_BASE_URL:-}" ] || [ "${AP_DRY:-0}" = 1 ] || { echo "[agent_provider] set GATEWAY_BASE_URL (RUNNING.md)" >&2; return 2; }
    printf 'model = "%s"\nmodel_provider = "gw"\n[model_providers.gw]\nname = "gw"\nbase_url = "%s/v1"\nenv_key = "GW_KEY"\nwire_api = "responses"\nrequest_max_retries = 50\nstream_max_retries = 50\nstream_idle_timeout_ms = 900000\n' \
      "$cmodel" "${GATEWAY_BASE_URL:-<GATEWAY_BASE_URL unset>}" > "$chome/config.toml"
    if [ "${AP_DRY:-0}" = 1 ]; then
      ( umask 077; printf 'GW_KEY=<redacted:%s>\n' "${GATEWAY_KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/gateway.key}" > "$envf" )
    else
      ( umask 077; printf 'GW_KEY=%s\n' "$(tr -d '\r\n' < "${GATEWAY_KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/gateway.key}")" > "$envf" )
    fi
    GWENV=(--env-file "$envf")
  elif [ "${AP_DRY:-0}" != 1 ]; then
    cp "${CODEX_HOME_SRC:-$HOME/.codex}/auth.json" "${CODEX_HOME_SRC:-$HOME/.codex}/config.toml" "$chome/" 2>/dev/null || true
  fi
  chmod -R u+rwX "$chome" 2>/dev/null || true
}

# ap_codex_watch CHOME CMODEL CBUDGET STOPF CAPF STOP_CMD... -> AP_WATCHER (pid). Codex has no spend flag: a watcher
# prices the session log's cumulative token counts (codex_cost.py) and runs STOP_CMD once the API-equivalent cost
# reaches CBUDGET, and runs STOP_CMD (after touching CAPF) once the rollout's output tokens reach OUTPUT_TOKEN_CAP.
# AP_WATCH_PRE (array, optional) runs before every check (e.g. to make root-owned rollouts readable).
ap_codex_watch () {
  local chome="$1" cmodel="$2" cbudget="$3" stopf="$4" capf="$5"; shift 5
  declare -p AP_WATCH_PRE >/dev/null 2>&1 || AP_WATCH_PRE=()
  ( while sleep "${CODEX_BUDGET_POLL:-10}"; do
      [ "${#AP_WATCH_PRE[@]}" -eq 0 ] || "${AP_WATCH_PRE[@]}" >/dev/null 2>&1
      if [ -n "${OUTPUT_TOKEN_CAP:-}" ]; then   # output-token cap: stop once the rollout total reaches it
        python3 "$_AP_DIR/codex_output_cap.py" "$chome" --cap "$OUTPUT_TOKEN_CAP" && { touch "$capf"; "$@" >/dev/null 2>&1; break; }
      fi
      case "$cbudget" in none|0|"") continue;; esac
      python3 "$_AP_DIR/codex_cost.py" "$chome" --model "$cmodel" --budget "$cbudget" --stop-file "$stopf" \
        && { "$@" >/dev/null 2>&1; break; }
    done ) &
  AP_WATCHER=$!
}

# ap_codex_finish TRANSCRIPT BASE CHOME CMODEL CEFFORT CBUDGET STOPF CAPF: the transcript header must report the
# requested effort (a mismatch is recorded as BASE.effort_mismatch); keep the session rollout, its priced record,
# the budget / output-cap notes and BASE.meter.json.
ap_codex_finish () {
  local tr="$1" base="$2" chome="$3" cmodel="$4" ceffort="$5" cbudget="$6" stopf="$7" capf="$8"
  local heff; heff="$(grep -m1 -o 'reasoning effort: [a-z]*' "$tr" 2>/dev/null | cut -d' ' -f3)"
  if [ -n "$heff" ] && [ "$heff" != "$ceffort" ]; then
    echo "header=$heff requested=$ceffort" > "$base.effort_mismatch"
    echo "[codex effort] transcript header says $heff, requested $ceffort" >&2
  fi
  AP_HEADER_EFFORT="$heff"
  find "$chome/sessions" -name 'rollout-*.jsonl' -exec cat {} + > "$base.codex_rollout.jsonl" 2>/dev/null || true
  python3 "$_AP_DIR/codex_cost.py" "$chome" --model "$cmodel" --budget "$cbudget" --stop-file "$stopf" --json \
    > "$base.codex_cost.json" 2>/dev/null || true
  [ -f "$stopf" ] && printf '\n[codex budget] stopped at an estimated $%s (cap $%s)\n' "$(cat "$stopf")" "$cbudget" >> "$tr"
  if [ -n "${OUTPUT_TOKEN_CAP:-}" ]; then
    python3 "$_AP_DIR/codex_output_cap.py" "$chome" --cap "$OUTPUT_TOKEN_CAP" --json "$base.meter.json" || true
    [ -f "$capf" ] && printf '\n[output cap] stopped at %s output tokens (cap %s)\n' \
      "$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['output_tokens'])" "$base.meter.json")" "$OUTPUT_TOKEN_CAP" >> "$tr"
  fi
  return 0
}

# ap_finish BASE ARC CCHOME: BASE.rc (124/137 = wall-killed by timeout), BASE.meter.json (Claude Code meter), and
# Claude Code's own session logs (main agent + subagent sidechains) as BASE.claude_session.tgz; removes CCHOME.
ap_finish () {
  local base="$1" arc="$2" cchome="${3:-}"
  printf '%s\n' "$arc" > "$base.rc" 2>/dev/null || true
  [ -n "${METER_ID:-}" ] && cp "${METER_DIR:-/tmp/cyberdual_meter}/$METER_ID.json" "$base.meter.json" 2>/dev/null || true
  if [ -n "$cchome" ]; then
    [ -d "$cchome/projects" ] && tar -czf "$base.claude_session.tgz" -C "$cchome" projects 2>/dev/null || true
    rm -rf "$cchome"
  fi
  return 0
}
