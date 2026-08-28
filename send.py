#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
send.py - sends webrte.bin to the console and checks that it came up.

  python send.py 192.168.1.50
  python send.py 192.168.1.50 --payload path/to/webrte.bin

Standard library only, so it runs anywhere Python 3 does. GoldHEN's payload
receiver listens on 9090; WebRTE itself then listens on 771.
"""
import argparse, socket, sys, time

PAYLOAD_PORT = 9090          # GoldHEN's receiver
WEBRTE_PORT = 771            # what the payload opens once it is running


def send(ip, path):
    with open(path, "rb") as fh:
        data = fh.read()

    print("payload : %s (%d bytes)" % (path, len(data)))
    print("sending : %s:%d" % (ip, PAYLOAD_PORT))

    s = socket.create_connection((ip, PAYLOAD_PORT), timeout=10)
    try:
        s.sendall(data)
    finally:
        s.close()
    print("sent, watch the klog now\n")


def wait(ip, seconds=40):
    print("waiting for port %d (up to %ds)..." % (WEBRTE_PORT, seconds))
    t0 = time.time()
    while time.time() - t0 < seconds:
        try:
            socket.create_connection((ip, WEBRTE_PORT), timeout=2).close()
            print("port %d open after %.1fs" % (WEBRTE_PORT, time.time() - t0))
            return True
        except OSError:
            time.sleep(1)
    return False


def check(ip):
    s = socket.create_connection((ip, WEBRTE_PORT), timeout=10)
    try:
        s.sendall(b"GET /list HTTP/1.1\r\nHost: h\r\nConnection: close\r\n\r\n")
        buf = b""
        s.settimeout(10)
        try:
            while True:
                d = s.recv(8192)
                if not d:
                    break
                buf += d
        except socket.timeout:
            pass
    finally:
        s.close()

    body = buf.partition(b"\r\n\r\n")[2]
    return body.count(b'"pid"')


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ip", help="the console's IP - check Settings > Network")
    ap.add_argument("--payload", default="webrte.bin")
    ap.add_argument("--no-wait", action="store_true", help="send and exit")
    a = ap.parse_args()

    try:
        send(a.ip, a.payload)
    except FileNotFoundError:
        print("no such file: %s" % a.payload)
        return 2
    except OSError as e:
        print("could not send: %s" % e)
        print("  Is the console on, jailbroken, and is GoldHEN's payload")
        print("  receiver listening on %d?" % PAYLOAD_PORT)
        return 2

    if a.no_wait:
        return 0

    time.sleep(3)
    if not wait(a.ip):
        print("\nWebRTE is not listening. The klog's last line says which stage failed:")
        print("  'patching kernel'   -> the kernel patches")
        print("  'loading kdebugger' -> kernel symbols for this firmware")
        print("  'loading webrte'    -> libkernel thread-creation offsets")
        print("  a crash and reboot  -> thread offsets are wrong")
        return 1

    try:
        n = check(a.ip)
    except OSError as e:
        print("port opened but /list failed: %s" % e)
        return 1

    print("/list returned %d processes\n" % n)
    print("dashboard: python dashboard/serve.py --ip %s" % a.ip)
    return 0


if __name__ == "__main__":
    sys.exit(main())
