#include "telemetry-agent.h"
#include "simulation-environment.h"

namespace ns3 {

TelemetryAgent::TelemetryAgent() : m_env(nullptr) {}

void TelemetryAgent::Initialize(ns3::SimulationEnvironment* env) {
    m_env = env;
}

void TelemetryAgent::Tick(double currentTime) {
    // This could be used for periodic status prints or metrics export
    // to external tools. For now, throughput is managed inside SimulationEnvironment
    // but we could migrate it here.
}

} // namespace ns3
