#!/usr/bin/env python3
"""Figures of the article on mesh resilience to node overload (python3 paper/figs.py [--check]).

Reads runs/campaign-*/results.csv and runs/warehouse/results.csv; figures 1 and 2 also need the raw
per-run CSVs (regenerate them with campaign.py, see README). Writes paper/figures/*.png; --check prints
the numbers quoted in the text, including the airtime-model fit T_hop(L) = T0 + 8L/R_eff.
Figure 5 (node state before/after a bridge failure) is kept as a static PNG.
"""
import csv, json, math, statistics as st, sys
from collections import defaultdict
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Rectangle
from matplotlib.ticker import FuncFormatter

HERE = Path(__file__).resolve().parent
RUNS = HERE.parent / "runs"
OUT = HERE / "figures"
plt.rcParams.update({"font.family": "Times New Roman", "font.size": 10, "axes.spines.top": False, "axes.spines.right": False,
                     "axes.grid": True, "grid.color": "#e2e2e2", "grid.linewidth": 0.6, "axes.edgecolor": "#555", "savefig.dpi": 300})
ACC, OR, GREY, RED, GREEN = "#1f5fbf", "#d9822b", "#9a9a9a", "#c0392b", "#2e8b3a"
W = 6.3
med = st.median
comma = FuncFormatter(lambda v, _: f"{v:g}".replace(".", ","))


def rows(name):
    return list(csv.DictReader(open(RUNS / name / "results.csv")))


def f(x):
    return float(x) if x not in ("", None) else None


def save(fig, name):
    fig.tight_layout(); fig.savefig(OUT / name, bbox_inches="tight"); plt.close(fig)


def per_node_sent(inst, until):
    """NodeID -> packets sent in (10, until]; plus NodeID -> cell, and the node types."""
    sent, cell = defaultdict(float), {}
    for r in csv.DictReader(open(next((RUNS / inst).glob("Bianchi-*.csv")))):
        a = r["Action"]
        if a == "NODE_SENT" and 10 < float(r["Time_s"]) <= until:
            sent[r["NodeID"]] += float(r["Value"] or 0)
        elif a == "STA_CELL":
            cell[r["NodeID"]] = int(float(r["Value"]))
    return sent, cell


# ---------- 1. normal: PDR vs offered load per gateway; runs in front, median faint
def fig1(check):
    pts = []
    for r in rows("campaign-normal"):
        inst = RUNS / "campaign-normal" / r["run"]
        thr = [float(x["Value"]) for x in csv.DictReader(open(next(inst.glob("Bianchi-*.csv"))))
               if x["Action"] == "AGGREGATE_THROUGHPUT_MBPS" and float(x["Time_s"]) > 10]
        pdr = f(r["pdr_pre"])
        pts.append({"off": st.fmean(thr) / pdr / int(r["gateways"]), "pdr": pdr * 100, "gw": int(r["gateways"])})
    if check:
        low = [p["pdr"] for p in pts if p["off"] <= 1.5]
        print("fig1: n", len(pts), "PDR at <=1.5 Mbit/s per gw: median", round(med(low), 1), "IQR", [round(x, 1) for x in st.quantiles(low, n=4)])
        for g in (1, 2, 3):
            v = [p["pdr"] for p in pts if p["gw"] == g]
            print(f"   {g} gw: n={len(v)} median PDR {med(v):.1f}")
    fig, ax = plt.subplots(figsize=(W, 3.2))
    ax.axvline(1.5, color="#888", ls=":", lw=0.8); ax.axvline(2.5, color="#666", ls="--", lw=0.8)
    edges = [0, .5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 8]; bx, by = [], []
    for a, b in zip(edges, edges[1:]):
        sel = [p for p in pts if a <= p["off"] < b]
        if sel: bx.append(med([p["off"] for p in sel])); by.append(med([p["pdr"] for p in sel]))
    ax.plot(bx, by, "-", color=GREY, lw=1.2, alpha=.55, zorder=2, label="медіана за інтервалами")
    ax.scatter([p["off"] for p in pts], [p["pdr"] for p in pts], s=20, color=ACC, alpha=.85, edgecolor="white", lw=.4, zorder=3, label="прогін")
    ax.text(0.08, 6, "нормальний режим", fontsize=9, color="#444"); ax.text(2.58, 6, "перевантаження", fontsize=9, color="#444")
    ax.set_xlim(0, 8); ax.set_ylim(0, 102); ax.set_xlabel("Запропоноване навантаження на один шлюз, Мбіт/с"); ax.set_ylabel("PDR, %")
    ax.xaxis.set_major_formatter(comma); ax.legend(loc="upper right", frameon=False, fontsize=9)
    save(fig, "fig1.png")


