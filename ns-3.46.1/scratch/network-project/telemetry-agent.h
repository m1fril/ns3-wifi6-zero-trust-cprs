#ifndef TELEMETRY_AGENT_H
#define TELEMETRY_AGENT_H

#include "agent-interface.h"

namespace ns3 {

class TelemetryAgent : public IAgent {
public:
    TelemetryAgent();
    void Initialize(ns3::SimulationEnvironment* env) override;
    void Tick(double currentTime) override;

private:
    SimulationEnvironment* m_env;
};

} // namespace ns3

#endif // TELEMETRY_AGENT_H
