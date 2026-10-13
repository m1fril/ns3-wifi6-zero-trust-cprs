#ifndef AGENT_ORCHESTRATOR_H
#define AGENT_ORCHESTRATOR_H

#include "agent-interface.h"
#include <csignal>
#include <memory>
#include <string>
#include <vector>

namespace ns3 {

// Set from SIGINT/SIGTERM handlers; the next heartbeat stops the simulator cleanly.
extern volatile std::sig_atomic_t g_stopRequested;

class AgentOrchestrator {
public:
    void RegisterAgent(const std::string& name, std::shared_ptr<IAgent> agent);
    bool SetAgentEnabled(const std::string& name, bool enabled);  // false if unknown
    void ExecuteHeartbeat();
private:
    struct Entry {
        std::string name;
        std::shared_ptr<IAgent> agent;
        bool enabled;
    };
    std::vector<Entry> m_agents;
};

} // namespace ns3

#endif // AGENT_ORCHESTRATOR_H
