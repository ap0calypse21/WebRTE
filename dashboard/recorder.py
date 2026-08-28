# -*- coding: utf-8 -*-
"""
recorder - a flight recorder for the console.

Samples process list, sensors, fan and per-core CPU on a fixed loop into a
rolling buffer, diffs each sample against the one before it, and freezes the
whole window to disk when something goes wrong. The point is the seconds
*before* a crash: by the time the console is gone there is nothing left to ask.

On resolution, plainly: every sample costs an HTTP round trip, so the real
sampling period is tens of milliseconds at best -- not nanoseconds. Timestamps
are taken with perf_counter_ns because that ordering is exact and monotonic,
but the interval between samples is what it is. What nanosecond stamps buy you
is knowing which of two events came first, not measuring a bus transaction.

The one genuinely exact signal is the console's own time_uptime: when it goes
backwards, the console rebooted, and that is a crash with no interpretation
needed.
"""
import base64, json, os, socket, struct, threading, time

# Kernel offsets, verified live on 13.00. See PS4DEBUG-1300-PORT notes.
K_TIME_UPTIME   = 0x224C9B0
K_MP_MAXID      = 0x21D0F68
K_CPUID_TO_PCPU = 0x21ABFE0
PCPU_CP_TIME    = 0x1A0        # 5 x int64: user nice sys intr idle

KREAD_MAX = 4096               # the endpoint's own cap

DEFAULT_PERIOD = 0.4
DEFAULT_WINDOW = 900           # seconds of history to keep
POST_TRIGGER   = 20            # samples to keep collecting after an incident

# The console's HTTP server handles one connection at a time and takes roughly
# 60 ms per request, so cost is measured in requests, not bytes. /list and
# /sensors run every tick because a console that stops answering is the signal
# we care about; the rest moves slowly enough to sample every few ticks.
DEEP_EVERY = 4


