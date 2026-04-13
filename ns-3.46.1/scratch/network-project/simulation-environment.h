#ifndef SIMULATION_ENVIRONMENT_H
#define SIMULATION_ENVIRONMENT_H

#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/wifi-phy-common.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/nstime.h"
#include "simulation-modes.h"

#include <fstream>
#include <map>
#include <set>
#include <string>

namespace ns3 {

class SimulationEnvironment {
public:
    SimulationEnvironment();
    ~SimulationEnvironment();

    int Run(uint32_t nAps,
            uint32_t nStas,
            uint32_t queueSize,
            uint32_t pktIntervalUs,
            double errorRate,
            bool visual,
            double duration,
            double roomSize,
            std::string wifiRate,
            uint32_t pSize,
            std::string scenario);

    // Agent API
    SimulationMode GetCurrentMode() const { return m_currentMode; }
    void SetCurrentMode(SimulationMode mode) { m_currentMode = mode; }

    void AdjustAllStaLoads(double intervalUs);
    void AdjustAllStaSpeeds(double speed);
    void ToggleApGroupPower(bool turnOn, bool oddGroup);

    // Logging helper
    void LogEvent(const std::string& action, double value = 0.0, uint32_t nodeId = 0);

private:
    SimulationMode m_currentMode = SimulationMode::NORMAL_OPERATION;
    
    std::ofstream m_csvLogFile;
    std::map<uint32_t, std::string> m_nodeTypes;
    std::map<uint32_t, uint64_t> m_totalRxBytes;
    double m_lastThroughputCalcTime;
    std::set<uint32_t> m_associated;

    NodeContainer m_apNodes;
    NodeContainer m_staNodes;
    double m_duration;

    // Internal Traces (can be called by agents if needed, but usually connected via Config::Connect)
    void CalculateThroughput(double intervalSeconds);
    uint32_t ContextToNodeId(std::string context);
    std::string GetNodeType(uint32_t nodeId);

    void TracePacketReception(std::string context,
                               Ptr<const Packet> p,
                               uint16_t channelFreqMhz,
                               WifiTxVector txVector,
                               MpduInfo aMpdu,
                               SignalNoiseDbm signalNoise,
                               uint16_t staId);
    void WifiMacDropTrace(std::string context, Ptr<const Packet> p);
    void P2PRxDrop(std::string context, Ptr<const Packet> p);
    void SocketSendTrace(std::string context, Ptr<const Packet> p, const Address& addr);
    void AssociationLog(std::string context, Mac48Address address);
};

} // namespace ns3

#endif // SIMULATION_ENVIRONMENT_H
