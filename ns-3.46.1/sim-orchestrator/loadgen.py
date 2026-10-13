"""Rule-based randomizer: turns the config into a timeline for TimelineAgent.

One timeline line = "<time_s> <COMMAND> <target> <args...>" (see README.md).
Everything random is drawn from random.Random(seed), so a seed reproduces a run.
"""

import heapq
import random
import re

# Largest payload per topology: infra sends raw frames (WifiNetDevice MTU), mesh sends UDP/IPv4
# over a 1500-byte MeshPointDevice. TrafficApp clamps bigger payloads.
MAX_PAYLOAD = {"infra": 2296, "mesh": 1472}
COMMANDS = {"NODE", "APP", "TRAFFIC", "BURST", "NETEM", "ISOLATE", "AGENT", "MODE", "STOP"}
TARGET_RE = re.compile(r"^(all|(sta|ap|cell)(:(\*|\d+(-\d+)?))?)$")


class ConfigError(ValueError):
    pass


def dist_spec(d, what):
    """{'dist': 'uniform', 'min': 1, 'max': 2} -> 'uniform:1:2' (TrafficApp Dist syntax)."""
    kind = d.get("dist")
    try:
        if kind == "const":
            return f"const:{d['value']}"
        if kind == "uniform":
            if d["min"] > d["max"]:
                raise ConfigError(f"{what}: min > max")
            return f"uniform:{d['min']}:{d['max']}"
        if kind == "normal":
            bounds = f":{d['min']}:{d['max']}" if "min" in d else ""
            return f"normal:{d['mean']}:{d['std']}{bounds}"
        if kind == "exp":
            return f"exp:{d['mean']}" + (f":{d['max']}" if "max" in d else "")
    except KeyError as e:
        raise ConfigError(f"{what}: missing {e}") from None
    raise ConfigError(f"{what}: unknown dist {kind!r} (const|uniform|normal|exp)")


def _range(v, what, lo=None, hi=None):
    if not (isinstance(v, (list, tuple)) and len(v) == 2 and v[0] <= v[1]):
        raise ConfigError(f"{what}: expected [min, max] with min <= max, got {v!r}")
    if (lo is not None and v[0] < lo) or (hi is not None and v[1] > hi):
        raise ConfigError(f"{what}: must lie within [{lo}, {hi}]")
    return v


def _prob(v, what):
    if not 0.0 <= v <= 1.0:
        raise ConfigError(f"{what}: probability must be in [0, 1]")
    return v


