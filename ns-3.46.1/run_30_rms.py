import os
import subprocess
import pandas as pd
import numpy as np

out_dir = "results/rms_30_runs"
os.makedirs(out_dir, exist_ok=True)

runs = 30
n_aps = 1
n_stas = 30
duration = 10
room_size = 25.0

all_throughputs = []
all_latencies = []

print("Starting 30 simulation runs...")

for i in range(1, runs + 1):
    print(f"Run {i}/{runs}...")
    cmd = f'./ns3 run network-project-main --command-template="%s --nAps={n_aps} --nStas={n_stas} --duration={duration} --roomSize={room_size} --scenario=normal --RngRun={i}"'
    subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    csv_file = f"Bianchi-{n_aps}AP-{n_stas}STA-500Q-Events.csv"
    if os.path.exists(csv_file):
        df = pd.read_csv(csv_file)
        
        # Collect all individual throughput values and latency values
        df_tp = df[df['Action'] == 'THROUGHPUT_MBPS']
        if not df_tp.empty:
            all_throughputs.extend(df_tp['Value'].astype(float).values)
            
        df_lat = df[df['Action'] == 'LATENCY_MS']
        if not df_lat.empty:
            all_latencies.extend(df_lat['Value'].astype(float).values)
        
        # move csv to out_dir
        os.rename(csv_file, os.path.join(out_dir, f"run_{i}.csv"))

# Calculate RMS
def calculate_rms(values):
    if not values:
        return 0.0
    return np.sqrt(np.mean(np.square(values)))

rms_tp = calculate_rms(all_throughputs)
rms_lat = calculate_rms(all_latencies)

print(f"RMS Throughput (Mbps): {rms_tp:.4f}")
print(f"RMS Latency (ms): {rms_lat:.4f}")

# Write to README
readme_path = os.path.join(out_dir, "README.md")
with open(readme_path, "w") as f:
    f.write("# 30 Runs - RMS Calculation\n\n")
    f.write(f"**Scenario:** Normal Behavior\n")
    f.write(f"**Stations (STA):** {n_stas}\n")
    f.write(f"**Runs:** {runs}\n")
    f.write(f"**Duration:** {duration}s\n")
    f.write(f"**Room Size:** {room_size}m\n\n")
    f.write("## Root Mean Square (RMS) Results\n\n")
    f.write("The Root Mean Square (RMS) was calculated across all 30 independent runs (different random seeds).\n\n")
    f.write(f"- **RMS Throughput:** {rms_tp:.4f} Mbps\n")
    f.write(f"- **RMS Latency:** {rms_lat:.4f} ms\n")

print(f"Done. Saved results to {readme_path}")
