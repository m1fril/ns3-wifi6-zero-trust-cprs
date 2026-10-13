#!/usr/bin/env python3
"""Per-run metrics of a campaign, with pre/post-event windows and anomaly detection.

  python3 analyze_campaign.py runs/campaign-<mode> [...]   -> <dir>/results.csv + printed overview

Windows: "pre" = [10 s, event), "post" = [event + 5 s, end) - 5 s let the network notice the
failure and reroute. Runs without an event use [10 s, end) for both. delivered_share = post
delivery rate / pre delivery rate: the share of the traffic that still arrives.
"""

import csv
import json
import verify
import statistics
import sys
from collections import defaultdict
from pathlib import Path

WARMUP, SETTLE = 10.0, 5.0
INDUCED_WINDOW = (-1.0, 15.0)
MIN_SECONDS = {"route_storm": 3}  # shorter route bursts are HWMP's routine path refresh  # an anomaly this close after an injected event is attributed to it


def load_run(inst):
    params = json.loads((inst / "params.json").read_text())
    cfg = json.loads((inst / "config.json").read_text())
    per_s = defaultdict(dict)                     # second -> metric -> value
    node = defaultdict(lambda: defaultdict(dict))  # node id -> second -> metric -> value
    counts, types, links = defaultdict(int), {}, []
    files = sorted(inst.glob("Bianchi-*.csv"))
    if not files:
        return None
    with open(files[0], newline="") as f:
        for r in csv.DictReader(f):
            a, t, nid = r["Action"], float(r["Time_s"]), r["NodeID"]
            v = float(r["Value"] or 0)
            counts[a] += 1
            types[nid] = r["NodeType"]
            if a.startswith("NODE_") and a not in ("NODE_DOWN", "NODE_UP"):
                node[nid][round(t)][a] = v
            elif a.endswith("_COUNT") or a in ("ACTIVE_STAS", "LATENCY_P95_MS", "HOPS_MEAN", "AGGREGATE_THROUGHPUT_MBPS"):
                per_s[round(t)][a] = v
            elif a in ("PEER_LINK_OPEN", "PEER_LINK_CLOSE", "ROUTE_DISCOVERY_MS"):
                links.append((t, a, nid, v))
    return params, cfg, per_s, node, counts, types, links


def window(per_s, lo, hi):
    secs = [s for s in per_s if lo < s <= hi and "SENT_COUNT" in per_s[s]]
    sent = sum(per_s[s]["SENT_COUNT"] for s in secs)
    recv = sum(per_s[s].get("RECV_COUNT", 0) for s in secs)
    p95 = [per_s[s]["LATENCY_P95_MS"] for s in secs if "LATENCY_P95_MS" in per_s[s]]
    hops = [per_s[s]["HOPS_MEAN"] for s in secs if "HOPS_MEAN" in per_s[s]]
    return {"sent": sent, "recv": recv, "secs": len(secs), "pdr": recv / sent if sent else None,
            "rate": recv / len(secs) if secs else 0.0, "p95": statistics.median(p95) if p95 else None,
            "hops": statistics.fmean(hops) if hops else None}


def anomalies(per_s, node, links, types, end, event_t):
    """Rule-based anomaly detection; returns (kind, time, detail) tuples."""
    found = []
    secs = sorted(s for s in per_s if s > WARMUP and "SENT_COUNT" in per_s[s])
    p95 = [per_s[s].get("LATENCY_P95_MS") for s in secs if per_s[s].get("LATENCY_P95_MS")]
    base_p95 = statistics.median(p95) if p95 else None
    pdrs = {s: per_s[s].get("RECV_COUNT", 0) / per_s[s]["SENT_COUNT"] for s in secs if per_s[s]["SENT_COUNT"] > 0}
    base_pdr = statistics.median(pdrs.values()) if pdrs else None
    for s in secs:
        lat = per_s[s].get("LATENCY_P95_MS")
        if base_p95 and lat and lat > max(5 * base_p95, base_p95 + 50):
            found.append(("latency_spike", s, f"p95 {lat:.0f} ms vs median {base_p95:.0f} ms"))
        if base_pdr and s in pdrs and pdrs[s] < base_pdr - 0.3:
            found.append(("delivery_dip", s, f"PDR {pdrs[s]:.0%} vs median {base_pdr:.0%}"))
    # a router that sends for 10+ s in a row and gets nothing through: cut off from its gateway
    for nid, sec in node.items():
        if types.get(nid) != "MESH":
            continue
        run = 0
        for s in sorted(sec):
            if sec[s].get("NODE_SENT", 0) > 0 and sec[s].get("NODE_RECV", 0) == 0:
                run += 1
                if run == 10:
                    found.append(("node_cut_off", s - 9, f"node {nid}"))
            else:
                run = 0
    # peer links closing in bursts, route discovery storms
    closes, disc = defaultdict(int), defaultdict(int)
    for t, a, _, _ in links:
        if a == "PEER_LINK_CLOSE":
            closes[int(t)] += 1
        elif a == "ROUTE_DISCOVERY_MS":
            disc[int(t)] += 1
    for s, c in closes.items():
        if c >= 6:
            found.append(("link_flap_burst", s, f"{c} link closes in 1 s"))
    # HWMP refreshes all paths together every ~5 s (1-2 s bursts of 15-30 discoveries); a storm is
    # 20+ per second for 3+ seconds in a row (kept by episodes(), MIN_SECONDS)
    for s, c in disc.items():
        if c >= 20:
            found.append(("route_storm", s, f"{c} route discoveries in 1 s"))
    # forwarding hot spot: one relay carries > 50% of all relayed packets for 10+ s
    fwd_total = defaultdict(float)
    for nid, sec in node.items():
        for s, m in sec.items():
            fwd_total[s] += m.get("NODE_FWD", 0)
    for nid, sec in node.items():
        hot = [s for s, m in sec.items() if fwd_total[s] > 50 and m.get("NODE_FWD", 0) > 0.5 * fwd_total[s]]
        if len(hot) >= 10:
            found.append(("relay_hotspot", min(hot), f"node {nid} relays >50% for {len(hot)} s"))
    return episodes(found)


