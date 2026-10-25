# Reviewer credential. Every Claude reviewer call authenticates with REVIEW_KEY_FILE (default
# ${CYBERDUAL_KEY_DIR:-~/.config/cyberdual}/review.key) when that file is non-empty, and otherwise with the general KEY_FILE.
# Attacker calls always use KEY_FILE; Codex reviewers use their Codex login. Sourced by every script that
# launches a reviewer; the key is read here and never printed.
review_key_file() {
  local f="${REVIEW_KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/review.key}"
  if [ -s "$f" ]; then printf '%s\n' "$f"; else printf '%s\n' "${KEY_FILE:-${CYBERDUAL_KEY_DIR:-$HOME/.config/cyberdual}/claude.key}"; fi
}
# AUTHENV for `docker run` (the variable carries its value, as the run_* drivers pass it)
review_authenv() {
  local c; c="$(cat "$(review_key_file)" 2>/dev/null)"
  case "$c" in sk-ant-oat*) AUTHENV=(-e CLAUDE_CODE_OAUTH_TOKEN="$c");; *) AUTHENV=(-e ANTHROPIC_API_KEY="$c");; esac
}