class Recorder:
    def __init__(self, ip, port=771, outdir=None):
        self.ip, self.port = ip, port
        self.outdir = outdir or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                             "incidents")
        self.lock = threading.Lock()
        self.samples = []          # rolling window
        self.changes = []          # diffs worth showing
        self.incidents = []        # frozen windows written to disk
        self.seq = 0
        self.period = DEFAULT_PERIOD
        self.effective = DEFAULT_PERIOD    # after backoff
        self.deep_every = DEEP_EVERY
        self.tick = 0
        self.want = False
        self.deep = True           # also sample per-core cpu and the fan
        self.err = ""
        self.klog = None           # set by serve.py so incidents carry log lines
        self._thread = None
        self._kbase = None
        self._pcpu = None
        self._pending = 0          # samples still to collect after a trigger
        self._armed = None         # incident being filled
        self._last_deep = None
        self.stopped = ""          # why the loop last ended, if it did

    # ---- control -------------------------------------------------------
    def start(self, period=None, deep=None):
        if period:
            self.period = max(0.15, float(period))
        if deep is not None:
            self.deep = bool(deep)
        self.stopped = ""
        if self._thread and self._thread.is_alive():
            self.want = True
            return
        self.want = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self, why="asked to"):
        self.want = False
        self.stopped = why

    # ---- console access ------------------------------------------------
    def _get(self, path, timeout=6):
        s = socket.create_connection((self.ip, self.port), timeout=timeout)
        try:
            s.sendall(("GET %s HTTP/1.1\r\nHost: h\r\nConnection: close\r\n\r\n"
                       % path).encode())
            buf = b""
            s.settimeout(timeout)
            try:
                while True:
                    d = s.recv(16384)
                    if not d:
                        break
                    buf += d
            except socket.timeout:
                pass
        finally:
            s.close()
        return buf.partition(b"\r\n\r\n")[2]

    def _json(self, path):
        return json.loads(self._get(path))

    def _kread(self, addr, length):
        return base64.b64decode(self._get("/kread?address=%d&length=%d" % (addr, length)))

    def _prime(self):
        """Kernel base and the pcpu pointers change every boot, so re-read both."""
        self._kbase = self._json("/kernbase")["kernbase"]
        n = struct.unpack_from("<I", self._kread(self._kbase + K_MP_MAXID, 8), 0)[0] + 1
        n = min(n, 16)
        raw = self._kread(self._kbase + K_CPUID_TO_PCPU, n * 8)
        self._pcpu = [p for p in struct.unpack("<%dQ" % n, raw) if p]

    def _cp_times(self):
        """One read per group of pcpus that fits under the kread cap.

        The structs sit 0x300 apart, so several cores come back in a single
        request instead of one round trip each.
        """
        out = []
        i = 0
        while i < len(self._pcpu):
            base = self._pcpu[i] + PCPU_CP_TIME
            j = i
            while (j + 1 < len(self._pcpu)
                   and self._pcpu[j + 1] + PCPU_CP_TIME + 40 - base <= KREAD_MAX):
                j += 1
            span = self._pcpu[j] + PCPU_CP_TIME + 40 - base
            raw = self._kread(base, span)
            for k in range(i, j + 1):
                off = self._pcpu[k] + PCPU_CP_TIME - base
                out.append(list(struct.unpack_from("<5q", raw, off)))
            i = j + 1
        return out

    # ---- sampling ------------------------------------------------------
    def _sample(self, deep_now):
        t0 = time.perf_counter_ns()
        s = {"t": t0, "wall": time.time_ns(), "ok": False, "deep": bool(deep_now)}

        try:
            procs = self._json("/list")
            s["procs"] = {p["pid"]: p["name"] for p in procs}

            sen = self._json("/sensors")
            s["cpu_c"] = sen.get("cpu_temp_c")
            s["soc_c"] = sen.get("soc_temp_c")
            s["mhz"] = sen.get("cpu_freq_mhz")
            s["free_pages"] = sen.get("availpages")

            if self.deep and deep_now:
                if self._kbase is None:
                    self._prime()
                s["uptime"] = struct.unpack_from(
                    "<q", self._kread(self._kbase + K_TIME_UPTIME, 8), 0)[0]
                s["cp"] = self._cp_times()
                try:
                    f = self._json("/fan")
                    s["fan"] = f.get("op1")
                except (OSError, ValueError):
                    s["fan"] = None

            else:
                for k in ("uptime", "cp", "fan"):
                    if self._last_deep and k in self._last_deep:
                        s[k] = self._last_deep[k]

            if self.deep and deep_now:
                self._last_deep = {k: s[k] for k in ("uptime", "cp", "fan") if k in s}

            s["ok"] = True
            self.err = ""
        except (OSError, ValueError, KeyError, struct.error) as e:
            s["err"] = str(e)
            self.err = str(e)
            # A boot invalidates the cached pointers; force a re-prime.
            self._kbase = self._pcpu = None

        s["rtt_us"] = (time.perf_counter_ns() - t0) // 1000
        return s

    # ---- diffing -------------------------------------------------------
    def _diff(self, a, b):
        """What changed between two samples. Only things worth a line."""
        out = []

        if a["ok"] and not b["ok"]:
            out.append(("gone", "console stopped answering: %s" % b.get("err", "?")))
        elif not a["ok"] and b["ok"]:
            out.append(("back", "console answering again"))

        if not (a["ok"] and b["ok"]):
            return out

        ap, bp = a.get("procs", {}), b.get("procs", {})
        for pid in set(ap) - set(bp):
            out.append(("proc-", "exited: %s (pid %d)" % (ap[pid], pid)))
        for pid in set(bp) - set(ap):
            out.append(("proc+", "started: %s (pid %d)" % (bp[pid], pid)))

        # An uptime that went backwards is a reboot, full stop.
        ua, ub = a.get("uptime"), b.get("uptime")
        if ua is not None and ub is not None and ub < ua:
            out.append(("reboot", "uptime went %ds -> %ds: the console restarted" % (ua, ub)))

        for key, label, thr in (("cpu_c", "cpu", 3), ("soc_c", "soc", 3)):
            x, y = a.get(key), b.get(key)
            if x is not None and y is not None and abs(y - x) >= thr:
                out.append(("temp", "%s %d -> %d C" % (label, x, y)))

        if a.get("mhz") != b.get("mhz") and b.get("mhz"):
            out.append(("freq", "cpu %s -> %s MHz" % (a.get("mhz"), b.get("mhz"))))

        # The fan readback jitters by a couple of counts at a steady speed, so
        # only a real step is worth a line.
        fa, fb = a.get("fan"), b.get("fan")
        if fa and fb and abs(fb - fa) >= 16:
            out.append(("fan", "fan %s -> %s" % (fa, fb)))

        # Response time is itself a symptom: the console slows before it dies.
        # Only compare like with like -- every fourth tick does the extra kernel
        # reads and is legitimately several times slower.
        if a.get("deep") == b.get("deep") and b["rtt_us"] > max(400000, a["rtt_us"] * 4):
            out.append(("slow", "sample took %.0f ms (was %.0f)"
                        % (b["rtt_us"] / 1000, a["rtt_us"] / 1000)))

        ca, cb = a.get("cp"), b.get("cp")
        if ca and cb and len(ca) == len(cb):
            for i, (x, y) in enumerate(zip(ca, cb)):
                d = [q - p for p, q in zip(x, y)]
                tot = sum(d)
                if tot > 0:
                    busy = (1 - d[4] / tot) * 100
                    if busy >= 90:
                        out.append(("cpu", "core %d saturated at %.0f%%" % (i, busy)))
        return out

    TRIGGERS = {"gone", "reboot"}

    def _run(self):
        try:
            self._loop()
        except BaseException as e:
            # Silence here means the recorder looks alive and records nothing,
            # which is worse than it plainly being down.
            self.stopped = "loop died: %r" % (e,)
            self.want = False

    def _loop(self):
        os.makedirs(self.outdir, exist_ok=True)
        prev = None

        while self.want:
            started = time.perf_counter()
            self.tick += 1
            s = self._sample(self.deep and (self.tick % self.deep_every == 1))

            with self.lock:
                self.seq += 1
                s["seq"] = self.seq
                self.samples.append(s)

                # Trim by age, not by count, so the window means what it says.
                cutoff = s["t"] - DEFAULT_WINDOW * 1_000_000_000
                while self.samples and self.samples[0]["t"] < cutoff:
                    self.samples.pop(0)

                if prev is not None:
                    for kind, text in self._diff(prev, s):
                        self.changes.append({"seq": self.seq, "t": s["t"],
                                             "wall": s["wall"], "kind": kind, "text": text})
                        if kind in self.TRIGGERS and self._armed is None:
                            self._arm(kind, text, s)
                    if len(self.changes) > 4000:
                        del self.changes[:len(self.changes) - 4000]

                # Keep filling an armed incident, then write it out.
                if self._armed is not None:
                    self._pending -= 1
                    if self._pending <= 0:
                        self._write_incident()

            prev = s
            spent = time.perf_counter() - started
            if spent > self.period * 0.6:
                self.effective = min(8.0, max(self.period, spent / 0.6))
            else:
                self.effective = max(self.period, self.effective * 0.9)
            time.sleep(max(0.05, self.effective - spent))

    # ---- incidents -----------------------------------------------------
    def _arm(self, kind, text, s):
        self._armed = {"kind": kind, "text": text, "at": s["wall"], "seq": s["seq"]}
        self._pending = POST_TRIGGER

    def _write_incident(self):
        inc = self._armed
        self._armed = None

        # The whole retained window, so the slow drift before the event is
        # there too, not just the last second.
        window = list(self.samples)
        changes = [c for c in self.changes if c["seq"] >= inc["seq"] - 600]

        lines = []
        if self.klog is not None:
            try:
                lines = [l for l in self.klog.since(0, limit=4000)["lines"]][-600:]
            except Exception:
                lines = []

        name = "incident_%s_%s.json" % (
            time.strftime("%Y%m%d_%H%M%S", time.localtime(inc["at"] / 1e9)), inc["kind"])
        path = os.path.join(self.outdir, name)
        try:
            with open(path, "w") as fh:
                json.dump({"trigger": inc, "samples": window,
                           "changes": changes, "klog": lines}, fh)
        except OSError as e:
            self.err = "could not write incident: %s" % e
            return

        self.incidents.insert(0, {"file": name, "kind": inc["kind"],
                                  "text": inc["text"], "at": inc["at"],
                                  "samples": len(window), "changes": len(changes),
                                  "klog": len(lines)})
        del self.incidents[40:]

    # ---- readers -------------------------------------------------------
    def status(self, since=0, limit=400):
        with self.lock:
            latest = self.samples[-1] if self.samples else None
            span = 0
            if len(self.samples) > 1:
                span = (self.samples[-1]["t"] - self.samples[0]["t"]) / 1e9
            return {
                "want": self.want, "deep": self.deep, "period": self.period,
                "alive": bool(self._thread and self._thread.is_alive()),
                "stopped": self.stopped,
                "effective": round(self.effective, 2),
                "seq": self.seq, "samples": len(self.samples),
                "span_s": round(span, 1), "error": self.err,
                "ok": bool(latest and latest["ok"]),
                # A mean over the recent window: the instantaneous figure swings
                # between a light tick and a deep one and reads as instability.
                "rtt_ms": round(sum(x["rtt_us"] for x in self.samples[-24:])
                                / max(1, len(self.samples[-24:])) / 1000, 1)
                          if self.samples else None,
                "armed": self._armed is not None,
                "incidents": self.incidents,
                "changes": [c for c in self.changes if c["seq"] > since][-limit:],
            }

    def clear(self):
        with self.lock:
            self.samples, self.changes = [], []
