#!/usr/bin/env python3
"""token_meter.py — per-call OUTPUT-token cap for Claude Code agents (Claude, GLM, Kimi).

A Claude Code agent's ANTHROPIC_BASE_URL points here:  http://<meter>:8899/m/<call_id>/<upstream>
where <upstream> is one of anthropic | zai | kimi | gateway (an Anthropic-compatible model gateway, GATEWAY_HOST). Each request is forwarded (through the egress proxy) to the real
provider; the provider's own output-token count is read from the response (the final `message_delta` usage of a
streamed response, or `usage` of a plain JSON one) and added to the call's running total. Once the total reaches
the cap, further requests get an Anthropic-style 400 error and Claude Code ends the session. The transcript's
per-message usage cannot be used for this: Claude Code streams placeholder output counts (0-3% of the real total).

State: METER_DIR/<call_id>.json = {call_id, upstream, cap, output_tokens, input_tokens, requests, capped}.
Env: METER_PORT (8899), METER_DIR (/meter), OUTPUT_TOKEN_CAP (default cap, 250000), UPSTREAM_PROXY (host:port of the
egress CONNECT proxy; empty = direct). A request may override the cap with the header x-cyberdual-output-cap.
"""
import http.client
import http.server
import json
import os
import re
import socketserver
import threading
import time

PORT = int(os.environ.get('METER_PORT', '8899'))
DIR = os.environ.get('METER_DIR', '/meter')
CAP = int(os.environ.get('OUTPUT_TOKEN_CAP', '250000'))
PROXY = os.environ.get('UPSTREAM_PROXY', '')
UPSTREAMS = {'anthropic': ('api.anthropic.com', ''), 'zai': (os.environ.get('ZAI_HOST', ''), os.environ.get('ZAI_PATH', '')),
             'kimi': ('api.moonshot.ai', '/anthropic'),
             'gateway': (os.environ.get('GATEWAY_HOST', ''), '')}
HOP = {'host', 'connection', 'keep-alive', 'proxy-connection', 'transfer-encoding', 'upgrade', 'accept-encoding',
       'content-length', 'te', 'trailer', 'x-cyberdual-output-cap'}
LOCK = threading.Lock()
ROUTE = re.compile(r'^/m/([A-Za-z0-9_.-]{1,120})/(anthropic|zai|kimi|gateway)(/.*)?$')


def state_path(cid):
    return os.path.join(DIR, cid + '.json')


def load(cid, upstream, cap):
    try:
        with open(state_path(cid)) as fh:
            return json.load(fh)
    except (OSError, ValueError):
        return dict(call_id=cid, upstream=upstream, cap=cap, output_tokens=0, input_tokens=0, requests=0, capped=False,
                    started=time.time())


def save(st):
    tmp = state_path(st['call_id']) + '.tmp'
    with open(tmp, 'w') as fh:
        json.dump(st, fh)
    os.replace(tmp, state_path(st['call_id']))


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *a):
        pass

    def _error(self, code, msg):
        body = json.dumps({'type': 'error', 'error': {'type': 'invalid_request_error', 'message': msg}}).encode()
        self.send_response(code)
        self.send_header('content-type', 'application/json')
        self.send_header('content-length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _proxy(self):
        m = ROUTE.match(self.path.split('?')[0])
        if not m:
            return self._error(404, 'token_meter: unknown route')
        cid, up, rest = m.group(1), m.group(2), (m.group(3) or '/')
        query = ('?' + self.path.split('?', 1)[1]) if '?' in self.path else ''
        cap = int(self.headers.get('x-cyberdual-output-cap') or CAP)
        with LOCK:
            st = load(cid, up, cap)
            st['cap'] = cap
            if st['output_tokens'] >= cap:
                st['capped'] = True
                save(st)
                capped = True
            else:
                capped = False
        if capped:
            return self._error(400, f'CYBERDUAL output-token cap reached ({st["output_tokens"]} >= {cap}); this call is over.')
        n = int(self.headers.get('content-length') or 0)
        body = self.rfile.read(n) if n else None
        host, base = UPSTREAMS[up]
        if PROXY:
            ph, pp = PROXY.rsplit(':', 1)
            conn = http.client.HTTPSConnection(ph, int(pp), timeout=3600)
            conn.set_tunnel(host, 443)
        else:
            conn = http.client.HTTPSConnection(host, 443, timeout=3600)
        headers = {k: v for k, v in self.headers.items() if k.lower() not in HOP}
        headers['host'] = host
        try:
            conn.request(self.command, base + rest + query, body=body, headers=headers)
            resp = conn.getresponse()
        except OSError as exc:
            return self._error(502, f'token_meter: upstream error {exc}')
        self.send_response(resp.status)
        for k, v in resp.getheaders():
            if k.lower() not in HOP:
                self.send_header(k, v)
        self.send_header('transfer-encoding', 'chunked')
        self.end_headers()
        out_tok = in_tok = 0          # this request's provider-reported totals so far
        counted = [0, 0]              # what has already been added to the call's state (output, input)

        def bump(final=False):        # add the not-yet-counted part now, so an interrupted request keeps its count
            d_out, d_in = out_tok - counted[0], in_tok - counted[1]
            if d_out <= 0 and d_in <= 0 and not final:
                return
            with LOCK:
                st = load(cid, up, cap)
                st['output_tokens'] += max(0, d_out)
                st['input_tokens'] += max(0, d_in)
                if final:
                    st['requests'] += 1
                st['last'] = time.time()
                if st['output_tokens'] >= st['cap']:
                    st['capped'] = True
                save(st)
            counted[0] += max(0, d_out); counted[1] += max(0, d_in)
        buf = b''
        sse = 'text/event-stream' in (resp.getheader('content-type') or '')
        client_gone = False
        whole = []
        while True:
            chunk = resp.read1(65536)
            if not chunk:
                break
            try:
                self.wfile.write(b'%x\r\n%s\r\n' % (len(chunk), chunk))
                self.wfile.flush()
            except OSError:
                client_gone = True
            if sse:
                buf += chunk
                *lines, buf = buf.split(b'\n')
                for ln in lines:
                    if not ln.startswith(b'data:'):
                        continue
                    try:
                        ev = json.loads(ln[5:])
                    except ValueError:
                        continue
                    if ev.get('type') == 'message_start':
                        u = (ev.get('message') or {}).get('usage') or {}
                        in_tok = max(in_tok, u.get('input_tokens') or 0)
                        out_tok = max(out_tok, u.get('output_tokens') or 0)
                        bump()
                    elif ev.get('type') == 'message_delta':
                        u = ev.get('usage') or {}
                        out_tok = max(out_tok, u.get('output_tokens') or 0)
                        in_tok = max(in_tok, u.get('input_tokens') or 0)
                        bump()
            else:
                whole.append(chunk)
        if not client_gone:
            try:
                self.wfile.write(b'0\r\n\r\n')
                self.wfile.flush()
            except OSError:
                pass
        if not sse and whole:
            try:
                u = json.loads(b''.join(whole)).get('usage') or {}
                out_tok, in_tok = u.get('output_tokens') or 0, u.get('input_tokens') or 0
            except (ValueError, AttributeError):
                pass
        bump(final=True)

    do_POST = do_GET = do_PUT = do_DELETE = _proxy


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


if __name__ == '__main__':
    os.makedirs(DIR, exist_ok=True)
    Server(('0.0.0.0', PORT), Handler).serve_forever()
