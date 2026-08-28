# -*- coding: utf-8 -*-
"""
klogbridge - keeps a live connection to GoldHEN's klog server and hands the
lines to the dashboard.

A browser cannot open a raw TCP socket, so the local proxy holds the connection
instead and the page polls for whatever is new. Lines are stamped on arrival
with time.time_ns(), because when a console dies the ordering of the last few
lines is the whole story.
"""
import re, socket, threading, time

KLOG_PORT = 3232
RING = 20000          # lines kept in memory
RECONNECT_WAIT = 3.0
CHURN_SECS = 5.0      # a session shorter than this means someone else has the port

# What counts as the console being in trouble. Kept deliberately narrow: a
# matcher that fires on ordinary boot noise trains you to ignore it.
PATTERNS = [
    # Tight on purpose. The first version matched "terminat" and "crash", which
    # turned one ordinary reboot into fifty alerts -- SceShellUI logs a hundred
    # "Disposing JobQueue ... on terminate" lines and owns a scene literally
    # named CrashReport. A matcher that cries wolf is worse than none.
    ("panic",  re.compile(r"\bpanic:|Fatal trap \d|double fault|\bKDB:\s|"
                          r"Uptime: \d+[dhms]", re.I)),
    ("fault",  re.compile(r"page fault while in kernel|general protection fault|"
                          r"invalid opcode|\bTrap \d+, code=|"
                          r"supervisor (read|write), page not present", re.I)),
    ("signal", re.compile(r"\bSIG(SEGV|BUS|ILL|ABRT|FPE)\b|received signal \d+|"
                          r"core dumped", re.I)),
    ("crash",  re.compile(r"\bcrashed\b|crashed/died|\bcoredump\b|"
                          r"assertion fail|\babort\(\)", re.I)),
    ("mem",    re.compile(r"out of memory|no space left|kmem_map too small|"
                          r"vm_fault:\s", re.I)),
]


class KlogBridge:
    def __init__(self, ip, port=KLOG_PORT):
        self.ip, self.port = ip, port
        self.lock = threading.Lock()
        self.lines = []            # (seq, ns, level, text)
        self.seq = 0
        self.connected = False
        self.last_error = ""
        self.want = False
        self.attempts = 0
        self.churn = 0            # consecutive sessions that died almost at once
        self._thread = None

    # ---- control -------------------------------------------------------
    def start(self):
        if self._thread and self._thread.is_alive():
            self.want = True
            return
        self.want = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self.want = False

    # ---- reader --------------------------------------------------------
    def _run(self):
        while self.want:
            try:
                s = socket.create_connection((self.ip, self.port), timeout=5)
            except OSError as e:
                self.attempts += 1
                self.connected = False
                self.last_error = str(e)
                self._add("-- klog: %s (is the klog server enabled in GoldHEN?)" % e,
                          "meta")
                for _ in range(int(RECONNECT_WAIT * 10)):
                    if not self.want:
                        return
                    time.sleep(0.1)
                continue

            self.connected = True
            self.last_error = ""
            opened = time.time()
            if self.churn < 2:
                self._add("-- klog: connected to %s:%d" % (self.ip, self.port), "meta")
            s.settimeout(1.0)
            buf = b""

            try:
                while self.want:
                    try:
                        d = s.recv(8192)
                    except socket.timeout:
                        continue
                    if not d:
                        if self.churn < 2:
                            self._add("-- klog: connection closed by the console", "meta")
                        break
                    buf += d
                    # Stamp on arrival, and keep a trailing partial line for the
                    # next read rather than emitting half a message.
                    while b"\n" in buf:
                        raw, buf = buf.split(b"\n", 1)
                        self._add(raw.decode("utf-8", "replace").rstrip("\r"))
            except OSError as e:
                self._add("-- klog: %s" % e, "meta")
            finally:
                try:
                    s.close()
                except OSError:
                    pass
                self.connected = False

                # GoldHEN's klog server serves one client at a time, so a
                # session that ends in seconds means something else took it.
                if time.time() - opened < CHURN_SECS:
                    self.churn += 1
                    if self.churn == 2:
                        self._add("-- klog: the connection keeps being taken away. "
                                  "GoldHEN's klog server allows one client at a time - "
                                  "close klog.py or any other listener, then reconnect.",
                                  "meta")
                else:
                    self.churn = 0

            wait = RECONNECT_WAIT * (4 if self.churn >= 2 else 1)
            for _ in range(int(wait * 10)):
                if not self.want:
                    return
                time.sleep(0.1)

    def _add(self, text, level=None):
        if level is None:
            level = ""
            for name, rx in PATTERNS:
                if rx.search(text):
                    level = name
                    break
        with self.lock:
            self.seq += 1
            self.lines.append((self.seq, time.time_ns(), level, text))
            if len(self.lines) > RING:
                del self.lines[:len(self.lines) - RING]

    # ---- readers -------------------------------------------------------
    def since(self, seq, limit=2000):
        with self.lock:
            out = [l for l in self.lines if l[0] > seq][:limit]
            return {
                "connected": self.connected,
                "want": self.want,
                "seq": self.seq,
                "error": self.last_error,
                "churn": self.churn,
                "lines": [{"seq": a, "ns": b, "level": c, "text": d} for a, b, c, d in out],
            }

    def clear(self):
        with self.lock:
            self.lines = []
