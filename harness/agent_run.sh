# agent_run.sh — shared Role A / Role B agent dispatch for the CYBERDUAL pipeline.
# A role runs as EITHER claude-code or codex, in the SAME egress-controlled docker image
# (the agent image, which ships both agents + the build toolchain; see Dockerfile.agent).
# Provider is inferred from the model name: gpt-*/o<N>/codex -> codex; kimi-* -> Kimi (Moonshot) and
# glm-*/zai-* -> GLM (z.ai), grok-* -> Grok, all through claude-code against an Anthropic-compatible endpoint;
# anything else -> claude. Grok, and GLM when GLM_VIA=gateway, go through an Anthropic-compatible model gateway
# (GATEWAY_BASE_URL, key in GATEWAY_KEY_FILE). The provider pieces (vendor env, output-token meter, Codex home,
# Codex watcher, files kept beside the transcript) live in agent_provider.sh.
#
# The caller must already define: IMAGE, BUDGET, NETOPTS, AUTHENV (array). Optional: WALLPFX (array),
# EGRESS_PROXY, CODEX_MODEL (default when model is the literal "codex"), CODEX_HOME_SRC (default ~/.codex).
# Usage: source "$(dirname "${BASH_SOURCE[0]}")/agent_run.sh"

_AGENTRUN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$_AGENTRUN_DIR/agent_provider.sh"

