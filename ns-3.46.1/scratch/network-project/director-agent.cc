#include "director-agent.h"
#include "simulation-environment.h"
#include <iostream>

namespace ns3 {

DirectorAgent::DirectorAgent(const std::vector<Phase>& phases)
    : m_env(nullptr), m_phases(phases), m_currentPhaseIndex(0) {}

void DirectorAgent::Initialize(ns3::SimulationEnvironment* env) {
    m_env = env;
}

void DirectorAgent::Tick(double currentTime) {
    if (!m_env) return;

    if (m_currentPhaseIndex + 1 < m_phases.size()) {
        if (currentTime >= m_phases[m_currentPhaseIndex + 1].startTime) {
            m_currentPhaseIndex++;
            m_env->SetCurrentMode(m_phases[m_currentPhaseIndex].mode);
            std::cout << "[DIRECTOR] Transition to " << m_phases[m_currentPhaseIndex].label 
                      << " at " << currentTime << "s" << std::endl;
        }
    }
}

} // namespace ns3
