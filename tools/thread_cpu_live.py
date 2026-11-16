"""Per-thread CPU of the running game, by thread name (no admin needed).

Usage: python -I tools/thread_cpu_live.py [seconds=10] [names_out=dumps/threads.txt]

Reads every defaultmp.exe thread's user/kernel time (GetThreadTimes) at the start and end of the
window and prints the busiest threads with their names (SetThreadDescription, which the runtime
sets for guest and host threads). Also writes "tid<TAB>name" for all threads to names_out, which
tools/kernel_trace_analyze.py uses to label threads.
"""
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
TH32CS_SNAPPROCESS, TH32CS_SNAPTHREAD = 0x2, 0x4
THREAD_QUERY_LIMITED_INFORMATION = 0x0800


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.c_void_p), ("th32ModuleID", wt.DWORD),
                ("cntThreads", wt.DWORD), ("th32ParentProcessID", wt.DWORD),
                ("pcPriClassBase", ctypes.c_long), ("dwFlags", wt.DWORD),
                ("szExeFile", ctypes.c_wchar * 260)]


class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ThreadID", wt.DWORD),
                ("th32OwnerProcessID", wt.DWORD), ("tpBasePri", ctypes.c_long),
                ("tpDeltaPri", ctypes.c_long), ("dwFlags", wt.DWORD)]


k32.CreateToolhelp32Snapshot.restype = wt.HANDLE
k32.OpenThread.restype = wt.HANDLE
k32.GetThreadDescription.argtypes = [wt.HANDLE, ctypes.POINTER(ctypes.c_wchar_p)]
k32.GetThreadTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(wt.FILETIME)] * 4


def find_pid(name="defaultmp.exe"):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    e = PROCESSENTRY32W()
    e.dwSize = ctypes.sizeof(e)
    ok = k32.Process32FirstW(snap, ctypes.byref(e))
    pid = None
    while ok:
        if e.szExeFile.lower() == name:
            pid = e.th32ProcessID
        ok = k32.Process32NextW(snap, ctypes.byref(e))
    k32.CloseHandle(snap)
    return pid


def thread_ids(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    e = THREADENTRY32()
    e.dwSize = ctypes.sizeof(e)
    ok = k32.Thread32First(snap, ctypes.byref(e))
    tids = []
    while ok:
        if e.th32OwnerProcessID == pid:
            tids.append(e.th32ThreadID)
        ok = k32.Thread32Next(snap, ctypes.byref(e))
    k32.CloseHandle(snap)
    return tids


def ft(f):
    return (f.dwHighDateTime << 32) | f.dwLowDateTime


def sample(tids, handles):
    out = {}
    for tid in tids:
        h = handles.get(tid)
        if not h:
            continue
        c, e, kt, ut = wt.FILETIME(), wt.FILETIME(), wt.FILETIME(), wt.FILETIME()
        if k32.GetThreadTimes(h, ctypes.byref(c), ctypes.byref(e), ctypes.byref(kt),
                              ctypes.byref(ut)):
            out[tid] = (ft(ut), ft(kt))
    return out


def main():
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0
    names_out = sys.argv[2] if len(sys.argv) > 2 else os.path.join("dumps", "threads.txt")
    pid = find_pid()
    if not pid:
        sys.exit("defaultmp.exe is not running")
    tids = thread_ids(pid)
    handles, names = {}, {}
    for tid in tids:
        h = k32.OpenThread(THREAD_QUERY_LIMITED_INFORMATION, False, tid)
        if not h:
            continue
        handles[tid] = h
        p = ctypes.c_wchar_p()
        names[tid] = ""
        if k32.GetThreadDescription(h, ctypes.byref(p)) >= 0 and p.value:
            names[tid] = p.value
            k32.LocalFree(p)
    a = sample(tids, handles)
    t0 = time.perf_counter()
    time.sleep(seconds)
    b = sample(tids, handles)
    span = (time.perf_counter() - t0) * 1e7  # 100 ns units
    rows = []
    for tid in b:
        if tid in a:
            u = (b[tid][0] - a[tid][0]) / span * 100.0
            k = (b[tid][1] - a[tid][1]) / span * 100.0
            rows.append((u + k, u, k, tid))
    rows.sort(reverse=True)
    total = sum(r[0] for r in rows)
    print("pid %d, %d threads, %.1f s window, total %.0f%% of one core (%.1f cores)" %
          (pid, len(rows), seconds, total, total / 100.0))
    print("  %-7s %6s %6s %6s  %s" % ("tid", "cpu%", "user%", "kern%", "name"))
    for cpu, u, k, tid in rows[:25]:
        print("  %-7d %6.1f %6.1f %6.1f  %s" % (tid, cpu, u, k, names.get(tid, "")))
    os.makedirs(os.path.dirname(names_out) or ".", exist_ok=True)
    with open(names_out, "w", encoding="utf-8") as f:
        for tid in tids:
            f.write("%d\t%s\n" % (tid, names.get(tid, "")))
    for h in handles.values():
        k32.CloseHandle(h)


if __name__ == "__main__":
    main()
