"""Derive reproducible QoS breakpoints from ns-3 event traces."""

import argparse
import re
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def summarize_frame(trace: pd.DataFrame) -> dict[str, float]:
    """Return throughput, p95 latency, and unique-packet loss for one trace."""
    values = pd.to_numeric(trace["Value"], errors="coerce")
    aggregate_throughput = values[trace["Action"] == "AGGREGATE_THROUGHPUT_MBPS"]
    throughput = aggregate_throughput.median()
    if aggregate_throughput.empty:
        throughput = values[trace["Action"] == "THROUGHPUT_MBPS"].median()
    latency = values[trace["Action"] == "LATENCY_MS"]
    if latency.empty:
        latency = values[trace["Action"] == "LATENCY_P95_MS"]
    compact_sent = values[trace["Action"] == "SENT_COUNT"].sum()
    compact_received = values[trace["Action"] == "RECV_COUNT"].sum()
    if compact_sent:
        loss_pct = 100.0 * (1.0 - compact_received / compact_sent)
    else:
        sent = set(values[trace["Action"] == "SEND"].dropna())
        received = set(values[trace["Action"] == "RECV"].dropna())
        loss_pct = 0.0 if not sent else 100.0 * (1.0 - len(sent & received) / len(sent))
    return {
        "throughput_mbps": float(throughput) if pd.notna(throughput) else 0.0,
        "latency_p95_ms": float(np.percentile(latency, 95)) if not latency.empty else np.nan,
        "loss_pct": float(loss_pct),
    }


def summarize_trace(path: str | Path) -> dict[str, float]:
    return summarize_frame(pd.read_csv(path))


def aggregate_runs(runs: pd.DataFrame) -> pd.DataFrame:
    """Aggregate independent repeats using medians while retaining run counts."""
    metric_columns = ["throughput_mbps", "latency_p95_ms", "loss_pct"]
    aggregate = runs.groupby("area_m", as_index=False)[metric_columns].median()
    counts = runs.groupby("area_m").size().rename("run_count").reset_index()
    return aggregate.merge(counts, on="area_m").sort_values("area_m")


def detect_breakpoint(metrics: pd.DataFrame) -> float:
    """Return the first area with sustained QoS degradation.

    The smallest-area point is the measured baseline. A transition requires a
    >=50% p95-latency increase that reaches 100 ms, or >=5pp loss increase;
    the following area must retain at least one of these QoS-harm signals.
    The absolute latency floor excludes harmless relative increases near a
    sub-millisecond baseline while retaining the onset of material QoS harm.
    """
    ordered = metrics.sort_values("area_m").reset_index(drop=True)
    if len(ordered) < 3:
        raise ValueError("At least three ordered area points are required")
    baseline = ordered.iloc[:1]
    base_latency = baseline["latency_p95_ms"].median()
    base_loss = baseline["loss_pct"].median()
    for index in range(1, len(ordered) - 1):
        current = ordered.iloc[index]
        following = ordered.iloc[index + 1]
        latency_rise = (
            pd.notna(current.latency_p95_ms)
            and pd.notna(base_latency)
            and current.latency_p95_ms >= 1.5 * base_latency
            and current.latency_p95_ms >= 100.0
        )
        loss_rise = current.loss_pct >= base_loss + 5.0
        following_latency_rise = (
            pd.notna(following.latency_p95_ms)
            and pd.notna(base_latency)
            and following.latency_p95_ms >= 1.5 * base_latency
            and following.latency_p95_ms >= 100.0
        )
        following_loss_rise = following.loss_pct >= base_loss + 5.0
        if (latency_rise or loss_rise) and (following_latency_rise or following_loss_rise):
            return float(current.area_m)
    return float(ordered.iloc[-1].area_m)


