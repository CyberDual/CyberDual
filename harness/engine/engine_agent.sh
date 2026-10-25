#!/usr/bin/env bash
# engine_agent.sh — one attacker / reviewer agent call on the V8 / SpiderMonkey arm, under the LIBRARY protocol.
#
# Engine containers are long-lived root containers (IS_SANDBOX=1, work in /src/v8 or /src/gecko-dev) kept for
# docker cp / commit / exec, so the call is a `docker exec` into the caller's container. The provider plumbing is
# harness/agent_provider.sh, the same code harness/agent_run.sh uses for the library arm:
#   - model -> provider + agent: claude-* (Claude Code), gpt-*/o<N>/codex (Codex), kimi-*/glm-*/grok-* (Claude Code
#     against the vendor's Anthropic-compatible endpoint; grok and GLM_VIA=gateway through the model gateway),
#     glm-* with GLM_AGENT=codex GLM_VIA=gateway (Codex against the gateway's /v1/responses).
#   - limits: OUTPUT_TOKEN_CAP through token_meter.py (Claude Code) or the rollout watcher (Codex, which stops the codex
#     PROCESS with `docker exec pkill`, never the work container); WALL (default 21600 s) is the hang guard of every
#     call; --max-budget-usd only when BUDGET is a number.
#   - credentials reach the container only at `docker exec` time and only by variable NAME (values never on argv),
#     so a `docker commit` of the work container (the shipped attack image) never contains a key. The Anthropic
#     KEY_FILE is read only for Claude models (REVIEW_KEY_FILE for --role review); Codex with a ChatGPT login copies
#     CODEX_HOME_SRC/auth.json into the call's CODEX_HOME; GLM_AGENT=codex writes the gateway config.toml instead and
#     passes the key through --env-file (a file outside the container).
#   - network: EGRESS_NET / EGRESS_PROXY (default cyberdual_egress / cyberdual_proxy, the library pair). A gateway model
#     (grok-*, GLM_VIA=gateway) on a default network moves to GW_EGRESS_NET / GW_EGRESS_PROXY (default cyberdual_gw_egress /
#     cyberdual_gw_proxy), whose allow-list has the gateway host. EGRESS=0: no egress network.
#   - files beside the transcript (BASE = transcript minus its last extension): BASE.rc, BASE.meter.json,
#     BASE.claude_session.tgz, BASE.codex_rollout.jsonl, BASE.codex_cost.json, BASE.effort_mismatch, BASE.agent.json
#     (provider, agent, effective effort, header effort, mismatch flag, limits, network, exit status).
#
# Usage (CLI or sourced: engine_agent_resolve / engine_agent_exec / engine_agent_egress_up):
#   engine_agent.sh resolve MODEL                 KEY=VALUE lines: provider agent gateway gwcodex net proxy effort ...
#   engine_agent.sh egress-up MODEL               start the resolved egress network + proxy + meter (harness/egress_up.sh)
#   engine_agent.sh exec --cid CID --model M --workdir WD --transcript TX [--role attack|review] [--perm MODE]
#        [--sys FILE --task FILE] [--prompt FILE] [--home DIR] [--no-codex-config] [--dry-run] [-- EXTRA_CLAUDE_ARGS...]
#     Claude Code: -p "$(cat TASK)" --append-system-prompt "$(cat SYS)". Codex: "$(cat PROMPT)" (default SYS + blank
#     line + TASK). --home: the host dir mounted at the container's /codex (required for Codex agents).
#     --dry-run prints the exact docker argv (keys redacted, nothing run, no key read). Exit status = the agent's.
# Container contract (callers create it): `docker run -d --name CID $(network args) -v HOME_DIR:/codex -e HOME=/tmp
#   -e IS_SANDBOX=1 [build env] IMAGE sleep infinity`; engine_agent_container_args MODEL prints the network args.

_EA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$_EA_DIR/../agent_provider.sh"

