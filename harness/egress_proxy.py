#!/usr/bin/env python3
"""Minimal HTTPS-CONNECT allowlist proxy for the SEC-bench worker egress firewall.
Only permits CONNECT to allow-listed hosts on :443 (default api.anthropic.com). Everything else -> 403.
The worker runs on an --internal docker network (no direct internet) with HTTPS_PROXY pointed here, so its
ONLY reachable destination is the Claude API — it cannot fetch the public CVE fix. Run: python3 egress_proxy.py
Env: ALLOW_HOSTS (comma-sep, default api.anthropic.com), PROXY_PORT (8888)."""
import os, socket, threading, select

ALLOW = set(h.strip() for h in os.environ.get("ALLOW_HOSTS", "api.anthropic.com").split(",") if h.strip())
PORT = int(os.environ.get("PROXY_PORT", "8888"))

# ---- black-box completeness oracle bridge --------------------------------------------------------
# The ONLY extra destination we allow: a single sentinel host `oracle.local` that we forward to the
# HOST-side oracle daemon (oracle_daemon.py). This proxy runs INSIDE the secbproxy container, so the
# host is reachable at the container's default gateway. The daemon returns exactly one word
# (COMPLETE/INCOMPLETE/INCONCLUSIVE) over TLS; it never leaks PoC content. This is the only bridge
# from the --internal network to the host, so the oracle inherits the API-egress trust boundary.
ORACLE_SENTINEL = "oracle.local"
ORACLE_PORT = int(os.environ.get("ORACLE_PORT", "8899"))


def _host_gateway():
    if os.environ.get("ORACLE_HOST"):
        return os.environ["ORACLE_HOST"]
    try:  # default-route gateway as seen from inside this container == the docker host
        import struct
        with open("/proc/net/route") as f:
            for ln in f.readlines()[1:]:
                p = ln.split()
                if len(p) > 2 and p[1] == "00000000":
                    return socket.inet_ntoa(struct.pack("<L", int(p[2], 16)))
    except Exception:
        pass
    return "172.17.0.1"


ORACLE_HOST = _host_gateway()


def pipe(a, b):
    try:
        while True:
            r, _, _ = select.select([a, b], [], [], 120)
            if not r:
                break
            for s, d in ((a, b), (b, a)):
                if s in r:
                    buf = s.recv(65536)
                    if not buf:
                        return
                    d.sendall(buf)
    except Exception:
        pass
    finally:
        for s in (a, b):
            try:
                s.close()
            except Exception:
                pass


def handle(c, addr):
    up = None
    try:
        req = b""
        while b"\r\n\r\n" not in req:
            chunk = c.recv(4096)
            if not chunk:
                c.close(); return
            req += chunk
        line = req.split(b"\r\n", 1)[0].decode("latin1")
        parts = line.split()
        if len(parts) < 2 or parts[0].upper() != "CONNECT":
            c.sendall(b"HTTP/1.1 405 Method Not Allowed\r\n\r\n"); c.close(); return
        hostport = parts[1]
        host = hostport.rsplit(":", 1)[0]
        port = int(hostport.rsplit(":", 1)[1]) if ":" in hostport else 443
        if host == ORACLE_SENTINEL and port == 443:
            print(f"ORACLE {ORACLE_HOST}:{ORACLE_PORT}", flush=True)
            up = socket.create_connection((ORACLE_HOST, ORACLE_PORT), timeout=15)
            c.sendall(b"HTTP/1.1 200 Connection established\r\n\r\n")
            pipe(c, up); return
        if host not in ALLOW or port != 443:
            print(f"DENY {host}:{port} from {addr[0]}", flush=True)
            c.sendall(b"HTTP/1.1 403 Forbidden (egress allowlist)\r\n\r\n"); c.close(); return
        print(f"ALLOW {host}:{port}", flush=True)
        up = socket.create_connection((host, port), timeout=15)
        c.sendall(b"HTTP/1.1 200 Connection established\r\n\r\n")
        pipe(c, up)
    except Exception as e:
        try:
            c.sendall(b"HTTP/1.1 502 Bad Gateway\r\n\r\n")
        except Exception:
            pass
        try:
            c.close()
        except Exception:
            pass


def main():
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", PORT)); s.listen(128)
    print(f"egress allowlist proxy :{PORT} | ALLOW={sorted(ALLOW)}", flush=True)
    while True:
        c, addr = s.accept()
        threading.Thread(target=handle, args=(c, addr), daemon=True).start()


if __name__ == "__main__":
    main()
