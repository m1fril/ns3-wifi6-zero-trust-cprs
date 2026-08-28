from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RUNNER = PROJECT_ROOT / "run_goodput_20sta_5mbps_wifi6_30_125_10runs.sh"


def test_wifi6_runner_preserves_the_validated_20_sta_five_mbps_experiment():
    source = RUNNER.read_text()

    assert 'output_dir="results/goodput_20sta_5mbps_wifi6_ap4x4_30_125_10runs"' in source
    assert "--nAps=1" in source
    assert "--nStas=20" in source
    assert "--pktInterval=2400" in source
    assert "--fixedStaRingPlacement=1" in source
    assert "--fixedTxPowerDbm=24" in source
    assert "--enableNakagamiFading=1" in source
    assert "--compactMetrics=1" in source
    assert "--RngRun=\"$run\"" in source
    assert 'run_filter="${RUN_FILTER:-}"' in source
