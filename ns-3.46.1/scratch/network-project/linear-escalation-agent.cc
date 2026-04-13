#include "linear-escalation-agent.h"
#include "simulation-environment.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace ns3 {

LinearEscalationAgent::LinearEscalationAgent(double intervalDecreaseRatePerSec, double speedIncreaseRatePerSec, 
                                             double baseInterval, double minInterval, 
                                             double baseSpeed, double maxSpeed)
    : m_env(nullptr), m_intervalRate(intervalDecreaseRatePerSec), m_speedRate(speedIncreaseRatePerSec),
      m_baseInterval(baseInterval), m_minInterval(minInterval),
      m_baseSpeed(baseSpeed), m_maxSpeed(maxSpeed),
      m_escalationStartTime(-1.0) {}

void LinearEscalationAgent::Initialize(ns3::SimulationEnvironment* env) {
    m_env = env;
}

void LinearEscalationAgent::Tick(double t) {
    if (!m_env) return;

    if (m_env->GetCurrentMode() == SimulationMode::STRESS_ESCALATION) {
        // Capture the exact moment the phase started to calculate Delta T
        if (m_escalationStartTime < 0.0) {
            m_escalationStartTime = t;
            std::cout << "[ESCALATION AGENT] Stress test initiated at " << t << "s\n";
        }

        double deltaT = t - m_escalationStartTime;

        // 1. Calculate new Interval (Decreasing over time to increase load)
        double targetInterval = m_baseInterval - (m_intervalRate * deltaT);
        double currentIntervalUs = std::max(m_minInterval, targetInterval);

        // 2. Calculate new Speed (Increasing over time)
        double targetSpeed = m_baseSpeed + (m_speedRate * deltaT);
        double currentSpeed = std::min(m_maxSpeed, targetSpeed);

        // 3. Apply to environment
        m_env->AdjustAllStaLoads(currentIntervalUs);
        m_env->AdjustAllStaSpeeds(currentSpeed);
        
        // Optional: Log every 10 seconds to monitor the ramp
        if (std::fmod(t, 10.0) < 0.1) {
            std::cout << "[ESCALATION] t=" << t << "s | Interval: " 
                      << currentIntervalUs << "us | Speed: " << currentSpeed << "m/s\n";
        }
    } else {
        // Reset the timer if we exit the stress test mode
        m_escalationStartTime = -1.0; 
    }
}

} // namespace ns3
