#include "agent-orchestrator.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"

using namespace ns3;

namespace ns3 {

void AgentOrchestrator::RegisterAgent(std::shared_ptr<IAgent> agent) {
    m_agents.push_back(agent);
}

void AgentOrchestrator::ExecuteHeartbeat() {
    double t = Simulator::Now().GetSeconds();
    for (auto& agent : m_agents) {
        agent->Tick(t);
    }
    // Schedule next tick (100ms for high resolution chaos)
    Simulator::Schedule(MilliSeconds(100), &AgentOrchestrator::ExecuteHeartbeat, this);
}

} // namespace ns3
