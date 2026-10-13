#ifndef TIMELINE_AGENT_H
#define TIMELINE_AGENT_H

#include "agent-interface.h"

#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace ns3 {

class AgentOrchestrator;

// Executes a timeline file produced by the Python orchestrator, one command per line:
//   <time_s|now> <COMMAND> <args...>
// The whole file is scheduled at start; lines appended later (live control) are
// picked up on every heartbeat. Command reference: sim-orchestrator/README.md.
class TimelineAgent : public IAgent {
public:
    TimelineAgent(const std::string& path, AgentOrchestrator* orchestrator);

    void Initialize(ns3::SimulationEnvironment* env) override;
    void Tick(double currentTime) override;

private:
    void Poll();
    void Execute(const std::string& line);
    // Expands sta:3 | sta:0-9 | sta:* | ap:1 | ap:* | cell:2 | all into (isAp, index) pairs.
    bool Expand(const std::string& target, std::vector<std::pair<bool, uint32_t>>& out) const;

    SimulationEnvironment* m_env;
    AgentOrchestrator* m_orchestrator;
    std::string m_path;
    std::ifstream m_in;
    std::string m_partial;
};

} // namespace ns3

#endif // TIMELINE_AGENT_H
