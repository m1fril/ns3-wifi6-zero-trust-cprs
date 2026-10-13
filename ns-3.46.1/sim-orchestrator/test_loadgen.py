"""Invariants of the generated timeline. Run: python3 test_loadgen.py (or pytest)."""

from pathlib import Path

import orchestrator


def replay_active(lines, n_sta):
    """Active STA count after every NODE event (restart = down now, up after downtime)."""
    up, returns, counts = set(range(n_sta)), [], []
    for line in lines:
        t, cmd, target, *args = line.split()
        t = float(t)
        for rt, i in sorted(returns):
            if rt <= t:
                up.add(i)
                returns.remove((rt, i))
        if cmd != "NODE" or not target.startswith("sta:"):
            continue
        i = int(target[4:])
        if args[0] == "up":
            up.add(i)
        else:
            up.discard(i)
            if args[0] == "restart":
                returns.append((t + float(args[1]), i))
        counts.append((t, len(up) + len(returns)))
    return counts


def test_timeline():
    cfg = orchestrator.load_config(str(Path(__file__).with_name("config.yaml")), [])
    lines = orchestrator.loadgen.generate(cfg, seed=7)
    times = [float(line.split()[0]) for line in lines]
    assert times == sorted(times) and times[-1] < cfg["sim"]["duration"]
    assert lines == orchestrator.loadgen.generate(cfg, seed=7), "same seed must give same timeline"

    ch = cfg["churn"]
    counts = replay_active(lines, cfg["sim"]["args"]["nStas"])
    assert [c for t, c in counts if t == 0][-1] == ch["initial_active"]
    for step in ch["scale"]:
        at = [c for t, c in counts if t == step["at"]]
        assert at and at[-1] == step["active"], f"scale step {step} not reached: {at}"
    churn_only = [c for t, c in counts if 0 < t < ch["scale"][0]["at"]]
    assert churn_only and all(ch["min_active"] <= c <= ch["max_active"] for c in churn_only)

    assert any(" BURST " in line for line in lines)
    assert any("@" not in line and "TRAFFIC sta:0-3 size=normal" in line for line in lines)


def test_ap_failures_do_not_overlap():
    cfg = orchestrator.load_config(str(Path(__file__).with_name("config.yaml")),
                                   ["churn.ap_failures.rate_per_s=2", "churn.ap_failures.downtime_s=[3, 6]"])
    for seed in range(5):
        up_at = {}
        for line in orchestrator.loadgen.generate(cfg, seed):
            t, cmd, target, *args = line.split()
            if cmd == "NODE" and target.startswith("ap:"):
                assert float(t) >= up_at.get(target, 0.0), f"{target} restarted while down (seed {seed})"
                up_at[target] = float(t) + float(args[1])


if __name__ == "__main__":
    test_timeline()
    test_ap_failures_do_not_overlap()
    print("ok")
