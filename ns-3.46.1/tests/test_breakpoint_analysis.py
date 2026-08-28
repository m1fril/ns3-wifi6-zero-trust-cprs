import sys
from pathlib import Path

import pandas as pd
import numpy as np


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import breakpoint_analysis
from breakpoint_analysis import _mark_breakpoint, _place_figure4_legend, _plot_run_metric_points, _plot_run_points, _set_compact_x_label, aggregate_runs, detect_breakpoint, format_window_label, load_sweep, operational_probability, render_figures, summarize_frame, sustained_degradation_indicator


class _AxisProbe:
    def __init__(self):
        self.spans = []
        self.lines = []
        self.annotations = []

    def axvspan(self, *args, **kwargs):
        self.spans.append((args, kwargs))

    def axvline(self, *args, **kwargs):
        self.lines.append((args, kwargs))

    def annotate(self, *args, **kwargs):
        self.annotations.append((args, kwargs))

    def scatter(self, *args, **kwargs):
        self.scatter_args = (args, kwargs)

    def legend(self, *args, **kwargs):
        self.legend_args = (args, kwargs)


def test_article_figures_4_to_9_omit_breakpoint_line_and_legend_entry():
    assert hasattr(breakpoint_analysis, "article_figure_shows_breakpoint_marker")
    marker_policy = breakpoint_analysis.article_figure_shows_breakpoint_marker

    assert [marker_policy(number) for number in range(4, 10)] == [False] * 6


def test_window_label_is_derived_from_the_detected_breakpoint():
    assert format_window_label(40.0) == "20–60 м"


def test_mark_breakpoint_keeps_only_a_line_and_annotation_without_colored_zone():
    axis = _AxisProbe()

    _mark_breakpoint(axis, 120)

    assert axis.spans == []
    assert axis.lines[0][0] == (120,)
    assert axis.lines[0][1]["label"] == "Точка зламу"
    assert axis.annotations[0][0][0] == "Основна зона\nпогіршення: 120 м"


def test_set_compact_x_label_reduces_the_gap_to_the_x_axis():
    import matplotlib.pyplot as plt

    figure, axis = plt.subplots()
    _set_compact_x_label(axis, "Відстань AP–STA, м")

    assert axis.get_xlabel() == "Відстань AP–STA, м"
    assert axis.xaxis.labelpad == 2
    plt.close(figure)


def test_plot_run_points_shows_every_measurement_with_deterministic_jitter():
    axis = _AxisProbe()
    runs = pd.DataFrame(
        {
            "area_m": [100.0] * 10 + [105.0] * 10,
            "throughput_mbps": list(range(20)),
        }
    )

    _plot_run_points(axis, runs)

    x_values, y_values = axis.scatter_args[0]
    assert len(x_values) == 20
    assert len(y_values) == 20
    assert len(set(x_values[:10])) == 10
    assert axis.scatter_args[1]["label"] == "Окремі прогони (n=20)"


def test_plot_run_metric_points_shows_all_140_repetitions():
    axis = _AxisProbe()
    runs = pd.DataFrame(
        {
            "area_m": np.repeat([30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 85.0, 90.0, 100.0, 105.0, 110.0, 115.0, 120.0, 125.0], 10),
            "latency_p95_ms": np.arange(140, dtype=float),
        }
    )

    _plot_run_metric_points(axis, runs, "latency_p95_ms", "Окремі p95-виміри")

    x_values, y_values = axis.scatter_args[0]
    assert len(x_values) == 140
    assert len(y_values) == 140
    assert axis.scatter_args[1]["label"] == "Окремі p95-виміри (n=140)"


def test_place_figure4_legend_uses_the_upper_right_corner():
    axis = _AxisProbe()

    _place_figure4_legend(axis)

    assert axis.legend_args[1]["loc"] == "upper right"


def test_summarize_frame_reports_qos_metrics():
    trace = pd.DataFrame(
        {
            "Action": ["SEND", "SEND", "SEND", "RECV", "RECV", "LATENCY_MS", "LATENCY_MS", "THROUGHPUT_MBPS"],
            "Value": [1, 2, 3, 1, 2, 10, 30, 4.0],
        }
    )

    metrics = summarize_frame(trace)

    assert metrics["throughput_mbps"] == 4.0
    assert metrics["latency_p95_ms"] == 29.0
    assert metrics["loss_pct"] == 100 / 3


def test_summarize_frame_prefers_ap_aggregate_goodput_for_multi_sta_traces():
    trace = pd.DataFrame(
        {
            "Action": ["THROUGHPUT_MBPS", "THROUGHPUT_MBPS", "AGGREGATE_THROUGHPUT_MBPS"],
            "Value": [0.3, 0.4, 9.7],
        }
    )

    metrics = summarize_frame(trace)

    assert metrics["throughput_mbps"] == 9.7


def test_summarize_frame_supports_compact_aggregate_metrics():
    trace = pd.DataFrame(
        {
            "Action": [
                "AGGREGATE_THROUGHPUT_MBPS",
                "LATENCY_P95_MS",
                "LATENCY_P95_MS",
                "SENT_COUNT",
                "SENT_COUNT",
                "RECV_COUNT",
                "RECV_COUNT",
            ],
            "Value": [286.0, 11.0, 17.0, 2500.0, 2500.0, 2450.0, 2400.0],
        }
    )

    metrics = summarize_frame(trace)

    assert metrics["throughput_mbps"] == 286.0
    assert metrics["latency_p95_ms"] == 16.7
    assert np.isclose(metrics["loss_pct"], 3.0)