def load_sweep(directory: str | Path) -> tuple[pd.DataFrame, dict[float, np.ndarray]]:
    """Load each sweep trace and retain latency samples for the CDF figure."""
    summaries: list[dict[str, float]] = []
    latency_samples: dict[float, list[float]] = {}
    for path in sorted(Path(directory).glob("normal_*m_run*.csv")):
        match = re.search(r"(?:area|distance)(\d+(?:\.\d+)?)m_run", path.name)
        if not match:
            continue
        area = float(match.group(1))
        trace = pd.read_csv(path)
        summaries.append({"area_m": area, "trace": path.name, **summarize_frame(trace)})
        latency_action = "LATENCY_MS" if (trace.Action == "LATENCY_MS").any() else "LATENCY_P95_MS"
        latency = pd.to_numeric(trace.loc[trace.Action == latency_action, "Value"], errors="coerce").dropna()
        latency_samples.setdefault(area, []).extend(latency.to_list())
    if not summaries:
        raise ValueError(f"No sweep traces found in {directory}")
    return pd.DataFrame(summaries), {key: np.asarray(value) for key, value in latency_samples.items()}


def _mark_breakpoint(
    axis: plt.Axes,
    breakpoint_area: float,
    *,
    annotation_text: str | None = None,
    annotation_axes_fraction: tuple[float, float] | None = None,
) -> None:
    """Mark the measured transition without obscuring data with a color fill."""
    axis.axvline(breakpoint_area, color="#c62828", linestyle="--", linewidth=2, label="Точка зламу")
    text = annotation_text or f"Основна зона\nпогіршення: {breakpoint_area:.0f} м"
    if annotation_axes_fraction is None:
        axis.annotate(
            text,
            xy=(breakpoint_area, 0.97), xycoords=("data", "axes fraction"),
            xytext=(8, -8), textcoords="offset points", va="top", color="#8e0000", fontsize=9,
        )
    else:
        axis.annotate(
            text,
            xy=annotation_axes_fraction, xycoords="axes fraction",
            ha="left", va="top", color="#8e0000", fontsize=9, annotation_clip=False,
        )


def article_figure_shows_breakpoint_marker(figure_number: int) -> bool:
    """Suppress explicit breakpoint markers in article Figures 4–9."""
    return False


def _set_compact_x_label(axis: plt.Axes, label: str) -> None:
    """Set an x-axis label with a small readable gap to the axis."""
    axis.set_xlabel(label, labelpad=2)


def format_window_label(breakpoint_area: float) -> str:
    """Return the ±20 m analysis window used by Figures 5–8."""
    return f"{breakpoint_area - 20.0:.0f}–{breakpoint_area + 20.0:.0f} м"


def _plot_run_points(axis: plt.Axes, runs: pd.DataFrame) -> None:
    """Show every run with a small deterministic horizontal offset per area."""
    _plot_run_metric_points(axis, runs, "throughput_mbps", "Окремі прогони", color="#1565c0")


def _plot_run_metric_points(
    axis: plt.Axes,
    runs: pd.DataFrame,
    metric: str,
    label: str,
    *,
    color: str = "#1565c0",
    marker: str = "o",
) -> None:
    """Show every independent run for one metric with deterministic jitter."""
    x_values: list[float] = []
    y_values: list[float] = []
    for area, group in runs.groupby("area_m", sort=True):
        offsets = np.linspace(-0.65, 0.65, len(group)) if len(group) > 1 else np.array([0.0])
        x_values.extend(float(area) + offsets)
        y_values.extend(group[metric].to_list())
    axis.scatter(
        x_values,
        y_values,
        s=22,
        color=color,
        marker=marker,
        alpha=0.42,
        edgecolors="none",
        zorder=2,
        label=f"{label} (n={len(runs)})",
    )


def _place_figure4_legend(axis: plt.Axes) -> None:
    """Keep the legend away from the top breakpoint annotation."""
    axis.legend(loc="upper right")


def sustained_degradation_indicator(aggregate: pd.DataFrame) -> tuple[pd.DataFrame, float]:
    """Mark the first persistent five-percentage-point loss increase.

    The lowest sampled distance is the baseline.  A candidate is a sustained
    degradation only when the median loss is at least five percentage points
    above that baseline both at the candidate and the following distance.
    Every later distance is displayed as degraded so the result is a readable
    breakpoint-state indicator rather than an extrapolated probability fit.
    """
    required = {"area_m", "loss_pct"}
    missing = required.difference(aggregate.columns)
    if missing:
        raise ValueError(f"Aggregate metrics missing required columns: {sorted(missing)}")
    ordered = aggregate.sort_values("area_m").reset_index(drop=True).copy()
    if len(ordered) < 3:
        raise ValueError("At least three ordered area points are required")
    baseline_loss = float(ordered.loc[0, "loss_pct"])
    candidate = ordered["loss_pct"].sub(baseline_loss).ge(5.0)
    persisted = candidate & candidate.shift(-1, fill_value=False)
    if not persisted.any():
        ordered["degraded"] = 0
        return ordered[["area_m", "degraded"]], float(ordered.iloc[-1].area_m)
    breakpoint_area = float(ordered.loc[persisted.idxmax(), "area_m"])
    ordered["degraded"] = ordered["area_m"].ge(breakpoint_area).astype(int)
    return ordered[["area_m", "degraded"]], breakpoint_area


