#!/usr/bin/env bash
set -euo pipefail

binary="build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/breakpoint_refined_100_125_3valid"
areas=(100 105 110 115 120 125)
runs=(1 4 7)

mkdir -p "$output_dir"

for area in "${areas[@]}"; do
  for run in "${runs[@]}"; do
    "$binary" \
      --nAps=1 \
      --nStas=1 \
      --duration=120 \
      --roomSize="$area" \
      --scenario=normal \
      --RngRun="$run"
    mv Bianchi-1AP-1STA-500Q-Events.csv "$output_dir/normal_area${area}m_run${run}.csv"
  done
done

echo "completed: ${#areas[@]} areas × ${#runs[@]} valid-seed repetitions"
