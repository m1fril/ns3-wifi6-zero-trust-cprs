#!/usr/bin/env bash
set -euo pipefail

binary="build/scratch/network-project/ns3.46.1-network-project-main-optimized"
output_dir="results/goodput_fixed_distance_30_125_10runs"
distances=(30 40 50 60 70 80 85 90 100 105 110 115 120 125)
runs=(1 2 3 4 5 6 7 8 9 10)

mkdir -p "$output_dir"

# This campaign deliberately does not reuse the legacy PHY-rate traces.  Every
# result is produced after goodput was moved to PacketSocketServer::Rx.
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
