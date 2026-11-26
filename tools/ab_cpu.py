"""In-match A/B of CPU use: alternates two settings files into run\\live.txt and measures the
game's process CPU (in cores) and frame rate (run\\mp_fps.txt, needs REX_FPS_LOG=1) per window.

Usage: python -I tools/ab_cpu.py A.txt B.txt [--seconds 15] [--cycles 5] [--settle 3]

Each window: write the file, wait --settle seconds for live_settings.cpp (polls once a second) and
the threads to adapt, then measure for the rest of the window. Leaves A in effect at the end.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import re
import statistics
import time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
k32.OpenProcess.restype = wt.HANDLE
k32.GetProcessTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(wt.FILETIME)] * 4
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUN = os.path.join(ROOT, "run")
FPS_LINE = re.compile(r"^(\d\d:\d\d:\d\d) fps=([\d.]+)")


def find_pid(name="defaultmp.exe"):
    out = os.popen(f'tasklist /FI "IMAGENAME eq {name}" /FO CSV /NH').read()
    m = re.search(r'"' + re.escape(name) + r'","(\d+)"', out, re.I)
    return int(m.group(1)) if m else None


def cpu_seconds(handle):
    t = [wt.FILETIME() for _ in range(4)]
    if not k32.GetProcessTimes(handle, *[ctypes.byref(x) for x in t]):
        raise OSError(ctypes.get_last_error())
    ft = lambda f: (f.dwHighDateTime << 32 | f.dwLowDateTime) / 1e7
    return ft(t[2]) + ft(t[3])


def fps_between(start, end):
    path = os.path.join(RUN, "mp_fps.txt")
    if not os.path.exists(path):
        return []
    values = []
    for line in open(path, encoding="utf-8", errors="replace"):
        m = FPS_LINE.match(line)
        if m and start <= m.group(1) < end and float(m.group(2)) >= 5:
            values.append(float(m.group(2)))
    return values


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--seconds", type=int, default=15)
    ap.add_argument("--cycles", type=int, default=5)
    ap.add_argument("--settle", type=int, default=3)
    args = ap.parse_args()
    files = {"A": open(args.a).read(), "B": open(args.b).read()}
    pid = find_pid()
    if not pid:
        raise SystemExit("defaultmp.exe is not running")
    handle = k32.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    live = os.path.join(RUN, "live.txt")
    results = {"A": [], "B": []}
    for cycle in range(1, args.cycles + 1):
        for tag in ("A", "B"):
            with open(live, "w", newline="") as f:
                f.write(files[tag])
            time.sleep(args.settle)
            t0, c0, s0 = time.perf_counter(), cpu_seconds(handle), time.strftime("%H:%M:%S")
            time.sleep(args.seconds - args.settle)
            t1, c1, s1 = time.perf_counter(), cpu_seconds(handle), time.strftime("%H:%M:%S")
            cores = (c1 - c0) / (t1 - t0)
            fps = fps_between(s0, s1)
            results[tag].append((cores, fps))
            med = statistics.median(fps) if fps else float("nan")
            print(f"{s1} cycle {cycle} {tag}: {cores:.2f} cores, fps median {med:.1f} "
                  f"min {min(fps) if fps else float('nan'):.1f}", flush=True)
    with open(live, "w", newline="") as f:
        f.write(files["A"])
    for tag in ("A", "B"):
        cores = [c for c, _ in results[tag]]
        fps = [v for _, f in results[tag] for v in f]
        print(f"{tag}: cores mean {statistics.mean(cores):.2f} (min {min(cores):.2f}, max "
              f"{max(cores):.2f}); fps median {statistics.median(fps) if fps else float('nan'):.1f}"
              f", p5 {sorted(fps)[len(fps) // 20] if fps else float('nan'):.1f}")
    wins = sum(b[0] < a[0] for a, b in zip(results["A"], results["B"]))
    print(f"B used less CPU in {wins}/{args.cycles} cycles")


if __name__ == "__main__":
    main()
