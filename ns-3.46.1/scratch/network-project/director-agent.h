#ifndef DIRECTOR_AGENT_H
#define DIRECTOR_AGENT_H

#include "agent-interface.h"
#include "simulation-modes.h"
#include <vector>
#include <string>

namespace ns3 {

class DirectorAgent : public IAgent {
public:
    struct Phase {
        double startTime;
        SimulationMode mode;
        std::string label;
    };

    DirectorAgent(const std::vector<Phase>& phases);

    void Initialize(ns3::SimulationEnvironment* env) override;
    void Tick(double currentTime) override;

private:
    SimulationEnvironment* m_env;
    std::vector<Phase> m_phases;
    size_t m_currentPhaseIndex;
};

} // namespace ns3

#endif // DIRECTOR_AGENT_H