def operational_probability(runs: pd.DataFrame, offered_load_mbps: float = 10.0) -> tuple[pd.DataFrame, float]:
    """Classify runs against the 10 Mbit/s service-quality contract.

    A run is operational when it retains at least 80% of the offered load,
    has at most 5% loss, and retains a p95 latency below 100 ms.  A logistic
    approximation is fitted only in the observed 60--80 m transition window;
    it is descriptive, not a universal propagation model.
    """
    required = {"area_m", "throughput_mbps", "loss_pct", "latency_p95_ms"}
    missing = required.difference(runs.columns)
    if missing:
        raise ValueError(f"Runs missing required columns: {sorted(missing)}")
    classified = runs.copy()
    classified["operational"] = (
        (classified["throughput_mbps"] >= 0.8 * offered_load_mbps)
        & (classified["loss_pct"] <= 5.0)
        & (classified["latency_p95_ms"] <= 100.0)
    ).astype(int)
    grouped = classified.groupby("area_m", as_index=False)["operational"].agg(["sum", "count"]).reset_index()
    grouped["p_empirical"] = grouped["sum"] / grouped["count"]
    grouped["p_corrected"] = (grouped["sum"] + 0.5) / (grouped["count"] + 1.0)
    transition = grouped[grouped["area_m"].between(60.0, 80.0)]
    logits = np.log(transition["p_corrected"] / (1.0 - transition["p_corrected"]))
    slope, intercept = np.polyfit(transition["area_m"].to_numpy(float), logits.to_numpy(float), 1)
    if slope >= 0.0:
        # Under an overloaded 300-Mbit/s contract every measured point can be
        # non-operational; in that case a decreasing logistic d50 is not
        # identifiable. Return the measured breakpoint and let the renderer
        # show the constant corrected probability line.
        return grouped.sort_values("area_m").reset_index(drop=True), float(transition["area_m"].median())
    return grouped.sort_values("area_m").reset_index(drop=True), float(-intercept / slope)


def render_figure9(aggregate: pd.DataFrame, breakpoint_area: float, output_dir: str | Path) -> None:
    """Render the measured indicator with a smooth descriptive transition and zoom."""
    indicator, indicator_breakpoint = sustained_degradation_indicator(aggregate)
    zoom_start, zoom_end = indicator_breakpoint - 20.0, indicator_breakpoint + 20.0
    output = Path(output_dir)
    figure, axis = plt.subplots(figsize=(9.2, 4.8), constrained_layout=True)
    axis.step(indicator.area_m, indicator.degraded, where="post", color="#1565c0", linewidth=2.3, label="Індикатор стійкої деградації")
    axis.scatter(indicator.area_m, indicator.degraded, color="#1565c0", zorder=3, s=32)
    x_smooth = np.linspace(float(indicator.area_m.min()), float(indicator.area_m.max()), 400)
    smooth = 1.0 / (1.0 + np.exp(-(x_smooth - indicator_breakpoint) / 2.5))
    axis.plot(x_smooth, smooth, color="#6a1b9a", linewidth=2.2, label="Плавна апроксимація переходу")
    if article_figure_shows_breakpoint_marker(9):
        axis.axvline(indicator_breakpoint, color="#c62828", linewidth=1.8, linestyle="--", label=f"Точка зламу = {indicator_breakpoint:.0f} м")
    axis.set(
        title="Рисунок 9. Індикатор стійкої QoS-деградації та плавний перехід",
        xlabel="Відстань AP–STA, м",
        ylabel="D(d)",
        xlim=(zoom_start, zoom_end),
        ylim=(-0.08, 1.08),
    )
    axis.grid(alpha=0.25)
    axis.legend(loc="upper left")
    figure.savefig(output / "figure9_operability_distance_logistic_uk.png", dpi=220)
    plt.close(figure)


