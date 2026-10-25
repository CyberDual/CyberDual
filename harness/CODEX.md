# Running claude-code OR codex as either role

Both roles (Role A attacker, Role B reviewer) can run as **claude-code** or **codex**, in the **same
docker image** with the same build toolchain and the same egress control. The provider is inferred
from the model name.

## Image

`Dockerfile.agent` ships both agents. `./cyberdual build` builds it as
`cyberdual-agent:cc2.1.293-codex0.159.2` and passes it to every driver as `IMAGE`:

```bash
./cyberdual build
```

It is `node:22-bookworm` + `@anthropic-ai/claude-code` + `@openai/codex` (both pinned) + the build toolchain
(`git build-essential cmake clang`, meson, ninja and the link libraries). A driver run by hand without `IMAGE` uses
the agent image tag `cyberdual-agent:cc2.1.293-codex0.159.2`.

## Dispatch

`agent_run.sh` is sourced by every driver and exposes `run_agent`:

```
run_agent <workdir> <system.md> <task> <model> <permission-mode> <transcript> [extra claude args...]
```

`agent_provider` routes by model name: `gpt-*` / `o<N>` / `codex` → **codex**; anything else →
**claude-code**. So you pick the agent just by setting the model:

```bash
ATT_MODEL=gpt-6-sol REV=claude-sonnet-5 ...   # codex attacker, claude reviewer
ATT_MODEL=claude-opus-4-8 REV=gpt-6-sol  ...    # claude attacker, codex reviewer
```

### claude-code path
`docker run … claude -p <task> --model <m> --append-system-prompt … --permission-mode <perm>`, with
auth from `KEY_FILE` and egress via the proxy (`HTTPS_PROXY`).

### codex path
`docker run … codex exec --cd /work -m <m> --dangerously-bypass-approvals-and-sandbox`. Notes:
- **Docker is the sandbox.** codex's own landlock sandbox cannot initialise inside a container, so we
  bypass it and rely on the container for isolation (same as claude running `--permission-mode
  bypassPermissions`).
- **Auth** is a per-run writable copy of the host `~/.codex` (`auth.json` + `config.toml`) mounted at
  `CODEX_HOME`; it is never baked into the image. codex authenticates with the `OPENAI_API_KEY` in
  `auth.json` and reaches `api.openai.com` **through the egress proxy** (already on the allowlist).
- Override the default model for the literal `codex` alias with `CODEX_MODEL`.

## Where it is wired

- `run_month.sh` — multi-commit attacker + reviewer
- `run_single.sh` — single-commit from-scratch attacker + reviewer
- `run_single_golden.sh` — single-commit golden-given attacker + reviewer
- `rereview_any.sh` — replay any reviewer model against a stored valid attack

All four `source agent_run.sh`.
