#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
serve.py - serves the WebRTE dashboard locally and proxies the API to the console.

  python serve.py                       # opens the browser
  python serve.py --ip 192.168.8.8 --port 8080

Proxying removes every CORS and mixed-content problem: the browser only ever
talks to localhost, and this script is what talks to the PS4.

Responses are streamed rather than buffered, because /dl and /kdump can carry
tens of megabytes and reading those into memory first would stall the page and
blow the old 20 second timeout.
"""
import argparse, http.server, socketserver, urllib.request, urllib.error
import json, os, shutil, sys, threading, webbrowser
from urllib.parse import parse_qs, urlparse

from klogbridge import KlogBridge
from recorder import Recorder

HERE = os.path.dirname(os.path.abspath(__file__))

# /dl and /kdump move real volume; everything else should answer promptly.
SLOW_OPS = ("dl", "kdump")
FAST_TIMEOUT, SLOW_TIMEOUT = 25, 900

# One bridge and one recorder per process, started on first use.
klog = None
rec = None
_rec_lock = threading.Lock()


class Handler(http.server.SimpleHTTPRequestHandler):
    ps4 = "192.168.8.8"
    ps4_port = 771
    protocol_version = "HTTP/1.1"

    def __init__(self, *a, **k):
        super().__init__(*a, directory=HERE, **k)

    def do_GET(self):
        if not self.path.startswith("/api/"):
            # Compare without the query string: "/?tab=klog" is still the root,
            # and matching on the raw path served a directory listing instead.
            path, _, query = self.path.partition("?")
            if path in ("/", ""):
                self.path = "/dashboard.html" + (("?" + query) if query else "")
            return super().do_GET()

        rest = self.path[5:]
        op = rest.split("?", 1)[0]

        if op == "klog":
            return self._klog(rest)

        if op == "rec":
            return self._rec(rest)

        timeout = SLOW_TIMEOUT if op in SLOW_OPS else FAST_TIMEOUT
        target = "http://%s:%d/%s" % (self.ps4, self.ps4_port, rest)

        try:
            r = urllib.request.urlopen(target, timeout=timeout)
        except urllib.error.HTTPError as e:
            body = e.read()
            self._head(e.code, "text/plain", len(body))
            self._write(body)
            return
        except OSError as e:
            body = ("cannot reach %s:%d - %s\n\nIs WebRTE still loaded? "
                    "The payload does not survive a reboot."
                    % (self.ps4, self.ps4_port, e)).encode()
            self._head(502, "text/plain; charset=utf-8", len(body))
            self._write(body)
            return

        with r:
            ctype = r.headers.get("Content-Type", "application/json")
            clen = r.headers.get("Content-Length")

            # A download deserves a filename the browser will actually use.
            extra = {}
            if op in SLOW_OPS:
                extra["Content-Disposition"] = 'attachment; filename="%s"' % _name_for(op, rest)

            self._head(200, ctype, int(clen) if clen is not None else None, extra)
            try:
                shutil.copyfileobj(r, self.wfile, 1 << 16)
            except (BrokenPipeError, ConnectionResetError):
                pass

    def _klog(self, rest):
        global klog
        q = parse_qs(urlparse(rest).query)
        arg = lambda k, d=None: (q.get(k) or [d])[0]

        if klog is None:
            klog = KlogBridge(self.ps4)

        action = arg("action", "poll")
        if action == "start":
            klog.start()
        elif action == "stop":
            klog.stop()
        elif action == "clear":
            klog.clear()

        try:
            since = int(arg("since", "0"))
        except ValueError:
            since = 0

        body = json.dumps(klog.since(since)).encode()
        self._head(200, "application/json", len(body))
        self._write(body)

    def _rec(self, rest):
        global rec, klog
        q = parse_qs(urlparse(rest).query)
        arg = lambda k, d=None: (q.get(k) or [d])[0]

        # Two requests can land at once on a threading server, and both
        # creating a Recorder would leave the started one orphaned.
        with _rec_lock:
            if rec is None:
                rec = Recorder(self.ps4, self.ps4_port)

        # The recorder folds klog lines into an incident, so hand it the bridge
        # whenever one exists.
        if klog is not None:
            rec.klog = klog

        action = arg("action", "status")
        if action == "start":
            deep = arg("deep", "1") not in ("0", "false")
            rec.start(period=float(arg("period", "0.4")), deep=deep)
        elif action == "stop":
            rec.stop()
        elif action == "clear":
            rec.clear()
        elif action == "incident":
            return self._incident(arg("file", ""))

        try:
            since = int(arg("since", "0"))
        except ValueError:
            since = 0

        body = json.dumps(rec.status(since)).encode()
        self._head(200, "application/json", len(body))
        self._write(body)

    def _incident(self, name):
        """Serves one frozen window back, by name only -- never a path."""
        global rec
        safe = os.path.basename(name or "")
        path = os.path.join(rec.outdir, safe)
        if not safe.startswith("incident_") or not os.path.exists(path):
            body = b'{"error": "no such incident"}'
            self._head(404, "application/json", len(body))
            self._write(body)
            return

        with open(path, "rb") as fh:
            body = fh.read()
        self._head(200, "application/json", len(body))
        self._write(body)

    def _head(self, code, ctype, length, extra=None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        if length is None:
            self.send_header("Connection", "close")
            self.close_connection = True
        else:
            self.send_header("Content-Length", str(length))
        self.send_header("Cache-Control", "no-store")
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()

    def _write(self, body):
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, fmt, *args):
        line = fmt % args
        if "/api/" in line:
            sys.stderr.write("  %s\n" % line)


def _name_for(op, rest):
    """A sensible download name: the file's own for /dl, the range for /kdump."""
    from urllib.parse import parse_qs, urlparse
    q = parse_qs(urlparse(rest).query)
    if op == "dl":
        p = (q.get("path") or ["download.bin"])[0]
        return p.rstrip("/").split("/")[-1] or "download.bin"
    addr = (q.get("address") or ["0"])[0]
    length = (q.get("length") or ["0"])[0]
    try:
        return "kernel_%s_%s.bin" % (hex(int(addr, 0)), hex(int(length, 0)))
    except ValueError:
        return "kernel.bin"


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", default="192.168.8.8", help="console address")
    ap.add_argument("--ps4-port", type=int, default=771, help="WebRTE port")
    ap.add_argument("--port", type=int, default=8080, help="local dashboard port")
    ap.add_argument("--no-browser", action="store_true")
    a = ap.parse_args()

    Handler.ps4, Handler.ps4_port = a.ip, a.ps4_port
    url = "http://127.0.0.1:%d/" % a.port

    print("dashboard : %s" % url)
    print("console   : http://%s:%d" % (a.ip, a.ps4_port))
    print("stop      : Ctrl+C\n")

    if not a.no_browser:
        threading.Timer(0.8, lambda: webbrowser.open(url)).start()

    try:
        with Server(("127.0.0.1", a.port), Handler) as srv:
            srv.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped.")
    except OSError as e:
        print("cannot listen on port %d: %s" % (a.port, e))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