# ---------- 2. single failure: PDR after vs share of traffic that did not pass the failed node
def fig2(check):
    pts = []
    for r in rows("campaign-single_failure"):
        if f(r["pdr_post"]) is None:
            continue
        inst = f"campaign-single_failure/{r['run']}"
        sent, cell = per_node_sent(inst, float(r["event_t"]))
        total = sum(sent.values())
        g, n = int(r["gateways"]), int(r["routers"])
        if r["variant"] == "gateway":
            failed = int(r["target"].split(":")[1])
            kept = sum(v for nid, v in sent.items() if cell.get(nid) is not None and cell[nid] != failed)
        else:  # bridge = last router; cluster B = the cluster_b routers before it
            n_a = n - 1 - int(r["cluster_b"])
            kept = sum(v for nid, v in sent.items() if int(nid) - g - 1 < n_a)
        pts.append({"v": r["variant"], "pred": 100 * kept / total, "pdr": 100 * f(r["pdr_post"])})
    if check:
        for v in ("gateway", "bridge"):
            p = [x for x in pts if x["v"] == v]
            xs, ys = [x["pred"] for x in p], [x["pdr"] for x in p]
            rr = st.correlation(xs, ys); z = math.atanh(rr); se = 1 / math.sqrt(len(xs) - 3)
            mae = st.fmean(abs(a - b) for a, b in zip(xs, ys))
            print(f"fig2 {v}: n={len(p)} r={rr:.3f} 95% CI [{math.tanh(z - 1.96 * se):.2f}; {math.tanh(z + 1.96 * se):.2f}] MAE={mae:.1f} pp median PDR {med(ys):.1f}")
    fig, ax = plt.subplots(figsize=(W, 3.6))
    for v, c, lab, mk in (("gateway", ACC, "відмова одного з 3 шлюзів", "o"), ("bridge", OR, "відмова мосту між кластерами", "s")):
        p = [x for x in pts if x["v"] == v]
        ax.scatter([x["pred"] for x in p], [x["pdr"] for x in p], s=18, color=c, marker=mk, alpha=.85, edgecolor="white", lw=.4, label=lab)
    ax.set_xlim(0, 102); ax.set_ylim(0, 102); ax.set_xlabel("Частка трафіку, що йшов НЕ через вузол, який відмовив, %")
    ax.set_ylabel("PDR після відмови, %"); ax.legend(loc="lower right", frameon=False, fontsize=9)
    save(fig, "fig2.png")


# ---------- 3. collapse: slope lines before -> after for each run, one panel per mechanism
def fig3(check):
    rs = rows("campaign-collapse")
    names = (("gateway_only", "Втрата єдиного шлюзу"), ("overload", "Перевантаження"), ("mass_failure", "Відмова 70–95 % вузлів"))
    fig, axs = plt.subplots(1, 3, figsize=(W, 2.9), sharey=True)
    for ax, (v, title) in zip(axs, names):
        p = [(100 * f(r["pdr_pre"]), 100 * f(r["pdr_post"])) for r in rs if r["variant"] == v and f(r["pdr_post"]) is not None]
        for a, b in p:
            ax.plot([0, 1], [a, b], "-", color=GREY, lw=0.8, alpha=.55)
        ma, mb = med([a for a, _ in p]), med([b for _, b in p])
        ax.plot([0, 1], [ma, mb], "-o", color=ACC, lw=2.2, ms=4, zorder=3)
        ax.text(1.04, mb, f"{mb:.0f} %".replace(".", ","), va="center", fontsize=8.5, color=ACC)
        ax.text(-0.04, ma, f"{ma:.0f} %", va="center", ha="right", fontsize=8.5, color=ACC)
        ax.axhspan(0, 5, color="#f3d6d3", zorder=0)
        ax.set_xticks([0, 1]); ax.set_xticklabels(["до", "після"]); ax.set_xlim(-0.3, 1.35); ax.set_title(f"{title}\n(n = {len(p)})", fontsize=9)
        if check:
            print(f"fig3 {v}: n={len(p)} median pre {ma:.1f} post {mb:.1f} <=5%: {sum(b <= 5 for _, b in p)}")
    axs[0].set_ylabel("PDR, %"); axs[0].set_ylim(-2, 102)
    h = [Line2D([], [], color=GREY, lw=1, label="окремий прогін"), Line2D([], [], color=ACC, lw=2.2, marker="o", label="медіана"),
         Rectangle((0, 0), 1, 1, color="#f3d6d3", label="PDR ≤ 5 % (повний розпад)")]
    fig.legend(handles=h, loc="lower center", ncol=3, frameon=False, fontsize=8.5, bbox_to_anchor=(0.5, -0.07))
    save(fig, "fig3.png")