def episodes(found, gap=2):
    """Per-second hits of one kind closer than `gap` s merge into one episode (first second's detail)."""
    out, last = [], {}
    for kind, t, detail in sorted(found, key=lambda f: (f[0], f[1])):
        if kind in ("node_cut_off", "relay_hotspot") or kind not in last or t - last[kind] > gap:
            out.append([kind, t, detail, 1])
        else:
            out[-1][3] += 1  # same episode, one more second
        last[kind] = t
    return [(k, t, f"{d}; {n} s" if n > 1 else d) for k, t, d, n in out if n >= MIN_SECONDS.get(k, 1)]


def analyze(run_dir):
    rows = []
    for inst in sorted(p for p in Path(run_dir).iterdir() if (p / "params.json").exists()):
        data = load_run(inst)
        if not data:
            continue
        params, cfg, per_s, node, counts, types, links = data
        end = float(cfg["sim"]["duration"])
        ev = params.get("event_t")
        pre = window(per_s, WARMUP, ev if ev else end)
        post = window(per_s, ev + SETTLE, end) if ev else pre
        found = anomalies(per_s, node, links, types, end, ev)
        # injected = every executed timeline command after setup (failures, netem, bursts, churn)
        injected = [t for t, _, _ in verify.executed_timeline(inst) if t > 0.5]
        lo, hi = INDUCED_WINDOW
        tagged = [(k, t, d, "induced" if any(lo <= t - e <= hi for e in injected) else "spontaneous") for k, t, d in found]
        kinds = defaultdict(int)
        for k, _, _, cause in tagged:
            kinds[k] += 1
            kinds[f"{cause}"] += 1
        found = tagged
        mesh_nodes = [n for n, ty in types.items() if ty == "MESH"]
        silent = sum(1 for n in mesh_nodes if sum(m.get("NODE_SENT", 0) for m in node[n].values()) > 0
                     and sum(m.get("NODE_RECV", 0) for m in node[n].values()) == 0)
        sent_all = max(1, sum(v.get("SENT_COUNT", 0) for v in per_s.values()))
        rows.append({**params, "run": inst.name,
                     "pdr_total": round(sum(v.get("RECV_COUNT", 0) for v in per_s.values()) / max(1, sum(v.get("SENT_COUNT", 0) for v in per_s.values())), 4),
                     "pdr_pre": round(pre["pdr"], 4) if pre["pdr"] is not None else None,
                     "pdr_post": round(post["pdr"], 4) if post["pdr"] is not None else None,
                     "delivered_share": round(post["rate"] / pre["rate"], 4) if pre["rate"] else None,
                     "p95_pre_ms": pre["p95"], "p95_post_ms": post["p95"],
                     "hops_pre": pre["hops"], "hops_post": post["hops"],
                     "link_closes": counts.get("PEER_LINK_CLOSE", 0), "route_discoveries": counts.get("ROUTE_DISCOVERY_MS", 0),
                     "isolated_routers": silent, "routers_seen": len(mesh_nodes),
                     "retry_drop_share": round(sum(v.get("RETRY_DROP_COUNT", 0) for v in per_s.values()) / sent_all, 4),
                     "anomalies": len(found), **{f"an_{k}": v for k, v in kinds.items()}})
        (inst / "anomalies.json").write_text(json.dumps(found, indent=1))
    keys = sorted({k for r in rows for k in r}, key=lambda k: (k.startswith("an_"), k))
    with open(Path(run_dir) / "results.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)
    return rows


def main():
    for d in sys.argv[1:]:
        rows = analyze(d)
        print(f"== {d}: {len(rows)} runs")
        for r in rows:
            print({k: r.get(k) for k in ("run", "variant", "routers", "pdr_pre", "pdr_post", "delivered_share", "p95_post_ms", "anomalies")})


if __name__ == "__main__":
    main()
