#ifndef SIMULATION_MODES_H
#define SIMULATION_MODES_H

namespace ns3 {

enum class SimulationMode {
    NORMAL_OPERATION,
    CONTROLLED_LOAD,
    NETWORK_FAILURE,
    STRESS_ESCALATION
};

} // namespace ns3

#endif // SIMULATION_MODES_H
