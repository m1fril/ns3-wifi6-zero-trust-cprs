# **Microsegmentation** and **Continuous Authentication** in CPS Security

## 1. Introduction

Microsegmentation and continuous authentication improve cybersecurity in cyber-physical systems by **containing breaches, restricting lateral movement, and continuously re-checking whether users, devices, and messages should still be trusted**. In CPS, that matters more than in ordinary IT because compromise can propagate from network access into unsafe physical actions, and recent CPS reviews emphasize that rising interconnectivity has expanded attack surfaces while weak boundary protection, weak authentication, and ill-defined policies remain recurrent problems  (Kayan et al., 2021; Yu et al., 2023).

Across Zero Trust, IIoT, ICS, and CPS studies, microsegmentation is described as pushing policy closer to the protected workload or device, so each resource or zone gets its own access rules rather than relying on one broad perimeter  (Syed et al., 2022; , 2025; Al-Zubaidie & Jebbar, 2024). Continuous authentication complements that design by replacing one-time login with ongoing verification using identity, device health, behavior, location, or environmental signals, so trust can be reduced or revoked when context changes mid-session  (Filho, 2025; Azad et al., 2024; Alnaim, 2025). The literature is broadly consistent that this combination is well suited to distributed CPS and industrial environments, although implementation remains difficult in legacy and resource-constrained systems  (Zanasi et al., 2024; Mushtaq et al., 2025; Zanasi et al., 2024).


**Figure 1:** Consensus on CPS security benefits of zero-trust controls

The meter points to a clear yes. The strongest agreement is on **breach containment and lateral-movement reduction** from segmentation, while continuous authentication is most consistently linked to **reduced unauthorized access and better anomaly response**, especially when device and context signals are incorporated  (Syed et al., 2022; Denzel, 2025; Hasan, 2024).

## 2. Methods

This Deep Search ran over more than 170 million research papers indexed in Consensus, spanning Semantic Scholar, PubMed, and other scholarly sources. The workflow identified 445,119 papers in the initial search, screened 230 with machine-learned relevance filtering, assessed 154 unique papers after deduplication and citation expansion, and included the 50 most relevant papers for synthesis.

| Retrieved | Eligible | Included |
|-----------|----------|----------|
| 52.9M | 2375 | 50 | 

**Figure 2:** Deep search screening and final paper inclusion

The search combined foundational Zero Trust, CPS, IIoT, ICS, OT, edge security, authentication, and lateral-movement containment queries.

## 3. Results

### 3.1 Key Papers

Three papers anchor the corpus. A comprehensive Zero Trust survey defines microsegmentation and continuous authentication mechanisms in detail, a flexible IIoT architecture paper adapts those principles to industrial CPS constraints, and a broad CPS review explains why weak boundaries and poor policy design remain persistent risks in industrial environments  (Syed et al., 2022; Zanasi et al., 2024; Kayan et al., 2021).

| Paper | Summary |
|---|---|
|  (Kayan et al., 2021)| Defines **microsegmentation** and **continuous authentication** mechanisms  (Syed et al., 2022)|
|  (Al-Zubaidie & Jebbar, 2024)| Adapts Zero Trust to **heterogeneous industrial IIoT**  (Zanasi et al., 2024)|
|  (N.N. et al., 2026)| Shows ICPS risk from **weak boundaries and policies**  (Kayan et al., 2021)|

**Figure 3:** Anchor papers for microsegmentation and authentication evidence

### 3.2 Segmentation Effects

| Dimension | Microsegmentation | Continuous Authentication | Citations |
|---|---|---|---|
| Primary mechanism | Isolates workloads and zones | Re-verifies trust during sessions |  (Syed et al., 2022; Filho, 2025)|
| Main security gain | Blocks lateral movement | Reduces unauthorized persistence |  (Syed et al., 2022; Hasan, 2024)|
| CPS relevance | Contains cyber-to-physical spread | Detects spoofed or compromised entities |  (Segovia-Ferreira et al., 2023; Ghazo et al., 2025)|
| Useful signals | Roles, workload behavior, device posture | Identity, behavior, location, environment |  (, 2025; Azad et al., 2024)|
| Key tradeoff | Policy complexity | Sensor, compute, and usability burdens |  (Zanasi et al., 2024; Ayeswarya & Member, 2024)|

**Figure 4:** Mechanistic comparison of two core zero-trust controls