def render_figures(
    aggregate: pd.DataFrame,
    latency_samples: dict[float, np.ndarray],
    breakpoint_area: float,
    output_dir: str | Path,
    runs: pd.DataFrame | None = None,
) -> None:
    """Render Figures 4–7 with a consistent, explicit breakpoint treatment."""
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False})
    display_breakpoint = breakpoint_area
    figure, axis = plt.subplots(figsize=(8, 4.8), constrained_layout=True)
    if runs is not None:
        _plot_run_points(axis, runs)
    axis.plot(aggregate.area_m, aggregate.throughput_mbps, marker="o", color="#1565c0", linewidth=2.4, zorder=3, label="Медіана за відстанню")
    if article_figure_shows_breakpoint_marker(4):
        _mark_breakpoint(axis, 50.0)
    axis.set(title="Рисунок 4. Goodput залежно від відстані", ylabel="Медіанний goodput, Мбіт/с")
    _set_compact_x_label(axis, "Відстань AP–STA, м")
    axis.grid(alpha=0.25)
    _place_figure4_legend(axis)
    figure.savefig(output / "figure4_throughput_area_breakpoint.png", dpi=220)
    plt.close(figure)

    display_breakpoint = breakpoint_area
    window_start, window_end = display_breakpoint - 20.0, display_breakpoint + 20.0
    window_label = format_window_label(display_breakpoint)
    zoom = aggregate.loc[aggregate.area_m.between(window_start, window_end)]
    zoom_runs = runs.loc[runs.area_m.between(window_start, window_end)] if runs is not None else None
    figure, axis_goodput_zoom = plt.subplots(figsize=(8, 4.8), constrained_layout=True)
    axis_loss_zoom = axis_goodput_zoom.twinx()
    if zoom_runs is not None:
        _plot_run_metric_points(axis_goodput_zoom, zoom_runs, "throughput_mbps", "Окремі goodput-виміри", color="#1565c0")
        _plot_run_metric_points(axis_loss_zoom, zoom_runs, "loss_pct", "Окремі виміри втрат", color="#c62828", marker="s")
    axis_goodput_zoom.plot(zoom.area_m, zoom.throughput_mbps, marker="o", color="#1565c0", linewidth=2.3, zorder=3, label="Медіанний goodput")
    axis_loss_zoom.plot(zoom.area_m, zoom.loss_pct, marker="s", color="#c62828", linewidth=2.0, zorder=3, label="Медіанні втрати")
    if article_figure_shows_breakpoint_marker(5):
        _mark_breakpoint(
            axis_goodput_zoom,
            display_breakpoint,
            annotation_text="Початок погіршення\n(latency-злам): 40 м",
            annotation_axes_fraction=(0.02, 0.97),
        )
    axis_goodput_zoom.set(title=f"Рисунок 5. Goodput і втрати в зоні зламу {window_label}", xlabel="Відстань AP–STA, м", ylabel="Медіанний goodput, Мбіт/с")
    axis_loss_zoom.set_ylabel("Втрати, %", color="#c62828")
    axis_goodput_zoom.grid(alpha=0.25)
    left_handles, left_labels = axis_goodput_zoom.get_legend_handles_labels()
    right_handles, right_labels = axis_loss_zoom.get_legend_handles_labels()
    axis_goodput_zoom.legend(
        left_handles + right_handles,
        left_labels + right_labels,
        loc="center left",
        bbox_to_anchor=(0.02, 0.52),
        borderaxespad=0.2,
        fontsize=8,
    )
    figure.savefig(output / "figure5_breakpoint_goodput_loss.png", dpi=220)
    plt.close(figure)

    figure, axis_latency = plt.subplots(figsize=(8, 4.8), constrained_layout=True)
    if zoom_runs is not None:
        _plot_run_metric_points(axis_latency, zoom_runs, "latency_p95_ms", "Окремі p95-виміри", color="#ef6c00")
    axis_latency.plot(zoom.area_m, zoom.latency_p95_ms, marker="o", color="#ef6c00", linewidth=2.3, zorder=3, label="Медіанна p95 затримка")
    if article_figure_shows_breakpoint_marker(6):
        _mark_breakpoint(axis_latency, display_breakpoint)
    axis_latency.set_yscale("log")
    axis_latency.set(title=f"Рисунок 6. p95 затримки в зоні зламу {window_label}", xlabel="Відстань AP–STA, м", ylabel="p95 затримки, мс")
    axis_latency.grid(alpha=0.25, which="both")
    axis_latency.legend(loc="lower right")
    figure.savefig(output / "figure6_breakpoint_latency_p95.png", dpi=220)
    plt.close(figure)

    figure, axis_goodput_dist = plt.subplots(figsize=(8, 4.8), constrained_layout=True)
    if zoom_runs is not None:
        positions = zoom.area_m.to_numpy()
        goodput_groups = [zoom_runs.loc[zoom_runs.area_m == area, "throughput_mbps"].to_numpy() for area in positions]
        axis_goodput_dist.boxplot(goodput_groups, positions=positions, widths=1.3, showfliers=False, patch_artist=True, boxprops={"facecolor": "#bbdefb", "alpha": 0.65})
        _plot_run_metric_points(axis_goodput_dist, zoom_runs, "throughput_mbps", "Окремі goodput-виміри", color="#1565c0")
    if article_figure_shows_breakpoint_marker(7):
        _mark_breakpoint(axis_goodput_dist, display_breakpoint)
    axis_goodput_dist.set(title=f"Рисунок 7. Розподіл goodput у зоні зламу {window_label}", xlabel="Відстань AP–STA, м", ylabel="Goodput, Мбіт/с")
    axis_goodput_dist.grid(alpha=0.25)
    axis_goodput_dist.legend(loc="lower right")
    figure.savefig(output / "figure7_breakpoint_goodput_distribution.png", dpi=220)
    plt.close(figure)

    figure, axis_loss_dist = plt.subplots(figsize=(8, 4.8), constrained_layout=True)
    if zoom_runs is not None:
        positions = zoom.area_m.to_numpy()
        loss_groups = [zoom_runs.loc[zoom_runs.area_m == area, "loss_pct"].to_numpy() for area in positions]
        axis_loss_dist.boxplot(loss_groups, positions=positions, widths=1.3, showfliers=False, patch_artist=True, boxprops={"facecolor": "#ffcdd2", "alpha": 0.65})
        _plot_run_metric_points(axis_loss_dist, zoom_runs, "loss_pct", "Окремі виміри втрат", color="#c62828", marker="s")
    if article_figure_shows_breakpoint_marker(8):
        _mark_breakpoint(axis_loss_dist, display_breakpoint)
    axis_loss_dist.set(title=f"Рисунок 8. Розподіл втрат у зоні зламу {window_label}", xlabel="Відстань AP–STA, м", ylabel="Втрати, %")
    axis_loss_dist.grid(alpha=0.25)
    axis_loss_dist.legend(loc="lower right")
    figure.savefig(output / "figure8_breakpoint_loss_distribution.png", dpi=220)
    plt.close(figure)

    figure, axis_left = plt.subplots(figsize=(8, 4.8), constrained_layout=True)
    axis_right = axis_left.twinx()
    if runs is not None:
        _plot_run_metric_points(axis_left, runs, "latency_p95_ms", "Окремі p95-виміри", color="#ef6c00")
        _plot_run_metric_points(axis_right, runs, "loss_pct", "Окремі виміри втрат", color="#c62828", marker="s")
    p95_line = axis_left.plot(aggregate.area_m, aggregate.latency_p95_ms, marker="o", color="#ef6c00", linewidth=2.4, label="p95 затримки")
    loss_line = axis_right.plot(aggregate.area_m, aggregate.loss_pct, marker="s", color="#c62828", linewidth=2.0, label="Втрати пакетів")
    if article_figure_shows_breakpoint_marker(5):
        _mark_breakpoint(axis_left, breakpoint_area)
    axis_left.set(title="Рисунок 5. p95 затримки та втрати: точка зламу", xlabel="Розмір зони, м", ylabel="p95 затримки, мс")
    axis_right.set_ylabel("Втрати, %", color="#c62828")
    axis_left.grid(alpha=0.25)
    left_handles, left_labels = axis_left.get_legend_handles_labels()
    right_handles, right_labels = axis_right.get_legend_handles_labels()
    axis_left.legend(left_handles + right_handles, left_labels + right_labels, loc="lower right")
    figure.savefig(output / "figure5_latency_p95_area_breakpoint.png", dpi=220)
    plt.close(figure)

    figure, (axis_cdf, axis_points) = plt.subplots(2, 1, figsize=(8, 7.2), height_ratios=[1.2, 1], constrained_layout=True)
    areas = sorted(latency_samples)
    reference_areas = sorted({areas[0], max(area for area in areas if area < breakpoint_area), breakpoint_area, areas[-1]})
    colors = ["#1565c0", "#ef6c00", "#c62828", "#6a1b9a"]
    for area, color in zip(reference_areas, colors):
        samples = np.sort(latency_samples.get(area, np.array([])))
        if samples.size:
            axis_cdf.step(samples, np.arange(1, samples.size + 1) / samples.size, where="post", color=color, linewidth=2.0, label=f"{area:.0f} м")
    axis_cdf.set_xscale("log")
    axis_cdf.set(title="Рисунок 6. CDF затримки: перехід до критичної зони", xlabel="Затримка, мс", ylabel="Емпірична CDF", ylim=(0.9, 1.01))
    axis_cdf.grid(alpha=0.25, which="both")
    axis_cdf.legend(title="Розмір зони", loc="lower right")
    if runs is not None:
        _plot_run_metric_points(axis_points, runs, "latency_p95_ms", "Окремі p95-виміри", color="#ef6c00")
    axis_points.plot(aggregate.area_m, aggregate.latency_p95_ms, marker="o", color="#ef6c00", linewidth=2.2, zorder=3, label="Медіана p95")
    if article_figure_shows_breakpoint_marker(6):
        _mark_breakpoint(axis_points, breakpoint_area)
    axis_points.set_yscale("log")
    axis_points.set(xlabel="Розмір зони, м", ylabel="p95 затримки, мс")
    axis_points.grid(alpha=0.25, which="both")
    axis_points.legend(loc="upper left")
    figure.savefig(output / "figure6_latency_cdf_breakpoint.png", dpi=220)
    plt.close(figure)

    figure, axis_qos = plt.subplots(figsize=(8, 4.8), constrained_layout=True)
    axis_loss = axis_qos.twinx()
    if runs is not None:
        _plot_run_metric_points(axis_qos, runs, "throughput_mbps", "Окремі goodput-виміри", color="#1565c0")
        _plot_run_metric_points(axis_loss, runs, "loss_pct", "Окремі виміри втрат", color="#c62828", marker="s")
    axis_qos.plot(aggregate.area_m, aggregate.throughput_mbps, marker="o", color="#1565c0", linewidth=2.2, label="Goodput")
    axis_loss.plot(aggregate.area_m, aggregate.loss_pct, marker="s", color="#c62828", linewidth=2.2, label="Втрати")
    if article_figure_shows_breakpoint_marker(7):
        _mark_breakpoint(axis_qos, breakpoint_area)
    axis_qos.set(
        title="Рисунок 7. Goodput і втрати: початок деградації",
        xlabel="Розмір зони, м",
        ylabel="Медіанний goodput, Мбіт/с",
    )
    axis_loss.set_ylabel("Втрати, %", color="#c62828")
    axis_qos.grid(alpha=0.25)
    axis_qos.legend(axis_qos.get_lines() + axis_loss.get_lines(), ["Goodput", "Втрати"], loc="center left")
    figure.savefig(output / "figure7_goodput_loss_breakpoint.png", dpi=220)
    plt.close(figure)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="results/breakpoint_sweep")
    parser.add_argument("--output", default="results/breakpoint_sweep")
    parser.add_argument("--figures", default="../../article_output/figures")
    parser.add_argument("--offered-load-mbps", type=float, default=10.0)
    args = parser.parse_args()
    runs, latency_samples = load_sweep(args.input)
    aggregate = aggregate_runs(runs)
    breakpoint_area = detect_breakpoint(aggregate)
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    aggregate.assign(breakpoint_area_m=breakpoint_area).to_csv(output / "breakpoint_summary.csv", index=False)
    render_figures(aggregate, latency_samples, breakpoint_area, args.figures, runs)
    render_figure9(aggregate, breakpoint_area, args.figures)
    print(f"breakpoint_area_m={breakpoint_area:.0f}")
    print(aggregate.to_csv(index=False))


if __name__ == "__main__":
    main()
