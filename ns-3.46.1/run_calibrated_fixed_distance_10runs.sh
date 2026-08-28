#!/usr/bin/env bash
set -euo pipefail

binary="build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/calibrated_fixed_distance_100_125_10runs"
distances=(100 105 110 115 120 125)
runs=(1 2 3 4 5 6 7 8 9 10)

mkdir -p "$output_dir"

# Preserve the already completed first five repeats as part of the expanded campaign.
for distance in "${distances[@]}"; do
  for run in 1 2 3 4 5; do
    source="results/calibrated_fixed_distance_100_125_5runs/normal_distance${distance}m_run${run}.csv"
    target="$output_dir/normal_distance${distance}m_run${run}.csv"
    if [[ ! -f "$target" ]]; then
      cp "$source" "$target"
    fi
  done
done

# Execute five additional independent ns-3 repetitions at every fixed distance.
for distance in "${distances[@]}"; do
  for run in 6 7 8 9 10; do
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
