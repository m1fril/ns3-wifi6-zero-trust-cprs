# Network Project - Modular Agent-Based WiFi Simulation

This project simulates a WiFi network with Multiple Access Points (APs) and Stations (STAs) using the ns-3 network simulator. It features a modular **Agent-Based Architecture** designed for high flexibility and advanced stress testing scenarios.

## 🏗 Architecture Overview

The simulation is refactored from a monolithic script into a specialized directory: `scratch/network-project/`.

- **SimulationEnvironment**: Manages physical topology, logging, and exposes an API for environmental adjustments.
- **AgentOrchestrator**: Executes high-resolution heartbeats (100ms) to trigger agent behaviors.
- **Specialized Agents**:
    - `DirectorAgent`: Manages scenario transitions (e.g., Normal -> Stress).
    - `ChaosAgent`: Injects sine-wave mathematical perturbations to load and mobility.
    - `LinearEscalationAgent`: Implements a linear ramp for stress testing (load increase/speed increase).
    - `TelemetryAgent`: Observer layer for metrics.

## 📋 Prerequisites

- ns-3 (version 3.46.1)
- CMake
- NetSimulyzer module (installed in `contrib/netsimulyzer`)

## 🛠 Building the Project

The modular structure requires building the specific scratch target:

```bash
./ns3 build scratch/network-project/network-project-main
```

## 🚀 Running the Simulation

To run the simulation with default parameters:

```bash
./ns3 run scratch/network-project/network-project-main
```

### Simulation Parameters

- `--nAps`: Number of Access Points (Default: 1)
- `--nStas`: Number of Stations (Default: 50)
- `--queueSize`: Maximum size of the Wi-Fi MAC Queue (Default: 500p)
- `--pktInterval`: Microseconds between packets (Default: 500000)
- `--errorRate`: Packet error rate for wired backbone (Default: 0.0)
- `--visual`: Enable NetSimulyzer 3D output (Default: false)
- `--duration`: Simulation duration in seconds (Default: 1000)
- `--roomSize`: Size of the grid in meters (Default: 1000.0)
- `--wifiRate`: WiFi PHY Data Rate (Default: OfdmRate12Mbps)
- `--pSize`: Packet payload size (Default: 1500 bytes)

### Advanced Launch Example

To run a stress-test scenario with custom parameters:

```bash
./ns3 run scratch/network-project/network-project-main -- --nAps=4 --nStas=20 --duration=100 --pktInterval=400000 --queueSize=1000
```

## 📊 Output

- **CSV Logs**: Generates `Bianchi-*.csv` containing time-stamped events (LATENCY, RECV, THROUGHPUT, JOINED, etc.) prefixed with NodeID and NodeType.
- **Visualizer**: If `--visual=true` is used, a NetSimulyzer JSON file will be generated for 3D playback.
- **Console**: Real-time agent transition logs (e.g., `[DIRECTOR] Transition to Linear_Ramp_To_Failure`).