def validate(cfg):
    """Fail fast with a readable message instead of a broken run."""
    args = cfg["sim"]["args"]
    n_sta, n_ap = int(args["nStas"]), int(args["nAps"])
    if n_ap < 1 or n_sta < 1:
        raise ConfigError("sim.args: nAps and nStas must be >= 1")
    topology = args.get("topology", "infra")
    if topology not in MAX_PAYLOAD:
        raise ConfigError("sim.args.topology must be infra or mesh")
    if cfg["sim"]["duration"] <= 1:
        raise ConfigError("sim.duration must be > 1 s (traffic starts at 1 s)")

    ch = cfg.get("churn", {})
    lo, hi = ch.get("min_active", 0), ch.get("max_active", n_sta)
    if not 0 <= lo <= ch.get("initial_active", n_sta) <= hi <= n_sta:
        raise ConfigError(f"churn: need 0 <= min_active <= initial_active <= max_active <= nStas ({n_sta})")
    _prob(ch.get("fail_probability", 0.5), "churn.fail_probability")
    _prob(ch.get("restart_probability", 0.0), "churn.restart_probability")
    _range(ch.get("restart_downtime_s", [1, 1]), "churn.restart_downtime_s", lo=0)
    _range(ch.get("ap_failures", {}).get("downtime_s", [1, 1]), "churn.ap_failures.downtime_s", lo=0)
    for s in ch.get("scale", []):
        if not 0 <= s["active"] <= n_sta:
            raise ConfigError(f"churn.scale: active={s['active']} outside pool 0..{n_sta}")

    tr = cfg.get("traffic", {})
    for name, p in tr.get("profiles", {}).items():
        dist_spec(p["size"], f"traffic.profiles.{name}.size")
        dist_spec(p["interval_us"], f"traffic.profiles.{name}.interval_us")
        top = p["size"].get("max", p["size"].get("value", 0))
        if top > MAX_PAYLOAD[topology]:
            raise ConfigError(f"traffic.profiles.{name}.size: max {top} exceeds the {topology} payload limit {MAX_PAYLOAD[topology]}")
    b = tr.get("bursts", {})
    if b.get("rate_per_s", 0) > 0:
        _range(b["nodes"], "traffic.bursts.nodes", lo=1)
        _range(b["packets"], "traffic.bursts.packets", lo=1)

    net = cfg.get("network", {})
    for r in net.get("rules", []):
        if not TARGET_RE.match(r["target"]):
            raise ConfigError(f"network.rules: bad target {r['target']!r}")
        if r["until"] <= r["at"]:
            raise ConfigError("network.rules: until must be > at")
        _prob(r.get("loss", 0.0), "network.rules.loss")
    ri = net.get("random_impairments", {})
    if ri.get("rate_per_s", 0) > 0:
        _range(ri["duration_s"], "network.random_impairments.duration_s", lo=0)
        _range(ri["loss"], "network.random_impairments.loss", lo=0, hi=1)

    for line in cfg.get("script", []):
        parts = str(line).split()
        if len(parts) < 2 or parts[1] not in COMMANDS:
            raise ConfigError(f"script: bad line {line!r}")
        if parts[0] != "now":
            float(parts[0])
        for tok in parts[2:]:
            if tok.startswith("@") and tok[1:] not in tr.get("profiles", {}):
                raise ConfigError(f"script: unknown profile {tok!r}")


def _poisson(rng, rate, t0, t1):
    t = t0
    while rate > 0:
        t += rng.expovariate(rate)
        if t >= t1:
            return
        yield t


