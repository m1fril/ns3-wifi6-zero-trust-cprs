#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
binary="$script_dir/build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/goodput_30sta_ring_30_125_10runs"
duration="${DURATION:-20}"
distances=(30 40 50 60 70 80 85 90 100 105 110 115 120 125)
runs=(1 2 3 4 5 6 7 8 9 10)
distance_filter="${DISTANCE_FILTER:-}"

mkdir -p "$output_dir"

for distance in "${distances[@]}"; do
  if [[ -n "$distance_filter" && " $distance_filter " != *" $distance "* ]]; then
    continue
  fi
  for run in "${runs[@]}"; do
    target="$output_dir/normal_distance${distance}m_run${run}.csv"
    if [[ -f "$target" ]]; then
      continue
    fi
    run_dir=$(mktemp -d)
    (
      cd "$run_dir"
      "$binary" \
      --nAps=1 \
      --nStas=30 \
      --duration="$duration" \
      --roomSize=125 \
      --fixedStaPlacement=1 \
      --fixedStaRingPlacement=1 \
      --fixedStaDistance="$distance" \
      --fixedTxPowerDbm=24 \
      --enableNakagamiFading=1 \
      --compactMetrics=1 \
      --scenario=normal \
      --RngRun="$run"
    )
    mv "$run_dir/Bianchi-1AP-30STA-500Q-Events.csv" "$target"
    rmdir "$run_dir"
  done
done

echo "completed: ${#distances[@]} distances × ${#runs[@]} repetitions"
