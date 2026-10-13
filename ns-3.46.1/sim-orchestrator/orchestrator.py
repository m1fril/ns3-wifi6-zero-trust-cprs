#!/usr/bin/env python3
"""Orchestrator for scratch/network-project simulations.

  run  -c config.yaml [--set key.path=value ...] [--dry-run]   launch and supervise instances
  gen  -c config.yaml [--seed N] [-o timeline.txt]             print the generated timeline
  ctl  <instance_dir> <COMMAND> <target> [args...]             live command into a running instance

Each instance is one ns-3 process (the whole network lives inside it). Nodes, services and
network conditions inside the instance are driven by its timeline file (see loadgen.py).
"""

import argparse
import csv
import glob
import json
import logging
import os
import signal
import statistics
import subprocess
import sys
import time
from pathlib import Path

import loadgen

log = logging.getLogger("orchestrator")


# ---------------------------------------------------------------- config

def load_config(path, overrides):
    text = Path(path).read_text()
    if path.endswith(".json"):
        cfg = json.loads(text)
        parse = json.loads
    else:
        try:
            import yaml
        except ImportError:
            sys.exit("PyYAML is not installed: pip install pyyaml (or use a .json config)")
        cfg = yaml.safe_load(text)
        parse = yaml.safe_load
    for item in overrides:  # --set churn.rate_per_s=1.5
        key, _, raw = item.partition("=")
        node = cfg
        *parents, leaf = key.split(".")
        for p in parents:
            node = node.setdefault(p, {})
        try:
            node[leaf] = parse(raw)
        except Exception:
            node[leaf] = raw
    base = Path(path).resolve().parent
    cfg["sim"]["binary"] = str((base / cfg["sim"]["binary"]).resolve())
    cfg.setdefault("run", {})
    cfg["run"]["output_dir"] = str((base / cfg["run"].get("output_dir", "runs")).resolve())
    loadgen.validate(cfg)
    return cfg


# ---------------------------------------------------------------- supervision

class Instance:
    """One simulation process. `cfg` may differ per instance (campaigns draw parameters per run)."""

    def __init__(self, idx, cfg, run_dir, meta=None):
        self.idx = idx
        self.cfg = cfg
        self.meta = meta or {}
        self.seed = cfg["run"].get("base_seed", 1) + idx
        self.dir = run_dir / f"{idx:03d}"
        self.proc = None
        self.restarts = 0
        self.status = "pending"
        self.started = None
        self.elapsed = 0.0
        args = {**cfg["sim"]["args"], "duration": cfg["sim"]["duration"], "RngRun": self.seed,
                "timeline": "timeline.txt"}
        self.cmd = [cfg["sim"]["binary"]] + [f"--{k}={v}" for k, v in args.items()]

    def prepare(self):
        self.dir.mkdir(parents=True, exist_ok=True)
        lines = loadgen.generate(self.cfg, self.seed)
        if self.meta:  # per-instance parameters: verify.py and the campaign analysis read these
            (self.dir / "config.json").write_text(json.dumps(self.cfg, indent=2))
            (self.dir / "params.json").write_text(json.dumps(self.meta, indent=2))
        (self.dir / "timeline.txt").write_text(f"# seed={self.seed}\n" + "\n".join(lines) + "\n")
        (self.dir / "cmd.txt").write_text(" ".join(self.cmd) + "\n")
        log.info("[%03d] timeline: %d events (seed %d)", self.idx, len(lines), self.seed)

    def start(self):
        for old in self.dir.glob("Bianchi-*.csv"):  # a restart overwrites the previous attempt
            old.unlink()
        out = open(self.dir / "stdout.log", "a")
        out.write(f"--- start attempt {self.restarts + 1} at {time.ctime()}\n")
        out.flush()
        # own session: Ctrl-C reaches only us, and we decide how children stop
        self.proc = subprocess.Popen(self.cmd, cwd=self.dir, stdout=out, stderr=subprocess.STDOUT,
                                     start_new_session=True)
        out.close()
        self.status, self.started = "running", time.monotonic()
        log.info("[%03d] started pid %d", self.idx, self.proc.pid)

    def signal(self, sig):
        if self.proc and self.proc.poll() is None:
            self.proc.send_signal(sig)


