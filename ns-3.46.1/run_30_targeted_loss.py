import subprocess
import random
import os
import time

# Create folder
os.makedirs("results/targeted_loss_30_runs", exist_ok=True)

# Helper to run simulation
def run_sim(run_id, s1, s2):
    print(f"Starting Run {run_id}: STAs {s1}, {s2} failing at T=100s")
    
    # Use different seed for each run
    os.environ["NS_GLOBAL_VALUE"] = f"RngRun={run_id+1}"
    
    cmd = [
        "./ns3", "run", "network-project-main", "--",
        "--duration=200",
        "--nStas=30",
        "--scenario=targeted_loss",
        "--targetSta1=" + str(s1),
        "--targetSta2=" + str(s2),
        "--triggerTime=100",
        "--roomSize=100"
    ]
    
    start_time = time.time()
    subprocess.run(cmd, check=True)
    end_time = time.time()
    
    print(f"Run {run_id} finished in {end_time - start_time:.2f}s")
    
    # Move CSV to results folder
    csv_name = "Bianchi-1AP-30STA-500Q-Events.csv"
    if os.path.exists(csv_name):
        dest = f"results/targeted_loss_30_runs/run_{run_id}_s1_{s1}_s2_{s2}.csv"
        os.rename(csv_name, dest)
        return dest
    return None

# Run 30 experiments
all_results = []
for i in range(30):
    s1 = random.randint(0, 29)
    s2 = random.randint(0, 29)
    while s2 == s1:
        s2 = random.randint(0, 29)
    
    res = run_sim(i, s1, s2)
    if res:
        all_results.append((i, s1, s2, res))

print("All 30 runs completed.")
