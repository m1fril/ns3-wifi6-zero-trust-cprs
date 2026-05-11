import glob
import os
import csv

def analyze_file(filename):
    base = os.path.basename(filename)
    parts = base.replace(".csv", "").split("_")
    s1_idx = int(parts[3])
    s2_idx = int(parts[5])
    s1_node = s1_idx + 2
    s2_node = s2_idx + 2
    
    # Use sets to count unique UIDs
    stats = {
        's1': {'pre_send': set(), 'pre_recv': set(), 'post_send': set(), 'post_recv': set()},
        's2': {'pre_send': set(), 'pre_recv': set(), 'post_send': set(), 'post_recv': set()},
        'others': {'pre_send': set(), 'pre_recv': set(), 'post_send': set(), 'post_recv': set()}
    }
    
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                t = float(row['Time_s'])
                node = int(row['NodeID'])
                action = row['Action']
                uid = row['Value']
                
                if t < 1.0: continue
                
                phase = 'pre' if t < 20.0 else 'post'
                key = ''
                if node == s1_node: key = 's1'
                elif node == s2_node: key = 's2'
                elif 2 <= node <= 31: key = 'others'
                else: continue
                
                if action == 'SEND':
                    stats[key][f'{phase}_send'].add(uid)
                elif action == 'RECV':
                    stats[key][f'{phase}_recv'].add(uid)
            except:
                continue
                
    def calc_loss(send_set, recv_set):
        n_send = len(send_set)
        # We only count RECVs for packets that were actually SENT (in our trace)
        # Actually, let's just count unique recvs
        n_recv = len(recv_set)
        return (1 - n_recv / n_send) * 100 if n_send > 0 else 0

    return {
        's1': (calc_loss(stats['s1']['pre_send'], stats['s1']['pre_recv']), 
               calc_loss(stats['s1']['post_send'], stats['s1']['post_recv'])),
        's2': (calc_loss(stats['s2']['pre_send'], stats['s2']['pre_recv']), 
               calc_loss(stats['s2']['post_send'], stats['s2']['post_recv'])),
        'others': (calc_loss(stats['others']['pre_send'], stats['others']['pre_recv']), 
                   calc_loss(stats['others']['post_send'], stats['others']['post_recv']))
    }

results_dir = "results/targeted_loss_30_runs"
files = glob.glob(os.path.join(results_dir, "*.csv"))

summary = []
for f in files:
    res = analyze_file(f)
    summary.append(res)

if summary:
    def avg(idx, key):
        return sum(r[key][idx] for r in summary) / len(summary)

    avg_s1_pre = avg(0, 's1')
    avg_s1_post = avg(1, 's1')
    avg_s2_pre = avg(0, 's2')
    avg_s2_post = avg(1, 's2')
    avg_others_pre = avg(0, 'others')
    avg_others_post = avg(1, 'others')

    print(f"--- Summary of {len(summary)} runs (Unique UIDs) ---")
    print(f"Target STA 1 Loss: {avg_s1_pre:.2f}% -> {avg_s1_post:.2f}%")
    print(f"Target STA 2 Loss: {avg_s2_pre:.2f}% -> {avg_s2_post:.2f}%")
    print(f"Other STAs Avg Loss: {avg_others_pre:.2f}% -> {avg_others_post:.2f}%")
else:
    print("No results to analyze yet.")
