#include "simulation-environment.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/string.h"

using namespace ns3;

int main(int argc, char* argv[]) {
    uint32_t nAps = 1;
    uint32_t nStas = 50;
    uint32_t queueSize = 500;
    uint32_t pktInterval = 500000;
    double errorRate = 0.0;
    bool visual = false;
    double duration = 1000.0;
    double roomSize = 1000.0;
    std::string wifiRate = "OfdmRate12Mbps";
    uint32_t pSize = 1500;
    std::string scenario = "normal";

    CommandLine cmd(__FILE__);
    cmd.AddValue("nAps", "Number of Access Points", nAps);
    cmd.AddValue("nStas", "Number of stations", nStas);
    cmd.AddValue("queueSize", "Maximum size of the Wi-Fi MAC Queue", queueSize);
    cmd.AddValue("pktInterval", "Microseconds between packets", pktInterval);
    cmd.AddValue("errorRate", "Packet error rate for wired backbone", errorRate);
    cmd.AddValue("visual", "Enable NetSimulyzer 3D output", visual);
    cmd.AddValue("duration", "Simulation duration in seconds", duration);
    cmd.AddValue("roomSize", "Size of the room in meters (X and Y bounds)", roomSize);
    cmd.AddValue("wifiRate", "Wi-Fi PHY Data Rate", wifiRate);
    cmd.AddValue("pSize", "Packet payload size in bytes", pSize);
    cmd.AddValue("scenario", "Scenario runbook: normal, controlled, failure, escalation", scenario);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue(std::to_string(queueSize) + "p"));

    SimulationEnvironment env;
    env.Run(nAps, nStas, queueSize, pktInterval, errorRate, visual, duration, roomSize, wifiRate, pSize, scenario);

    return 0;
}
