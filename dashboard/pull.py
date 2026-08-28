#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pull.py - pulls the live kernel or a file off the console.

  python pull.py kernel                       # both mapped windows -> kernel_live.bin
  python pull.py kernel --out k.bin
  python pull.py file /system/common/lib/libkernel.sprx
  python pull.py dir /system/common/lib        # every file in one directory

The browser can do this too, but not the whole kernel: /kdump caps one request
at 8 MB and the data window is larger than that, so this loops instead.

A dumped image is not the same thing as the firmware file on disk. This is the
kernel as it is running: patched bytes, resolved pointers, and the .bss tables
that only exist at runtime.
"""
import argparse, json, os, struct, sys, time
import urllib.error, urllib.parse, urllib.request

CHUNK = 0x400000          # 4 MB per request, half the endpoint's cap


def api(ip, port, path, timeout=600):
    url = "http://%s:%d/%s" % (ip, port, path)
    return urllib.request.urlopen(url, timeout=timeout)


def api_json(ip, port, path):
    with api(ip, port, path, timeout=30) as r:
        return json.load(r)


def human(n):
    for u in ("B", "KB", "MB", "GB"):
        if n < 1024 or u == "GB":
            return "%.1f %s" % (n, u)
        n /= 1024.0


def bar(done, total, t0):
    pct = done / total * 100 if total else 0
    w = 34
    filled = int(w * pct / 100)
    rate = done / max(1e-6, time.time() - t0)
    sys.stdout.write("\r  [%s%s] %5.1f%%  %s  %s/s   " % (
        "#" * filled, "." * (w - filled), pct, human(done), human(rate)))
    sys.stdout.flush()


def pull_range(ip, port, offset, length, fh):
    """Loops /kdump because one request is capped at 8 MB."""
    done = 0
    t0 = time.time()
    while done < length:
        n = min(CHUNK, length - done)
        with api(ip, port, "kdump?address=%d&length=%d" % (offset + done, n)) as r:
            got = 0
            while got < n:
                buf = r.read(min(1 << 16, n - got))
                if not buf:
                    break
                fh.write(buf)
                got += len(buf)
                bar(done + got, length, t0)
        if not got:
            print("\n  short read at +0x%X - stopping" % (offset + done))
            break
        done += got
    print()
    return done


def write_elf(out, kernbase, windows, chunks):
    """Wraps the windows in an ELF so a disassembler maps them where they live.

    The two windows are 0x8218A8 apart in memory. Written back to back in a flat
    file they land next to each other instead, and every data address then reads
    low by that gap -- which is silently wrong rather than visibly broken.
    """
    EHDR, PHDR = 0x40, 0x38
    body = EHDR + PHDR * len(windows)
    body = (body + 0xFFF) & ~0xFFF          # page-align the first segment

    ph = b""
    off = body
    for w, data in zip(windows, chunks):
        flags = 5 if w["name"] == "text" else 6      # r-x / rw-
        ph += struct.pack("<IIQQQQQQ", 1, flags, off,
                          kernbase + w["offset"], kernbase + w["offset"],
                          len(data), len(data), 0x1000)
        off += len(data)

    eh = b"\x7fELF" + bytes([2, 1, 1, 9]) + b"\0" * 8
    eh += struct.pack("<HHIQQQIHHHHHH",
                      2, 62, 1,                      # ET_EXEC, EM_X86_64
                      kernbase,                      # entry
                      EHDR, 0, 0,                    # phoff, shoff, flags
                      EHDR, PHDR, len(windows),
                      0x40, 0, 0)

    with open(out, "wb") as fh:
        fh.write(eh)
        fh.write(ph)
        fh.write(b"\0" * (body - fh.tell()))
        for data in chunks:
            fh.write(data)
    return out


def cmd_elf(a):
    """Rebuilds an ELF from a dump that was already pulled."""
    meta_path = a.path + ".json"
    if not os.path.exists(meta_path):
        print("need %s next to the dump - it records where each window came from" % meta_path)
        return 1

    meta = json.load(open(meta_path))
    raw = open(a.path, "rb").read()
    windows = meta["windows"]

    total = sum(w["length"] for w in windows)
    if len(raw) != total:
        print("%s is %d bytes, the windows add up to %d - refusing to guess"
              % (a.path, len(raw), total))
        return 1

    chunks, pos = [], 0
    for w in windows:
        chunks.append(raw[pos:pos + w["length"]])
        pos += w["length"]

    out = a.out or os.path.splitext(a.path)[0] + ".elf"
    write_elf(out, meta["kernbase"], windows, chunks)
    print("wrote %s (%s)" % (out, human(os.path.getsize(out))))
    for w in windows:
        print("  %-5s vaddr 0x%X  %s" % (w["name"], meta["kernbase"] + w["offset"],
                                         human(w["length"])))
    return 0


def cmd_kernel(a):
    info = api_json(a.ip, a.port, "kdump")
    print("kernel base 0x%X" % info["kernbase"])
    for w in info["windows"]:
        print("  %-5s +0x%-9X %s" % (w["name"], w["offset"], human(w["length"])))

    out = a.out or "kernel_live.bin"
    total = 0
    with open(out, "wb") as fh:
        for w in info["windows"]:
            print("\n%s:" % w["name"])
            # Windows are written back to back. They are not contiguous in
            # memory, so the offsets in this file are not kbase-relative --
            # the sidecar records where each one came from.
            total += pull_range(a.ip, a.port, w["offset"], w["length"], fh)

    meta = out + ".json"
    with open(meta, "w") as fh:
        json.dump({"kernbase": info["kernbase"], "windows": info["windows"],
                   "note": "windows are concatenated in order; see offset/length"},
                  fh, indent=2)

    print("\nwrote %s (%s) and %s" % (out, human(total), meta))

    # An ELF as well, because a flat file loads at the wrong addresses.
    raw = open(out, "rb").read()
    chunks, pos = [], 0
    for w in info["windows"]:
        chunks.append(raw[pos:pos + w["length"]])
        pos += w["length"]
    elf = write_elf(os.path.splitext(out)[0] + ".elf",
                    info["kernbase"], info["windows"], chunks)
    print("wrote %s - load this one in IDA" % elf)
    return 0


def cmd_file(a):
    st = api_json(a.ip, a.port, "fstat?path=" + urllib.parse.quote(a.path))
    if "error" in st:
        print("%s: %s" % (a.path, st["error"]))
        return 1

    size = st["size"]
    out = a.out or os.path.basename(a.path.rstrip("/")) or "download.bin"
    print("%s  %s" % (a.path, human(size)))

    t0 = time.time()
    done = 0
    with api(a.ip, a.port, "dl?path=" + urllib.parse.quote(a.path)) as r, open(out, "wb") as fh:
        while True:
            buf = r.read(1 << 16)
            if not buf:
                break
            fh.write(buf)
            done += len(buf)
            bar(done, size, t0)
    print("\nwrote %s (%s)" % (out, human(done)))
    return 0


def cmd_dir(a):
    ls = api_json(a.ip, a.port, "ls?path=" + urllib.parse.quote(a.path))
    if "error" in ls:
        print("%s: %s" % (a.path, ls["error"]))
        return 1

    files = [e for e in ls["entries"] if not e["dir"]]
    outdir = a.out or os.path.basename(a.path.rstrip("/")) or "dump"
    os.makedirs(outdir, exist_ok=True)
    print("%s: %d files -> %s/\n" % (a.path, len(files), outdir))

    for i, e in enumerate(files, 1):
        src = a.path.rstrip("/") + "/" + e["name"]
        dst = os.path.join(outdir, e["name"])
        print("[%d/%d] %s  %s" % (i, len(files), e["name"], human(e["size"])))
        t0 = time.time()
        done = 0
        try:
            with api(a.ip, a.port, "dl?path=" + urllib.parse.quote(src)) as r, open(dst, "wb") as fh:
                while True:
                    buf = r.read(1 << 16)
                    if not buf:
                        break
                    fh.write(buf)
                    done += len(buf)
                    bar(done, e["size"] or 1, t0)
            print()
        except (OSError, urllib.error.URLError) as ex:
            print("\n  failed: %s" % ex)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ip", default="192.168.8.8")
    ap.add_argument("--port", type=int, default=771)
    sub = ap.add_subparsers(dest="cmd", required=True)

    k = sub.add_parser("kernel", help="dump both mapped kernel windows")
    k.add_argument("--out")
    k.set_defaults(fn=cmd_kernel)

    f = sub.add_parser("file", help="download one file")
    f.add_argument("path")
    f.add_argument("--out")
    f.set_defaults(fn=cmd_file)

    d = sub.add_parser("dir", help="download every file in one directory")
    d.add_argument("path")
    d.add_argument("--out")
    d.set_defaults(fn=cmd_dir)

    e = sub.add_parser("elf", help="rebuild an ELF from a dump already on disk")
    e.add_argument("path", help="the .bin; its .json sidecar must sit beside it")
    e.add_argument("--out")
    e.set_defaults(fn=cmd_elf)

    a = ap.parse_args()
    try:
        return a.fn(a)
    except urllib.error.HTTPError as e:
        print("HTTP %s - is this payload webrte-1.014 or newer?" % e.code)
        return 1
    except OSError as e:
        print("cannot reach %s:%d - %s" % (a.ip, a.port, e))
        return 1


if __name__ == "__main__":
    sys.exit(main())
