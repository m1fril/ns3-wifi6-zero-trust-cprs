#!/usr/bin/env bash
set -euo pipefail

binary="build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/fixed_distance_100_125_5runs"
distances=(100 105 110 115 120 125)
runs=(1 2 3 4 5)

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
      --scenario=normal \
      --RngRun="$run"
    mv Bianchi-1AP-1STA-500Q-Events.csv "$target"
  done
done

echo "completed: ${#distances[@]} distances × ${#runs[@]} repetitions"
