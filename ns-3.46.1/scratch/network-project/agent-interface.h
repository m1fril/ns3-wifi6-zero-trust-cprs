#ifndef AGENT_INTERFACE_H
#define AGENT_INTERFACE_H

namespace ns3 {

class SimulationEnvironment;

class IAgent {
public:
    virtual ~IAgent() = default;
    virtual void Initialize(ns3::SimulationEnvironment* env) = 0;
    virtual void Tick(double currentTime) = 0;
};

} // namespace ns3

#endif // AGENT_INTERFACE_H
