#ifndef TARGETED_LOSS_AGENT_H
#define TARGETED_LOSS_AGENT_H

#include "agent-interface.h"

namespace ns3 {

class TargetedLossAgent : public IAgent {
public:
    TargetedLossAgent(uint32_t sta1, uint32_t sta2, double triggerTime, double lossPowerDbm);

    void Initialize(ns3::SimulationEnvironment* env) override;
    void Tick(double currentTime) override;

private:
    SimulationEnvironment* m_env;
    uint32_t m_sta1, m_sta2;
    double m_triggerTime;
    double m_lossPowerDbm;
    bool m_triggered;
};

} // namespace ns3

#endif // TARGETED_LOSS_AGENT_H
