#ifndef SIMULATION_ENVIRONMENT_H
#define SIMULATION_ENVIRONMENT_H

#include "simulation-modes.h"
#include "targeted-loss-agent.h"
#include "traffic-app.h"

#include "ns3/application-container.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/mac48-address.h"
#include "ns3/queue-item.h"
#include "ns3/node-container.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-mpdu.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-common.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/yans-wifi-helper.h"

#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ns3
{

class SimulationEnvironment
{
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
            std::string scenario,
            uint32_t targetSta1 = 0,
            uint32_t targetSta2 = 1,
            double triggerTime = 20.0,
            double fixedStaDistance = 0.0,
            bool fixedStaPlacement = false,
            double fixedTxPowerDbm = 0.0,
            bool enableNakagamiFading = false,
            bool fixedStaRingPlacement = false,
            bool compactMetrics = false);

    // Agent API
    SimulationMode GetCurrentMode() const
    {
        return m_currentMode;
    }

    void SetCurrentMode(SimulationMode mode)
    {
        m_currentMode = mode;
    }

    void AdjustAllStaLoads(double intervalUs);
    void AdjustAllStaSpeeds(double speed);
    void ToggleApGroupPower(bool turnOn, bool oddGroup);
    void SetStaTxPower(uint32_t staIdx, double powerDbm);

    // Timeline mode (enabled by a non-empty path before Run): STAs run TrafficApp and
    // TimelineAgent drives churn, traffic, netem and module switches from the file.
    void SetTimelinePath(const std::string& path) { m_timelinePath = path; }
    // Topology (set before Run). "infra": Gateway -p2p- AP -802.11ax- STA cells.
    // "mesh": 802.11s mesh (HWMP routing, 802.11a, UDP/IPv4); the nAps nodes become
    // gateways (traffic sinks) and every STA is a mesh router reporting to its nearest one.
    // layout "corridor": two clusters joined by ONE bridge router (the last STA); the gateways
    // sit in cluster A, clusterB routers form cluster B and reach them only through the bridge.
    void SetTopology(const std::string& topology, const std::string& layout, double spacing, uint32_t clusterB) {
        m_topology = topology; m_meshLayout = layout; m_meshSpacing = spacing; m_clusterB = clusterB;
    }
    bool IsMesh() const { return m_topology == "mesh"; }
    uint32_t GetNumStas() const { return m_staNodes.GetN(); }
    uint32_t GetNumAps() const { return m_apNodes.GetN(); }
    uint32_t GetStaCell(uint32_t staIdx) const { return m_staCell.at(staIdx); }
    uint32_t GetStaNodeId(uint32_t staIdx) const { return m_staNodes.Get(staIdx)->GetId(); }
    Ptr<TrafficApp> GetStaApp(uint32_t staIdx) const;
    void SetNodeUp(bool isAp, uint32_t idx, bool up);

    // Logging helper
    void LogEvent(const std::string& action, double value = 0.0, uint32_t nodeId = 0);

  private:
    SimulationMode m_currentMode = SimulationMode::NORMAL_OPERATION;

    std::ofstream m_csvLogFile;
    std::map<uint32_t, std::string> m_nodeTypes;
    std::map<uint32_t, uint64_t> m_totalRxBytes;
    bool m_compactMetrics = false;
    uint64_t m_intervalSentPackets = 0;
    uint64_t m_intervalReceivedPackets = 0;
    std::vector<double> m_intervalLatenciesMs;
    double m_lastThroughputCalcTime;
    std::set<uint32_t> m_associated;
    std::string m_timelinePath;
    std::vector<uint32_t> m_staCell;  // STA index -> serving AP index
    uint64_t m_intervalNetemDrops = 0;
    bool m_extended = false;           // timeline or mesh mode: TrafficApp + extended metrics
    std::string m_topology = "infra";
    std::string m_meshLayout = "grid";
    double m_meshSpacing = 50.0;
    uint32_t m_clusterB = 0;
    // Per-node counters for the current second (extended mode), keyed by node id
    struct NodeStats {
        uint64_t sent = 0, recv = 0, drops = 0, forwarded = 0;
        double latSum = 0.0, hopSum = 0.0;
    };
    std::map<uint32_t, NodeStats> m_nodeStats;
    uint64_t m_intervalRetryDrops = 0;  // MAC retry limit reached
    uint64_t m_intervalQueueDrops = 0;  // MAC queue overflow / lifetime expiry
    uint64_t m_intervalStackDrops = 0;  // IPv4 / ARP drops (mesh)
    // Mesh hop counting: packet uid -> (MAC transmissions so far, first seen time)
    std::unordered_map<uint64_t, std::pair<uint32_t, double>> m_txHops;
    double m_intervalHopSum = 0.0;
    uint64_t m_intervalHopCount = 0;
    std::map<Mac48Address, uint32_t> m_macToNode;

    NodeContainer m_apNodes;
    NodeContainer m_staNodes;
    double m_duration;

    // Internal Traces (can be called by agents if needed, but usually connected via
    // Config::Connect)
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
    void ServerRxTrace(std::string context, Ptr<const Packet> p, const Address& from);
    void SocketSendTrace(std::string context, Ptr<const Packet> p, const Address& addr);
    void AssociationLog(std::string context, Mac48Address address);
    void DeassociationLog(std::string context, Mac48Address address);
    void NetemDropTrace(std::string context, Ptr<const Packet> p);
    void DroppedMpduTrace(std::string context, WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu);
    void MacTxTrace(std::string context, Ptr<const Packet> p);
    void PeerLinkOpen(std::string context, Mac48Address own, Mac48Address peer);
    void PeerLinkClose(std::string context, Mac48Address own, Mac48Address peer);
    void RouteDiscoveryTrace(std::string context, Time t);
    void Ipv4DropTrace(std::string context, const Ipv4Header& h, Ptr<const Packet> p,
                       Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t iface);
    void QueueDiscDropTrace(std::string context, Ptr<const QueueDiscItem> item);
    void SendFailedTrace(std::string context, Ptr<const Packet> p);
    void SetupMesh(double roomSize, double duration, uint32_t pSize, uint32_t pktIntervalUs);
    void ReportHwmpDrops();
    void CountStackDrop(Ptr<const Packet> p, const char* action);
    bool IsSink(uint32_t nodeId) { const std::string t = GetNodeType(nodeId); return t == "AP" || t == "GW"; }
    // Like LogEvent but writes zero values too (LogEvent leaves them empty)
    void LogValue(const std::string& action, double value, uint32_t nodeId);
};

} // namespace ns3

#endif // SIMULATION_ENVIRONMENT_H
