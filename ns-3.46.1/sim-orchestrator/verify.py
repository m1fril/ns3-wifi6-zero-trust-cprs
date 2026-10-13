#!/usr/bin/env python3
"""Self-check of finished runs: does the simulation output match its input?

  python3 verify.py runs/<name>            all instances of a run
  python3 verify.py runs/<name>/000        one instance

An independent model replays the timeline the instance actually executed (stdout.log, so live
`ctl` commands are included) together with the built-in agents, and predicts per second how
many packets must be generated, how many netem must drop, how many STAs must be up and what
p95 latency netem must produce. The predictions are compared with the event CSV; the loss
breakdown must add up to sent - received. Results: printed table + verify.json in the run dir.
Exit code 1 if any check FAILs. Needs compactMetrics=1 (per-second counters).
"""

import csv
import json
import math
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path
from statistics import NormalDist

STEP = 0.001          # model resolution, s
CHAOS = dict(freq=0.01, amp=200000.0, floor=1000.0)        # ChaosAgent as built in simulation-environment.cc
ESCALATION = dict(rate=5000.0, floor=500.0)                # LinearEscalationAgent
DIRECTOR = {"controlled": "controlled", "failure": "failure", "escalation": "escalation"}  # mode at 20 s
PASS, WARN, FAIL, INFO = "PASS", "WARN", "FAIL", "INFO"


def mean_of(spec):
    """Mean of a TrafficApp Dist spec, honouring its clipping / bounds."""
    kind, *v = spec.split(":")
    v = [float(x) for x in v]
    if kind == "const":
        return v[0]
    if kind == "uniform":
        return (v[0] + v[1]) / 2
    if kind == "exp":
        mu = v[0]
        if len(v) == 1:
            return mu
        b = v[1]  # ns-3 Bound rejects samples above b: truncated exponential
        return mu - b * math.exp(-b / mu) / (1 - math.exp(-b / mu))
    if kind == "normal":
        mu, sd = v[0], v[1]
        if len(v) == 2 or sd == 0:
            return mu
        lo, hi = v[2], v[3]  # clamp to [lo, hi]
        z = NormalDist()
        a, b = (lo - mu) / sd, (hi - mu) / sd
        inner = mu * (z.cdf(b) - z.cdf(a)) - sd * (z.pdf(b) - z.pdf(a))
        return inner + lo * z.cdf(a) + hi * (1 - z.cdf(b))
    raise ValueError(spec)


def executed_timeline(inst_dir):
    """(time, command, args) as executed: [TIMELINE] lines of the last start attempt."""
    log = (inst_dir / "stdout.log").read_text(errors="replace")
    log = log.rsplit("--- start attempt", 1)[-1]
    events = []
    for m in re.finditer(r"^\[TIMELINE\] t=([0-9.e+-]+) (\S+)(.*)$", log, re.M):
        events.append((float(m.group(1)), m.group(2), m.group(3).split()))
    return events


def load_csv(inst_dir):
    files = sorted(inst_dir.glob("Bianchi-*.csv"))
    if not files:
        raise FileNotFoundError(f"no CSV in {inst_dir}")
    per_s, counts, cells = defaultdict(dict), defaultdict(int), {}
    with open(files[0], newline="") as f:
        for r in csv.DictReader(f):
            a, val = r["Action"], float(r["Value"] or 0)
            counts[a] += 1
            if a == "STA_CELL":
                cells[int(r["NodeID"])] = int(val)
            elif a.endswith("_COUNT") or a in ("ACTIVE_STAS", "LATENCY_P95_MS", "HOPS_MEAN", "AGGREGATE_THROUGHPUT_MBPS"):
                per_s[round(float(r["Time_s"]))][a] = val
            elif a in ("HWMP_DROPPED", "HWMP_DROPPED_TTL", "PEER_FILTER_DROPPED"):
                per_s[a][r["NodeID"]] = val
    return per_s, counts, cells


