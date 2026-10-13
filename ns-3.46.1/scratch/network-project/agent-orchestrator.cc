#include "agent-orchestrator.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include <iostream>

using namespace ns3;

namespace ns3 {

volatile std::sig_atomic_t g_stopRequested = 0;

void AgentOrchestrator::RegisterAgent(const std::string& name, std::shared_ptr<IAgent> agent) {
    m_agents.push_back({name, agent, true});
}

bool AgentOrchestrator::SetAgentEnabled(const std::string& name, bool enabled) {
    for (auto& e : m_agents) {
        if (e.name == name) {
            e.enabled = enabled;
            return true;
        }
    }
    return false;
}

void AgentOrchestrator::ExecuteHeartbeat() {
    double t = Simulator::Now().GetSeconds();
    if (g_stopRequested) {
        std::cout << "[ORCHESTRATOR] Stop requested by signal at " << t << "s" << std::endl;
        Simulator::Stop();
        return;
    }
    for (auto& e : m_agents) {
        if (e.enabled) e.agent->Tick(t);
    }
    // Schedule next tick (100ms for high resolution chaos)
    Simulator::Schedule(MilliSeconds(100), &AgentOrchestrator::ExecuteHeartbeat, this);
}

} // namespace ns3
