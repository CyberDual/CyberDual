#!/usr/bin/env bash
# Bring up the CyberDual egress firewall: an --internal docker network (no direct internet) + a proxy
# container that only permits CONNECT to api.anthropic.com. Role-A / reviewer containers attach to the
# internal net with HTTPS_PROXY -> the proxy, so their ONLY reachable destination is the Claude API —
# they cannot fetch the maintainer fix / CVE from GitHub. Idempotent. Prints the env for workers.
#
#   NET (default cyberdual_egress) · PROXY container cyberdual_proxy · ALLOW_HOSTS (default api.anthropic.com)
set -uo pipefail
NET="${NET:-cyberdual_egress}"
PROXY="${PROXY:-cyberdual_proxy}"
EGRESS_PY="${EGRESS_PY:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/egress_proxy.py}"
# Allow the MODEL endpoints for every provider we might use as Role A / reviewer — NOT the wider net.
# Extend via ALLOW_HOSTS="a,b,c". (egress_proxy matches the CONNECT host exactly.)
ALLOW="${ALLOW_HOSTS:-api.anthropic.com,api.openai.com,chatgpt.com,api.moonshot.ai,generativelanguage.googleapis.com,api.x.ai,openrouter.ai}"
# provider endpoints configured by env var (RUNNING.md): GATEWAY_BASE_URL (grok-*, GLM_VIA=gateway), ZAI_BASE_URL (glm-*)
_host () { printf '%s' "$1" | sed -E 's#^[a-z]+://##; s#/.*##'; }
_path () { printf '%s' "$1" | sed -E 's#^[a-z]+://[^/]*##; s#/$##'; }
GATEWAY_HOST="${GATEWAY_HOST:-$(_host "${GATEWAY_BASE_URL:-}")}"; ZAI_HOST="${ZAI_HOST:-$(_host "${ZAI_BASE_URL:-}")}"
ZAI_PATH="${ZAI_PATH:-$(_path "${ZAI_BASE_URL:-}")}"
[ -z "${ALLOW_HOSTS:-}" ] && for _h in "$GATEWAY_HOST" "$ZAI_HOST"; do [ -n "$_h" ] && ALLOW="$ALLOW,$_h"; done

docker network inspect "$NET" >/dev/null 2>&1 || docker network create --internal "$NET" >/dev/null
# (re)start the proxy: start on the DEFAULT bridge (has internet to reach the API), then also attach the
# internal net so workers can reach it by name. stdlib-only python image.
if ! docker ps --format '{{.Names}}' | grep -qx "$PROXY"; then
  docker rm -f "$PROXY" >/dev/null 2>&1 || true
  docker run -d --name "$PROXY" --restart unless-stopped \
    -e ALLOW_HOSTS="$ALLOW" -e PROXY_PORT=8888 \
    -v "$EGRESS_PY":/egress.py:ro python:3-slim python3 /egress.py >/dev/null
  docker network connect "$NET" "$PROXY" 2>/dev/null || true
  sleep 2
fi
# Output-token meter (token_meter.py): Claude Code agents reach their provider through it when OUTPUT_TOKEN_CAP is set.
# It sits on the internal net and forwards through the allow-list proxy, so it adds no new egress.
METER="${METER:-${PROXY}_meter}"; METER_PY="$(dirname "$EGRESS_PY")/token_meter.py"; METER_DIR="${METER_DIR:-/tmp/cyberdual_meter}"
mkdir -p "$METER_DIR"
if [ -f "$METER_PY" ] && ! docker ps --format '{{.Names}}' | grep -qx "$METER"; then
  docker rm -f "$METER" >/dev/null 2>&1 || true
  docker run -d --name "$METER" --restart unless-stopped --network "$NET" --user "$(id -u):$(id -g)" \
    -e UPSTREAM_PROXY="$PROXY:8888" -e METER_DIR=/meter -e OUTPUT_TOKEN_CAP="${OUTPUT_TOKEN_CAP:-250000}" \
    -e GATEWAY_HOST="$GATEWAY_HOST" -e ZAI_HOST="$ZAI_HOST" -e ZAI_PATH="$ZAI_PATH" \
    -v "$METER_DIR":/meter -v "$METER_PY":/token_meter.py:ro python:3-slim python3 /token_meter.py >/dev/null
fi
echo "NET=$NET PROXY=$PROXY METER=$METER ALLOW=$ALLOW"
echo "worker env: --network $NET -e HTTPS_PROXY=http://$PROXY:8888 -e HTTP_PROXY=http://$PROXY:8888"