# engine_agent_resolve MODEL -> AP_* (agent_provider.sh) + EA_NET EA_PROXY EA_EFFORT EA_EFFORT_NOTE EA_CMODEL
engine_agent_resolve () {
  ap_resolve "$1"
  EA_NET="${EGRESS_NET:-cyberdual_egress}"; EA_PROXY="${EGRESS_PROXY:-cyberdual_proxy}"
  if [ "$AP_GATEWAY" = 1 ]; then
    case "$EA_NET" in cyberdual_egress|revbench_egress)
      EA_NET="${GW_EGRESS_NET:-cyberdual_gw_egress}"; EA_PROXY="${GW_EGRESS_PROXY:-cyberdual_gw_proxy}" ;; esac
  fi
  [ "${EGRESS:-1}" = 0 ] && { EA_NET=""; EA_PROXY=""; }
  EA_EFFORT_NOTE=""; EA_CMODEL=""
  if [ "$AP_AGENT" = codex ]; then
    EA_CMODEL="$(ap_codex_model "$1")"
    EA_EFFORT="${VIPER_CODEX_EFFORT:-medium}"
    # BigModel (GLM) accepts only low / high / max reasoning effort and rejects Codex's default medium (error 1210).
    if [ "$AP_GWCODEX" = 1 ] && [ "$EA_EFFORT" = medium ]; then
      EA_EFFORT="${VIPER_GLM_CODEX_EFFORT:-high}"; EA_EFFORT_NOTE="GLM under Codex: medium is rejected by BigModel; ran at $EA_EFFORT"
    fi
  else
    EA_EFFORT="${VIPER_CLAUDE_EFFORT:-high}"
  fi
}

# the docker run arguments a work container needs for MODEL's egress (one per line)
engine_agent_container_args () { engine_agent_resolve "$1"; [ -z "$EA_NET" ] || printf '%s\n' --network "$EA_NET"; }

engine_agent_egress_up () {
  engine_agent_resolve "$1"
  [ -n "$EA_NET" ] || return 0
  NET="$EA_NET" PROXY="$EA_PROXY" bash "$_EA_DIR/../egress_up.sh" >/dev/null 2>&1 || true
}

vendor_env_placeholder () {   # dry run without a key file: show the argv with a placeholder key
  local f; f="$(mktemp)"; echo missing > "$f"
  KIMI_KEY_FILE="$f" ZAI_KEY_FILE="$f" GATEWAY_KEY_FILE="$f" vendor_env "$@"; rm -f "$f"; }

_ea_redact_argv () {   # print argv shell-quoted; the env file and name-only -e carry no values
  local a out=""; for a in "$@"; do out+="$(printf '%q' "$a") "; done; printf '%s\n' "${out% }"
}