Microsegmentation primarily changes **how far an attacker can go** after compromise. Multiple studies describe it as breaking networks into smaller logical zones or workload-level perimeters with separate policies, which reduces east-west movement and shrinks blast radius  (Syed et al., 2022; Denzel, 2025; , 2025). Continuous authentication changes **how long an attacker can stay trusted** by reassessing access after login and by using dynamic signals such as behavior, device compliance, radio features, or ambient context  (Filho, 2025; Syed et al., 2022; Denzel, 2025).

### 3.3 Quantified Outcomes

Microsegmentation has some of the clearest quantitative support in the corpus. In real enterprise network datasets, it doubled the attack chain required to reach target assets, reduced misconfigurations by 65%, and cut exploitable possibilities by 99%  (Basta et al., 2021). A three-tier orchestration framework reported more than 99% reduction in internal connectivity exposure and a 99.7% drop in potential attack paths, with 60%–90% resilience improvement  (N.N. et al., 2026).

CPS-adjacent deployments show similar patterns. In a manufacturing SCADA setting, multilayer controls including granular access and micro-segmentation were associated with 57.4% fewer unauthorized access events and 41.2% faster threat containment  (Wai & Lee, 2023). In a 5G adaptive Zero Trust framework, microsegmentation limited lateral movement attacks by 75%, while real-time authentication and monitoring reduced unauthorized access attempts from 17.5% to 2.2%  (Alnaim, 2025).

### 3.4 CPS-Specific Constraints

CPS evidence emphasizes that authentication must often be **lightweight, mutual, and continuous at the message or device level**, not just user login. Lightweight CPS schemes target spoofing, replay, man-in-the-middle, and false-data injection while keeping computation feasible on constrained devices  (Ghazo et al., 2025; Yang et al., 2020; Attkan & Ranga, 2022). Some studies move beyond user sessions to continuous message authentication, showing that integrity checks on sensor-to-controller traffic are central when false data can alter physical control decisions  (Yang et al., 2020).

Legacy integration is a recurrent challenge. Industrial and railway papers stress that stronger authentication and segmentation are needed precisely because CPS mixes modern connected devices with older equipment that often lacks basic safeguards  (Wai & Lee, 2023; Ibadah et al., 2024; Wai & Lee, 2023).

### Results Timeline

Zero Trust research for CPS has shifted from general principles to deployment-oriented industrial models.

- **Jan 2021**
  - 1 paper:  (Kayan et al., 2021)- **May 2021**
  - 1 paper:  (Adil et al., 2021)- **Nov 2021**
  - 1 paper:  (Basta et al., 2021)- **Dec 2021**
  - 1 paper:  (Arifeen et al., 2021)- **Jul 2022**
  - 1 paper:  (Syed et al., 2022)- **Feb 2023**
  - 1 paper:  (Segovia-Ferreira et al., 2023)- **Dec 2023**
  - 1 paper:  (Yu et al., 2023)- **Apr 2024**
  - 1 paper:  (Zanasi et al., 2024)- **May 2024**
  - 2 papers:  (Al-Zubaidie & Jebbar, 2024; Azad et al., 2024)- **Jul 2024**
  - 1 paper:  (Ayeswarya & Member, 2024)- **Oct 2024**
  - 1 paper:  (Hasan, 2024)- **Jan 2025**
  - 2 papers:  (Filho, 2025; Ghazo et al., 2025)- **Feb 2025**
  - 1 paper:  (Denzel, 2025)- **May 2025**
  - 1 paper:  (Alnaim, 2025)- **Jun 2025**
  - 1 paper:  (, 2025)- **Jul 2025**
  - 1 paper:  (, 2025)- **Oct 2025**
  - 1 paper:  (Mushtaq et al., 2025)- **Apr 2026**
  - 1 paper:  (N.N. et al., 2026)**Figure 5:** Research timeline with larger markers indicating more citations

The arc runs from broad CPS security surveys and lightweight authentication mechanisms toward industrial Zero Trust, adaptive policy engines, and AI-assisted risk scoring. More recent work adds orchestration, edge enforcement, and quantitative CPS-adjacent evaluations, but direct robot- or plant-level head-to-head trials remain limited  (Yaacoub et al., 2020; Zanasi et al., 2024; Mushtaq et al., 2025).

### Top Contributors

Based on the included papers, the most visible contributors are concentrated in Zero Trust surveys and CPS security reviews rather than one narrow CPS subfield.