# run_agent WK SYS TASK MODEL PERM TRANSCRIPT [EXTRA_CLAUDE_ARGS...]
# Leaves the agent's edits in WK; writes the agent transcript to TRANSCRIPT. Same artifact contract
# either way (the agent edits /work; the pipeline reads pocs/, selection.json, submission.json, the tree).
run_agent () {
  local wk="$1" sys="$2" task="$3" model="$4" perm="$5" tr="$6"; shift 6
  declare -p WALLPFX >/dev/null 2>&1 || WALLPFX=()
  # Every agent call has a wall-clock limit: 60 minutes unless the driver sets WALL.
  [ "${#WALLPFX[@]}" -eq 0 ] && WALLPFX=(timeout -k 60 --signal=TERM "${WALL:-3600}")
  local AP_PROV AP_AGENT AP_GWCODEX AP_GATEWAY; ap_resolve "$model"
  local prov="$AP_PROV"
  # GLM_AGENT=codex (with GLM_VIA=gateway): run GLM under Codex against the gateway's OpenAI Responses endpoint.
  # Under Claude Code the gateway's GLM thinks until the output cap on most calls; under Codex it stops normally.
  local GWCODEX="$AP_GWCODEX"; [ "$GWCODEX" = 1 ] && prov=codex
  local METER_URL="" METER_ID="" METER_HOST="" BUDGETARG=()
  ap_budget_args; ap_meter_begin "$prov"
  # Claude Code's own session logs ($HOME/.claude/projects: the main agent and every subagent sidechain) are kept
  # for every Claude Code call: a per-call host dir is mounted at the container's $HOME/.claude and archived beside the
  # transcript as <transcript>.claude_session.tgz. (Codex calls keep their session rollout as .codex_rollout.jsonl.)
  local CCHOME=""; [ "$prov" != codex ] && { CCHOME="$(mktemp -d /tmp/cchome.XXXXXX)"; chmod 700 "$CCHOME"; }
  local arc=""   # agent exit status, kept beside the transcript as <transcript>.rc (124/137 = wall-killed by timeout)
  # Claude Code refuses --dangerously-skip-permissions as root; the container is the sandbox, so say so when the host
  # user is root (cloud VMs, CI). Non-root hosts run exactly as before.
  local ROOTENV=(); [ "$(id -u)" = 0 ] && ROOTENV=(-e IS_SANDBOX=1)
  if [ "$prov" = codex ]; then
    local cmodel; cmodel="$(ap_codex_model "$model")"
    # codex has no --append-system-prompt; prepend the role system prompt to the task.
    local prompt; prompt="$(cat "$sys")"$'\n\n'"$task"
    # Each tool runs at its own default effort, pinned on the command line: medium for Codex (gpt-6-sol's
    # default) and high for Claude Code (--effort below). Never inherited from the host config.toml;
    # dropping the key from the config instead makes Codex run at "none".
    local ceffort="${CYBERDUAL_CODEX_EFFORT:-medium}"
    # Docker IS the sandbox (codex's own landlock cannot initialise in a container), so run codex with
    # --dangerously-bypass-approvals-and-sandbox. CODEX_HOME is a per-run writable copy of the host
    # auth+config (never baked into the image); model traffic exits via the egress proxy, same as claude.
    local chome; chome="$(mktemp -d /tmp/codexhome.XXXXXX)"
    local GWENV=(); ap_codex_home "$chome" "$cmodel" "$GWCODEX" "$chome/.gwenv"
    local PXY=""; [ -n "${NETOPTS:-}" ] && PXY="-e HTTP_PROXY=http://${EGRESS_PROXY:-cyberdual_proxy}:8888 -e https_proxy=http://${EGRESS_PROXY:-cyberdual_proxy}:8888 -e http_proxy=http://${EGRESS_PROXY:-cyberdual_proxy}:8888"
    # Codex has no spend flag; the watcher (agent_provider.sh) stops the container at the output cap or at the
    # API-equivalent cost cap (CODEX_BUDGET_USD overrides the shared BUDGET).
    local cname; cname="codex_$(basename "$chome")"
    local cbudget="${CODEX_BUDGET_USD:-${BUDGET:-25}}" stopf="$chome/.budget_stop"
    local capf="$chome/.output_cap_stop"
    local AP_WATCHER AP_WATCH_PRE=(); ap_codex_watch "$chome" "$cmodel" "$cbudget" "$stopf" "$capf" docker kill "$cname"
    local watcher="$AP_WATCHER"
    "${WALLPFX[@]}" docker run --rm -i --name "$cname" $NETOPTS $PXY "${GWENV[@]}" --user "$(id -u):$(id -g)" -e HOME=/tmp \
      -e CODEX_HOME=/codex -v "$chome":/codex -v "$wk":/work -w /work "$IMAGE" \
      codex exec --cd /work -m "$cmodel" -c model_reasoning_effort="$ceffort" --skip-git-repo-check \
        --dangerously-bypass-approvals-and-sandbox "$prompt" > "$tr" 2>&1 || arc=$?
    arc="${arc:-0}"
    kill "$watcher" 2>/dev/null; wait "$watcher" 2>/dev/null
    local AP_HEADER_EFFORT=""
    ap_codex_finish "$tr" "${tr%.jsonl}" "$chome" "$cmodel" "$ceffort" "$cbudget" "$stopf" "$capf"
    rm -rf "$chome"
  elif [ "$prov" = kimi ] || [ "$prov" = glm ] || [ "$prov" = grok ]; then
    local VENV=()
    if ! vendor_env "$prov" "$model"; then
      echo "[agent_run] no $prov API key: put it in the file named by $(ap_key_var "$prov")" | tee "$tr" >&2; arc=2
    else
      # Same claude-code invocation as the claude branch; the vendor endpoint replaces the Anthropic auth.
      local MENV=(); ap_meter_env "$prov"
      "${WALLPFX[@]}" docker run --rm -i $NETOPTS --user "$(id -u):$(id -g)" "${ROOTENV[@]}" -e HOME=/tmp -v "$CCHOME":/tmp/.claude "${VENV[@]}" "${MENV[@]}" \
        -e CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC=1 -e NO_MODEL_FALLBACK=1 -v "$wk":/work -w /work -v "$sys":/sys.md:ro "$IMAGE" \
        claude -p "$task" --model "$model" --effort "${CYBERDUAL_CLAUDE_EFFORT:-high}" --append-system-prompt "$(cat "$sys")" \
          --output-format stream-json --verbose "${BUDGETARG[@]}" --permission-mode "$perm" "$@" \
        | tee "$tr" >/dev/null
      arc="${PIPESTATUS[0]}"
    fi
  else
    local MENV=(); ap_meter_env claude
    "${WALLPFX[@]}" docker run --rm -i $NETOPTS --user "$(id -u):$(id -g)" "${ROOTENV[@]}" -e HOME=/tmp -v "$CCHOME":/tmp/.claude "${AUTHENV[@]}" "${MENV[@]}" \
      -e CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC=1 -e NO_MODEL_FALLBACK=1 -v "$wk":/work -w /work -v "$sys":/sys.md:ro "$IMAGE" \
      claude -p "$task" --model "$model" --effort "${CYBERDUAL_CLAUDE_EFFORT:-high}" --append-system-prompt "$(cat "$sys")" \
        --output-format stream-json --verbose "${BUDGETARG[@]}" --permission-mode "$perm" "$@" \
      | tee "$tr" >/dev/null
    arc="${PIPESTATUS[0]}"
  fi
  ap_finish "${tr%.jsonl}" "$arc" "$CCHOME"
}
