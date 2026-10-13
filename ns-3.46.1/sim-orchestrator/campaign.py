#!/usr/bin/env python3
"""Experiment campaigns on the mesh topology: ~100 runs per mode, parameters drawn per run.

  python3 campaign.py <mode|all> [--n N] [--parallel 8] [--duration 300] [--seed 1] [--dry-run]

modes:
  normal          intact network, artificial delay/jitter/loss on a random share of routers
  single_failure  one node fails for good: a gateway of three, or the bridge between two clusters
  collapse        network-wide failure: the only gateway, 70-95% of routers, or a traffic overload
  load            packet size x send rate grid on a fixed 5x5 mesh
  density         random placement, 15-40 nodes on 60..400 m squares, light churn

Each run lands in runs/campaign-<mode>/<idx>/ with params.json (what was drawn, the event time and
target) and config.json; parameters come from random.Random("<mode>:<seed>:<idx>"), so a campaign
is reproducible. Analysis: analyze_campaign.py.
"""

import argparse
import copy
import json
import logging
import math
import random
import sys
from pathlib import Path

import loadgen
import orchestrator

HERE = Path(__file__).resolve().parent
RADIO_RANGE_M = 51.0  # 802.11a, 16 dBm, log-distance n=3, -82 dBm detection (README)
MODES = ("normal", "single_failure", "collapse", "load", "density")
DEFAULT_N = {"normal": 100, "single_failure": 100, "collapse": 100, "load": 100, "density": 120}


def base_config(duration, sets=()):
    cfg = orchestrator.load_config(str(HERE / "config.mesh.yaml"), [f"sim.duration={duration}", *sets])
    cfg["churn"], cfg["network"], cfg["script"] = {}, {}, []
    cfg["traffic"]["bursts"] = {"rate_per_s": 0.0}
    cfg["traffic"]["profile_swap_rate_per_s"] = 0.0
    return cfg


def scale_load(cfg, m):
    """Send m times as often: every interval parameter divided by m."""
    for p in cfg["traffic"]["profiles"].values():
        for key in ("value", "mean", "min", "max"):
            if key in p["interval_us"]:
                p["interval_us"][key] = round(p["interval_us"][key] / m)


def mesh(cfg, n, gateways, layout="grid", spacing=30, room=150, cluster_b=0):
    cfg["sim"]["args"].update(nStas=n, nAps=gateways, meshLayout=layout, meshSpacing=spacing,
                              roomSize=round(room), meshClusterB=cluster_b)


def normal(cfg, rng, i, dur):
    n, g, spacing, m = rng.randint(16, 36), rng.randint(1, 3), rng.randint(25, 40), round(rng.uniform(0.5, 1.5), 2)
    mesh(cfg, n, g, spacing=spacing)
    scale_load(cfg, m)
    delayed = sorted(rng.sample(range(n), max(1, round(n * rng.uniform(0.2, 0.6)))))
    cfg["network"]["rules"] = [
        {"at": 0, "until": dur, "target": f"sta:{s}", "delay_ms": round(rng.uniform(5, 80), 1),
         "jitter_ms": round(rng.uniform(0, 20), 1), "loss": round(rng.uniform(0, 0.02), 4)} for s in delayed]
    cfg["traffic"]["bursts"] = {"rate_per_s": 0.02, "nodes": [1, 3], "packets": [50, 150], "gap_us": 2000}
    return {"routers": n, "gateways": g, "spacing_m": spacing, "load_x": m, "delayed_routers": len(delayed)}


def single_failure(cfg, rng, i, dur):
    n, m, t = rng.randint(18, 33), round(rng.uniform(0.5, 1.2), 2), round(rng.uniform(60, 120), 1)
    scale_load(cfg, m)
    if i % 2 == 0:  # one of three gateways fails; its routers keep sending to it (no failover)
        spacing, k = rng.randint(28, 38), rng.randrange(3)
        mesh(cfg, n, 3, spacing=spacing)
        cfg["script"] = [f"{t} NODE ap:{k} down"]
        return {"variant": "gateway", "routers": n, "gateways": 3, "spacing_m": spacing, "load_x": m,
                "event_t": t, "target": f"ap:{k}"}
    spacing, nb = rng.randint(28, 40), round(n * rng.uniform(0.25, 0.45))
    mesh(cfg, n, 1, layout="corridor", spacing=spacing, cluster_b=nb)
    cfg["script"] = [f"{t} NODE sta:{n - 1} down"]  # the bridge is the last router
    return {"variant": "bridge", "routers": n, "gateways": 1, "spacing_m": spacing, "load_x": m,
            "cluster_b": nb, "event_t": t, "target": f"sta:{n - 1}"}


