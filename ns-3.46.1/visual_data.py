import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os
import glob
import warnings

# Suppress minor future warnings from seaborn for cleaner terminal output
warnings.simplefilter(action='ignore', category=FutureWarning)

def visualize_large_ns3_trace(csv_source, num_time_bins=200):
    try:
        df = pd.read_csv(csv_source)
    except Exception as e:
        print(f"Error loading CSV: {e}")
        return

    if df.empty:
        print(f"Warning: {csv_source} is empty. Skipping.")
        return

    file_name = os.path.basename(csv_source)

    # 1. Create Time Bins
    max_time = df['Time_s'].max()
    bin_size = max(max_time / num_time_bins, 0.5) 
    df['TimeBin'] = (df['Time_s'] // bin_size) * bin_size

    sns.set_theme(style="whitegrid", context="paper")
    
    fig = plt.figure(figsize=(18, 14))
    fig.suptitle(f"Network Bottleneck Analysis: {file_name}", fontsize=18, fontweight='bold')
    
    gs = fig.add_gridspec(3, 2, height_ratios=[1.5, 1.5, 1.2])
    ax_timeline = fig.add_subplot(gs[0, :])
    ax_heatmap  = fig.add_subplot(gs[1, :])
    ax_drop_time = fig.add_subplot(gs[2, 0]) 
    ax_drop_dist = fig.add_subplot(gs[2, 1])

    action_palette = {
        "RECV": "#2ecc71", "SEND": "#3498db", "WIFI_DROP": "#c0392b", "JOINED": "#9b59b6"
    }

    # --- 1. Smoothed Timeline ---
    time_agg = df.groupby(['TimeBin', 'Action']).size().unstack(fill_value=0)
    smoothing_window = max(3, int(num_time_bins * 0.02)) 
    time_agg_smooth = time_agg.rolling(window=smoothing_window, min_periods=1).mean()

    if 'RECV' in time_agg_smooth.columns:
        ax_timeline_recv = ax_timeline.twinx()
        ax_timeline_recv.plot(time_agg_smooth.index, time_agg_smooth['RECV'], 
                              color=action_palette['RECV'], linewidth=1.5, alpha=0.3)
        ax_timeline_recv.set_ylabel("RECV Events (Sniffer Traffic)", color=action_palette['RECV'], fontsize=10)
        time_agg_smooth_main = time_agg_smooth.drop(columns=['RECV'])
    else:
        time_agg_smooth_main = time_agg_smooth

    # Draw specific lines differently so they don't hide each other!
    for col in time_agg_smooth_main.columns:
        if col == 'SEND':
            ax_timeline.plot(time_agg_smooth_main.index, time_agg_smooth_main[col],
                             color=action_palette.get(col, "#333"), linewidth=5.0, label=col)
        elif col == 'WIFI_DROP':
            ax_timeline.plot(time_agg_smooth_main.index, time_agg_smooth_main[col],
                             color=action_palette.get(col, "#333"), linewidth=2.5, linestyle='--', label=col)
        else:
            ax_timeline.plot(time_agg_smooth_main.index, time_agg_smooth_main[col],
                             color=action_palette.get(col, "#333"), linewidth=3.0, label=col)
    
    ax_timeline.set_title(f"Smoothed Application Traffic vs Drops (Rolling Avg Window: {smoothing_window} bins)", fontsize=14, fontweight='bold')
    ax_timeline.set_xlabel("Time (seconds)")
    ax_timeline.set_ylabel("Application/MAC Events per Second", fontweight='bold')
    ax_timeline.legend(title="Action", bbox_to_anchor=(1.01, 1), loc='upper left')

    for t in range(100, int(max_time) + 1, 100):
        ax_timeline.axvline(x=t, color='gray', linestyle='--', alpha=0.6)

    # --- 2. Heatmap (WIFI_DROP Intensity) ---
    df_drops = df[df['Action'] == 'WIFI_DROP']
    all_stas = sorted(df[df['NodeType'] == 'STA']['NodeID'].unique())
    
    if not df_drops.empty:
        heatmap_data = df_drops.groupby(['NodeID', 'TimeBin']).size().unstack(fill_value=0)
        heatmap_data = heatmap_data.reindex(index=all_stas).fillna(0)
        
        sns.heatmap(heatmap_data, ax=ax_heatmap, cmap="Reds", robust=True, 
                    cbar_kws={'label': 'Drops per Bin'}, linewidths=0, rasterized=True)
        
        ax_heatmap.set_title("WIFI_DROP Heatmap: Watch STAs Fail as Load Increases", fontsize=14, fontweight='bold')
        ax_heatmap.set_ylabel("STA Node ID")
        ax_heatmap.set_xlabel("Time Bins (seconds)")
        
        xticks = ax_heatmap.get_xticks()
        tick_step = max(1, len(xticks) // 15)
        ax_heatmap.set_xticks(xticks[::tick_step])
        ax_heatmap.set_xticklabels([f"{x * bin_size:.0f}" for x in xticks[::tick_step]], rotation=0)
        
        for t in range(100, int(max_time) + 1, 100):
            idx = t / bin_size
            ax_heatmap.axvline(x=idx, color='black', linestyle='--', alpha=0.5)

    # --- 3. Bottleneck Identification: Overall Drop Rate % vs Time ---
    sends_time = time_agg.get('SEND', pd.Series(0, index=time_agg.index))
    drops_time = time_agg.get('WIFI_DROP', pd.Series(0, index=time_agg.index))
    
    drop_rate_time = (drops_time / sends_time.replace(0, np.nan)) * 100
    drop_rate_time = drop_rate_time.fillna(0).clip(upper=100) 
    
    drop_rate_time_smooth = drop_rate_time.rolling(window=smoothing_window, min_periods=1).mean()
    
    ax_drop_time.plot(time_agg.index, drop_rate_time_smooth, color='#c0392b', linewidth=2.5)
    ax_drop_time.fill_between(time_agg.index, drop_rate_time_smooth, color='#c0392b', alpha=0.2)
    
    ax_drop_time.set_title("Network Bottleneck: Overall Drop Rate % over Time", fontsize=12, fontweight='bold')
    ax_drop_time.set_xlabel("Time (seconds)")
    ax_drop_time.set_ylabel("Packet Drop Rate (%)")
    ax_drop_time.set_ylim(-5, 105)
    
    for t in range(100, int(max_time) + 1, 100):
        ax_drop_time.axvline(x=t, color='gray', linestyle='--', alpha=0.6)

    # --- 4. STA Drop Rate Distribution per Load Stage ---
    df_stas = df[df['NodeType'] == 'STA'].copy()
    
    df_stas['LoadStage'] = (df_stas['Time_s'] // 100) * 100
    
    stage_stats = df_stas.groupby(['LoadStage', 'NodeID', 'Action']).size().unstack(fill_value=0)
    for action in ['SEND', 'WIFI_DROP']:
        if action not in stage_stats.columns:
            stage_stats[action] = 0
            
    stage_stats['Drop_Rate_Pct'] = (stage_stats['WIFI_DROP'] / stage_stats['SEND'].replace(0, np.nan)) * 100
    stage_stats['Drop_Rate_Pct'] = stage_stats['Drop_Rate_Pct'].fillna(0).clip(upper=100)
    stage_stats = stage_stats.reset_index()

    sns.boxplot(data=stage_stats, x='LoadStage', y='Drop_Rate_Pct', ax=ax_drop_dist, 
                palette="Reds", showfliers=False, width=0.6)
    
    sns.stripplot(data=stage_stats, x='LoadStage', y='Drop_Rate_Pct', ax=ax_drop_dist, 
                  color=".25", size=4, alpha=0.6, jitter=True)
    
    ax_drop_dist.set_title("STA Drop Rate Distribution per Load Stage (100s chunks)", fontsize=12, fontweight='bold')
    ax_drop_dist.set_xlabel("Load Stage Start Time (seconds)")
    ax_drop_dist.set_ylabel("STA Drop Rate (%)")
    ax_drop_dist.set_ylim(-5, 105)
    
    # Safe floating point to int conversion
    fig.canvas.draw()
    ax_drop_dist.set_xticklabels([f"{int(float(x.get_text()))}s" for x in ax_drop_dist.get_xticklabels()])

    plt.tight_layout(rect=[0, 0.03, 1, 0.96])

# --- Execution ---
if __name__ == "__main__":
    target_folder = "."
    csv_files = glob.glob(os.path.join(target_folder, "*Events*.csv"))
    
    if not csv_files:
        print(f"No matching CSV files found in '{target_folder}'")
    else:
        print(f"Found {len(csv_files)} simulation files. Generating updated dashboards...")
        csv_files.sort()
        
        for file_path in csv_files:
            print(f"Preparing window for: {file_path}")
            visualize_large_ns3_trace(file_path)
            
        print("Done generating. Opening all windows now!")
        plt.show()