def generate(cfg, seed):
    """Returns timeline lines sorted by time."""
    rng = random.Random(seed)
    args = cfg["sim"]["args"]
    n_sta, n_ap = int(args["nStas"]), int(args["nAps"])
    end = float(cfg["sim"]["duration"])
    ch, tr, net = cfg.get("churn", {}), cfg.get("traffic", {}), cfg.get("network", {})
    out = []

    def emit(t, cmd):
        out.append((round(t, 6), len(out), cmd))

    # Traffic profiles: weighted initial assignment
    profiles = {n: f"size={dist_spec(p['size'], n)} interval={dist_spec(p['interval_us'], n)}"
                for n, p in tr.get("profiles", {}).items()}
    names = list(profiles)
    weights = [tr["profiles"][n].get("weight", 1) for n in names]
    if names:
        for i in range(n_sta):
            emit(0, f"TRAFFIC sta:{i} {profiles[rng.choices(names, weights)[0]]}")

    # Event loop: one heap keeps churn, scale steps and restarts consistent with node state
    lo, hi = ch.get("min_active", 0), ch.get("max_active", n_sta)
    up = set(rng.sample(range(n_sta), ch.get("initial_active", n_sta)))
    down = set(range(n_sta)) - up      # eligible to join
    restarting = set()                 # down, but already scheduled to come back
    for i in sorted(down):
        emit(0, f"NODE sta:{i} down")

    heap, seq = [], 0

    def push(t, kind, data=None):
        nonlocal seq
        heapq.heappush(heap, (t, seq, kind, data))
        seq += 1

    for t in _poisson(rng, ch.get("rate_per_s", 0), 1.0, end):
        push(t, "churn")
    for s in ch.get("scale", []):
        push(float(s["at"]), "scale", int(s["active"]))
    b = tr.get("bursts", {})
    for t in _poisson(rng, b.get("rate_per_s", 0), 1.0, end):
        push(t, "burst")
    for t in _poisson(rng, tr.get("profile_swap_rate_per_s", 0), 1.0, end):
        push(t, "swap")

    def take_down(t, i):
        up.discard(i)
        if rng.random() < ch.get("restart_probability", 0.0):
            dt = rng.uniform(*ch.get("restart_downtime_s", [1, 1]))
            emit(t, f"NODE sta:{i} restart {dt:.3f}")
            restarting.add(i)
            push(t + dt, "back", i)
        else:
            emit(t, f"NODE sta:{i} down")
            down.add(i)

    def bring_up(t, i):
        down.discard(i)
        up.add(i)
        emit(t, f"NODE sta:{i} up")

    while heap:
        t, _, kind, data = heapq.heappop(heap)
        if kind == "back":
            restarting.discard(data)
            up.add(data)
        elif kind == "churn":
            # restarting nodes count as active: they are coming back on their own
            active = len(up) + len(restarting)
            can_fail = bool(up) and active > lo
            can_join = bool(down) and active < hi
            if can_fail and (not can_join or rng.random() < ch.get("fail_probability", 0.5)):
                take_down(t, rng.choice(sorted(up)))
            elif can_join:
                bring_up(t, rng.choice(sorted(down)))
        elif kind == "scale":
            while len(up) + len(restarting) < data and down:
                bring_up(t, rng.choice(sorted(down)))
            while len(up) + len(restarting) > data and up:
                i = rng.choice(sorted(up))
                up.discard(i)
                down.add(i)
                emit(t, f"NODE sta:{i} down")
        elif kind == "burst" and up:
            k = min(len(up), rng.randint(*b["nodes"]))
            for i in rng.sample(sorted(up), k):
                emit(t, f"BURST sta:{i} {rng.randint(*b['packets'])} {b.get('gap_us', 200)}")
        elif kind == "swap" and names:
            emit(t, f"TRAFFIC sta:{rng.randrange(n_sta)} {profiles[rng.choices(names, weights)[0]]}")

    # AP / gateway failures: radio off for a while. A failure that hits an AP which is still
    # down is skipped - otherwise the first restart would end the second outage early.
    apf = ch.get("ap_failures", {})
    ap_up_at = [0.0] * n_ap
    for t in _poisson(rng, apf.get("rate_per_s", 0), 1.0, end):
        k = rng.randrange(n_ap)
        downtime = rng.uniform(*apf.get("downtime_s", [1, 1]))
        if t < ap_up_at[k]:
            continue
        ap_up_at[k] = t + downtime
        emit(t, f"NODE ap:{k} restart {downtime:.3f}")

    # Network scenarios: fixed rules, then random impairment episodes on single STAs.
    # ponytail: overlapping rule/episode on the same STA - the later restore wins; track per-node stacks if that matters.
    for r in net.get("rules", []):
        if r.get("isolate"):
            emit(r["at"], f"ISOLATE {r['target']} on")
            emit(r["until"], f"ISOLATE {r['target']} off")
        else:
            emit(r["at"], f"NETEM {r['target']} {r.get('delay_ms', 0)} {r.get('jitter_ms', 0)} {r.get('loss', 0)}")
            emit(r["until"], f"NETEM {r['target']} 0 0 0")
    ri = net.get("random_impairments", {})
    busy_until = [0.0] * n_sta
    for t in _poisson(rng, ri.get("rate_per_s", 0), 1.0, end):
        i = rng.randrange(n_sta)
        if busy_until[i] > t:
            continue
        dur = rng.uniform(*ri["duration_s"])
        busy_until[i] = t + dur
        delay = rng.uniform(*ri.get("delay_ms", [0, 0]))
        jitter = rng.uniform(*ri.get("jitter_ms", [0, 0]))
        emit(t, f"NETEM sta:{i} {delay:.2f} {jitter:.2f} {rng.uniform(*ri['loss']):.4f}")
        emit(t + dur, f"NETEM sta:{i} 0 0 0")

    # Hand-written commands: module hot-swap, modes, @profile shortcuts
    for line in cfg.get("script", []):
        when, *rest = str(line).split()
        rest = [profiles[tok[1:]] if tok.startswith("@") else tok for tok in rest]
        emit(0 if when == "now" else float(when), " ".join(rest))

    out.sort()
    return [f"{t:.6f} {cmd}" for t, _, cmd in out if t < end]
