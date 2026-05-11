# Targeted Connectivity Degradation Analysis: 30 Experiments

## 1. Abstract
This report details the results of 30 independent experiments aimed at studying network resilience during the targeted failure of two randomly selected stations. The goal was to capture the change in metrics (packet loss) for the target nodes and evaluate the impact of reduced competition on other network stations.

## 2. Experiment Parameters
*   **Network:** 30 STA, 1 AP (802.11n).
*   **Scenario:** `targeted_loss`.
*   **Event:** At 20.0s, for two random STAs, signal power is reduced to -80 dBm and Gain is set to -100 dB.
*   **Duration:** 40 seconds (20s baseline, 20s post-failure).
*   **Runs:** 30 iterations with target node randomization.

## 3. Analysis Results (Averages)

| Parameter | Pre-Failure (T < 20s) | Post-Failure (T > 20s) | Change |
| :--- | :--- | :--- | :--- |
| **Target STA 1 Loss** | 36.22% | **100.00%** | +63.78% |
| **Target STA 2 Loss** | 31.66% | **100.00%** | +68.34% |
| **Other STAs Loss (AVG)** | 40.39% | **39.09%** | -1.30% |

## 4. Key Findings
1.  **Total Isolation:** The degradation mechanism via PHY parameters (TxGain/Power) successfully resulted in 100% loss for selected nodes, simulating complete hardware failure or out-of-range movement.
2.  **Unloading Effect:** After "disconnecting" two stations, a slight improvement (by 1.30%) in loss rates is observed for the remaining 28 stations. This confirms the hypothesis that reduced contention for the radio medium positively affects overall network stability, as fewer stations competing for airtime leads to fewer CSMA/CA backoff collisions.
3.  **Baseline Loss:** The high baseline loss level (~40%) is due to high node density (30 STAs per AP) and the use of standard 802.11n parameters, which creates significant pressure on the backoff mechanism.

## 5. Visual Evidence
To empirically verify the statistical metrics, we generated high-resolution (0.05s bin size) vertical dashboards for a random subset of runs. These visualizations explicitly highlight the sudden drop in `SEND/RECV` metrics for the targeted stations precisely at $T=20s$, and the resulting shift in the overall network drop rate.

**Sample Dashboards (Click to view):**
*   [View Visual Analysis: Run 14 (Targets: STA 10 & STA 29)](../../visuals/targeted_loss_random/run_14_s1_10_s2_29.png)
*   [View Visual Analysis: Run 22 (Targets: STA 5 & STA 20)](../../visuals/targeted_loss_random/run_22_s1_5_s2_20.png)
*   [View Visual Analysis: Run 7 (Targets: STA 21 & STA 22)](../../visuals/targeted_loss_random/run_7_s1_21_s2_22.png)
*   [View Visual Analysis: Run 9 (Targets: STA 11 & STA 9)](../../visuals/targeted_loss_random/run_9_s1_11_s2_9.png)
*   [View Visual Analysis: Run 23 (Targets: STA 9 & STA 16)](../../visuals/targeted_loss_random/run_23_s1_9_s2_16.png)

---

*Report automatically generated based on the analysis of 30 iterations.*
