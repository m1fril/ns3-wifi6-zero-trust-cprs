import subprocess
import tempfile
import unittest
from pathlib import Path

import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "build/scratch/network-project/ns3.46.1-network-project-main-optimized"
CSV_NAME = "Bianchi-1AP-{sta_count}STA-500Q-Events.csv"


def run_fixed_distance(
    run_number: int,
    tx_power_dbm: float | None = None,
    nakagami: bool = False,
    sta_count: int = 1,
    ring_placement: bool = False,
    fixed_distance: float = 100.0,
) -> pd.DataFrame:
    with tempfile.TemporaryDirectory() as directory:
        command = [
            str(BINARY),
            "--nAps=1",
            f"--nStas={sta_count}",
            "--duration=10",
            "--roomSize=125",
            "--scenario=normal",
            "--fixedStaPlacement=1",
            f"--fixedStaDistance={fixed_distance}",
            f"--RngRun={run_number}",
        ]
        if tx_power_dbm is not None:
            command.append(f"--fixedTxPowerDbm={tx_power_dbm}")
        if nakagami:
            command.append("--enableNakagamiFading=1")
        if ring_placement:
            command.append("--fixedStaRingPlacement=1")
        completed = subprocess.run(
            command,
            cwd=directory,
            capture_output=True,
            text=True,
            check=True,
        )
        csv_path = Path(directory) / CSV_NAME.format(sta_count=sta_count)
        if not csv_path.exists():
            raise AssertionError(completed.stdout + completed.stderr)
        return pd.read_csv(csv_path)


class FixedDistanceGeometryTest(unittest.TestCase):
    def test_fixed_geometry_logs_the_requested_ap_sta_distance(self):
        events = run_fixed_distance(1)
        fixed_distance = events.loc[events.Action == "FIXED_STA_DISTANCE", "Value"]
        self.assertEqual(fixed_distance.tolist(), [100.0])

    def test_calibrated_radio_logs_power_and_fading(self):
        events = run_fixed_distance(1, tx_power_dbm=24.0, nakagami=True)
        self.assertEqual(
            events.loc[events.Action == "FIXED_TX_POWER_DBM", "Value"].tolist(), [24.0]
        )
        self.assertEqual(
            events.loc[events.Action == "NAKAGAMI_FADING", "Value"].tolist(), [1.0]
        )

    def test_ring_placement_logs_the_same_requested_distance_for_all_30_stas(self):
        events = run_fixed_distance(1, sta_count=30, ring_placement=True)
        distances = events.loc[events.Action == "FIXED_STA_DISTANCE", "Value"]

        self.assertEqual(len(distances), 30)
        self.assertEqual(distances.tolist(), [100.0] * 30)

    def test_two_sta_aggregate_goodput_is_not_counted_once_per_server(self):
        events = run_fixed_distance(
            1, sta_count=2, ring_placement=True, fixed_distance=30.0
        )
        aggregate = events.loc[
            events.Action == "AGGREGATE_THROUGHPUT_MBPS", "Value"
        ]

        self.assertGreater(aggregate.max(), 15.0)
        self.assertLess(aggregate.max(), 25.0)


if __name__ == "__main__":
    unittest.main()
