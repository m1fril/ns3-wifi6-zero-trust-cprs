#!/bin/bash
export PATH="/opt/homebrew/bin:$PATH"


# Configuration
BINARY="scratch/network-project/network-project-main"
OUT_DIR="results/diploma_matrix_runs"
N_APS=1
N_STAS=1
DURATION=200

# Extended Distances
SCENARIOS=("normal" "controlled" "failure")
DISTANCES=(200.0 300.0 400.0)

# Create output directory
mkdir -p "$OUT_DIR"

echo "Starting Extended Execution Matrix (200m, 300m, 400m)..."
echo "=========================================================="

for mode in "${SCENARIOS[@]}"; do
    for dist in "${DISTANCES[@]}"; do
        
        echo "[RUNNING] Scenario: $mode | Area: ${dist}m"
        
        # Execute ns-3
        ./ns3 run $BINARY -- --nAps=$N_APS --nStas=$N_STAS --duration=$DURATION --roomSize=$dist --scenario=$mode > /dev/null 2>&1
        
        # Default CSV name
        DEFAULT_CSV="Bianchi-${N_APS}AP-${N_STAS}STA-500Q-Events.csv"
        
        # Specific filename
        NEW_CSV="${OUT_DIR}/run_${mode}_area${dist}m.csv"
        
        if [ -f "$DEFAULT_CSV" ]; then
            mv "$DEFAULT_CSV" "$NEW_CSV"
            echo "[SAVED] -> $NEW_CSV"
        else
            echo "[ERROR] CSV not found for Scenario: $mode, Area: ${dist}m"
        fi
        
    done
done

echo "=========================================================="
echo "Extended matrix execution complete. Data saved to $OUT_DIR/"