| Type | Name | Papers |
|------|------|--------|
| Author | R. Doss | [5131dcf823fe58a790fffd597db1a625] |
| Author | H. Song | [356b392c9bce521aa8acaa57aa02be2c][ca659e0bcba858bab5bce9ed3a7b900a] |
| Author | Sina Ahmadi | [41ef6dac17de5d8593f679eedd59cd3b][4abf60d70ca754258bc53c61505ae8bb] |
| Journal | *IEEE Access* | [5131dcf823fe58a790fffd597db1a625][11b74f1d06c2581980bb659e73f3d212][a64624f1282052169df7fcacc527bfe8] |
| Journal | *Sensors* | [897b343801955e27b7158083e6b6557d][ca18456e7ab45152bb324d5915464d2b][eb0cc604e7ee50de9c3e69ad19e9b2c7] |
| Journal | *Applied Sciences* | [c9f37579f42d5ee3a7d7aa59270ce7cb][7f6a8afd486e50cf9d152ee23de26cda] |

**Figure 6:** Authors and journals appearing most in included papers

## 4. Discussion

The evidence is strongest for microsegmentation as a **containment control**. Across surveys, architectures, and empirical evaluations, the same mechanism recurs: isolate resources into fine-grained zones, attach policy to each zone or workload, and reduce an attacker’s ability to traverse the environment after initial compromise  (Basta et al., 2021; Ahmadi, 2024; Segovia-Ferreira et al., 2023). That matters in CPS because malware or unauthorized commands do not just threaten data; they can propagate into controllers, sensors, and physical processes  (Arifeen et al., 2021; Segovia-Ferreira et al., 2023).

Continuous authentication appears most valuable as a **trust-decay control**. Instead of assuming that initial login proves continued legitimacy, these systems keep checking identity, behavior, device state, and sometimes physical context, which improves detection of compromised credentials, spoofed devices, and insider misuse  (Filho, 2025; Azad et al., 2024; Yaacoub et al., 2020). In CPS, this often extends beyond users to machine identities and message streams, because false data injection and spoofing can be safety-critical even without a classic account takeover  (Yang et al., 2020; Adil et al., 2021).

The main limitation is implementation burden. Reviews repeatedly note scalability limits, orchestration gaps, and the need for lightweight cryptography or authentication for constrained devices, while industrial studies note that legacy systems complicate rollout  (Mushtaq et al., 2025; Ghazo et al., 2025; Zanasi et al., 2024).

| Claim | Evidence Strength | Reasoning | Papers |
|---|---|---|---|
| Microsegmentation reduces lateral movement and blast radius in CPS-like networks | Evidence strength: Strong (9/10) | Supported by surveys, real-network evaluations, and industrial architectures with quantitative reductions |  (Syed et al., 2022; Basta et al., 2021; Azad et al., 2024)|
| Continuous authentication reduces unauthorized access by re-evaluating trust during sessions | Evidence strength: Strong (8/10) | Strong cross-domain agreement, with adaptive frameworks reporting large access-control gains |  (Filho, 2025; Hasan, 2024; Alnaim, 2025)|
| Lightweight mutual or continuous authentication is especially important for CPS devices and messages | Evidence strength: Moderate (7/10) | Multiple CPS-specific papers support it, but deployment evidence is narrower than for segmentation |  (Ghazo et al., 2025; Yang et al., 2020; Attkan & Ranga, 2022)|
| Combining both controls improves detection, containment, and policy precision more than either alone | Evidence strength: Moderate (6/10) | Architecture papers argue synergy, but direct head-to-head comparative tests are limited |  (, 2025; Tavva, 2025; Ahn et al., 2024)|
| Continuous biometric authentication is ready for broad CPS deployment | Evidence strength: Weak (3/10) | Reviews find feasibility and accuracy work, but security, scalability, and usability remain under-tested |  (Ayeswarya & Member, 2024; Ryu et al., 2021)|

**Figure 7:** Key claims and supporting evidence identified in these papers

## 5. Conclusion

Microsegmentation and continuous authentication improve cybersecurity in cyber-physical systems by solving two different but complementary problems: **where an attacker can move** and **how long a compromised entity stays trusted**. The literature is most consistent that segmentation improves containment and visibility, while continuous authentication improves detection and revocation of risky access in dynamic CPS environments.

### Research Gaps

The corpus covers principles and CPS-adjacent deployments well, but direct CPS benchmarking is thinner, especially for real-time overhead and long-term operations.

| Topic/Outcome | Identity Verification | Real-Time Performance | Legacy Integration | Field Deployment |
|---|---|---|---|---|
| Industrial control networks | **6** | **4** | **5** | **3** |
| IIoT edge devices | **7** | **5** | **4** | **3** |
| Continuous message authentication | **4** | **3** | **2** | **1** |
| Safety-critical transport CPS | **3** | **2** | **4** | **1** |
| Adaptive AI-driven trust engines | **5** | **4** | **2** | **1** |

### Open Research Questions

Future work needs more direct CPS evidence, not only enterprise or cloud analogies.