engine_agent_exec () {
  local cid="" model="" wd="" tr="" role=attack perm=bypassPermissions sysf="" taskf="" promptf="" home="" codexcfg=1 dry=0
  local extra=()
  while [ $# -gt 0 ]; do
    case "$1" in
      --cid) cid="$2"; shift 2;; --model) model="$2"; shift 2;; --workdir) wd="$2"; shift 2;;
      --transcript) tr="$2"; shift 2;; --role) role="$2"; shift 2;; --perm) perm="$2"; shift 2;;
      --sys) sysf="$2"; shift 2;; --task) taskf="$2"; shift 2;; --prompt) promptf="$2"; shift 2;;
      --home) home="$2"; shift 2;; --no-codex-config) codexcfg=0; shift;; --dry-run) dry=1; shift;;
      --) shift; extra=("$@"); break;;
      *) echo "engine_agent: unknown argument $1" >&2; return 2;;
    esac
  done
  [ -n "$cid" ] && [ -n "$model" ] && [ -n "$wd" ] && [ -n "$tr" ] || { echo "engine_agent: --cid --model --workdir --transcript required" >&2; return 2; }
  local AP_DRY="$dry"
  engine_agent_resolve "$model"
  local base="${tr%.*}" wall="${WALL:-21600}" arc=0
  local WALLPFX=(timeout -k 60 --signal=TERM "$wall")
  local PXY=(); [ -n "$EA_NET" ] && PXY=(-e HTTPS_PROXY="http://$EA_PROXY:8888" -e HTTP_PROXY="http://$EA_PROXY:8888"
                                         -e https_proxy="http://$EA_PROXY:8888" -e http_proxy="http://$EA_PROXY:8888")
  local EGRESS_PROXY="$EA_PROXY"   # the output-token meter is <proxy>_meter (agent_provider.sh)
  local METER_URL="" METER_ID="" METER_HOST="" BUDGETARG=() MENV=() VENV=() CRED=() GWENV=()
  local prov="$AP_PROV"; [ "$AP_AGENT" = codex ] && prov=codex
  [ "$dry" = 1 ] || mkdir -p "$(dirname "$tr")"
  ap_meter_begin "$prov"
  local AP_HEADER_EFFORT="" capped=0
  if [ "$AP_AGENT" = codex ]; then
    [ -n "$home" ] && { [ "$dry" = 1 ] || [ -d "$home" ]; } || { echo "engine_agent: Codex needs --home (the dir mounted at /codex)" >&2; return 2; }
    local prompt
    if [ -n "$promptf" ]; then prompt="$(cat "$promptf")"; else prompt="$(cat "$sysf")"$'\n\n'"$(cat "$taskf")"; fi
    local envf=""; [ "$AP_GWCODEX" = 1 ] && envf="$(mktemp /tmp/viper-engine-gwenv.XXXXXX)"
    if [ "$dry" = 1 ]; then GWENV=(); [ "$AP_GWCODEX" = 1 ] && GWENV=(--env-file "$envf")
    else
      if [ "$codexcfg" = 1 ] || [ "$AP_GWCODEX" = 1 ]; then ap_codex_home "$home" "$EA_CMODEL" "$AP_GWCODEX" "$envf"
      else cp "${CODEX_HOME_SRC:-$HOME/.codex}/auth.json" "$home/" 2>/dev/null || true; chmod -R u+rwX "$home" 2>/dev/null || true; fi
    fi
    local cargv=(docker exec -i "${PXY[@]}" "${GWENV[@]}" -e HOME=/tmp -e CODEX_HOME=/codex -w "$wd" "$cid"
                 sh -lc 'exec codex "$@"' sh exec --cd "$wd" -m "$EA_CMODEL" -c model_reasoning_effort="$EA_EFFORT"
                 --skip-git-repo-check --dangerously-bypass-approvals-and-sandbox "$prompt")
    if [ "$dry" = 1 ]; then
      echo "# agent=codex provider=$AP_PROV model=$EA_CMODEL effort=$EA_EFFORT${EA_EFFORT_NOTE:+ ($EA_EFFORT_NOTE)} net=${EA_NET:-none} proxy=${EA_PROXY:-none}"
      [ "$AP_GWCODEX" = 1 ] && echo "# CODEX_HOME=$home: gateway config.toml (base_url=${GATEWAY_BASE_URL:-<GATEWAY_BASE_URL unset>}/v1, wire_api=responses); GW_KEY via --env-file from ${GATEWAY_KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/gateway.key}" \
                                || echo "# CODEX_HOME=$home: copy of ${CODEX_HOME_SRC:-$HOME/.codex}/auth.json$([ "$codexcfg" = 1 ] && echo ' + config.toml')"
      echo "# output cap: ${OUTPUT_TOKEN_CAP:-none} (rollout watcher; stops with: docker exec $cid pkill -TERM -f 'codex exec')"
      _ea_redact_argv "${WALLPFX[@]}" "${cargv[@]:0:${#cargv[@]}-1}" "<prompt: $(printf '%s' "$prompt" | wc -c) bytes>"
      rm -f "$envf"; return 0
    fi
    local cbudget="${CODEX_BUDGET_USD:-${BUDGET:-none}}" stopf="$home/.budget_stop" capf="$home/.output_cap_stop"
    local AP_WATCHER AP_WATCH_PRE=(docker exec "$cid" chmod -R a+rX /codex/sessions)
    ap_codex_watch "$home" "$EA_CMODEL" "$cbudget" "$stopf" "$capf" docker exec "$cid" pkill -TERM -f "codex exec"
    "${WALLPFX[@]}" "${cargv[@]}" > "$tr" 2>&1 < /dev/null || arc=$?
    kill "$AP_WATCHER" 2>/dev/null; wait "$AP_WATCHER" 2>/dev/null
    rm -f "$envf"
    docker exec "$cid" chmod -R a+rX /codex/sessions >/dev/null 2>&1 || true
    ap_codex_finish "$tr" "$base" "$home" "$EA_CMODEL" "$EA_EFFORT" "$cbudget" "$stopf" "$capf"
    [ -f "$capf" ] && capped=1
    [ "$arc" = 124 ] || [ "$arc" = 137 ] && docker exec "$cid" pkill -TERM -f "codex exec" >/dev/null 2>&1
    docker exec "$cid" chown -R "$(id -u):$(id -g)" /codex >/dev/null 2>&1 || true
  else
    ap_budget_args
    if [ "$AP_PROV" = claude ]; then
      local kf="${KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/claude.key}" cred="" var=ANTHROPIC_API_KEY
      if [ "$role" = review ]; then source "$_EA_DIR/../review_cred.sh"; kf="$(review_key_file)"; fi
      if [ ! -s "$kf" ]; then
        [ "$dry" = 1 ] && { echo "# WARNING: no Claude credential in $kf"; } \
          || { echo "[engine_agent] no Claude credential in $kf" | tee "$tr" >&2; ap_finish "$base" 2 ""; return 2; }
      fi
      # the variable follows the key kind (an sk-ant-oat token is a Claude Code OAuth token); a dry run reads no key
      if [ "$dry" = 1 ]; then cred=""; var="<ANTHROPIC_API_KEY|CLAUDE_CODE_OAUTH_TOKEN>"; else cred="$(cat "$kf")"; fi
      case "$cred" in sk-ant-oat*) var=CLAUDE_CODE_OAUTH_TOKEN;; esac
      [ "$dry" = 1 ] || export "$var=$cred"
      CRED=(-e "$var")
      ap_meter_env claude
    else
      if ! vendor_env "$AP_PROV" "$model"; then
        if [ "$dry" = 1 ]; then echo "# WARNING: no $AP_PROV API key in the file named by $(ap_key_var "$AP_PROV")"; AP_DRY=1 vendor_env_placeholder "$AP_PROV" "$model"
        else
          echo "[engine_agent] no $AP_PROV API key: put it in the file named by $(ap_key_var "$AP_PROV")" | tee "$tr" >&2
          ap_finish "$base" 2 ""; return 2
        fi
      fi
      local i; for i in "${!VENV[@]}"; do   # the key travels by name, never on argv
        case "${VENV[$i]}" in ANTHROPIC_AUTH_TOKEN=*) [ "$dry" = 1 ] || export ANTHROPIC_AUTH_TOKEN="${VENV[$i]#*=}"; VENV[$i]=ANTHROPIC_AUTH_TOKEN;; esac
      done
      ap_meter_env "$AP_PROV"
    fi
    local system task; system="$(cat "$sysf")"; task="$(cat "$taskf")"
    local cargv=(docker exec -i "${PXY[@]}" "${CRED[@]}" "${VENV[@]}" "${MENV[@]}" -e HOME=/tmp -e IS_SANDBOX=1
                 -e CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC=1 -e NO_MODEL_FALLBACK=1 -w "$wd" "$cid"
                 sh -lc 'exec claude "$@"' sh -p "$task" --model "$model" --effort "$EA_EFFORT"
                 --append-system-prompt "$system" --output-format stream-json --verbose "${BUDGETARG[@]}"
                 --permission-mode "$perm" "${extra[@]}")
    if [ "$dry" = 1 ]; then
      echo "# agent=claude-code provider=$AP_PROV model=$model effort=$EA_EFFORT net=${EA_NET:-none} proxy=${EA_PROXY:-none} credential=${CRED[1]:-${VENV[3]:-}} ($([ "$AP_PROV" = claude ] && echo "$kf" || echo "\$$(ap_key_var "$AP_PROV")"))"
      echo "# output cap: ${OUTPUT_TOKEN_CAP:-none}${METER_URL:+ via $METER_HOST (token_meter.py)}; budget: ${BUDGET:-none}"
      local shown=() a skip=0
      for a in "${cargv[@]}"; do
        if [ "$skip" = 1 ]; then shown+=("<task: $(printf '%s' "$a" | wc -c) bytes>"); skip=0; continue; fi
        if [ "$skip" = 2 ]; then shown+=("<system prompt: $(printf '%s' "$a" | wc -c) bytes>"); skip=0; continue; fi
        shown+=("$a"); [ "$a" = -p ] && skip=1; [ "$a" = --append-system-prompt ] && skip=2
      done
      _ea_redact_argv "${WALLPFX[@]}" "${shown[@]}"
      return 0
    fi
    "${WALLPFX[@]}" "${cargv[@]}" > "$tr" 2> "$base.stderr.log" < /dev/null || arc=$?
    [ -s "$base.stderr.log" ] || rm -f "$base.stderr.log"
    [ "$arc" = 124 ] || [ "$arc" = 137 ] && docker exec "$cid" pkill -TERM -f "claude -p" >/dev/null 2>&1
    unset ANTHROPIC_API_KEY CLAUDE_CODE_OAUTH_TOKEN ANTHROPIC_AUTH_TOKEN
    local ccd; ccd="$(mktemp -d /tmp/viper-engine-cc.XXXXXX)"   # Claude Code's own session logs (main agent + subagents)
    docker cp "$cid":/tmp/.claude/projects "$ccd/projects" >/dev/null 2>&1 && tar -czf "$base.claude_session.tgz" -C "$ccd" projects 2>/dev/null
    rm -rf "$ccd"
    [ -n "$METER_ID" ] && python3 -c "import json,sys;sys.exit(0 if json.load(open(sys.argv[1])).get('capped') else 1)" \
      "${METER_DIR:-/tmp/cyberdual_meter}/$METER_ID.json" 2>/dev/null && capped=1
  fi
  ap_finish "$base" "$arc" ""
  EA_J_MODEL="$model" EA_J_PROVIDER="$AP_PROV" EA_J_AGENT="$AP_AGENT" EA_J_GATEWAY="$AP_GATEWAY" EA_J_GWCODEX="$AP_GWCODEX" \
  EA_J_CMODEL="$EA_CMODEL" EA_J_EFFORT="$EA_EFFORT" EA_J_NOTE="$EA_EFFORT_NOTE" EA_J_HEADER="$AP_HEADER_EFFORT" \
  EA_J_REQ="$([ "$AP_AGENT" = codex ] && echo "${VIPER_CODEX_EFFORT:-medium}" || echo "${VIPER_CLAUDE_EFFORT:-high}")" \
  EA_J_WALL="$wall" EA_J_RC="$arc" EA_J_CAP="${OUTPUT_TOKEN_CAP:-}" EA_J_CAPPED="$capped" EA_J_BUDGET="${BUDGET:-none}" \
  EA_J_NET="$EA_NET" EA_J_PROXY="$EA_PROXY" EA_J_ROLE="$role" EA_J_CID="$cid" python3 -c '
