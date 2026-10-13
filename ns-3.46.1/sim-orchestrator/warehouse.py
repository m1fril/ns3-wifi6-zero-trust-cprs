#!/usr/bin/env python3
"""Warehouse example (article, section «склад 100 × 100 м»): 28 nodes = 3 gateways + 25 mesh points
on a 20 m grid, Linux-like peer-link timers, 120 s runs.

  python3 warehouse.py [out_dir] [--seeds N] [--parallel N]   -> <out_dir>/<run>/ + <out_dir>/results.csv

Load sweep: total offered 100…2000 pkt/s, 128 B and 512 B packets.
Failure sweep at 250 pkt/s: k random mesh points (with their devices) fail at t = 60 s, or the central gateway.
PDR windows: pre = (10, 60], post = (65, 120].
"""
import argparse, csv, random, statistics as st, subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
BIN = HERE.parent / "build/scratch/network-project/ns3.46.1-network-project-main-optimized"
P = "--ns3::dot11s::PeerLink::"
BASE = ["--topology=mesh", "--meshLayout=grid", "--meshSpacing=20", "--nAps=3", "--nStas=25", "--compactMetrics=1",
        f"{P}MaxBeaconLoss=10", f"{P}MaxPacketFailure=10", f"{P}RetryTimeout=+100ms", f"{P}ConfirmTimeout=+100ms",
        f"{P}HoldingTimeout=+100ms", f"{P}MaxRetries=3", "--meshClusterB=0", "--duration=120"]
N, EVENT = 25, 60.0


def run(out, name, pps, size, seed, fail=(), fail_gw=False):
    d = out / name
    d.mkdir(parents=True, exist_ok=True)
    mean = round(N / pps * 1e6)  # per-node mean interval, us
    lines = [f"0.000000 TRAFFIC sta:{i} size=const:{size} interval=exp:{mean}:{mean * 10}" for i in range(N)]
    lines += [f"{EVENT:.6f} NODE sta:{i} down" for i in fail]
    if fail_gw:
        lines.append(f"{EVENT:.6f} NODE ap:1 down")
    (d / "timeline.txt").write_text("\n".join(lines) + "\n")
    if not list(d.glob("Bianchi-*.csv")):
        with open(d / "out.txt", "w") as log:
            subprocess.run([str(BIN), *BASE, f"--RngRun={seed}", "--timeline=timeline.txt"], cwd=d, stdout=log, stderr=subprocess.STDOUT, check=True)
    per = {}
    for r in csv.DictReader(open(next(d.glob("Bianchi-*.csv")))):
        if r["Action"] in ("SENT_COUNT", "RECV_COUNT", "LATENCY_P95_MS", "HOPS_MEAN"):
            per.setdefault(round(float(r["Time_s"])), {})[r["Action"]] = float(r["Value"] or 0)

    def win(lo, hi):
        s = [v for t, v in per.items() if lo < t <= hi and "SENT_COUNT" in v]
        sent, recv = sum(v["SENT_COUNT"] for v in s), sum(v.get("RECV_COUNT", 0) for v in s)
        p95 = [v["LATENCY_P95_MS"] for v in s if v.get("LATENCY_P95_MS")]
        hops = [v["HOPS_MEAN"] for v in s if v.get("HOPS_MEAN")]
        return {"pdr": recv / sent if sent else None, "p95_ms": st.median(p95) if p95 else None,
                "hops": st.fmean(hops) if hops else None, "delivered_pps": recv / max(1, len(s))}
    pre, post = win(10, EVENT), win(EVENT + 5, 120)
    return {"run": name, "pps": pps, "size": size, "seed": seed, "failed_points": len(fail), "failed_gateway": int(fail_gw),
            **{f"pre_{k}": v for k, v in pre.items()}, **{f"post_{k}": v for k, v in post.items()}}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?", default=str(HERE / "runs/warehouse"))
    ap.add_argument("--seeds", type=int, default=5)
    ap.add_argument("--parallel", type=int, default=9)
    a = ap.parse_args()
    out = Path(a.out)
    jobs = []
    for seed in range(1, a.seeds + 1):
        for pps in (100, 250, 500, 1000, 2000):
            for size in (128, 512):
                jobs.append((f"load_{pps}_{size}_s{seed}", pps, size, seed))
        for k in (0, 5, 10, 15, 20):
            fail = random.Random(f"wh:{k}:{seed}").sample(range(N), k)
            jobs.append((f"fail_{k}_s{seed}", 250, 128, seed, fail))
        jobs.append((f"failgw_s{seed}", 250, 128, seed, (), True))
    with ThreadPoolExecutor(a.parallel) as ex:
        rows = list(ex.map(lambda j: run(out, *j), jobs))
    with open(out / "results.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    print("written", out / "results.csv", len(rows), "runs")


if __name__ == "__main__":
    main()