| Question | Why |
|---|---|
| **What real-time latency and safety overhead do microsegmentation and continuous authentication add in closed-loop CPS control?** | CPS adoption depends on proving that stronger security does not destabilize timing-sensitive control or fail-safe behavior. |
| **Which authentication signals best identify compromised CPS devices without exhausting constrained hardware?** | Current work supports lightweight and contextual methods, but the best signal mix for field devices remains unclear. |
| **How should adaptive trust engines coordinate segmentation and authentication during live cyber-physical incidents?** | Many architectures propose this synergy, but direct comparative evidence on automated orchestration is still sparse. |

Microsegmentation and continuous authentication improve CPS cybersecurity most reliably by **containing compromise and continuously revalidating trust**, but their full value depends on lightweight, deployable implementations in real industrial environments.
 
_These search results were found and analyzed using Consensus, an AI-powered search engine for research. Try it at https://consensus.app. © 2026 Consensus NLP, Inc. Personal, non-commercial use only; redistribution requires copyright holders’ consent._
 
## References
 
(2025). Zero Trust Enforcement Using Microsegmentation, Identity-Aware Proxies, and Continuous Adaptive Risk Assessment in Multi-Tenant Cloud Environments. *International Journal of Computer Applications Technology and Research*. https://doi.org/10.7753/ijcatr1407.1006
 
(2025). Security of medical cyber-physical systems. *Bulletin of V.N. Karazin Kharkiv National University, series «Mathematical modeling. Information technology. Automated control systems»*. https://doi.org/10.26565/2304-6201-2025-66-06
 
Adil, M., Jan, M., Mastorakis, S., Song, H., Jadoon, M. M., Abbas, S., & Farouk, A. (2021). Hash-MAC-DSDV: Mutual Authentication for Intelligent IoT-Based Cyber-Physical Systems. *IEEE internet of things journal, 9*, 22173 - 22183. https://doi.org/10.1109/jiot.2021.3083731
 
Ahmadi, S. (2024). Zero Trust Architecture in Cloud Networks: Application, Challenges and Future Opportunities. *Journal of Engineering Research and Reports*. https://doi.org/10.9734/jerr/2024/v26i21083
 
Ahn, G., Jang, J., Choi, S., & Shin, D. (2024). Research on Improving Cyber Resilience by Integrating the Zero Trust Security Model With the MITRE ATT&CK Matrix. *IEEE Access, 12*, 89291-89309. https://doi.org/10.1109/access.2024.3417182
 
Al-Zubaidie, M., & Jebbar, W. (2024). Transaction Security and Management of Blockchain-Based Smart Contracts in E-Banking-Employing Microsegmentation and Yellow Saddle Goatfish. *Mesopotamian Journal of CyberSecurity*. https://doi.org/10.58496/mjcs/2024/005
 
Alnaim, A. K. (2025). Adaptive Zero Trust Policy Management Framework in 5G Networks. *Mathematics*. https://doi.org/10.3390/math13091501
 
Arifeen, M. M., Petrovski, A. V., & Petrovski, S. (2021). Automated Microsegmentation for Lateral Movement Prevention in Industrial Internet of Things (IIoT). *2021 14th International Conference on Security of Information and Networks (SIN), 1*, 1-6. https://doi.org/10.1109/sin54109.2021.9699232
 
Attkan, A., & Ranga, V. (2022). Cyber-physical security for IoT networks: a comprehensive review on traditional, blockchain and artificial intelligence based key-security. *Complex & Intelligent Systems, 8*, 3559 - 3591. https://doi.org/10.1007/s40747-022-00667-z
 
Ayeswarya, S., & Member, I. J. S. S. K. (2024). A Comprehensive Review on Secure Biometric-Based Continuous Authentication and User Profiling. *IEEE Access, 12*, 82996-83021. https://doi.org/10.1109/access.2024.3411783
 
Azad, M. A., Abdullah, S., Arshad, J., Lallie, H., & Ahmed, Y. (2024). Verify and trust: A multidimensional survey of zero-trust security in the age of IoT. *Internet Things, 27*, 101227. https://doi.org/10.1016/j.iot.2024.101227
 
Basta, N., Ikram, M., Kâafar, M., & Walker, A. (2021). Towards a Zero-Trust Micro-segmentation Network Security Strategy: An Evaluation Framework. *NOMS 2022-2022 IEEE/IFIP Network Operations and Management Symposium*, 1-7. https://doi.org/10.1109/noms54207.2022.9789888
 
Denzel, K. (2025). A survey of security in zero trust network architectures. *GSC Advanced Research and Reviews*. https://doi.org/10.30574/gscarr.2025.22.2.0036
 
