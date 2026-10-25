"""agent_run.sh keeps the library arm's docker argv: run_agent is exercised with a fake `docker` on PATH for every
provider (claude, codex, kimi, glm, glm via the gateway, grok, GLM_AGENT=codex) and compared with
testdata/agent_run_argv.json, which was recorded from agent_run.sh before the provider code moved to
agent_provider.sh (origin/main 795a694cc). Dummy key files only; no model is called.

  python3 -m unittest test_agent_run_argv                      # compare with the fixture
  python3 test_agent_run_argv.py --record OLD_agent_run.sh      # (re)record the fixture from a given agent_run.sh
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
FIXTURE = HERE / 'testdata' / 'agent_run_argv.json'

CASES = [
    ('claude-opus-4-8', {}),
    ('claude-sonnet-5', {'BUDGET': '25'}),
    ('claude-opus-4-8', {'OUTPUT_TOKEN_CAP': ''}),
    ('gpt-6-sol', {}),
    ('codex', {'CODEX_MODEL': 'gpt-6.1-sol'}),
    ('gpt-6-sol', {'CYBERDUAL_CODEX_EFFORT': 'high', 'OUTPUT_TOKEN_CAP': ''}),
    ('kimi-k3', {}),
    ('glm-5.3', {}),
    ('glm-5.3', {'GLM_VIA': 'gateway'}),
    ('glm-5.3', {'GLM_VIA': 'gateway', 'GLM_MAX_THINKING': '32000', 'GLM_EXTRA_BODY': '{"x":1}'}),
    ('glm-5.3', {'GLM_AGENT': 'codex', 'GLM_VIA': 'gateway', 'CYBERDUAL_CODEX_EFFORT': 'high'}),
    ('grok-4.7', {}),
    ('grok-4.7', {'GATEWAY_KEY_FILE': '/nonexistent/key'}),
]

FAKE_DOCKER = '''#!/usr/bin/env bash
{ printf 'ARGV'; for a in "$@"; do printf '\\x1f%s' "$a"; done; printf '\\x1e'; } >> "$FAKE_DOCKER_LOG"
exit 0
'''

SCRIPT = r'''
set -u
source "$AGENT_RUN"
IMAGE=cyberdual-agent:test; NETOPTS="--network cyberdual_egress -e HTTPS_PROXY=http://cyberdual_proxy:8888"
EGRESS_PROXY=cyberdual_proxy; AUTHENV=(-e ANTHROPIC_API_KEY)
printf 'system text\n' > "$T/sys.md"; mkdir -p "$T/wk"
run_agent "$T/wk" "$T/sys.md" "the task" "$MODEL" bypassPermissions "$T/out/tr.jsonl" --disallowedTools WebSearch WebFetch
'''


def normalize(text, tmp):
    text = text.replace(tmp, '<T>')
    text = text.replace(f'{os.getuid()}:{os.getgid()}', '<UID>:<GID>')   # the fixture is host-independent
    text = re.sub(r'/tmp/(codexhome|cchome)\.[A-Za-z0-9]{6}', r'/tmp/\1.XXXXXX', text)
    text = re.sub(r'codex_codexhome\.[A-Za-z0-9]{6}', 'codex_codexhome.XXXXXX', text)
    text = re.sub(r'/m/\d+_\d+_\d+', '/m/<meter_id>', text)
    return text


def capture(agent_run, model, extra):
    with tempfile.TemporaryDirectory(prefix='argv_') as tmp:
        t = Path(tmp)
        (t / 'bin').mkdir(); (t / 'out').mkdir(); (t / 'meter').mkdir(); (t / 'codexsrc').mkdir()
        (t / 'bin' / 'docker').write_text(FAKE_DOCKER); (t / 'bin' / 'docker').chmod(0o755)
        for name, value in (('kimi', 'dummy-kimi-key'), ('zai', 'dummy-zai-key'), ('gw', 'dummy-gateway-key')):
            (t / f'{name}.key').write_text(value + '\n')
        (t / 'codexsrc' / 'auth.json').write_text('{"dummy": true}\n'); (t / 'codexsrc' / 'config.toml').write_text('# dummy\n')
        env = {k: v for k, v in os.environ.items() if not k.startswith(('GLM_', 'CYBERDUAL_', 'CODEX_', 'GATEWAY_', 'KIMI_',
                                                                         'ZAI_', 'METER', 'OUTPUT_TOKEN', 'WALL', 'BUDGET'))}
        env.update(PATH=f"{t / 'bin'}:{env['PATH']}", FAKE_DOCKER_LOG=str(t / 'docker.log'), AGENT_RUN=str(agent_run),
                   T=tmp, MODEL=model, KIMI_KEY_FILE=str(t / 'kimi.key'), ZAI_KEY_FILE=str(t / 'zai.key'),
                   GATEWAY_KEY_FILE=str(t / 'gw.key'), GATEWAY_BASE_URL='https://gateway.example.invalid',
                   ZAI_BASE_URL='https://zai.example.invalid/api/anthropic', CODEX_HOME_SRC=str(t / 'codexsrc'), METER_DIR=str(t / 'meter'),
                   OUTPUT_TOKEN_CAP='250000', BUDGET='none', WALL='21600', CODEX_BUDGET_POLL='1000')
        env.update(extra)
        if env.get('OUTPUT_TOKEN_CAP') == '':
            env.pop('OUTPUT_TOKEN_CAP')
        # no pipes: the Codex watcher's sleep would hold them open after the call returns
        subprocess.run(['bash', '-c', SCRIPT], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
        log = (t / 'docker.log').read_text() if (t / 'docker.log').exists() else ''
        calls = [c.split('\x1f')[1:] for c in log.split('\x1e') if c.startswith('ARGV')]
        files = sorted(p.name for p in (t / 'out').iterdir())
        tr = (t / 'out' / 'tr.jsonl').read_text() if (t / 'out' / 'tr.jsonl').exists() else ''
        return json.loads(normalize(json.dumps(dict(calls=calls, files=files, transcript=tr)), tmp))


def key(model, extra):
    return model + ' ' + ' '.join(f'{k}={v}' for k, v in sorted(extra.items()))


class AgentRunArgvTests(unittest.TestCase):
    def test_argv_matches_recorded_library_argv(self):
        want = json.loads(FIXTURE.read_text())
        for model, extra in CASES:
            with self.subTest(case=key(model, extra)):
                self.assertEqual(capture(HERE / 'agent_run.sh', model, extra), want[key(model, extra)])

    def test_keys_never_reach_argv_for_codex_gateway(self):
        got = capture(HERE / 'agent_run.sh', 'glm-5.3', {'GLM_AGENT': 'codex', 'GLM_VIA': 'gateway'})
        self.assertNotIn('dummy-gateway-key', json.dumps(got['calls']))


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--record':
        FIXTURE.parent.mkdir(exist_ok=True)
        FIXTURE.write_text(json.dumps({key(m, e): capture(Path(sys.argv[2]).resolve(), m, e) for m, e in CASES}, indent=1) + '\n')
        print(f'recorded {len(CASES)} cases from {sys.argv[2]} -> {FIXTURE}')
    else:
        unittest.main()