class Supervisor:
    def __init__(self, cfg):
        self.cfg = cfg
        run = cfg["run"]
        self.run_dir = Path(run["output_dir"]) / run.get("name", "run")
        self.max_parallel = run.get("max_parallel", os.cpu_count() or 1)
        self.max_restarts = run.get("max_restarts", 0)
        self.grace = run.get("grace_period_s", 10)
        self.instances = [Instance(i, cfg, self.run_dir) for i in range(run.get("replicas", 1))]
        self.stop_deadline = None
        self._last_status = None

    def write_status(self, state):
        """status.json is what the web UI polls while a run is in progress."""
        status = {"state": state, "pid": os.getpid(), "duration": self.cfg["sim"]["duration"],
                  "instances": {f"{i.idx:03d}": {"status": i.status, "restarts": i.restarts, "seed": i.seed,
                                                 "pid": i.proc.pid if i.proc else None} for i in self.instances}}
        if status != self._last_status:
            tmp = self.run_dir / "status.json.tmp"
            tmp.write_text(json.dumps(status, indent=2))
            tmp.replace(self.run_dir / "status.json")
            self._last_status = status

    def request_stop(self, signum, _frame):
        if self.stop_deadline is None:
            log.warning("%s: stopping, SIGTERM to running instances (grace %ss)",
                        signal.Signals(signum).name, self.grace)
            self.stop_deadline = time.monotonic() + self.grace
            for inst in self.instances:
                if inst.status == "pending":
                    inst.status = "cancelled"
                inst.signal(signal.SIGTERM)
        else:
            log.warning("second signal: killing instances now")
            self.stop_deadline = 0

    def run(self):
        signal.signal(signal.SIGINT, self.request_stop)
        signal.signal(signal.SIGTERM, self.request_stop)
        for inst in self.instances:
            inst.prepare()

        while any(i.status in ("pending", "running") for i in self.instances):
            running = [i for i in self.instances if i.status == "running"]
            for inst in [i for i in self.instances if i.status == "pending"][: self.max_parallel - len(running)]:
                inst.start()
            for inst in running:
                rc = inst.proc.poll()
                if rc is None:
                    continue
                inst.elapsed += time.monotonic() - inst.started
                if rc == 0:
                    inst.status = "done" if self.stop_deadline is None else "stopped"
                elif self.stop_deadline is None and inst.restarts < self.max_restarts:
                    inst.restarts += 1
                    log.warning("[%03d] exited with %d, restart %d/%d", inst.idx, rc, inst.restarts, self.max_restarts)
                    inst.start()
                    continue
                else:
                    inst.status = "stopped" if self.stop_deadline is not None else "failed"
                log.info("[%03d] %s (rc=%d, %.1fs)", inst.idx, inst.status, rc, inst.elapsed)
            if self.stop_deadline is not None and time.monotonic() > self.stop_deadline:
                for inst in self.instances:
                    inst.signal(signal.SIGKILL)
            self.write_status("stopping" if self.stop_deadline is not None else "running")
            time.sleep(0.2)

        summary = {f"{i.idx:03d}": {"status": i.status, "seed": i.seed, "restarts": i.restarts,
                                    "wall_s": round(i.elapsed, 2), **summarize(i.dir)} for i in self.instances}
        (self.run_dir / "summary.json").write_text(json.dumps(summary, indent=2))
        self.write_status("finished")
        print_summary(summary)
        log.info("results: %s", self.run_dir)
        return 0 if all(i.status == "done" for i in self.instances) else 1


# ---------------------------------------------------------------- results

def summarize(instance_dir):
    """Per-run KPIs from the event CSV (compactMetrics=1 gives per-second counters)."""
    files = glob.glob(str(Path(instance_dir) / "Bianchi-*.csv"))
    if not files:
        return {}
    series, counts, recv_at, hops_at = {}, {}, {}, {}
    with open(files[0], newline="") as f:
        for row in csv.DictReader(f):
            action = row["Action"]
            counts[action] = counts.get(action, 0) + 1
            if row["Value"]:
                series.setdefault(action, []).append(float(row["Value"]))
                if action == "RECV_COUNT":
                    recv_at[row["Time_s"]] = float(row["Value"])
                elif action == "HOPS_MEAN":
                    hops_at[row["Time_s"]] = float(row["Value"])

    def total(counter, per_packet):  # compact mode logs per-second counters, otherwise one row per packet
        return int(sum(series.get(counter, []))) or counts.get(per_packet, 0)

    sent, recv = total("SENT_COUNT", "SEND"), total("RECV_COUNT", "RECV")
    active = series.get("ACTIVE_STAS", [])
    p95 = series.get("LATENCY_P95_MS", [])
    thr = series.get("AGGREGATE_THROUGHPUT_MBPS", [])
    weight = sum(recv_at.get(t, 0) for t in hops_at)
    hops = (sum(h * recv_at.get(t, 0) for t, h in hops_at.items()) / weight if weight
            else statistics.fmean(series["HOPS"]) if series.get("HOPS") else None)
    discovery = series.get("ROUTE_DISCOVERY_MS", [])
    return {
        "sent": sent, "received": recv,
        "pdr": round(recv / sent, 4) if sent else None,
        "throughput_mbps_mean": round(statistics.fmean(thr), 3) if thr else None,
        "latency_p95_ms_median": round(statistics.median(p95), 3) if p95 else None,
        "active_stas_min_max": [min(active), max(active)] if active else None,
        "node_down": counts.get("NODE_DOWN", 0), "node_up": counts.get("NODE_UP", 0),
        "reassoc": counts.get("REASSOC", 0),
        "drops": {
            "netem": total("NETEM_DROP_COUNT", "NETEM_DROP"),
            "mac_enqueue": counts.get("WIFI_DROP", 0),
            "mac_retry": total("RETRY_DROP_COUNT", "RETRY_DROP"),
            "mac_queue": total("QUEUE_DROP_COUNT", "QUEUE_DROP"),
            "ip": total("STACK_DROP_COUNT", "IP_DROP"),
            "hwmp": int(sum(series.get("HWMP_DROPPED", [])) + sum(series.get("HWMP_DROPPED_TTL", []))),
            "peer_link_filter": int(sum(series.get("PEER_FILTER_DROPPED", []))),
        },
        "hops_mean": round(hops, 3) if hops else None,
        "peer_link_closes": counts.get("PEER_LINK_CLOSE", 0),
        "route_discoveries": len(discovery),
        "route_discovery_ms_median": round(statistics.median(discovery), 2) if discovery else None,
    }


