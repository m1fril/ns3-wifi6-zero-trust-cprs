import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def visualize_node_errors(csv_source):
    # 1. Load Data
    try:
        df = pd.read_csv(csv_source)
    except Exception as e:
        print(f"Error loading CSV: {e}")
        return

    # Filter for Stations (STAs) only, as APs primarily RECV in this upload scenario
    df_stas = df[df['NodeType'] == 'STA']

    # 2. Group data by NodeID and count each Action
    node_stats = df_stas.groupby(['NodeID', 'Action']).size().unstack(fill_value=0)

    # Ensure required columns exist (prevents KeyError if a run has zero drops or sends)
    for action in ['SEND', 'RECV', 'WIFI_DROP']:
        if action not in node_stats.columns:
            node_stats[action] = 0

    # 3. Calculate Error Rate (Drop Percentage)
    # Total attempts = Packets Sent by app + Packets Dropped by MAC before sending
    node_stats['Total_Attempts'] = node_stats['SEND'] + node_stats['WIFI_DROP']
    
    # Calculate % dropped. If a node did absolutely nothing, fill NaN with 0.
    node_stats['Drop_Rate_Pct'] = (node_stats['WIFI_DROP'] / node_stats['Total_Attempts']) * 100
    node_stats['Drop_Rate_Pct'] = node_stats['Drop_Rate_Pct'].fillna(0)

    # Sort so the most problematic nodes are on the left
    node_stats = node_stats.sort_values(by='Drop_Rate_Pct', ascending=False).reset_index()

    # 4. Plotting
    sns.set_theme(style="whitegrid", context="talk")
    plt.figure(figsize=(16, 6))

    # Create a bar plot of the drop rates
    ax = sns.barplot(
        data=node_stats, 
        x='NodeID', 
        y='Drop_Rate_Pct', 
        color='#e74c3c', # Red to indicate errors
        order=node_stats['NodeID']
    )
    
    plt.title("Packet Drop Rate per Station (Worst to Best)", fontsize=16, fontweight='bold')
    plt.xlabel("Node ID", fontsize=14)
    plt.ylabel("WIFI_DROP Rate (%)", fontsize=14)
    
    # Rotate Node ID labels so they don't overlap when you have 50-100 nodes
    plt.xticks(rotation=90, fontsize=9)
    
    # Add a horizontal line at 50% for quick visual reference
    plt.axhline(50, color='black', linestyle='--', alpha=0.5, label='50% Drop Threshold')
    plt.legend()

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    # Replace with your actual CSV filename
    visualize_node_errors("Bianchi-1AP-100STA-100Q-OfdmRate54Mbps-d1000-Events.csv")