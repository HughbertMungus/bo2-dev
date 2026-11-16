"""Summarize an in-match A/B from mp_fps.txt by the switch tags on each line.

Usage: python -I tools/ab_summary.py run/mp_fps.txt [--from HH:MM:SS] [--to HH:MM:SS] [--drop 2]
                                     [--toggle-log ab_toggle_output.txt]
--toggle-log adds an ab=A / ab=B tag from ab_toggle.ps1's output ("HH:MM:SS cycle i/n: A"), for
switches the fps log does not tag (settings outside the GPU, such as runtime.xma_decode_threads).
Groups consecutive seconds with the same tags into rounds, drops the first --drop seconds after
each switch and seconds under 5 fps (loading), then prints each round and a mean per tag set.
"""
import argparse
import re
import statistics

LINE = re.compile(r"^(\d\d:\d\d:\d\d) fps=([\d.]+) frames=\d+ total=\d+ ?(.*)$")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--from", dest="start")
    ap.add_argument("--to", dest="end")
    ap.add_argument("--drop", type=int, default=2)
    ap.add_argument("--toggle-log")
    args = ap.parse_args()
    switches = []
    if args.toggle_log:
        for line in open(args.toggle_log, encoding="utf-8-sig", errors="replace"):
            m = re.match(r"^(\d\d:\d\d:\d\d) (?:cycle \d+/\d+: (\w+)|done)", line.strip())
            if m:
                switches.append((m.group(1), m.group(2) or "A"))
    rounds = []
    for line in open(args.path, encoding="utf-8", errors="replace"):
        m = LINE.match(line.strip())
        if not m:
            continue
        t, fps, tags = m.group(1), float(m.group(2)), m.group(3).strip()
        if (args.start and t < args.start) or (args.end and t > args.end):
            continue
        if switches:
            side = None
            for st, name in switches:
                if st <= t:
                    side = name
            if side is None:
                continue
            tags = (tags + " ab=" + side).strip()
        if not rounds or rounds[-1]["tags"] != tags:
            rounds.append({"tags": tags, "start": t, "fps": []})
        rounds[-1]["fps"].append(fps)
    per_tags = {}
    for r in rounds:
        v = [x for x in r["fps"][args.drop:] if x >= 5]
        if len(v) < 3:
            continue
        mean = statistics.fmean(v)
        print(f"{r['start']}  {r['tags']:<32} n={len(v):3d} mean={mean:7.1f} min={min(v):7.1f}")
        per_tags.setdefault(r["tags"], []).append(mean)
    print()
    for tags, means in per_tags.items():
        print(f"{tags:<32} rounds={len(means)} mean of means={statistics.fmean(means):7.1f}")


if __name__ == "__main__":
    main()