def print_summary(summary):
    cols = ["status", "restarts", "sent", "pdr", "throughput_mbps_mean", "latency_p95_ms_median",
            "active_stas_min_max", "node_down", "hops_mean"]
    print("inst  " + "  ".join(f"{c:>12.12}" for c in cols))
    for name, row in summary.items():
        print(f"{name:5} " + "  ".join(f"{str(row.get(c, '-')):>12.12}" for c in cols))


# ---------------------------------------------------------------- live control

def send_ctl(instance_dir, text):
    """Appends 'now <command>' to a running instance's timeline; returns the queued command."""
    timeline = Path(instance_dir) / "timeline.txt"
    if not timeline.exists():
        raise ValueError(f"no timeline at {timeline}")
    command = text.split()  # also strips newlines, so one call = one timeline line
    if not command or command[0] not in loadgen.COMMANDS:
        raise ValueError(f"unknown command; one of {sorted(loadgen.COMMANDS)}")
    with open(timeline, "a") as f:
        f.write("now " + " ".join(command) + "\n")
    return " ".join(command)


# ---------------------------------------------------------------- cli

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="action", required=True)
    for name in ("run", "gen"):
        p = sub.add_parser(name)
        p.add_argument("-c", "--config", default=str(Path(__file__).with_name("config.yaml")))
        p.add_argument("--set", action="append", default=[], metavar="KEY=VALUE",
                       help="override a config value, e.g. --set churn.rate_per_s=2")
    sub.choices["run"].add_argument("--dry-run", action="store_true", help="write timelines, do not launch")
    sub.choices["gen"].add_argument("--seed", type=int, default=1)
    sub.choices["gen"].add_argument("-o", "--output")
    ctl = sub.add_parser("ctl", help="append a live command to a running instance")
    ctl.add_argument("instance_dir")
    ctl.add_argument("command", nargs="+")
    a = ap.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)-7s %(message)s", datefmt="%H:%M:%S")

    if a.action == "ctl":
        try:
            log.info("queued: %s (executed on the next 100 ms heartbeat)", send_ctl(a.instance_dir, " ".join(a.command)))
        except ValueError as e:
            sys.exit(str(e))
        return 0

    try:
        cfg = load_config(a.config, a.set)
    except (loadgen.ConfigError, KeyError, TypeError, ValueError) as e:
        sys.exit(f"config error: {e}")

    if a.action == "gen":
        text = "\n".join(loadgen.generate(cfg, a.seed)) + "\n"
        if a.output:
            Path(a.output).write_text(text)
        else:
            sys.stdout.write(text)
        return 0

    if not os.access(cfg["sim"]["binary"], os.X_OK):
        sys.exit(f"binary not found: {cfg['sim']['binary']}\nbuild it: ./ns3 build network-project-main")
    sup = Supervisor(cfg)
    sup.run_dir.mkdir(parents=True, exist_ok=True)
    fh = logging.FileHandler(sup.run_dir / "orchestrator.log")
    fh.setFormatter(logging.getLogger().handlers[0].formatter)
    logging.getLogger().addHandler(fh)
    (sup.run_dir / "config.resolved.json").write_text(json.dumps(cfg, indent=2))
    if a.dry_run:
        for inst in sup.instances:
            inst.prepare()
        log.info("dry run: timelines in %s", sup.run_dir)
        return 0
    return sup.run()


if __name__ == "__main__":
    sys.exit(main())
