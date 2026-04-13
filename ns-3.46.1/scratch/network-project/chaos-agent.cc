#include "chaos-agent.h"
#include "simulation-environment.h"
#include <iostream>
#include <algorithm>

namespace ns3 {

ChaosAgent::ChaosAgent(double frequency, double loadAmp, double speedAmp, double baseInterval, double baseSpeed)
    : m_env(nullptr), m_freq(frequency), m_loadAmp(loadAmp), m_speedAmp(speedAmp), 
      m_baseInterval(baseInterval), m_baseSpeed(baseSpeed) {}

void ChaosAgent::Initialize(ns3::SimulationEnvironment* env) {
    m_env = env;
}

void ChaosAgent::Tick(double t) {
    if (!m_env) return;

    SimulationMode mode = m_env->GetCurrentMode();
    
    // Only apply sine perturbations in CONTROLLED_LOAD or NETWORK_FAILURE modes
    if (mode != SimulationMode::CONTROLLED_LOAD && mode != SimulationMode::NETWORK_FAILURE) {
        return;
    }

    double sineMod = std::sin(2.0 * M_PI * m_freq * t);
    
    double currentIntervalUs = std::max(1000.0, m_baseInterval + (m_loadAmp * sineMod));
    double currentSpeed = std::max(0.0, m_baseSpeed + (m_speedAmp * sineMod));

    m_env->AdjustAllStaLoads(currentIntervalUs);
    m_env->AdjustAllStaSpeeds(currentSpeed);

    if (mode == SimulationMode::NETWORK_FAILURE) {
        bool failureState = (sineMod < -0.5);
        m_env->ToggleApGroupPower(!failureState, false); // Turn off even APs during failure state
    }
}

} // namespace ns3
