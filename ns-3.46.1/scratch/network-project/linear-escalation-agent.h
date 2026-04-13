#ifndef LINEAR_ESCALATION_AGENT_H
#define LINEAR_ESCALATION_AGENT_H

#include "agent-interface.h"

namespace ns3 {

class LinearEscalationAgent : public IAgent {
public:
    LinearEscalationAgent(double intervalDecreaseRatePerSec, double speedIncreaseRatePerSec, 
                          double baseInterval, double minInterval, 
                          double baseSpeed, double maxSpeed);

    void Initialize(ns3::SimulationEnvironment* env) override;
    void Tick(double currentTime) override;

private:
    SimulationEnvironment* m_env;
    double m_intervalRate; 
    double m_speedRate;
    double m_baseInterval;
    double m_minInterval;
    double m_baseSpeed;
    double m_maxSpeed;
    double m_escalationStartTime;
};

} // namespace ns3

#endif // LINEAR_ESCALATION_AGENT_H