import json, os, sys
e = lambda k: os.environ.get("EA_J_" + k) or None
json.dump(dict(model=e("MODEL"), provider=e("PROVIDER"), agent=e("AGENT"), gateway=e("GATEWAY") == "1",
               glm_under_codex=e("GWCODEX") == "1", codex_model=e("CMODEL"), effort_requested=e("REQ"), effort=e("EFFORT"),
               effort_note=e("NOTE"), header_effort=e("HEADER"),
               effort_mismatch=bool(e("HEADER")) and e("HEADER") != e("EFFORT"),
               wall_s=int(e("WALL")), wall_killed=e("RC") in ("124", "137"), output_token_cap=e("CAP"),
               ended_at_output_cap=e("CAPPED") == "1", budget=e("BUDGET"), net=e("NET"), proxy=e("PROXY"), role=e("ROLE"),
               container=e("CID"), exit_code=int(e("RC") or 0)), open(sys.argv[1], "w"), indent=1)
' "$base.agent.json" || true
  return "$arc"
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  cmd="${1:-}"; shift || true
  case "$cmd" in
    resolve) engine_agent_resolve "${1:?model}"
      printf 'provider=%s\nagent=%s\ngateway=%s\ngwcodex=%s\nnet=%s\nproxy=%s\neffort=%s\ncodex_model=%s\neffort_note=%s\n' \
        "$AP_PROV" "$AP_AGENT" "$AP_GATEWAY" "$AP_GWCODEX" "$EA_NET" "$EA_PROXY" "$EA_EFFORT" "$EA_CMODEL" "$EA_EFFORT_NOTE" ;;
    egress-up) engine_agent_egress_up "${1:?model}" ;;
    container-args) engine_agent_container_args "${1:?model}" ;;
    exec) engine_agent_exec "$@"; exit $? ;;
    *) sed -n '2,40p' "$0"; exit 2 ;;
  esac
fi
