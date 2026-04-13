#ifndef AGENT_ORCHESTRATOR_H
#define AGENT_ORCHESTRATOR_H

#include "agent-interface.h"
#include <vector>
#include <memory>

namespace ns3 {

class AgentOrchestrator {
public:
    void RegisterAgent(std::shared_ptr<IAgent> agent);
    void ExecuteHeartbeat();
private:
    std::vector<std::shared_ptr<IAgent>> m_agents;
};

} // namespace ns3

#endif // AGENT_ORCHESTRATOR_H