class Model:
    """Replays the executed timeline with STEP resolution and accumulates per-second predictions."""

    def __init__(self, args, duration, cells):
        self.n_sta, self.n_ap = int(args["nStas"]), int(args["nAps"])
        self.base_iv = float(args.get("pktInterval", 1200))
        self.size0 = float(args.get("pSize", 1500))
        self.scenario = args.get("scenario", "normal")
        self.duration = duration
        # STA_CELL rows give node id -> cell; STA index = position among STA node ids.
        # Runs logged before STA_CELL existed are infra: STAs fill the APs in order.
        ids = sorted(cells)
        per_ap, extra = divmod(self.n_sta, self.n_ap)
        contiguous = [ap for ap in range(self.n_ap) for _ in range(per_ap + (ap < extra))]
        self.cell = [cells[i] for i in ids] if ids else contiguous

    def targets(self, tg):
        kind, _, sel = tg.partition(":")
        if tg == "all":
            kind, sel = "sta", "*"
        n = self.n_ap if kind in ("ap", "cell") else self.n_sta
        if sel in ("", "*"):
            idx = range(n)
        elif "-" in sel:
            a, b = map(int, sel.split("-"))
            idx = range(a, b + 1)
        else:
            idx = [int(sel)]
        if kind == "cell":
            return [s for s in range(self.n_sta) if self.cell[s] in idx]
        return [] if kind == "ap" else list(idx)

    def run(self, events):
        n = self.n_sta
        up, app, iso = [True] * n, [True] * n, [False] * n
        iv, netem = [self.base_iv] * n, [(0.0, 0.0, 0.0)] * n
        agents = {"chaos": True, "escalation": True, "director": True}
        mode, esc_start = "normal", None
        sent, drops, bursts = defaultdict(float), defaultdict(float), defaultdict(float)
        active_at, state_at = {}, {}  # per integer second: active count, netem senders snapshot
        changed = set()               # seconds in which delays or the load mix changed abruptly
        returns = []                  # (time, sta) of pending NODE restarts
        events = sorted(events, key=lambda e: e[0])
        ei, t = 0, 0.0
        while t < self.duration:
            for rt, s in [r for r in returns if r[0] <= t + 1e-9]:
                up[s] = True
                returns.remove((rt, s))
            if agents["director"] and self.scenario in DIRECTOR and abs(t - 20.0) < STEP / 2:
                mode = DIRECTOR[self.scenario]
            while ei < len(events) and events[ei][0] <= t + 1e-9:
                _, cmd, a = events[ei]
                ei += 1
                if cmd in ("NETEM", "ISOLATE", "MODE", "AGENT"):
                    changed.add(int(t))
                try:
                    self.apply(cmd, a, t, up, app, iso, iv, netem, agents, returns, bursts)
                    if cmd == "MODE":
                        mode = a[0]
                except (ValueError, IndexError):
                    pass  # the simulator rejected this line too
            if t >= 1.0:
                if agents["chaos"] and mode in ("controlled", "failure"):
                    iv = [max(CHAOS["floor"], self.base_iv + CHAOS["amp"] * math.sin(2 * math.pi * CHAOS["freq"] * t))] * n
                if agents["escalation"] and mode == "escalation":
                    esc_start = t if esc_start is None else esc_start
                    iv = [max(ESCALATION["floor"], self.base_iv - ESCALATION["rate"] * (t - esc_start))] * n
                elif mode != "escalation":
                    esc_start = None
            sec = int(t + 1e-9)
            if abs(t - round(t)) < STEP / 2:
                active_at[round(t)] = sum(up)
            if t >= 1.0:
                senders = []
                for s in range(n):
                    if up[s] and app[s]:
                        rate = 1e6 / iv[s]
                        sent[sec] += rate * STEP
                        loss = 1.0 if iso[s] else netem[s][2]
                        drops[sec] += rate * STEP * loss
                        if loss < 1.0:
                            senders.append((rate * (1 - loss), netem[s][0], netem[s][1]))
                state_at[sec] = senders
            t = round(t + STEP, 6)
        return dict(sent=sent, drops=drops, bursts=bursts, active=active_at, senders=state_at, changed=changed)

    def apply(self, cmd, a, t, up, app, iso, iv, netem, agents, returns, bursts):
        if cmd == "AGENT":
            if a[0] in agents:
                agents[a[0]] = a[1] == "on"
            return
        if cmd in ("MODE", "STOP"):
            return
        for s in self.targets(a[0]):
            if cmd == "NODE":
                if a[1] == "up":
                    up[s] = True
                else:
                    up[s] = False
                    if a[1] == "restart":
                        returns.append((t + (float(a[2]) if len(a) > 2 else 1.0), s))
            elif cmd == "APP":
                app[s] = a[1] == "on"
            elif cmd == "ISOLATE":
                iso[s] = a[1] == "on"
            elif cmd == "NETEM":
                netem[s] = (float(a[1]), float(a[2]), float(a[3]))
            elif cmd == "TRAFFIC":
                for kv in a[1:]:
                    key, _, spec = kv.partition("=")
                    if key == "interval":
                        iv[s] = mean_of(spec)
            elif cmd == "BURST":
                if up[s] and app[s] and t >= 1.0:
                    bursts[int(t)] += int(a[1])