def test_detect_breakpoint_returns_first_sustained_joint_qos_degradation():
    metrics = pd.DataFrame(
        {
            "area_m": [25, 75, 125, 150, 175, 200],
            "throughput_mbps": [10.0, 9.7, 9.0, 6.5, 5.8, 4.2],
            "latency_p95_ms": [5.0, 5.2, 5.4, 11.0, 13.0, 18.0],
            "loss_pct": [0.0, 0.0, 1.0, 12.0, 18.0, 25.0],
        }
    )

    assert detect_breakpoint(metrics) == 150


def test_detect_breakpoint_accepts_sustained_qos_harm_before_20_percent_goodput_drop():
    metrics = pd.DataFrame(
        {
            "area_m": [70, 80, 85],
            "throughput_mbps": [10.0, 8.1, 5.4],
            "latency_p95_ms": [17.2, 497.7, 499.9],
            "loss_pct": [0.0, 48.4, 67.6],
        }
    )

    assert detect_breakpoint(metrics) == 80


def test_detect_breakpoint_ignores_small_absolute_latency_rises_before_qos_collapse():
    metrics = pd.DataFrame(
        {
            "area_m": [30, 40, 50, 60, 70, 80, 85],
            "throughput_mbps": [10.0, 10.0, 10.0, 10.0, 10.0, 8.1, 5.4],
            "latency_p95_ms": [0.7, 1.3, 3.8, 8.2, 17.2, 497.7, 499.9],
            "loss_pct": [0.0, 0.0, 0.0, 0.0, 0.0, 48.4, 67.6],
        }
    )

    assert detect_breakpoint(metrics) == 80


def test_detect_breakpoint_detects_a_transition_immediately_after_the_baseline():
    metrics = pd.DataFrame(
        {
            "area_m": [100, 125, 150],
            "throughput_mbps": [12.0, 0.0, 0.0],
            "latency_p95_ms": [0.5, float("nan"), float("nan")],
            "loss_pct": [0.0, 100.0, 100.0],
        }
    )

    assert detect_breakpoint(metrics) == 125


def test_sustained_degradation_marks_50_m_after_a_persistent_five_point_loss_increase():
    aggregate = pd.DataFrame(
        {
            "area_m": [30.0, 40.0, 50.0, 60.0],
            "loss_pct": [91.79, 96.01, 98.44, 99.38],
        }
    )

    indicator, breakpoint = sustained_degradation_indicator(aggregate)

    assert indicator["degraded"].tolist() == [0, 0, 1, 1]
    assert breakpoint == 50.0


def test_aggregate_runs_uses_median_per_area_and_reports_sample_count():
    runs = pd.DataFrame(
        {
            "area_m": [100, 100, 150, 150],
            "throughput_mbps": [10.0, 8.0, 5.0, 3.0],
            "latency_p95_ms": [5.0, 7.0, 10.0, 14.0],
            "loss_pct": [0.0, 2.0, 10.0, 12.0],
        }
    )

    aggregate = aggregate_runs(runs)

    assert aggregate.loc[aggregate.area_m == 100, "throughput_mbps"].item() == 9.0
    assert aggregate.loc[aggregate.area_m == 150, "run_count"].item() == 2


def test_operational_probability_uses_the_10_mbit_service_quality_contract():
    runs = pd.DataFrame(
        {
            "area_m": [60.0] * 10 + [70.0] * 10 + [80.0] * 10,
            "throughput_mbps": [9.0] * 10 + [9.0] * 9 + [7.9] + [8.1] * 10,
            "loss_pct": [0.0] * 20 + [6.0] * 10,
            "latency_p95_ms": [20.0] * 20 + [500.0] * 10,
        }
    )

    grouped, d50 = operational_probability(runs)

    assert grouped.p_empirical.tolist() == [1.0, 0.9, 0.0]
    assert 70.0 < d50 < 80.0


def test_load_sweep_supports_latest_distance_named_traces(tmp_path):
    trace = pd.DataFrame(
        {
            "Action": ["SEND", "RECV", "LATENCY_MS", "THROUGHPUT_MBPS"],
            "Value": [1, 1, 0.5, 10.0],
        }
    )
    trace.to_csv(tmp_path / "normal_distance120m_run1.csv", index=False)

    runs, latency_samples = load_sweep(tmp_path)

    assert runs.area_m.tolist() == [120.0]
    assert latency_samples[120.0].tolist() == [0.5]


def test_render_figures_writes_separate_latency_cdf_and_qos_figures(tmp_path):
    metrics = pd.DataFrame(
        {
            "area_m": [70.0, 80.0, 85.0],
            "throughput_mbps": [10.0, 8.1, 5.4],
            "latency_p95_ms": [17.2, 497.7, 499.9],
            "loss_pct": [0.0, 48.4, 67.6],
            "run_count": [10, 10, 10],
        }
    )

    render_figures(metrics, {70.0: [17.2], 80.0: [497.7], 85.0: [499.9]}, 80.0, tmp_path)

    assert (tmp_path / "figure5_breakpoint_goodput_loss.png").is_file()
    assert (tmp_path / "figure6_breakpoint_latency_p95.png").is_file()
    assert (tmp_path / "figure7_breakpoint_goodput_distribution.png").is_file()
    assert (tmp_path / "figure8_breakpoint_loss_distribution.png").is_file()
    assert (tmp_path / "figure6_latency_cdf_breakpoint.png").is_file()
    assert (tmp_path / "figure7_goodput_loss_breakpoint.png").is_file()