# ---------- 4. load: delivered vs offered, one small panel per packet size, with the airtime model (5)
def capacity_model():
    """Fits T_hop(L) = T0 + b·L to the measured maxima (per hop, h at saturation). Returns T0 [us], b [us/B], {L: (S_max, h)}."""
    R = rows("campaign-load")
    best = {}
    for size in (64, 256, 512, 1024, 1472):
        cand = []
        for iv in (10, 20, 50, 100, 200):
            rr = [r for r in R if int(r["size_b"]) == size and int(r["interval_ms"]) == iv]
            cand.append((med([f(r["offered_mbps"]) * f(r["pdr_pre"]) for r in rr]), med([f(r["hops_pre"]) for r in rr])))
        best[size] = max(cand)
    xs = list(best); ys = [8 * L / best[L][0] / best[L][1] for L in xs]  # us per packet per hop
    mx, my = st.fmean(xs), st.fmean(ys)
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)
    return my - b * mx, b, best


def fig4(check):
    R = rows("campaign-load")
    T0, b, best = capacity_model()
    sizes = (64, 256, 512, 1024, 1472)
    fig, axs = plt.subplots(1, 5, figsize=(W, 2.7), sharex=True, sharey=True)
    for ax, size in zip(axs, sizes):
        pts = defaultdict(list)
        for r in R:
            if int(r["size_b"]) == size:
                pts[int(r["interval_ms"])].append((f(r["offered_mbps"]), f(r["offered_mbps"]) * f(r["pdr_pre"])))
        xs = sorted(pts, reverse=True)
        off = [med([o for o, _ in pts[i]]) for i in xs]; dl = [med([d for _, d in pts[i]]) for i in xs]
        for i in xs:
            ax.scatter([o for o, _ in pts[i]], [d for _, d in pts[i]], s=6, color=GREY, alpha=.6, zorder=2)
        ax.plot(off, dl, "-o", color=ACC, lw=1.6, ms=3, zorder=3)
        cap, h = best[size]
        smax = 8 * size / (h * (T0 + b * size))  # model (5), Mbit/s (bits per microsecond)
        grid = [0.03 * 1.15 ** k for k in range(60)]
        ax.plot(grid, [min(x, smax) for x in grid], "--", color=GREEN, lw=1.1, zorder=2)
        ax.axhline(cap, color=OR, lw=0.8, ls=":")
        num = lambda v: f"{v:.1f}".replace(".", ",")
        ax.set_title(f"{size} Б\nмакс. {num(cap)} · модель {num(smax)}", fontsize=8.5)
        ax.set_xscale("log"); ax.set_yscale("log"); ax.set_xlim(0.03, 40); ax.set_ylim(0.03, 40)
        ax.set_xticks([0.1, 1, 10]); ax.set_yticks([0.1, 1, 10])
        ax.xaxis.set_major_formatter(comma); ax.yaxis.set_major_formatter(comma); ax.minorticks_off()
        if check:
            print(f"fig4 {size} B: max {cap:.2f}, model {smax:.2f} Mbit/s (h={h:.2f})")
    if check:
        print(f"fig4 model: T0={T0:.0f} us, b={b:.3f} us/B, R_eff={8 / b:.2f} Mbit/s")
    axs[0].set_ylabel("Доставлено, Мбіт/с")
    fig.supxlabel("Запропоноване навантаження на мережу, Мбіт/с", fontsize=10)
    h = [Line2D([], [], color=ACC, marker="o", ms=3, label="медіана 4 прогонів"), Line2D([], [], color=GREY, marker="o", ls="", ms=3, label="прогін"),
         Line2D([], [], color=GREEN, ls="--", label="модель (5)"), Line2D([], [], color=OR, ls=":", label="максимальне значення")]
    fig.legend(handles=h, loc="lower center", ncol=4, frameon=False, fontsize=8, bbox_to_anchor=(0.5, -0.1))
    save(fig, "fig4.png")


# ---------- 6–7. density (two timer settings): PDR and anomalies as separate figures
def density_pts(name):
    return [{"d": f(r["density_per_ha"]), "pdr": 100 * f(r["pdr_pre"]), "sp": int(r.get("an_spontaneous") or 0)} for r in rows(name)]