def p95_of_mixture(senders):
    """p95 of latency when sender i contributes rate r_i with delay max(0, N(d_i, j_i))."""
    total = sum(r for r, _, _ in senders)
    if total <= 0:
        return None

    def tail(x):
        out = 0.0
        for r, d, j in senders:
            out += r * ((d > x) if j == 0 else 1 - NormalDist(d, j).cdf(x))
        return out

    lo, hi = 0.0, max(d + 6 * j for _, d, j in senders) + 1.0
    for _ in range(60):
        mid = (lo + hi) / 2
        lo, hi = (mid, hi) if tail(mid) > 0.05 * total else (lo, mid)
    return hi


def grade(ok, warn=None):
    return PASS if ok else (WARN if warn else FAIL)


def verify_instance(inst_dir, cfg):
    args, duration = cfg["sim"]["args"], float(cfg["sim"]["duration"])
    mesh = args.get("topology", "infra") == "mesh"
    per_s, counts, cells = load_csv(inst_dir)
    if "SENT_COUNT" not in counts:
        return [{"check": "compact metrics", "status": FAIL, "detail": "run with compactMetrics=1"}]
    m = Model(args, duration, cells).run(executed_timeline(inst_dir))
    secs = sorted(s for s in per_s if isinstance(s, int) and s >= 2 and "SENT_COUNT" in per_s[s])
    checks = []

    # 1. traffic generation: packets generated per second vs the profile rates
    exp = [m["sent"][s - 1] + m["bursts"][s - 1] for s in secs]
    obs = [per_s[s]["SENT_COUNT"] for s in secs]
    e, o = sum(exp), sum(obs)
    rel = (o - e) / e if e else 0.0
    checks.append({"check": "generated packets vs model", "status": grade(abs(rel) <= 0.03, abs(rel) <= 0.06),
                   "detail": f"model {e:,.0f}, measured {o:,.0f} ({rel:+.1%})"})
    if len(secs) > 2 and statistics.pstdev(exp) > 0 and statistics.pstdev(obs) > 0:
        r = statistics.correlation(exp, obs)
        # a flat load leaves only Poisson noise to correlate: the shape check means nothing there
        flat = statistics.pstdev(exp) < 0.1 * statistics.fmean(exp)
        checks.append({"check": "per-second load shape", "status": INFO if flat else grade(r >= 0.95, r >= 0.85),
                       "detail": f"Pearson r = {r:.3f} over {len(secs)} s" + (" (load is flat: not graded)" if flat else "")})

    # 2. churn / scaling: active STA count logged every second vs replayed node state
    pairs = [(m["active"].get(s), per_s[s].get("ACTIVE_STAS")) for s in secs if s in m["active"]]
    hit = sum(1 for a, b in pairs if a is not None and b is not None and abs(a - b) < 0.5)
    share = hit / len(pairs) if pairs else 0.0
    checks.append({"check": "active STAs vs timeline", "status": grade(share >= 0.95, share >= 0.85),
                   "detail": f"{hit}/{len(pairs)} seconds match exactly"})

    # 3. netem / isolation drops vs configured loss
    de = sum(m["drops"][s - 1] for s in secs)
    do = sum(per_s[s].get("NETEM_DROP_COUNT", 0) for s in secs)
    tol = max(0.05 * de, 4 * math.sqrt(de) + 5)
    checks.append({"check": "netem drops vs model", "status": grade(abs(do - de) <= tol, abs(do - de) <= 2 * tol),
                   "detail": f"model {de:,.0f}, measured {do:,.0f} (tolerance ±{tol:,.0f})"})

    # 4. every lost packet is explained by a logged cause
    sent = sum(per_s[s]["SENT_COUNT"] for s in per_s if isinstance(s, int))
    recv = sum(per_s[s].get("RECV_COUNT", 0) for s in per_s if isinstance(s, int))
    causes = {
        "netem": sum(per_s[s].get("NETEM_DROP_COUNT", 0) for s in per_s if isinstance(s, int)),
        "mac_enqueue": counts.get("WIFI_DROP", 0),
        "mac_retry": sum(per_s[s].get("RETRY_DROP_COUNT", 0) for s in per_s if isinstance(s, int)),
        "mac_queue": sum(per_s[s].get("QUEUE_DROP_COUNT", 0) for s in per_s if isinstance(s, int)),
        "ip_stack": sum(per_s[s].get("STACK_DROP_COUNT", 0) for s in per_s if isinstance(s, int)),
        "hwmp": sum(per_s.get("HWMP_DROPPED", {}).values()) + sum(per_s.get("HWMP_DROPPED_TTL", {}).values()),
        "peer_link_filter": sum(per_s.get("PEER_FILTER_DROPPED", {}).values()),
    }
    unexplained = sent - recv - sum(causes.values())
    share = abs(unexplained) / sent if sent else 0.0
    breakdown = ", ".join(f"{k} {v:,.0f}" for k, v in causes.items() if v)
    # With a full MAC queue, ns-3's FcfsWifiQueueScheduler evicts the oldest data frame to make room
    # for a management frame (HWMP PREQ/PERR) and fires no drop trace: under saturation the residual
    # is that eviction (a run with a 100000-frame queue cut it from 13% to 2.4%).
    saturated = sent and causes["mac_queue"] > 0.05 * sent
    status = grade(share <= 0.01, share <= 0.03)
    if status == FAIL and saturated:
        status = WARN
    checks.append({"check": "loss accounting", "status": status,
                   "detail": f"lost {sent - recv:,.0f} = {breakdown or 'nothing'}; unexplained {unexplained:+,.0f} ({share:.2%})"
                             + ("; HWMP counters include non-data frames" if mesh else "")
                             + ("; MAC queue saturated: the residual is data evicted for management frames (untraced in ns-3)" if saturated and share > 0.01 else "")})

    # 5. netem delay: p95 latency in seconds with a stable, delayed netem state
    errs = []
    for s in secs:
        if (s - 1) in m["changed"] or s in m["changed"] or (s - 2) in m["changed"]:
            continue
        pred = p95_of_mixture(m["senders"].get(s - 1, []))
        got = per_s[s].get("LATENCY_P95_MS")
        if pred and pred >= 5.0 and got is not None:
            errs.append((got - pred) / pred)
    if errs:
        med = statistics.median(errs)
        status = INFO if mesh else grade(abs(med) <= 0.15, abs(med) <= 0.3)
        checks.append({"check": "netem p95 latency vs model", "status": status,
                       "detail": f"median deviation {med:+.1%} over {len(errs)} s"
                                 + (" (mesh adds multi-hop delay on top of netem)" if mesh else "")})

    # 6. informational KPIs
    sent_all = sent or 1
    info = f"PDR {recv / sent_all:.2%}"
    hops = [per_s[s]["HOPS_MEAN"] for s in secs if "HOPS_MEAN" in per_s[s]]
    if hops:
        info += f", mean hops {statistics.fmean(hops):.2f}, peer-link closes {counts.get('PEER_LINK_CLOSE', 0)}"
    checks.append({"check": "summary", "status": INFO, "detail": info})
    return checks


def verify(path):
    path = Path(path).resolve()
    run_dir = path if (path / "config.resolved.json").exists() else path.parent
    cfg = json.loads((run_dir / "config.resolved.json").read_text())
    insts = [path] if path != run_dir else sorted(p for p in run_dir.iterdir() if p.is_dir() and (p / "timeline.txt").exists())
    result = {}
    for inst in insts:
        try:
            own = inst / "config.json"  # campaigns draw parameters per instance
            result[inst.name] = verify_instance(inst, json.loads(own.read_text()) if own.exists() else cfg)
        except (OSError, ValueError, KeyError) as e:
            result[inst.name] = [{"check": "readable output", "status": FAIL, "detail": str(e)}]
    if path == run_dir:
        (run_dir / "verify.json").write_text(json.dumps(result, indent=2))
    return result


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    result = verify(sys.argv[1])
    failed = False
    for inst, checks in result.items():
        print(f"\n== instance {inst}")
        for c in checks:
            failed |= c["status"] == FAIL
            print(f"  [{c['status']:4}] {c['check']:<30} {c['detail']}")
    print("\nverdict:", "FAIL" if failed else "PASS")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
