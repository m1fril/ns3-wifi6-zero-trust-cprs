#!/bin/bash

# Configuration
BINARY="scratch/network-project/network-project-main"
OUT_DIR="results/diploma_matrix_runs"
N_APS=1
N_STAS=1
DURATION=200 # Adjust based on your DirectorAgent timeline

# Arrays for our 3x3 matrix
SCENARIOS=("normal" "controlled" "failure")
DISTANCES=(25.0 75.0 150.0)

# Create output directory
mkdir -p "$OUT_DIR"

echo "Starting 3x3 Execution Matrix..."
echo "======================================"

for mode in "${SCENARIOS[@]}"; do
    for dist in "${DISTANCES[@]}"; do
        
        echo "[RUNNING] Scenario: $mode | Area: ${dist}m"
        
        # Execute ns-3
        # Note: Added export PATH and relative ./ns3 path for clarity
        ./ns3 run $BINARY -- --nAps=$N_APS --nStas=$N_STAS --duration=$DURATION --roomSize=$dist --scenario=$mode > /dev/null 2>&1
        
        # The default hardcoded CSV name from your C++ code
        DEFAULT_CSV="Bianchi-${N_APS}AP-${N_STAS}STA-500Q-Events.csv"
        
        # The new specific filename for this matrix point
        NEW_CSV="${OUT_DIR}/run_${mode}_area${dist}m.csv"
        
        # Move and rename the artifact so it isn't overwritten
        if [ -f "$DEFAULT_CSV" ]; then
            mv "$DEFAULT_CSV" "$NEW_CSV"
            echo "[SAVED] -> $NEW_CSV"
        else
            echo "[ERROR] CSV not found for Scenario: $mode, Area: ${dist}m"
        fi
        
    done
done

echo "======================================"
echo "Matrix execution complete. Data saved to $OUT_DIR/"
