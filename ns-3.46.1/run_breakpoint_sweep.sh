#!/usr/bin/env bash
set -euo pipefail

binary="build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/breakpoint_sweep"
areas=(100 125 150 175 200 225 250 275 300)
runs=(1 2 3 4 5)

mkdir -p "$output_dir"

for area in "${areas[@]}"; do
  for run in "${runs[@]}"; do
    "$binary" --nAps=1 --nStas=1 --duration=120 --roomSize="$area" --scenario=normal --RngRun="$run"
    mv Bianchi-1AP-1STA-500Q-Events.csv "$output_dir/normal_area${area}m_run${run}.csv"
  done
done
