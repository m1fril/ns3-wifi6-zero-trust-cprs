#!/usr/bin/env bash
set -euo pipefail

binary="build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/control_90m_no_fading_10runs"
mkdir -p "$output_dir"

for run in 1 2 3 4 5 6 7 8 9 10; do
  target="$output_dir/normal_distance90m_run${run}.csv"
  if [[ -f "$target" ]]; then
    continue
  fi
  "$binary" \
    --nAps=1 \
    --nStas=1 \
    --duration=120 \
    --roomSize=125 \
    --fixedStaPlacement=1 \
    --fixedStaDistance=90 \
    --fixedTxPowerDbm=24 \
    --enableNakagamiFading=0 \
    --scenario=normal \
    --RngRun="$run"
  mv Bianchi-1AP-1STA-500Q-Events.csv "$target"
done

echo "completed: 90 m no-fading control × 10 repetitions"
