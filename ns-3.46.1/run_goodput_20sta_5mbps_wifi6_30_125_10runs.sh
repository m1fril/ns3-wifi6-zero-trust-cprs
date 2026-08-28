#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd "$(dirname "$0")" && pwd)
binary="$script_dir/build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/goodput_20sta_5mbps_wifi6_ap4x4_30_125_10runs"
duration="${DURATION:-20}"
distances=(30 40 50 60 70 80 85 90 100 105 110 115 120 125)
runs=(1 2 3 4 5 6 7 8 9 10)
distance_filter="${DISTANCE_FILTER:-}"
run_filter="${RUN_FILTER:-}"

mkdir -p "$output_dir"

for distance in "${distances[@]}"; do
  if [[ -n "$distance_filter" && " $distance_filter " != *" $distance "* ]]; then
    continue
  fi
  for run in "${runs[@]}"; do
    if [[ -n "$run_filter" && " $run_filter " != *" $run "* ]]; then
      continue
    fi
    target="$output_dir/normal_distance${distance}m_run${run}.csv"
    if [[ -f "$target" ]]; then
      continue
    fi
    run_dir=$(mktemp -d)
    (
      cd "$run_dir"
      "$binary" \
        --nAps=1 \
        --nStas=20 \
        --duration="$duration" \
        --pktInterval=2400 \
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
    mv "$run_dir/Bianchi-1AP-20STA-500Q-Events.csv" "$target"
    rmdir "$run_dir"
  done
done

echo "completed: ${#distances[@]} distances × ${#runs[@]} repetitions"
