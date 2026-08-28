#!/usr/bin/env bash
set -euo pipefail

binary="build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/calibrated_fixed_distance_breakpoint_106_109_10runs"
distances=(106 107 108 109)
runs=(1 2 3 4 5 6 7 8 9 10)

mkdir -p "$output_dir"
for distance in "${distances[@]}"; do
  for run in "${runs[@]}"; do
    target="$output_dir/normal_distance${distance}m_run${run}.csv"
    if [[ -f "$target" ]]; then
      continue
    fi
    "$binary" \
      --nAps=1 \
      --nStas=1 \
      --duration=120 \
      --roomSize=125 \
      --fixedStaPlacement=1 \
      --fixedStaDistance="$distance" \
      --fixedTxPowerDbm=24 \
      --enableNakagamiFading=1 \
      --scenario=normal \
      --RngRun="$run"
    mv Bianchi-1AP-1STA-500Q-Events.csv "$target"
  done
done

echo "completed: ${#distances[@]} distances × ${#runs[@]} repetitions"
