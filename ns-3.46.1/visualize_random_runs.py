import os
import glob
import random
from visual_data import visualize_large_ns3_trace

def visualize_random_from_dir(target_dir, output_dir, count=5):
    csv_files = glob.glob(os.path.join(target_dir, "*.csv"))
    if not csv_files:
        print(f"No CSV files found in {target_dir}")
        return
    
    selected_files = random.sample(csv_files, min(len(csv_files), count))
    print(f"Visualizing {len(selected_files)} files from {target_dir} to {output_dir}...")
    
    for f in selected_files:
        print(f"  Processing: {f}")
        visualize_large_ns3_trace(f, save_path=output_dir)

if __name__ == "__main__":
    # 1. RMS Runs
    visualize_random_from_dir(
        "results/rms_30_runs", 
        "results/visuals/rms_random", 
        count=5
    )
    
    # 2. Targeted Loss Runs
    visualize_random_from_dir(
        "results/targeted_loss_30_runs", 
        "results/visuals/targeted_loss_random", 
        count=5
    )
    
    print("\nVisualization complete. Check results/visuals/ for the generated dashboards.")
