#ifndef CHAOS_AGENT_H
#define CHAOS_AGENT_H

#include "agent-interface.h"
#include <cmath>

namespace ns3 {

class ChaosAgent : public IAgent {
public:
    ChaosAgent(double frequency, double loadAmp, double speedAmp, double baseInterval, double baseSpeed);

    void Initialize(ns3::SimulationEnvironment* env) override;
    void Tick(double currentTime) override;

private:
    SimulationEnvironment* m_env;
    double m_freq, m_loadAmp, m_speedAmp, m_baseInterval, m_baseSpeed;
};

} // namespace ns3

#endif // CHAOS_AGENT_H