def collapse(cfg, rng, i, dur):
    t = round(rng.uniform(60, 120), 1)
    variant = ("gateway_only", "mass_failure", "overload")[i % 3]
    if variant == "gateway_only":
        n, spacing, m = rng.randint(16, 36), rng.randint(25, 40), round(rng.uniform(0.5, 1.5), 2)
        mesh(cfg, n, 1, spacing=spacing)
        scale_load(cfg, m)
        cfg["script"] = [f"{t} NODE ap:0 down"]
        return {"variant": variant, "routers": n, "gateways": 1, "spacing_m": spacing, "load_x": m,
                "event_t": t, "target": "ap:0"}
    if variant == "mass_failure":
        n, g, spacing, m = rng.randint(20, 36), rng.randint(1, 3), rng.randint(25, 40), round(rng.uniform(0.5, 1.5), 2)
        share = round(rng.uniform(0.7, 0.95), 2)
        mesh(cfg, n, g, spacing=spacing)
        scale_load(cfg, m)
        failed = sorted(rng.sample(range(n), round(n * share)))
        cfg["script"] = [f"{round(t + rng.uniform(0, 2), 3)} NODE sta:{s} down" for s in failed]
        return {"variant": variant, "routers": n, "gateways": g, "spacing_m": spacing, "load_x": m,
                "event_t": t, "failed_share": share, "failed": len(failed)}
    n, g, spacing = rng.randint(16, 30), rng.randint(1, 2), rng.randint(28, 38)
    interval = rng.randint(1000, 4000)
    mesh(cfg, n, g, spacing=spacing)
    cfg["script"] = [f"{t} TRAFFIC sta:* size=const:1400 interval=const:{interval}"]
    offered = n * 1400 * 8 / interval  # Mbit/s after the switch
    return {"variant": variant, "routers": n, "gateways": g, "spacing_m": spacing, "event_t": t,
            "overload_interval_us": interval, "offered_mbps": round(offered, 1)}


SIZES = (64, 256, 512, 1024, 1472)
INTERVALS_MS = (200, 100, 50, 20, 10)


def load(cfg, rng, i, dur):
    combo = (i // 4) % 25
    size, interval = SIZES[combo // 5], INTERVALS_MS[combo % 5]
    mesh(cfg, 23, 2, spacing=30)
    cfg["traffic"]["profiles"] = {"fixed": {"weight": 1, "size": {"dist": "const", "value": size},
                                            "interval_us": {"dist": "exp", "mean": interval * 1000}}}
    offered = 23 * size * 8 / (interval / 1000) / 1e6
    return {"routers": 23, "gateways": 2, "spacing_m": 30, "size_b": size, "interval_ms": interval,
            "offered_mbps": round(offered, 3)}


def density(cfg, rng, i, dur):
    n, g = rng.randint(15, 40), rng.choice((1, 2))
    side = math.exp(rng.uniform(math.log(60), math.log(400)))
    m = round(rng.uniform(0.5, 1.0), 2)
    mesh(cfg, n, g, layout="random", room=side)
    scale_load(cfg, m)
    cfg["churn"] = {"initial_active": n, "min_active": max(0, n - 4), "max_active": n, "rate_per_s": 0.02,
                    "fail_probability": 0.5, "restart_probability": 1.0, "restart_downtime_s": [5, 20]}
    per_m2 = (n + g) / side ** 2
    return {"routers": n, "gateways": g, "side_m": round(side, 1), "load_x": m,
            "density_per_ha": round(per_m2 * 1e4, 2), "mean_degree": round(per_m2 * math.pi * RADIO_RANGE_M ** 2, 2)}


SAMPLERS = {"normal": normal, "single_failure": single_failure, "collapse": collapse, "load": load, "density": density}


def build(mode, n, duration, seed, parallel, sets=(), tag=""):
    base = base_config(duration, sets)
    base["run"].update(name=f"campaign-{mode}" + (f"-{tag}" if tag else ""), replicas=n, max_parallel=parallel, base_seed=seed, max_restarts=1)
    instances = []
    sup = orchestrator.Supervisor(base)
    for i in range(n):
        cfg = copy.deepcopy(base)
        rng = random.Random(f"{mode}:{seed}:{i}")
        meta = {"mode": mode, "idx": i, **SAMPLERS[mode](cfg, rng, i, duration)}
        loadgen.validate(cfg)
        instances.append(orchestrator.Instance(i, cfg, sup.run_dir, meta))
    sup.instances = instances
    return sup, base


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=MODES + ("all",))
    ap.add_argument("--n", type=int, help="runs per mode (default: 100, density 120)")
    ap.add_argument("--parallel", type=int, default=8)
    ap.add_argument("--duration", type=float, default=300)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--set", action="append", default=[], help="config override, as in orchestrator.py")
    ap.add_argument("--tag", default="", help="suffix of the run dir: runs/campaign-<mode>-<tag>")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)-7s %(message)s", datefmt="%H:%M:%S")

    status = 0
    for mode in (MODES if a.mode == "all" else (a.mode,)):
        sup, base = build(mode, a.n or DEFAULT_N[mode], a.duration, a.seed, a.parallel, a.set, a.tag)
        sup.run_dir.mkdir(parents=True, exist_ok=True)
        (sup.run_dir / "config.resolved.json").write_text(json.dumps(base, indent=2))
        if a.dry_run:
            for inst in sup.instances:
                inst.prepare()
            continue
        handler = logging.FileHandler(sup.run_dir / "orchestrator.log")
        handler.setFormatter(logging.getLogger().handlers[0].formatter)
        logging.getLogger().addHandler(handler)
        status |= sup.run()
        logging.getLogger().removeHandler(handler)
        if sup.stop_deadline is not None:  # interrupted: do not start the next mode
            break
    return status


if __name__ == "__main__":
    sys.exit(main())