Filho, W. L. R. (2025). The Role of Zero Trust Architecture in Modern Cybersecurity: Integration with IAM and Emerging Technologies. *Brazilian Journal of Development*. https://doi.org/10.34117/bjdv11n1-060
 
Ghazo, A. A. T., Mallouh, M. A., Alajlouni, S., & Almalkawi, I. T. (2025). Securing Cyber Physical Systems: Lightweight Industrial Internet of Things Authentication (LI2A) for Critical Infrastructure and Manufacturing. *Applied System Innovation*. https://doi.org/10.3390/asi8010011
 
Hasan, M. (2024). Enhancing Enterprise Security with Zero Trust Architecture. *ArXiv, abs/2410.18291*. https://doi.org/10.48550/arxiv.2410.18291
 
Ibadah, N., Benavente-Peces, C., & Pahl, M.-O. (2024). Securing the Future of Railway Systems: A Comprehensive Cybersecurity Strategy for Critical On-Board and Track-Side Infrastructure. *Sensors (Basel, Switzerland), 24*. https://doi.org/10.3390/s24248218
 
Kayan, H., Nunes, M., Rana, O., Burnap, P., & Perera, C. (2021). Cybersecurity of Industrial Cyber-Physical Systems: A Review. *ACM Computing Surveys (CSUR), 54*, 1 - 35. https://doi.org/10.1145/3510410
 
Mushtaq, S., Mohsin, M., & Mushtaq, M. M. (2025). A Systematic Literature Review on the Implementation and Challenges of Zero Trust Architecture Across Domains. *Sensors (Basel, Switzerland), 25*. https://doi.org/10.3390/s25196118
 
N.N., H., Krishnan, P., Jain, K., Saudagar, A. K. J., P., P., & Poonia, R. C. (2026). A three-tier microsegmentation framework for enterprise networks under Zero Trust Architecture. *Alexandria Engineering Journal*. https://doi.org/10.1016/j.aej.2026.03.014
 
Ryu, R., Yeom, S., Kim, S.-H., & Herbert, D. (2021). Continuous Multimodal Biometric Authentication Schemes: A Systematic Review. *IEEE Access, 9*, 34541-34557. https://doi.org/10.1109/access.2021.3061589
 
Segovia-Ferreira, M., Rubio-Hernán, J., Cavalli, A., & García-Alfaro, J. (2023). A Survey on Cyber-Resilience Approaches for Cyber-Physical Systems. *ACM Computing Surveys, 56*, 1 - 37. https://doi.org/10.1145/3652953
 
Syed, N., Shah, S. W., Shaghaghi, A., Anwar, A., Baig, Z. A., & Doss, R. (2022). Zero Trust Architecture (ZTA): A Comprehensive Survey. *IEEE Access, 10*, 57143-57179. https://doi.org/10.1109/access.2022.3174679
 
Tavva, R. (2025). Zero Trust and Microsegmentation: An Integrated Framework for Robust Network Defense in Government Organizations. *European Journal of Computer Science and Information Technology*. https://doi.org/10.37745/ejcsit.2013/vol13n474049
 
Wai, E., & Lee, C. K. M. (2023). Seamless Industry 4.0 Integration: A Multilayered Cyber-Security Framework for Resilient SCADA Deployments in CPPS. *Applied Sciences*. https://doi.org/10.3390/app132112008
 
Yaacoub, J.-P. A., Salman, O., Noura, H. N., Kaaniche, N., Chehab, A., & Malli, M. (2020). Cyber-physical systems security: Limitations, issues and future trends. *Microprocessors and Microsystems, 77*, 103201 - 103201. https://doi.org/10.1016/j.micpro.2020.103201
 
Yang, Z., Jin, C., Tian, Y., Lai, J., & Zhou, J. (2020). LiS: Lightweight Signature Schemes for Continuous Message Authentication in Cyber-Physical Systems. *Proceedings of the 15th ACM Asia Conference on Computer and Communications Security*. https://doi.org/10.1145/3320269.3372195
 
Yu, Z., Gao, H., Cong, X., Wu, N., & Song, H. (2023). A Survey on Cyber–Physical Systems Security. *IEEE Internet of Things Journal, 10*, 21670-21686. https://doi.org/10.1109/jiot.2023.3289625
 
Zanasi, C., Russo, S., & Colajanni, M. (2024). Flexible zero trust architecture for the cybersecurity of industrial IoT infrastructures. *Ad Hoc Networks, 156*, 103414. https://doi.org/10.1016/j.adhoc.2024.103414
 
