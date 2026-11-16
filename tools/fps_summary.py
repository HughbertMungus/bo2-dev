"""Summarize mp_fps.txt (REX_FPS_LOG=1, one line per second) for bo2-dev A/B runs.

Usage: python -I tools/fps_summary.py run/mp_fps.txt [--from HH:MM:SS] [--to HH:MM:SS] [--min-fps N]
Seconds below --min-fps (default 5) are treated as loading screens/menus and dropped.
Prints seconds counted, mean, median, p5, p1, min and seconds below 60.
"""
import argparse
import re
import statistics
import sys

LINE = re.compile(r"^(\d\d:\d\d:\d\d) fps=([\d.]+)")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--from", dest="start")
    ap.add_argument("--to", dest="end")
    ap.add_argument("--min-fps", type=float, default=5.0)
    args = ap.parse_args()
    values = []
    with open(args.path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.match(line)
            if not m:
                continue
            t, fps = m.group(1), float(m.group(2))
            if args.start and t < args.start:
                continue
            if args.end and t > args.end:
                continue
            if fps >= args.min_fps:
                values.append(fps)
    if not values:
        print("no samples")
        return 1
    s = sorted(values)

    def pct(p: float) -> float:
        return s[min(len(s) - 1, int(p / 100.0 * len(s)))]

    print(
        f"n={len(s)}s mean={statistics.fmean(s):.1f} median={statistics.median(s):.1f} "
        f"p5={pct(5):.1f} p1={pct(1):.1f} min={s[0]:.1f} below60={sum(v < 60 for v in s)}s"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
