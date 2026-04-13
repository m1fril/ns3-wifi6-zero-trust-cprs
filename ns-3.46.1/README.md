# Network Project - WiFi Bianchi Simulation

This project simulates a WiFi network with Multiple Access Points (APs) and Stations (STAs) using the ns-3 network simulator. It incorporates NetSimulyzer for 3D visualization.

## Overview

The simulation `network-project.cc` (formerly `wifi-bianchi-netsimulyzer.cc`) provides a robust environment to study Bianchi-like scenarios with modern ns-3 WiFi models and NetSimulyzer visualization.

## Prerequisites

- ns-3 (version 3.46.1)
- CMake
- NetSimulyzer module (installed in `contrib/netsimulyzer`)

## Building the Project

To build the project, run:

```bash
./ns3 build network-project
```

## Running the Simulation

To run the simulation with default parameters:

```bash
./ns3 run network-project
```

### Simulation Parameters

- `--nAps`: Number of Access Points (Default: 1)
- `--nStas`: Number of Stations (Default: 50)
- `--duration`: Simulation duration in seconds (Default: 1000)
- `--visual`: Enable NetSimulyzer 3D output (Default: false)
- `--wifiRate`: WiFi PHY Data Rate (Default: OfdmRate12Mbps)

### Full Launch Command

For a complete simulation with custom parameters and 3D visualization enabled, use:

```bash
./ns3 run network-project -- --nAps=1 --nStas=50 --duration=1000 --visual=true --wifiRate=OfdmRate12Mbps
```


## Output

- **Console**: Progress logs and simulation results.
- **Visualizer**: If `--visual=true` is used, a `.json` file for NetSimulyzer will be generated.
- **Data**: CSV logs for packet events and performance metrics.