def fig67(check):
    old, lin = density_pts("campaign-density"), density_pts("campaign-density-linux")
    edges = [1, 3, 6, 10, 15, 25, 40, 60, 75, 100]

    def medians(p, k):
        bx, by = [], []
        for a, b in zip(edges, edges[1:]):
            sel = [x for x in p if a <= x["d"] < b]
            if sel: bx.append(med([x["d"] for x in sel])); by.append(med([x[k] for x in sel]))
        return bx, by
    if check:
        pairs = [(a["pdr"], b["pdr"]) for a, b in zip(old, lin) if a["d"] >= 60]
        print("fig6 >=60/ha pairs", len(pairs), "median ns3", med([a for a, _ in pairs]), "linux", med([b for _, b in pairs]))
    for name, k, ylab, ylim in (("fig6.png", "pdr", "PDR, %", (-2, 102)), ("fig7.png", "sp", "Спонтанних аномалій за прогін", (-2, 80))):
        fig, ax = plt.subplots(figsize=(W, 3.1))
        ax.axvspan(15, 60, color="#eeeeee", zorder=0); ax.axvline(6, color="#666", ls="--", lw=0.8); ax.axvline(60, color="#666", ls="--", lw=0.8)
        ax.scatter([x["d"] for x in lin], [x[k] for x in lin], s=13, color=GREY, alpha=.8, label="прогін (таймери як у Linux)")
        ax.plot(*medians(old, k), "--s", color=OR, lw=1.4, ms=3.5, label="медіана, таймери ns-3 за замовчуванням (41 мс)")
        ax.plot(*medians(lin, k), "-o", color=ACC, lw=2, ms=4, label="медіана, таймери як у Linux (100 мс)")
        ax.set_xscale("log"); ax.set_xlim(1, 100); ax.set_ylim(*ylim); ax.set_xticks([1, 2, 5, 10, 20, 50, 100]); ax.xaxis.set_major_formatter(comma)
        ax.set_xlabel("Щільність, вузлів на гектар (логарифмічна шкала)"); ax.set_ylabel(ylab)
        top = ylim[1] * 1.0
        ax.text(1.1, top + 1, "мережа рветься", fontsize=9, color="#444"); ax.text(18, top + 1, "оптимум", fontsize=9, weight="bold"); ax.text(61, top + 1, "деградація", fontsize=9, color="#444")
        fig.legend(*ax.get_legend_handles_labels(), loc="lower center", ncol=2, frameon=False, fontsize=8, bbox_to_anchor=(0.5, -0.1))
        save(fig, name)


# ---------- 8. warehouse plan
def fig8(check):
    gws = [(0, 0), (40, 40), (60, 80)]
    pts = [(x, y) for y in range(0, 81, 20) for x in range(0, 101, 20) if (x, y) not in gws][:25]
    fig, ax = plt.subplots(figsize=(W, 5.0))
    ax.add_patch(Rectangle((-12, -12), 124, 104, fill=False, lw=1.6, color="#333"))
    for y0 in (6, 26, 46, 66):  # rack rows between the rows of mesh points
        for x0 in (-6, 34, 74):
            ax.add_patch(Rectangle((x0 + 2, y0), 32, 8, color="#d9d2c3", ec="#a89f8c", lw=0.6, zorder=1))
    for x in range(-6, 112, 8):
        ax.add_patch(Rectangle((x, -12), 5, 2.2, color="#8a8a8a", zorder=1))
    ax.text(50, -7.5, "ворота зони відвантаження", ha="center", fontsize=8.5, color="#444")
    ax.add_patch(Rectangle((92, 84), 18, 6.5, color="#e8eef7", ec=ACC, lw=0.8, zorder=1))
    ax.text(101, 87.2, "серверна", ha="center", va="center", fontsize=8, color=ACC)
    for x, y in pts:
        for a, b in pts + gws:
            if (a, b) > (x, y) and math.dist((x, y), (a, b)) <= 51:
                ax.plot([x, a], [y, b], color="#cfd8e6", lw=0.4, zorder=0)
    for gx, gy in gws:
        ax.plot([gx, gx, 92], [gy, 87, 87], color=ACC, lw=0.9, ls="--", zorder=2)  # up the aisle, then along the wall
    ax.scatter(*zip(*pts), s=42, marker="^", color=GREEN, edgecolor="white", lw=0.6, zorder=3)
    ax.scatter(*zip(*gws), s=110, marker="s", color=ACC, edgecolor="#123", lw=0.8, zorder=4)
    ax.annotate("", xy=(20, 3.5), xytext=(40, 3.5), arrowprops=dict(arrowstyle="<->", color="#444", lw=0.8))
    ax.text(30, 4.5, "20 м", ha="center", fontsize=8.5)
    ax.annotate("", xy=(0, 96), xytext=(100, 96), arrowprops=dict(arrowstyle="<->", color="#444", lw=0.8))
    ax.text(50, 97.5, "100 м", ha="center", fontsize=8.5)
    ax.annotate("", xy=(-15, 0), xytext=(-15, 80), arrowprops=dict(arrowstyle="<->", color="#444", lw=0.8))
    ax.text(-16.5, 40, "80 м", ha="right", va="center", rotation=90, fontsize=8.5)
    ax.set_xlim(-22, 116); ax.set_ylim(-15, 101); ax.set_aspect("equal"); ax.axis("off")
    h = [Line2D([], [], marker="s", ls="", color=ACC, mec="#123", ms=8, label="шлюз (кабель до серверної)"),
         Line2D([], [], marker="^", ls="", color=GREEN, ms=7, label="точка-ретранслятор над стелажами"),
         Rectangle((0, 0), 1, 1, color="#d9d2c3", ec="#a89f8c", label="ряд стелажів"),
         Line2D([], [], color="#cfd8e6", lw=1.2, label="зв’язок між точками (≤ 51 м)"),
         Line2D([], [], color=ACC, lw=0.9, ls="--", label="дротове підключення")]
    fig.legend(handles=h, loc="lower center", ncol=2, frameon=False, fontsize=8.5, bbox_to_anchor=(0.5, -0.09))
    save(fig, "fig8.png")


