import glob
import os
import csv
import numpy as np

def analyze_targeted_loss():
    results_dir = "results/targeted_loss_30_runs"
    files = glob.glob(os.path.join(results_dir, "*.csv"))
    
    # We will accumulate lists of values to calculate means and std devs across all runs
    summary = []
    
    for filename in files:
        base = os.path.basename(filename)
        parts = base.replace(".csv", "").split("_")
        s1_idx = int(parts[3])
        s2_idx = int(parts[5])
        s1_node = s1_idx + 2
        s2_node = s2_idx + 2
        
        # To calculate packet loss based on UIDs
        stats = {
            's1': {'pre_send': set(), 'pre_recv': set(), 'post_send': set(), 'post_recv': set()},
            's2': {'pre_send': set(), 'pre_recv': set(), 'post_send': set(), 'post_recv': set()},
            'others': {'pre_send': set(), 'pre_recv': set(), 'post_send': set(), 'post_recv': set()}
        }
        
        # To calculate throughput and latency means per run
        metrics = {
            's1': {'pre_tp': [], 'post_tp': [], 'pre_lat': [], 'post_lat': []},
            's2': {'pre_tp': [], 'post_tp': [], 'pre_lat': [], 'post_lat': []},
            'others': {'pre_tp': [], 'post_tp': [], 'pre_lat': [], 'post_lat': []}
        }
        
        with open(filename, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    t = float(row['Time_s'])
                    node = int(row['NodeID'])
                    action = row['Action']
                    val = row['Value']
                    
                    if t < 1.0: continue
                    
                    phase = 'pre' if t < 100.0 else 'post'
                    key = ''
                    if node == s1_node: key = 's1'
                    elif node == s2_node: key = 's2'
                    elif 2 <= node <= 31: key = 'others'
                    else: continue
                    
                    if action == 'SEND':
                        stats[key][f'{phase}_send'].add(val)
                    elif action == 'RECV':
                        stats[key][f'{phase}_recv'].add(val)
                    elif action == 'THROUGHPUT_MBPS':
                        metrics[key][f'{phase}_tp'].append(float(val))
                    elif action == 'LATENCY_MS':
                        metrics[key][f'{phase}_lat'].append(float(val))
                except:
                    continue
                    
        def calc_loss(send_set, recv_set):
            n_send = len(send_set)
            n_recv = len(recv_set)
            return (1 - n_recv / n_send) * 100 if n_send > 0 else 0
            
        def safe_mean(l):
            return np.mean(l) if len(l) > 0 else 0.0

        res = {
            's1': {
                'loss_pre': calc_loss(stats['s1']['pre_send'], stats['s1']['pre_recv']),
                'loss_post': calc_loss(stats['s1']['post_send'], stats['s1']['post_recv']),
                'tp_pre': safe_mean(metrics['s1']['pre_tp']),
                'tp_post': safe_mean(metrics['s1']['post_tp']),
                'lat_pre': safe_mean(metrics['s1']['pre_lat']),
                'lat_post': safe_mean(metrics['s1']['post_lat'])
            },
            's2': {
                'loss_pre': calc_loss(stats['s2']['pre_send'], stats['s2']['pre_recv']),
                'loss_post': calc_loss(stats['s2']['post_send'], stats['s2']['post_recv']),
                'tp_pre': safe_mean(metrics['s2']['pre_tp']),
                'tp_post': safe_mean(metrics['s2']['post_tp']),
                'lat_pre': safe_mean(metrics['s2']['pre_lat']),
                'lat_post': safe_mean(metrics['s2']['post_lat'])
            },
            'others': {
                'loss_pre': calc_loss(stats['others']['pre_send'], stats['others']['pre_recv']),
                'loss_post': calc_loss(stats['others']['post_send'], stats['others']['post_recv']),
                'tp_pre': safe_mean(metrics['others']['pre_tp']),
                'tp_post': safe_mean(metrics['others']['post_tp']),
                'lat_pre': safe_mean(metrics['others']['pre_lat']),
                'lat_post': safe_mean(metrics['others']['post_lat'])
            }
        }
        summary.append(res)
        
    print(f"\n=== TARGETED LOSS RESULTS ({len(summary)} RUNS) ===")
    
    # Calculate averages and std devs across all runs
    for target in ['s1', 's2', 'others']:
        print(f"\n--- {target.upper()} ---")
        for metric in ['loss', 'tp', 'lat']:
            pre_vals = [r[target][f'{metric}_pre'] for r in summary]
            post_vals = [r[target][f'{metric}_post'] for r in summary]
            
            pre_mean = np.mean(pre_vals)
            pre_std = np.std(pre_vals)
            post_mean = np.mean(post_vals)
            post_std = np.std(post_vals)
            
            diffs = np.array(post_vals) - np.array(pre_vals)
            diff_mean = np.mean(diffs)
            diff_std = np.std(diffs)
            diff_min = np.min(diffs)
            diff_max = np.max(diffs)
            
            unit = "%" if metric == 'loss' else ("Mbps" if metric == 'tp' else "ms")
            print(f"  {metric.upper()}:")
            print(f"    Pre : {pre_mean:7.4f} {unit} (± {pre_std:7.4f})")
            print(f"    Post: {post_mean:7.4f} {unit} (± {post_std:7.4f})")
            print(f"    Diff: {diff_mean:7.4f} {unit} (± {diff_std:7.4f}) [Min: {diff_min:7.4f}, Max: {diff_max:7.4f}]")

def analyze_rms():
    results_dir = "results/rms_30_runs"
    files = glob.glob(os.path.join(results_dir, "*.csv"))
    
    all_tp = []
    all_lat = []
    all_loss = []
    
    for filename in files:
        stats = {'send': set(), 'recv': set()}
        with open(filename, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    t = float(row['Time_s'])
                    if t < 1.0: continue
                    
                    action = row['Action']
                    val = row['Value']
                    
                    if action == 'SEND':
                        stats['send'].add(val)
                    elif action == 'RECV':
                        stats['recv'].add(val)
                    elif action == 'THROUGHPUT_MBPS':
                        all_tp.append(float(val))
                    elif action == 'LATENCY_MS':
                        all_lat.append(float(val))
                except:
                    continue
        n_send = len(stats['send'])
        n_recv = len(stats['recv'])
        loss = (1 - n_recv / n_send) * 100 if n_send > 0 else 0
        all_loss.append(loss)
        
    print(f"\n=== RMS RESULTS ({len(files)} RUNS) ===")
    
    tp_mean = np.mean(all_tp)
    tp_std = np.std(all_tp)
    tp_rms = np.sqrt(np.mean(np.square(all_tp)))
    
    lat_mean = np.mean(all_lat)
    lat_std = np.std(all_lat)
    lat_rms = np.sqrt(np.mean(np.square(all_lat)))
    
    loss_mean = np.mean(all_loss)
    loss_std = np.std(all_loss)
    
    print(f"Throughput : Mean {tp_mean:.4f} Mbps (± {tp_std:.4f}), RMS {tp_rms:.4f} Mbps")
    print(f"Latency    : Mean {lat_mean:.4f} ms (± {lat_std:.4f}), RMS {lat_rms:.4f} ms")
    print(f"Loss       : Mean {loss_mean:.4f} % (± {loss_std:.4f})")

if __name__ == "__main__":
    analyze_rms()
    analyze_targeted_loss()
