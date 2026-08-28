# ns3-wifi6-zero-trust-cprs

Simulation environment, campaign runners and measured traces for IEEE 802.11ax
(Wi‑Fi 6) link-resilience thresholds in **Zero Trust cyber-physical robotic
systems (CPRS)**.

This repository is the reproducibility artifact for the paper:

> Пахомов М. В. **Архітектура нульової довіри для кіберфізичних робототехнічних
> систем: базові вимоги та межі стійкості безпроводового каналу.**
> Київський столичний університет імені Бориса Грінченка.
> ORCID [0009-0007-7343-6912](https://orcid.org/0009-0007-7343-6912)

The paper formulates five baseline Zero Trust requirements for CPRS and derives
two *separate* wireless resilience thresholds from the measurements below. The
network results characterise the conditions under which telemetry is delivered
and a response mode is chosen; they do **not** by themselves prove that
identity, authorisation or segmentation are correctly implemented.

---

## What is measured

One AP and 20 stations placed evenly on a ring around the AP, at a fixed
AP–STA distance. Each STA sends a 1500-byte packet every 2400 µs
(5 Mbit/s per STA, **100 Mbit/s offered load in total**) to a single
application server behind the AP.

The campaign sweeps **14 distances from 30 to 125 m** with **10 independent
repetitions each** (`RngRun=1..10`), i.e. **140 traces**.

### Radio configuration

| Parameter | Value |
|---|---|
| Standard | IEEE 802.11ax (`WIFI_STANDARD_80211ax`) |
| Channel | `{42, 80, BAND_5GHZ, 0}` — channel 42, 80 MHz, centre 5210 MHz |
| Rate control | `IdealWifiManager` |
| Tx power (AP and STA) | 24 dBm relative to 1 mW (≈ 251 mW) |
| AP antennas / streams | 4 antennas, 4 Tx and 4 Rx spatial streams |
| STA antennas / streams | 2 antennas, 2 Tx and 2 Rx spatial streams |
| Link-layer queue | 500 packets |
| Path loss | `LogDistancePropagationLossModel`, exponent n = 3, d₀ = 1 m, L₀ = 46.6777 dB (Friis at 1 m for 5.15 GHz — ns‑3 default) |
| Fading | `NakagamiPropagationLossModel`, m₀ = 1.5 (d < 80 m), m₁ = 0.75 (80 ≤ d < 200 m), m₂ = 0.75 (d ≥ 200 m) |
| Duration | 20 s per run |

All propagation values are ns‑3 defaults; the scenario does not override them.

---

## How the metrics are computed

`compactMetrics=1` writes one record per second at the AP:
`AGGREGATE_THROUGHPUT_MBPS`, `SENT_COUNT`, `RECV_COUNT`, `LATENCY_P95_MS`.
Aggregate goodput is measured **once at the AP**, over the useful bytes received
by the single server from all 20 STAs, so there is no double counting and no
control traffic is credited as payload.

Reducing a trace to one number per distance is a two-stage operation:

| Metric | Per run | Across the 10 runs |
|---|---|---|
| Aggregate goodput, Mbit/s | **median** of the per-second `AGGREGATE_THROUGHPUT_MBPS` series | **median** |
| p95 latency, ms | **95th percentile** of the per-second `LATENCY_P95_MS` series | **median** |
| Packet loss, % | `(Σ SENT_COUNT − Σ RECV_COUNT) / Σ SENT_COUNT · 100` | **median** |

Dispersion is reported as the quartile deviation `Q = (Q3 − Q1) / 2`, with
quartiles obtained by linear interpolation. `Q` is a non-parametric measure: it
is not a confidence interval and assumes no symmetry.

> **Survivorship bias.** `LATENCY_P95_MS` is computed over *delivered* packets
> only. Beyond ~90 % loss the delivered subset shrinks and becomes biased
> towards packets that caught a favourable fading window, so p95 stops being
> monotone (747.3 ms at 105 m vs 649.6 ms at 110 m). That non-monotonicity is a
> measurement artefact, not an improving channel, and it is not statistically
> significant (Mann–Whitney U over the two groups of 10 runs gives p ≈ 0.65).
> In that region loss, not latency, is the reliable indicator.

---

## Results

Median ± quartile deviation `Q` over 10 repetitions per distance:

| AP–STA, m | Aggregate goodput, Mbit/s | p95 latency, ms | Loss, % |
|---:|---:|---:|---:|
| 30 | 99.936 ± 0.121 | 189.4 ± 6.4 | 0.27 ± 0.03 |
| 40 | 99.210 ± 0.319 | 351.8 ± 10.8 | 0.86 ± 0.15 |
| 50 | 90.531 ± 0.428 | 525.9 ± 7.3 | 9.61 ± 0.40 |
| 60 | 77.541 ± 1.048 | 568.1 ± 3.0 | 22.74 ± 0.75 |
| 70 | 64.050 ± 1.528 | 581.8 ± 2.9 | 35.64 ± 1.18 |
| 80 | 43.122 ± 1.781 | 555.2 ± 1.2 | 56.68 ± 2.06 |
| 85 | 30.246 ± 1.951 | 549.9 ± 2.1 | 70.02 ± 1.67 |
| 90 | 19.296 ± 2.315 | 549.0 ± 3.4 | 80.19 ± 2.52 |
| 100 | 7.785 ± 0.517 | 584.5 ± 34.1 | 91.50 ± 0.41 |
| 105 | 5.121 ± 0.372 | 747.3 ± 210.6 | 94.66 ± 0.17 |
| 110 | 2.289 ± 0.345 | 649.6 ± 55.1 | 97.59 ± 0.42 |
| 115 | 1.140 ± 0.203 | 577.9 ± 276.8 | 98.68 ± 0.21 |
| 120 | 0.678 ± 0.086 | 569.0 ± 16.7 | 99.45 ± 0.09 |
| 125 | 0.387 ± 0.074 | 552.8 ± 14.6 | 99.81 ± 0.05 |

### Two separate thresholds

Degradation is **two-stage**, and the two thresholds are kept apart on purpose:

* **Early timing threshold `d_T* = 40 m`.** With the 30 m baseline,
  `R_T(40) = 351.8 / 189.4 = 1.86 ≥ 1.5`, the condition also holds at the next
  distance, and goodput is still 99.210 Mbit/s at 0.86 % loss. Latency degrades
  well before application data is lost.
* **Sustained loss threshold `d_L* = 50 m`.** Loss rises by
  `ΔL = 9.34` percentage points over the 0.27 % baseline and keeps rising at the
  next distance (22.74 % at 60 m).

The 100 ms level is an auxiliary materiality condition, not a threshold on its
own: the 30 m baseline already exceeds it, and the baseline is never a
candidate.

For a Zero Trust policy this means the operating mode must not be driven by a
single network metric: 40 m is where telemetry-freshness checks and a local
fallback mode should be prepared, and 50 m is where limited operation, caching
of the last valid decision or safe handover become justified.

---

## Reproducing the campaign

Build ns‑3 (this tree is the `ns-allinone-3.46.1` distribution) and then the
scenario:

```bash
cd ns-3.46.1
./ns3 configure --build-profile=optimized
./ns3 build
```

This produces `build/scratch/network-project/ns3.46.1-network-project-main-optimized`,
which the campaign runner expects.

Run the full campaign (skips any trace that already exists):

```bash
cd ns-3.46.1
./run_goodput_20sta_5mbps_wifi6_30_125_10runs.sh
```

A subset can be selected with environment variables:

```bash
DISTANCE_FILTER="30 40 50" RUN_FILTER="1 2" ./run_goodput_20sta_5mbps_wifi6_30_125_10runs.sh
```

A single run directly:

```bash
./build/scratch/network-project/ns3.46.1-network-project-main-optimized \
  --nAps=1 --nStas=20 --duration=20 --pktInterval=2400 --roomSize=125 \
  --fixedStaPlacement=1 --fixedStaRingPlacement=1 --fixedStaDistance=50 \
  --fixedTxPowerDbm=24 --enableNakagamiFading=1 --compactMetrics=1 \
  --scenario=normal --RngRun=1
```

Tests:

```bash
cd ns-3.46.1 && python3 -m pytest tests/
```

---

## Layout

```
ns-3.46.1/
├── scratch/network-project/            # scenario sources
│   ├── simulation-environment.cc/.h    # topology, PHY/MAC setup, metric tracing
│   └── ...                             # scenario agents and orchestration
├── run_goodput_20sta_5mbps_wifi6_30_125_10runs.sh   # 20 STA campaign runner
├── run_goodput_30sta_ring_30_125_10runs.sh          # 30 STA variant
├── tests/                              # runner and analysis tests
└── results/
    └── goodput_20sta_5mbps_wifi6_ap4x4_30_125_10runs/
        ├── README_20sta_5mbps_results.md   # campaign notes (Ukrainian)
        ├── breakpoint_summary.csv           # one row per distance
        └── normal_distance{d}m_run{r}.csv   # 140 traces
```

Trace provenance is encoded in the file name: `normal_distance{d}m_run{r}.csv`
carries the distance and the repetition number. Each trace opens with
`FIXED_STA_DISTANCE`, `FIXED_TX_POWER_DBM` and `NAKAGAMI_FADING` events that
confirm the key scenario parameters, so a file can be validated on its own.

---

## Scope and limitations

* One configuration only: 20 homogeneous **stationary** STAs on a ring, one AP,
  identical load, no interference from other networks, no mobility.
* Application-level delivery, latency and loss only. The traces contain no
  full packet sequences, no SNR, no retransmission or MCS history.
* The traces do **not** exercise identity, authorisation or segmentation.
  Validating those requires synchronised policy logs, security events and robot
  physical state — the data pipeline proposed in the paper.

---

## Citing

```bibtex
@software{pakhomov_ns3_wifi6_zero_trust_cprs,
  author  = {Pakhomov, Mykhailo V.},
  title   = {ns3-wifi6-zero-trust-cprs: ns-3 simulation environment for the
             20 STA / 1 AP IEEE 802.11ax campaign},
  year    = {2026},
  url     = {https://github.com/m1fril/ns3-wifi6-zero-trust-cprs},
  note    = {Source code and measured traces}
}
```

---

## License

GNU General Public License v2.0 — inherited from ns‑3, which this tree
redistributes. See [LICENSE](LICENSE).

This repository is the `ns-allinone-3.46.1` distribution of
[ns-3](https://www.nsnam.org/) with the scenario, campaign runners and measured
traces above added under `ns-3.46.1/`. ns‑3 itself is the work of the nsnam
project; upstream documentation is at <https://www.nsnam.org/documentation/>.
