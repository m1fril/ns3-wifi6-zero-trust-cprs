#include "targeted-loss-agent.h"
#include "simulation-environment.h"
#include <iostream>

namespace ns3 {

TargetedLossAgent::TargetedLossAgent(uint32_t sta1, uint32_t sta2, double triggerTime, double lossPowerDbm)
    : m_env(nullptr), m_sta1(sta1), m_sta2(sta2), m_triggerTime(triggerTime), 
      m_lossPowerDbm(lossPowerDbm), m_triggered(false) {}

void TargetedLossAgent::Initialize(ns3::SimulationEnvironment* env) {
    m_env = env;
}

void TargetedLossAgent::Tick(double t) {
    if (!m_env || m_triggered) return;

    if (t >= m_triggerTime) {
        m_env->SetStaTxPower(m_sta1, m_lossPowerDbm);
        m_env->SetStaTxPower(m_sta2, m_lossPowerDbm);
        m_triggered = true;
        std::cout << "[TargetedLossAgent] Triggered loss for STAs " << m_sta1 << " and " << m_sta2 
                  << " at T=" << t << "s with Power=" << m_lossPowerDbm << "dBm" << std::endl;
    }
}

} // namespace ns3