# ---------- 9–10. warehouse: load and relay failures (means over seeds)
def wh_rows():
    p = RUNS / "warehouse/results.csv"
    return list(csv.DictReader(open(p))) if p.exists() else []


def fig910(check):
    R = wh_rows()
    if not R:
        print("warehouse results missing"); return
    loads = (100, 250, 500, 1000, 2000)
    fig, ax = plt.subplots(figsize=(W, 3.0))
    for size, c, lab, mk in ((128, ACC, "пакети 128 Б (датчики)", "o"), (512, OR, "пакети 512 Б (сканери)", "s")):
        ys = []
        for L in loads:
            v = [100 * f(r["pre_pdr"]) for r in R if r["run"].startswith(f"load_{L}_{size}_")]
            ax.scatter([L] * len(v), v, s=10, color=c, alpha=.35)
            ys.append(st.fmean(v))
            if check:
                p95 = [f(r["pre_p95_ms"]) for r in R if r["run"].startswith(f"load_{L}_{size}_")]
                print(f"wh load {L} {size}: n={len(v)} mean PDR {st.fmean(v):.1f} sd {st.stdev(v):.1f} mean p95 {st.fmean(p95):.1f} ms")
        ax.plot(loads, ys, "-" + mk, color=c, lw=1.8, ms=4, label=lab)
    ax.set_xscale("log"); ax.set_xticks(loads); ax.set_xticklabels([str(L) for L in loads]); ax.minorticks_off()
    ax.set_ylim(0, 102); ax.set_xlabel("Сумарне навантаження, пакетів/с"); ax.set_ylabel("PDR, % (середнє за 5 прогонами)")
    ax.legend(loc="lower left", frameon=False, fontsize=8.5)
    save(fig, "fig9.png")
    ks = (0, 5, 10, 15, 20)
    fig, ax = plt.subplots(figsize=(W, 2.8))
    means = []
    for k in ks:
        v = [100 * f(r["post_pdr"]) for r in R if r["run"].startswith(f"fail_{k}_")]
        ax.scatter([k] * len(v), v, s=12, color=ACC, alpha=.4)
        means.append(st.fmean(v))
        if check:
            print(f"wh fail {k}: n={len(v)} mean PDR {st.fmean(v):.1f} sd {st.stdev(v):.1f}")
    if check:
        v = [100 * f(r["post_pdr"]) for r in R if r["run"].startswith("failgw_")]
        print(f"wh fail gw: n={len(v)} mean PDR {st.fmean(v):.1f} sd {st.stdev(v):.1f}")
    ax.plot(ks, means, "-o", color=ACC, lw=1.8, ms=4)
    lo = min(min(100 * f(r["post_pdr"]) for r in R if r["run"].startswith("fail_")), min(means)) - 2
    ax.set_ylim(max(0, math.floor(lo)), 100.5); ax.set_xticks(ks)
    ax.set_xlabel("Відмовило точок-ретрансляторів із 25"); ax.set_ylabel("PDR решти пристроїв, %")
    ax.yaxis.set_major_formatter(comma)
    save(fig, "fig10.png")


if __name__ == "__main__":
    check = "--check" in sys.argv
    for fn in (fig1, fig2, fig3, fig4, fig67, fig8, fig910):
        fn(check)
