# Design: zero-trust cybersecurity for cyber-physical robotic systems overview

## Purpose and position

This first PhD article will be a domain-oriented overview of models and methods for ensuring the cybersecurity of cyber-physical robotic systems (CPRS) through a zero-trust architecture. Its principal contribution is a structured literature synthesis, not a new network-simulation method.

The existing ns-3.46.1 Wi-Fi data will be used only as an illustrative case study. It demonstrates the wireless-resilience conditions under which zero-trust controls in CPRS must operate; it must not be represented as a full implementation or evaluation of a zero-trust architecture.

Working title: *Zero-Trust Cybersecurity for Cyber-Physical Robotic Systems: Models, Methods, and Wireless-Resilience Implications.*

Central research question: Which zero-trust models and methods best strengthen the cybersecurity and operational resilience of CPRS, particularly where wireless communication is constrained by latency, packet loss, mobility, and failures?

## Evidence boundary

- Literature review is the primary evidence base, with 56 distinct sources allocated to a primary section.
- The simulation case study draws on the spatial scenario matrix, 30-run normal-state stability experiment, and 30-run targeted-loss experiment.
- The results directory currently contains event CSV files and PNG dashboards, but no pcap, ns-3 trace, or FlowMonitor XML files. Packet-delivery ratio, PHY/MAC loss, retransmissions, SNR, and MCS statistics must not be claimed unless those data are generated separately.

## IMRAD article outline and literature strategy

### 1. Introduction — Robotic CPS security problem and zero-trust rationale (9 sources)

Define CPRS and their cyber-physical safety consequences, then introduce the attack surface: compromised robot identity, sensor or actuator spoofing, lateral movement, insecure wireless links, and cloud/edge dependencies. Present zero trust as the security paradigm of continuous verification and least privilege.

Required source themes: CPRS and robotics-security surveys; safety-security co-engineering; foundational zero-trust publications; industrial and robotic threat models.

### 2. Introduction — Research gap, aim, and article contribution (8 sources)

Explain why general zero-trust guidance does not fully address robotic CPS constraints: real-time deadlines, intermittent connectivity, device limitations, mobility, mission continuity, and human-robot interaction. State the review question and delimit the ns-3 evidence as an illustrative communication-resilience case study.

Required source themes: zero-trust reference architectures and standards; robotics-CPS security gaps; edge and IoT zero-trust adaptations; prior reviews lacking robotics-specific operational constraints.

### 3. Methods — Review design and literature-selection protocol (8 sources)

Describe a reproducible narrative or scoping-review process: selected databases, search strings, date and language bounds, inclusion and exclusion criteria, quality screening, and thematic coding. Code each study by identity, device posture, microsegmentation, continuous authentication and authorization, telemetry, trust decision, incident response, and resilience.

Required source themes: review-methodology guidance; cybersecurity systematic or scoping reviews; threat-taxonomy methods; source-quality assessment criteria.

Outputs: a study-selection diagram and a table mapping selected sources to zero-trust control categories.

### 4. Methods — Conceptual model: zero trust for robotic CPS (6 sources)

Specify the reference architecture that organizes the synthesis: robot, actuator, and sensor layer; local network; edge gateway; policy decision and enforcement points; identity and trust services; telemetry or SIEM; and cloud or fleet-management layer. Describe the continuous-verification and least-privilege data flow.

Required source themes: zero-trust architecture; identity and device attestation; microsegmentation; policy engines; edge/fog robotics security.

Output: one conceptual architecture figure identified as a synthesis of literature.

### 5. Methods — ns-3 case-study design, data provenance, and analysis plan (5 sources)

Document ns-allinone-3.46.1/ns-3.46.1, the 802.11 scenarios, one AP, station density, CBR traffic, room-size sweep, Normal/Controlled/Failure states, and targeted loss at 100 s. Explain that the simulation evaluates communication-resilience conditions relevant to zero-trust control placement and telemetry reliability.

Input datasets:

- `results/diploma_matrix_runs/run_{normal|controlled|failure}_area{25,75,150,200,300,400}.0m.csv`
- `results/rms_30_runs/run_1.csv` through `run_30.csv`
- `results/targeted_loss_30_runs/run_*.csv`

The event schema is `Time_s,NodeID,NodeType,Action,Value`; relevant actions include `LATENCY_MS`, `THROUGHPUT_MBPS`, `RECV`, `ASSOC`, and `JOINED`.

Required source themes: ns-3 and Wi-Fi simulation validity; 802.11 capacity and contention models; FlowMonitor and telemetry-measurement limitations.

### 6. Results — Literature synthesis: models and methods for zero-trust CPRS (8 sources)

Report results by control category rather than source-by-source: identity and device attestation; continuous authentication and authorization; microsegmentation and least privilege; behavioural trust or risk scoring; secure edge/cloud coordination; and detection, recovery, and safe degradation.

Output: a comparative matrix covering method, protected asset, required telemetry, latency or computational cost, maturity, and suitability for mobile robots.

Required source themes: empirical or design work representing each zero-trust control category, including limitations and deployment evidence.

### 7. Results — Illustrative ns-3 evidence: spatial stress and operational states (2 sources)

Analyze the 18 CSV files in `results/diploma_matrix_runs` using:

- faceted time-series plots of `THROUGHPUT_MBPS` by scenario and area;
- ECDF/CDF plots of `LATENCY_MS` for each area, with Normal, Controlled, and Failure series;
- distance-response plots of median throughput and median/95th-percentile latency against area size; and
- fixed-bin event-rate time series derived from `RECV` records to visualize communication activity before and during disruptions.

Use the corresponding `results/visuals/run_*.png` dashboards as illustrative supplementary examples only, never as the quantitative source. Do not infer unavailable packet- or PHY-level metrics from these event logs.

Required source themes: closely comparable Wi-Fi resilience and latency investigations.

### 8. Results — Stability, targeted failure, and zero-trust mapping (3 sources)

Use `results/rms_30_runs` to distinguish ordinary stochastic variation from anomalous behavior with box or violin plots and 95% confidence intervals for per-run mean latency and throughput. Use 0.05-s binned time series for representative runs, with the existing `results/visuals/rms_random` dashboards as examples.

Use `results/targeted_loss_30_runs` for paired pre/post-100-s plots comparing the two target stations with non-target stations. Conclude this section with a table mapping latency spikes, throughput collapse, station isolation, and cyclic outages to affected zero-trust controls and candidate mitigations.

Required source themes: anomaly detection, resilient authentication, and trustworthy telemetry under network impairment.

### 9. Discussion — Implications, limitations, and research agenda (7 sources)

Interpret the review and case study together: zero trust for CPRS should be adaptive, context-aware, and fail safely; edge-local enforcement reduces dependence on unstable links; and security telemetry needs availability and uncertainty awareness. Explicitly discuss scenario specificity, the lack of physical-testbed validation, the absence of a direct zero-trust protocol implementation, and the lack of packet-level or FlowMonitor output.

Close with a PhD research agenda: develop and evaluate a zero-trust CPRS architecture, create trust-aware adaptation policies, and validate them on a physical robotic testbed.

Required source themes: physical-testbed comparisons; security-performance trade-offs; fail-safe/fail-operational robotics; and open zero-trust CPS research challenges.

## Citation accounting

| Article section | Distinct sources |
| --- | ---: |
| Introduction, Sections 1-2 | 17 |
| Methods, Sections 3-5 | 19 |
| Results, Sections 6-8 | 13 |
| Discussion, Section 9 | 7 |
| **Total** | **56** |

Each source is counted once in its primary section even when cited again elsewhere. This prevents double-counting and keeps the review within the requested 50-60-source range.

## Quality and integrity controls

- Use primary standards, peer-reviewed articles, and high-quality surveys where possible; do not invent references.
- Separate claims supported by literature from observations drawn from the ns-3 event logs.
- State uncertainty and scope limitations adjacent to simulation findings.
- Report summary statistics, distributional plots, and confidence intervals for repeated runs; preserve raw-data file names and analysis parameters for reproducibility.
