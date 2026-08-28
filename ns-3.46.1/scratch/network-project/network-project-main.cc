#include "simulation-environment.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/string.h"

using namespace ns3;

int main(int argc, char* argv[]) {
    uint32_t nAps = 1;
    uint32_t nStas = 50;
    uint32_t queueSize = 500;
    // 1500 bytes every 1200 us = 10 Mbit/s nominal offered load per STA.
    uint32_t pktInterval = 1200;
    double errorRate = 0.0;
    bool visual = false;
    double duration = 1000.0;
    double roomSize = 1000.0;
    std::string wifiRate = "OfdmRate12Mbps";
    uint32_t pSize = 1500;
    std::string scenario = "normal";
    uint32_t targetSta1 = 0;
    uint32_t targetSta2 = 1;
    double triggerTime = 20.0;
    bool fixedStaPlacement = false;
    double fixedStaDistance = 0.0;
    double fixedTxPowerDbm = 0.0;
    bool enableNakagamiFading = false;
    bool fixedStaRingPlacement = false;
    bool compactMetrics = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nAps", "Number of Access Points", nAps);
    cmd.AddValue("nStas", "Number of stations", nStas);
    cmd.AddValue("queueSize", "Maximum size of the Wi-Fi MAC Queue", queueSize);
    cmd.AddValue("pktInterval", "Microseconds between packets (default: 10 Mbit/s offered load per STA)", pktInterval);
    cmd.AddValue("errorRate", "Packet error rate for wired backbone", errorRate);
    cmd.AddValue("visual", "Enable NetSimulyzer 3D output", visual);
    cmd.AddValue("duration", "Simulation duration in seconds", duration);
    cmd.AddValue("roomSize", "Size of the room in meters (X and Y bounds)", roomSize);
    cmd.AddValue("wifiRate", "Wi-Fi PHY Data Rate", wifiRate);
    cmd.AddValue("pSize", "Packet payload size in bytes", pSize);
    cmd.AddValue("scenario", "Scenario runbook: normal, controlled, failure, escalation, targeted_loss", scenario);
    cmd.AddValue("targetSta1", "Index of first STA to fail (targeted_loss scenario)", targetSta1);
    cmd.AddValue("targetSta2", "Index of second STA to fail (targeted_loss scenario)", targetSta2);
    cmd.AddValue("triggerTime", "Time in seconds to trigger the loss", triggerTime);
    cmd.AddValue("fixedStaPlacement", "Keep one STA at a fixed distance from one AP", fixedStaPlacement);
    cmd.AddValue("fixedStaDistance", "AP-STA separation in metres for fixed placement", fixedStaDistance);
    cmd.AddValue("fixedTxPowerDbm", "PHY transmit power in dBm for fixed placement", fixedTxPowerDbm);
    cmd.AddValue("enableNakagamiFading", "Enable Nakagami fading for fixed placement", enableNakagamiFading);
    cmd.AddValue("fixedStaRingPlacement", "Place all STAs evenly on a fixed-distance ring around one AP", fixedStaRingPlacement);
    cmd.AddValue("compactMetrics", "Log per-second aggregate QoS metrics instead of one row per packet", compactMetrics);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue(std::to_string(queueSize) + "p"));

    SimulationEnvironment env;
    env.Run(nAps, nStas, queueSize, pktInterval, errorRate, visual, duration, roomSize, wifiRate, pSize, scenario, targetSta1, targetSta2, triggerTime, fixedStaDistance, fixedStaPlacement, fixedTxPowerDbm, enableNakagamiFading, fixedStaRingPlacement, compactMetrics);

    return 0;
}
