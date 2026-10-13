#include "simulation-environment.h"
#include "agent-orchestrator.h"
#include "director-agent.h"
#include "chaos-agent.h"
#include "linear-escalation-agent.h"
#include "telemetry-agent.h"
#include "timeline-agent.h"

#include <algorithm>
#include <cstdlib>

#include "ns3/ampdu-subframe-header.h"
#include "ns3/arp-cache.h"
#include "ns3/hwmp-protocol.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface.h"
#include "ns3/mesh-helper.h"
#include "ns3/mesh-point-device.h"
#include "ns3/packet-sink.h"
#include "ns3/peer-management-protocol.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/boolean.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/error-model.h"
#include "ns3/integer.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-list.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-server.h"
#include "ns3/point-to-point-module.h"
#include "ns3/pointer.h"
#include "ns3/rectangle.h"
#include "ns3/ssid.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/simulator.h"

#include <cmath>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SimulationEnvironment");

class TimestampTag : public Tag {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ns3::SimulationTimestampTag")
                                .SetParent<Tag>()
                                .SetGroupName("Wifi")
                                .AddConstructor<TimestampTag>();
        return tid;
    }
    virtual TypeId GetInstanceTypeId(void) const { return GetTypeId(); }
    virtual uint32_t GetSerializedSize(void) const { return sizeof(Time) + sizeof(uint32_t); }
    virtual void Serialize(TagBuffer i) const { 
        i.Write((const uint8_t*)&m_timestamp, sizeof(Time)); 
        i.WriteU32(m_senderId);
    }
    virtual void Deserialize(TagBuffer i) { 
        i.Read((uint8_t*)&m_timestamp, sizeof(Time)); 
        m_senderId = i.ReadU32();
    }
    virtual void Print(std::ostream& os) const { os << "t=" << m_timestamp << ", sid=" << m_senderId; }
    void SetTimestamp(Time time) { m_timestamp = time; }
    Time GetTimestamp(void) const { return m_timestamp; }
    void SetSenderId(uint32_t id) { m_senderId = id; }
    uint32_t GetSenderId(void) const { return m_senderId; }
private:
    Time m_timestamp;
    uint32_t m_senderId;
};

SimulationEnvironment::SimulationEnvironment() : m_lastThroughputCalcTime(0) {}
SimulationEnvironment::~SimulationEnvironment() {
    if (m_csvLogFile.is_open()) m_csvLogFile.close();
}

void SimulationEnvironment::AdjustAllStaLoads(double intervalUs) {
    for (uint32_t i = 0; i < m_staNodes.GetN(); ++i) {
        Ptr<Application> app = m_staNodes.Get(i)->GetApplication(0);
        if (auto traffic = DynamicCast<TrafficApp>(app)) traffic->SetInterval(Dist::Const(intervalUs));
        else if (app) app->SetAttribute("Interval", TimeValue(MicroSeconds(intervalUs)));
    }
}

void SimulationEnvironment::AdjustAllStaSpeeds(double speed) {
    for (uint32_t i = 0; i < m_staNodes.GetN(); ++i) {
        Ptr<MobilityModel> mob = m_staNodes.Get(i)->GetObject<MobilityModel>();
        if (mob) {
            std::string speedStr = "ns3::ConstantRandomVariable[Constant=" + std::to_string(speed) + "]";
            mob->SetAttributeFailSafe("Speed", StringValue(speedStr));  // static nodes have no Speed
        }
    }
}

void SimulationEnvironment::ToggleApGroupPower(bool turnOn, bool oddGroup) {
    uint32_t startIdx = oddGroup ? 1 : 0;
    for (uint32_t i = startIdx; i < m_apNodes.GetN(); i += 2) {
        Ptr<NetDevice> dev = m_apNodes.Get(i)->GetDevice(1);
        Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
        if (wifiDev) {
            Ptr<WifiPhy> phy = wifiDev->GetPhy();
            double power = turnOn ? 18.0 : -100.0;
            phy->SetAttribute("TxPowerStart", DoubleValue(power));
            phy->SetAttribute("TxPowerEnd", DoubleValue(power));
        }
    }
}

void SimulationEnvironment::SetStaTxPower(uint32_t staIdx, double powerDbm) {
    if (staIdx < m_staNodes.GetN()) {
        Ptr<Node> node = m_staNodes.Get(staIdx);
        Ptr<WifiNetDevice> wifiDev;
        for (uint32_t d = 0; d < node->GetNDevices() && !wifiDev; ++d) {
            wifiDev = DynamicCast<WifiNetDevice>(node->GetDevice(d));
        }
        if (wifiDev) {
            Ptr<WifiPhy> phy = wifiDev->GetPhy();
            phy->SetAttribute("TxPowerStart", DoubleValue(powerDbm));
            phy->SetAttribute("TxPowerEnd", DoubleValue(powerDbm));
            phy->SetAttribute("TxGain", DoubleValue(-100.0)); // Extreme attenuation
            LogEvent("TX_POWER_ADJUST", powerDbm, m_staNodes.Get(staIdx)->GetId());
            std::cout << "Node " << m_staNodes.Get(staIdx)->GetId() << " (STA index " << staIdx 
                      << ") power set to " << powerDbm << " dBm and TxGain to -100" << std::endl;
        }
    }
}

Ptr<TrafficApp> SimulationEnvironment::GetStaApp(uint32_t staIdx) const {
    Ptr<Node> node = m_staNodes.Get(staIdx);
    return node->GetNApplications() ? DynamicCast<TrafficApp>(node->GetApplication(0)) : nullptr;
}

// Node churn: switches every Wi-Fi PHY of the node off/on; a STA also pauses its traffic.
void SimulationEnvironment::SetNodeUp(bool isAp, uint32_t idx, bool up) {
    Ptr<Node> node = (isAp ? m_apNodes : m_staNodes).Get(idx);
    for (uint32_t d = 0; d < node->GetNDevices(); ++d) {
        if (auto wifi = DynamicCast<WifiNetDevice>(node->GetDevice(d))) {
            Ptr<WifiPhy> phy = wifi->GetPhy();
            if (up) phy->ResumeFromOff();
            else if (!phy->IsStateOff()) phy->SetOffMode();
        }
    }
    if (!isAp) {
        if (Ptr<TrafficApp> app = GetStaApp(idx)) app->SetNodeUp(up);
    }
    LogEvent(up ? "NODE_UP" : "NODE_DOWN", 0, node->GetId());
}

void SimulationEnvironment::LogValue(const std::string& action, double value, uint32_t nodeId) {
    if (m_csvLogFile.is_open()) {
        m_csvLogFile << Simulator::Now().GetSeconds() << "," << nodeId << "," << GetNodeType(nodeId)
                     << "," << action << "," << value << "\n";
    }
}

void SimulationEnvironment::LogEvent(const std::string& action, double value, uint32_t nodeId) {
    if (m_csvLogFile.is_open()) {
        m_csvLogFile << Simulator::Now().GetSeconds() << "," << nodeId << "," << GetNodeType(nodeId)
                     << "," << action << "," << (value != 0.0 ? std::to_string(value) : "") << "\n";
    }
}

uint32_t SimulationEnvironment::ContextToNodeId(std::string context) {
    size_t first = context.find("/NodeList/");
    if (first != std::string::npos) {
        first += 10;
        size_t second = context.find("/", first);
        if (second != std::string::npos) return std::stoi(context.substr(first, second - first));
    }
    return 0;
}

std::string SimulationEnvironment::GetNodeType(uint32_t nodeId) {
    if (m_nodeTypes.find(nodeId) != m_nodeTypes.end()) return m_nodeTypes[nodeId];
    return "UNKNOWN";
}

void SimulationEnvironment::CalculateThroughput(double intervalSeconds) {
    double now = Simulator::Now().GetSeconds();
    double timeDiff = now - m_lastThroughputCalcTime;
    if (timeDiff > 0 && m_csvLogFile.is_open()) {
        double aggregateMbps = 0.0;
        for (const auto& [nodeId, bytes] : m_totalRxBytes) {
            double mbps = (bytes * 8.0) / (timeDiff * 1e6);
            LogEvent("THROUGHPUT_MBPS", mbps, nodeId);
            aggregateMbps += mbps;
        }
        if (!m_totalRxBytes.empty() && m_apNodes.GetN() > 0) {
            LogEvent("AGGREGATE_THROUGHPUT_MBPS", aggregateMbps, m_apNodes.Get(0)->GetId());
        }
        if (m_extended && m_apNodes.GetN() > 0) {
            const uint32_t sinkId = m_apNodes.Get(0)->GetId();
            uint32_t active = 0;
            for (uint32_t i = 0; i < m_staNodes.GetN(); ++i) {
                if (Ptr<TrafficApp> app = GetStaApp(i); app && app->IsNodeUp()) ++active;
            }
            LogValue("ACTIVE_STAS", active, sinkId);
            if (m_compactMetrics) {
                LogValue("NETEM_DROP_COUNT", m_intervalNetemDrops, sinkId);
                LogValue("RETRY_DROP_COUNT", m_intervalRetryDrops, sinkId);
                LogValue("QUEUE_DROP_COUNT", m_intervalQueueDrops, sinkId);
                if (IsMesh()) LogValue("STACK_DROP_COUNT", m_intervalStackDrops, sinkId);
            }
            if (IsMesh() && m_intervalHopCount > 0) {
                LogValue("HOPS_MEAN", m_intervalHopSum / m_intervalHopCount, sinkId);
            }
            if (m_compactMetrics) {  // the state of every node this second
                for (const auto& [id, n] : m_nodeStats) {
                    if (n.sent || n.recv) {
                        LogValue("NODE_SENT", n.sent, id);
                        LogValue("NODE_RECV", n.recv, id);
                    }
                    if (n.drops) LogValue("NODE_DROP", n.drops, id);
                    if (n.recv) LogValue("NODE_LAT_MS", n.latSum / n.recv, id);
                    if (n.recv && IsMesh()) LogValue("NODE_HOPS", n.hopSum / n.recv, id);
                    if (n.forwarded) LogValue("NODE_FWD", n.forwarded, id);
                }
            }
        }
        if (m_compactMetrics && m_apNodes.GetN() > 0) {
            const uint32_t apNodeId = m_apNodes.Get(0)->GetId();
            LogEvent("SENT_COUNT", static_cast<double>(m_intervalSentPackets), apNodeId);
            LogEvent("RECV_COUNT", static_cast<double>(m_intervalReceivedPackets), apNodeId);
            if (!m_intervalLatenciesMs.empty()) {
                std::sort(m_intervalLatenciesMs.begin(), m_intervalLatenciesMs.end());
                const size_t index = static_cast<size_t>(0.95 * (m_intervalLatenciesMs.size() - 1));
                LogEvent("LATENCY_P95_MS", m_intervalLatenciesMs[index], apNodeId);
            }
        }
    }
    m_totalRxBytes.clear();
    m_intervalSentPackets = 0;
    m_intervalReceivedPackets = 0;
    m_intervalLatenciesMs.clear();
    m_intervalNetemDrops = 0;
    m_intervalRetryDrops = 0;
    m_intervalQueueDrops = 0;
    m_intervalStackDrops = 0;
    m_intervalHopSum = 0.0;
    m_intervalHopCount = 0;
    m_nodeStats.clear();
    // packets that never arrived would stay in the hop table forever
    for (auto it = m_txHops.begin(); it != m_txHops.end();) {
        it = now - it->second.second > 30.0 ? m_txHops.erase(it) : std::next(it);
    }
    m_lastThroughputCalcTime = now;
    Simulator::Schedule(Seconds(intervalSeconds), &SimulationEnvironment::CalculateThroughput, this, intervalSeconds);
}

void SimulationEnvironment::TracePacketReception(std::string context, Ptr<const Packet> p, uint16_t, WifiTxVector, MpduInfo, SignalNoiseDbm, uint16_t) {
    // Only use for aggregate channel monitoring if needed, otherwise skip to avoid confusion
    // uint32_t nId = ContextToNodeId(context);
    // LogEvent("RECV_SNIFFER", 0, nId);
}

void SimulationEnvironment::ServerRxTrace(std::string context, Ptr<const Packet> p, const Address&) {
    uint32_t nodeId = ContextToNodeId(context);
    if (!m_extended && nodeId != 1) return; // Only AP 1 reception
    if (m_extended && !IsSink(nodeId)) return; // Timeline/mesh mode: every AP or gateway

    TimestampTag tag;
    if (p->PeekPacketTag(tag)) {
        m_totalRxBytes[tag.GetSenderId()] += p->GetSize();
        double latency = (Simulator::Now() - tag.GetTimestamp()).GetSeconds() * 1000.0;
        NodeStats& ns = m_nodeStats[tag.GetSenderId()];
        ++ns.recv;
        ns.latSum += latency;
        if (IsMesh()) {
            if (auto it = m_txHops.find(p->GetUid()); it != m_txHops.end()) {
                m_intervalHopSum += it->second.first;
                ++m_intervalHopCount;
                ns.hopSum += it->second.first;
                if (!m_compactMetrics) LogEvent("HOPS", it->second.first, tag.GetSenderId());
                m_txHops.erase(it);
            }
        }
        if (m_compactMetrics) {
            ++m_intervalReceivedPackets;
            m_intervalLatenciesMs.push_back(latency);
        } else {
            LogEvent("LATENCY_MS", latency, tag.GetSenderId());
            LogEvent("RECV", p->GetUid(), tag.GetSenderId());
        }
    }
}

void SimulationEnvironment::WifiMacDropTrace(std::string context, Ptr<const Packet> p) {
    TimestampTag tag;
    if (p->PeekPacketTag(tag)) {
        if (m_extended) ++m_nodeStats[tag.GetSenderId()].drops;
        LogEvent("WIFI_DROP", 0, tag.GetSenderId());
    } else {
        LogEvent("WIFI_DROP", 0, ContextToNodeId(context));
    }
}

void SimulationEnvironment::P2PRxDrop(std::string context, Ptr<const Packet> p) {
    TimestampTag tag;
    if (p->PeekPacketTag(tag)) {
        LogEvent("WIRED_DROP", 0, tag.GetSenderId());
    } else {
        LogEvent("WIRED_DROP", 0, ContextToNodeId(context));
    }
}

void SimulationEnvironment::SocketSendTrace(std::string context, Ptr<const Packet> p, const Address& addr) {
    uint32_t nId = ContextToNodeId(context);
    TimestampTag tag;
    tag.SetTimestamp(Simulator::Now());
    tag.SetSenderId(nId);
    p->AddPacketTag(tag);
    if (m_extended) ++m_nodeStats[nId].sent;
    if (m_compactMetrics) {
        ++m_intervalSentPackets;
    } else {
        LogEvent("SEND", p->GetUid(), nId);
    }
}

void SimulationEnvironment::AssociationLog(std::string context, Mac48Address address) {
    uint32_t nodeId = ContextToNodeId(context);
    if (m_associated.find(nodeId) == m_associated.end()) {
        m_associated.insert(nodeId);
        LogEvent("ASSOC", 0, nodeId);
        LogEvent("JOINED", 0, nodeId);
    } else if (m_extended) {
        LogEvent("REASSOC", 0, nodeId);
    }
}

void SimulationEnvironment::DeassociationLog(std::string context, Mac48Address) {
    LogEvent("DEASSOC", 0, ContextToNodeId(context));
}

void SimulationEnvironment::NetemDropTrace(std::string context, Ptr<const Packet> p) {
    ++m_nodeStats[ContextToNodeId(context)].drops;
    if (m_compactMetrics) {
        ++m_intervalNetemDrops;
    } else {
        LogEvent("NETEM_DROP", p->GetUid(), ContextToNodeId(context));
    }
}

// Data MPDUs the MAC gave up on (retry limit) or could not keep (queue full / lifetime).
// MacTxDrop covers the earlier refusal to enqueue (e.g. STA not associated), so no overlap.
void SimulationEnvironment::DroppedMpduTrace(std::string context, WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu) {
    TimestampTag tag;
    if (!mpdu->GetPacket()->PeekPacketTag(tag)) return;  // management / control frame
    const bool retry = reason == WIFI_MAC_DROP_REACHED_RETRY_LIMIT;
    ++m_nodeStats[tag.GetSenderId()].drops;
    if (m_compactMetrics) {
        ++(retry ? m_intervalRetryDrops : m_intervalQueueDrops);
    } else {
        LogEvent(retry ? "RETRY_DROP" : "QUEUE_DROP", mpdu->GetPacket()->GetUid(), tag.GetSenderId());
    }
}

// Every hop hands the data packet to a Wi-Fi MAC once (WifiNetDevice::SendFrom), so the number
// of MacTx calls per packet uid is its hop count when it reaches the gateway.
void SimulationEnvironment::MacTxTrace(std::string context, Ptr<const Packet> p) {
    TimestampTag tag;
    if (!p->PeekPacketTag(tag)) return;
    auto [it, fresh] = m_txHops.try_emplace(p->GetUid(), 0, Simulator::Now().GetSeconds());
    ++it->second.first;
    const uint32_t node = ContextToNodeId(context);
    if (node != tag.GetSenderId()) ++m_nodeStats[node].forwarded;  // relayed for someone else
}

void SimulationEnvironment::PeerLinkOpen(std::string context, Mac48Address, Mac48Address peer) {
    LogValue("PEER_LINK_OPEN", m_macToNode.count(peer) ? m_macToNode[peer] : 0, ContextToNodeId(context));
}

void SimulationEnvironment::PeerLinkClose(std::string context, Mac48Address, Mac48Address peer) {
    LogValue("PEER_LINK_CLOSE", m_macToNode.count(peer) ? m_macToNode[peer] : 0, ContextToNodeId(context));
}

void SimulationEnvironment::RouteDiscoveryTrace(std::string context, Time t) {
    LogValue("ROUTE_DISCOVERY_MS", t.GetSeconds() * 1000.0, ContextToNodeId(context));
}

void SimulationEnvironment::CountStackDrop(Ptr<const Packet> p, const char* action) {
    TimestampTag tag;
    if (!p->PeekPacketTag(tag)) return;
    ++m_nodeStats[tag.GetSenderId()].drops;
    if (m_compactMetrics) ++m_intervalStackDrops;
    else LogEvent(action, p->GetUid(), tag.GetSenderId());
}

void SimulationEnvironment::Ipv4DropTrace(std::string, const Ipv4Header&, Ptr<const Packet> p,
                                          Ipv4L3Protocol::DropReason, Ptr<Ipv4>, uint32_t) {
    CountStackDrop(p, "IP_DROP");
}

// The IPv4 helper puts the default queue disc (FqCoDel) on the mesh device; under overload it drops
void SimulationEnvironment::QueueDiscDropTrace(std::string, Ptr<const QueueDiscItem> item) {
    CountStackDrop(item->GetPacket(), "QDISC_DROP");
}

void SimulationEnvironment::SendFailedTrace(std::string, Ptr<const Packet> p) {
    CountStackDrop(p, "SEND_FAIL");
}

// HWMP drops packets without a trace source (no route after PREQ retries, full queue, TTL), and
// peer management silently discards unicast data from a neighbour whose peer link is no longer
// active on the receiving side (the sender already got its MAC ACK). Both only keep counters,
// exported once at the end, per node.
void SimulationEnvironment::ReportHwmpDrops() {
    for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i) {
        Ptr<Node> node = NodeList::GetNode(i);
        for (uint32_t d = 0; d < node->GetNDevices(); ++d) {
            auto mp = DynamicCast<MeshPointDevice>(node->GetDevice(d));
            if (!mp) continue;
            auto sum = [](const std::string& r, const std::string& key) {  // key="N", every occurrence
                double n = 0;
                const std::string pat = key + "=\"";
                for (auto pos = r.find(pat); pos != std::string::npos; pos = r.find(pat, pos + 1)) {
                    if (pos == 0 || !std::isalnum(static_cast<unsigned char>(r[pos - 1]))) {
                        n += std::stod(r.substr(pos + pat.size()));
                    }
                }
                return n;
            };
            std::ostringstream hwmp, pmp;
            mp->GetObject<dot11s::HwmpProtocol>()->Report(hwmp);
            mp->GetObject<dot11s::PeerManagementProtocol>()->Report(pmp);
            if (const char* path = std::getenv("MESH_REPORT")) {  // full dot11s stats for debugging
                std::ofstream(path, std::ios::app) << "<node id=\"" << node->GetId() << "\">\n"
                                                   << hwmp.str() << pmp.str() << "</node>\n";
            }
            const std::pair<const char*, double> counters[] = {
                {"HWMP_DROPPED", sum(hwmp.str(), "totalDropped")},
                {"HWMP_DROPPED_TTL", sum(hwmp.str(), "droppedTtl")},
                {"PEER_FILTER_DROPPED", sum(pmp.str(), "dropped")}};
            for (const auto& [action, n] : counters) {
                if (n > 0) LogValue(action, n, node->GetId());
            }
        }
    }
}

void SimulationEnvironment::SetupMesh(double roomSize, double duration, uint32_t pSize, uint32_t pktIntervalUs) {
    const uint32_t nGw = m_apNodes.GetN(), nSta = m_staNodes.GetN(), total = nGw + nSta;
    for (uint32_t i = 0; i < nGw; ++i) m_nodeTypes[m_apNodes.Get(i)->GetId()] = "GW";
    for (uint32_t i = 0; i < nSta; ++i) m_nodeTypes[m_staNodes.Get(i)->GetId()] = "MESH";

    // Placement: square grid with gateways spread over its slots, or uniform random in the room
    std::vector<Vector> gwPos, staPos;
    if (m_meshLayout == "random") {
        auto rv = CreateObject<UniformRandomVariable>();
        rv->SetAttribute("Max", DoubleValue(roomSize));
        for (uint32_t i = 0; i < total; ++i) {
            Vector v(rv->GetValue(), rv->GetValue(), 0.0);
            (i < nGw ? gwPos : staPos).push_back(v);
        }
    } else if (m_meshLayout == "corridor") {
        // cluster A (gateways + routers) | bridge router | cluster B; the clusters are 2 x gap
        // apart (> radio range), so the bridge is their only link
        const double s = m_meshSpacing, gap = 35.0;
        const uint32_t nB = std::min(m_clusterB, nSta - 1), nA = nSta - 1 - nB, slotsA = nGw + nA;
        const auto colsA = static_cast<uint32_t>(std::ceil(std::sqrt(slotsA)));
        const auto colsB = static_cast<uint32_t>(std::ceil(std::sqrt(std::max(1u, nB))));
        const double xA = (colsA - 1) * s, yMid = ((slotsA + colsA - 1) / colsA - 1) * s / 2.0;
        const double rowsB = (nB + colsB - 1) / colsB, yB = yMid - (rowsB - 1) * s / 2.0;
        std::set<uint32_t> gwSlots;
        for (uint32_t k = 0; k < nGw; ++k) {
            gwSlots.insert(nGw == 1 ? slotsA / 2 : static_cast<uint32_t>(std::lround(k * (slotsA - 1.0) / (nGw - 1))));
        }
        for (uint32_t slot = 0; slot < slotsA; ++slot) {
            Vector v((slot % colsA) * s, (slot / colsA) * s, 0.0);
            (gwSlots.count(slot) ? gwPos : staPos).push_back(v);
        }
        for (uint32_t b = 0; b < nB; ++b) {
            staPos.push_back(Vector(xA + 2 * gap + (b % colsB) * s, yB + (b / colsB) * s, 0.0));
        }
        staPos.push_back(Vector(xA + gap, yMid, 0.0));  // the bridge = last STA
    } else {
        const auto cols = static_cast<uint32_t>(std::ceil(std::sqrt(total)));
        std::set<uint32_t> gwSlots;
        for (uint32_t k = 0; k < nGw; ++k) {
            gwSlots.insert(nGw == 1 ? total / 2 : static_cast<uint32_t>(std::lround(k * (total - 1.0) / (nGw - 1))));
        }
        for (uint32_t slot = 0; slot < total; ++slot) {
            Vector v((slot % cols) * m_meshSpacing, (slot / cols) * m_meshSpacing, 0.0);
            (gwSlots.count(slot) ? gwPos : staPos).push_back(v);
        }
    }
    Ptr<ListPositionAllocator> alloc = CreateObject<ListPositionAllocator>();
    for (const auto& v : gwPos) alloc->Add(v);
    for (const auto& v : staPos) alloc->Add(v);
    NodeContainer meshNodes(m_apNodes, m_staNodes);
    MobilityHelper mobility;
    mobility.SetPositionAllocator(alloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // A node's "cell" is its nearest gateway: the sink of its traffic
    for (uint32_t s = 0; s < nSta; ++s) {
        uint32_t best = 0;
        for (uint32_t g = 1; g < nGw; ++g) {
            if (CalculateDistance(staPos[s], gwPos[g]) < CalculateDistance(staPos[s], gwPos[best])) best = g;
        }
        m_staCell.push_back(best);
    }

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStandard(WIFI_STANDARD_80211a);  // ns-3 mesh interfaces are non-HT only
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::ZERO_CHANNEL);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetRemoteStationManager("ns3::IdealWifiManager");  // same rate control as infra mode
    mesh.SetNumberOfInterfaces(1);
    NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer ifaces = ipv4.Assign(meshDevices);

    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        Ptr<Node> node = meshNodes.Get(i);
        for (uint32_t d = 0; d < node->GetNDevices(); ++d) {
            Address a = node->GetDevice(d)->GetAddress();
            if (Mac48Address::IsMatchingType(a)) m_macToNode[Mac48Address::ConvertFrom(a)] = node->GetId();
        }
    }
    // Static ARP for the gateways (provisioned like on real mesh routers): keeps ARP floods
    // out of the measurements. Data flows only node -> gateway, so nothing else is resolved.
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        Ptr<Ipv4L3Protocol> ip = meshNodes.Get(i)->GetObject<Ipv4L3Protocol>();
        Ptr<ArpCache> cache = ip->GetInterface(ip->GetInterfaceForDevice(meshDevices.Get(i)))->GetArpCache();
        for (uint32_t g = 0; g < nGw; ++g) {
            ArpCache::Entry* entry = cache->Add(ifaces.GetAddress(g));
            entry->SetMacAddress(meshDevices.Get(g)->GetAddress());
            entry->MarkAutoGenerated();
        }
    }

    const uint16_t port = 9;
    for (uint32_t g = 0; g < nGw; ++g) {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        Ptr<Application> sink = sinkHelper.Install(m_apNodes.Get(g)).Get(0);
        sink->TraceConnect("Rx", "/NodeList/" + std::to_string(m_apNodes.Get(g)->GetId()) + "/",
                           MakeCallback(&SimulationEnvironment::ServerRxTrace, this));
    }
    const uint32_t maxPayload = meshDevices.Get(0)->GetMtu() - 28;  // IPv4 (20) + UDP (8) headers
    for (uint32_t s = 0; s < nSta; ++s) {
        Ptr<TrafficApp> app = CreateObject<TrafficApp>();
        app->SetRemote(InetSocketAddress(ifaces.GetAddress(m_staCell[s]), port), "ns3::UdpSocketFactory", maxPayload);
        app->SetSize(Dist::Const(pSize));
        app->SetInterval(Dist::Const(pktIntervalUs));
        m_staNodes.Get(s)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(duration));
    }

    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        Vector v = meshNodes.Get(i)->GetObject<MobilityModel>()->GetPosition();
        LogValue("POS_X", v.x, meshNodes.Get(i)->GetId());
        LogValue("POS_Y", v.y, meshNodes.Get(i)->GetId());
    }
}

int SimulationEnvironment::Run(uint32_t nAps, uint32_t nStas, uint32_t queueSize, uint32_t pktIntervalUs, double errorRate, bool visual, double duration, double roomSize, std::string wifiRate, uint32_t pSize, std::string scenario, uint32_t targetSta1, uint32_t targetSta2, double triggerTime, double fixedStaDistance, bool fixedStaPlacement, double fixedTxPowerDbm, bool enableNakagamiFading, bool fixedStaRingPlacement, bool compactMetrics) {
    m_duration = duration;
    m_associated.clear();
    m_totalRxBytes.clear();
    m_compactMetrics = compactMetrics;
    m_intervalSentPackets = 0;
    m_intervalReceivedPackets = 0;
    m_intervalLatenciesMs.clear();
    m_intervalNetemDrops = 0;
    m_lastThroughputCalcTime = 0;
    m_staCell.clear();
    m_txHops.clear();
    m_macToNode.clear();
    const bool timelineMode = !m_timelinePath.empty();
    m_extended = timelineMode || IsMesh();

    NodeContainer gatewayNode; gatewayNode.Create(1);
    m_apNodes.Create(nAps);
    m_staNodes.Create(nStas);

    m_nodeTypes.clear();
    m_nodeTypes[gatewayNode.Get(0)->GetId()] = "Gateway";
    for (uint32_t i = 0; i < m_apNodes.GetN(); i++) m_nodeTypes[m_apNodes.Get(i)->GetId()] = "AP";
    for (uint32_t i = 0; i < m_staNodes.GetN(); i++) m_nodeTypes[m_staNodes.Get(i)->GetId()] = "STA";
    if (IsMesh()) m_nodeTypes[gatewayNode.Get(0)->GetId()] = "UNUSED";  // keeps node ids aligned with infra

    NodeContainer allNodes; allNodes.Add(gatewayNode); allNodes.Add(m_apNodes); allNodes.Add(m_staNodes);

    std::stringstream csvName; csvName << "Bianchi-" << nAps << "AP-" << nStas << "STA-" << queueSize << "Q-Events.csv";
    m_csvLogFile.open(csvName.str(), std::ios::out);
    m_csvLogFile << "Time_s,NodeID,NodeType,Action,Value\n";

    if (IsMesh()) {
        SetupMesh(roomSize, duration, pSize, pktIntervalUs);
    } else {
        PointToPointHelper p2p; p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps")); p2p.SetChannelAttribute("Delay", StringValue("2ms"));
        Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
        errorModel->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
        errorModel->SetAttribute("ErrorRate", DoubleValue(errorRate));
        p2p.SetDeviceAttribute("ReceiveErrorModel", PointerValue(errorModel));

        NetDeviceContainer p2pDevices;
        for (uint32_t i = 0; i < nAps; i++) p2pDevices.Add(p2p.Install(gatewayNode.Get(0), m_apNodes.Get(i)));

        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        if (fixedStaPlacement && enableNakagamiFading) {
            channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
        }
        Ptr<YansWifiChannel> wifiChannel = channel.Create();
        YansWifiPhyHelper apPhy;
        YansWifiPhyHelper staPhy;
        if (fixedStaPlacement && fixedTxPowerDbm > 0.0) {
            apPhy.Set("TxPowerStart", DoubleValue(fixedTxPowerDbm));
            apPhy.Set("TxPowerEnd", DoubleValue(fixedTxPowerDbm));
            staPhy.Set("TxPowerStart", DoubleValue(fixedTxPowerDbm));
            staPhy.Set("TxPowerEnd", DoubleValue(fixedTxPowerDbm));
        }
        apPhy.Set("ChannelSettings", StringValue("{42, 80, BAND_5GHZ, 0}"));
        apPhy.Set("Antennas", UintegerValue(4));
        apPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
        apPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));
        apPhy.SetChannel(wifiChannel);
        staPhy.Set("ChannelSettings", StringValue("{42, 80, BAND_5GHZ, 0}"));
        staPhy.Set("Antennas", UintegerValue(2));
        staPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue(2));
        staPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue(2));
        staPhy.SetChannel(wifiChannel);
        WifiHelper wifi; wifi.SetStandard(WIFI_STANDARD_80211ax);
        wifi.SetRemoteStationManager("ns3::IdealWifiManager");

        WifiMacHelper mac;
        uint32_t macStaCounter = 0;
        uint32_t macStasPerAp = nStas / nAps;

        for (uint32_t i = 0; i < nAps; i++) {
            std::string ssidStr = "ns3-80211n-" + std::to_string(i);
            Ssid ssid = Ssid(ssidStr);
        
            mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
            wifi.Install(apPhy, mac, m_apNodes.Get(i));

            mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(true));
            uint32_t count = macStasPerAp + (i < (nStas % nAps) ? 1 : 0);
            NodeContainer currentApStas;
            for (uint32_t j = 0; j < count; j++) {
                currentApStas.Add(m_staNodes.Get(macStaCounter++));
                m_staCell.push_back(i);
            }
            wifi.Install(staPhy, mac, currentApStas);
        }

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> gatewayAlloc = CreateObject<ListPositionAllocator>();
        gatewayAlloc->Add(Vector(-20.0, 0.0, 0.0));
        mobility.SetPositionAllocator(gatewayAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(gatewayNode);

        if (fixedStaPlacement && nAps == 1 && fixedStaDistance > 0.0 && (nStas == 1 || fixedStaRingPlacement)) {
            Ptr<ListPositionAllocator> fixedApAlloc = CreateObject<ListPositionAllocator>();
            fixedApAlloc->Add(Vector(0.0, 0.0, 0.0));
            mobility.SetPositionAllocator(fixedApAlloc);
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(m_apNodes);

            MobilityHelper fixedStaMobility;
            Ptr<ListPositionAllocator> fixedStaAlloc = CreateObject<ListPositionAllocator>();
            constexpr double twoPi = 6.28318530717958647692;
            for (uint32_t staIndex = 0; staIndex < nStas; ++staIndex) {
                const double angle = fixedStaRingPlacement ? twoPi * staIndex / nStas : 0.0;
                fixedStaAlloc->Add(Vector(fixedStaDistance * std::cos(angle), fixedStaDistance * std::sin(angle), 0.0));
            }
            fixedStaMobility.SetPositionAllocator(fixedStaAlloc);
            fixedStaMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            fixedStaMobility.Install(m_staNodes);
            for (uint32_t staIndex = 0; staIndex < nStas; ++staIndex) {
                LogEvent("FIXED_STA_DISTANCE", fixedStaDistance, m_staNodes.Get(staIndex)->GetId());
            }
            if (fixedTxPowerDbm > 0.0) {
                LogEvent("FIXED_TX_POWER_DBM", fixedTxPowerDbm, m_staNodes.Get(0)->GetId());
            }
            if (enableNakagamiFading) {
                LogEvent("NAKAGAMI_FADING", 1.0, m_staNodes.Get(0)->GetId());
            }
        } else {
            uint32_t gridWidth = std::ceil(std::sqrt(nAps));
            double spacing = roomSize / (gridWidth > 0 ? gridWidth : 1);
            Ptr<GridPositionAllocator> apAlloc = CreateObject<GridPositionAllocator>();
            apAlloc->SetMinX(spacing / 2.0); apAlloc->SetMinY(spacing / 2.0);
            apAlloc->SetDeltaX(spacing); apAlloc->SetDeltaY(spacing);
            apAlloc->SetAttribute("GridWidth", UintegerValue(gridWidth));
            apAlloc->SetAttribute("LayoutType", StringValue("RowFirst"));
            mobility.SetPositionAllocator(apAlloc);
            mobility.Install(m_apNodes);

            uint32_t staMobilityCounter = 0;
            for (uint32_t i = 0; i < nAps; i++) {
                Ptr<MobilityModel> apMob = m_apNodes.Get(i)->GetObject<MobilityModel>();
                Vector apPos = apMob->GetPosition();
                uint32_t count = macStasPerAp + (i < (nStas % nAps) ? 1 : 0);
                NodeContainer currentApStas;
                for (uint32_t j = 0; j < count; j++) currentApStas.Add(m_staNodes.Get(staMobilityCounter++));
                double radius = spacing / 2.0;
                double minX = std::max(0.0, apPos.x - radius); double maxX = std::min(roomSize, apPos.x + radius);
                double minY = std::max(0.0, apPos.y - radius); double maxY = std::min(roomSize, apPos.y + radius);
                MobilityHelper staMobility;
                staMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator", "X", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minX) + "|Max=" + std::to_string(maxX) + "]"), "Y", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minY) + "|Max=" + std::to_string(maxY) + "]"));
                staMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue(Rectangle(minX, maxX, minY, maxY)), "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"), "Mode", StringValue("Distance"));
                staMobility.Install(currentApStas);
            }
        }

        PacketSocketHelper packetSocket; packetSocket.Install(allNodes);
        uint32_t staCounter = 0;
        for (uint32_t i = 0; i < nAps; i++) {
            uint32_t count = macStasPerAp + (i < (nStas % nAps) ? 1 : 0);
            Ptr<Node> receiver = m_apNodes.Get(i);
            Ptr<NetDevice> apWifiDev = receiver->GetDevice(1);
            Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
            PacketSocketAddress serverAddr;
            serverAddr.SetSingleDevice(apWifiDev->GetIfIndex());
            serverAddr.SetProtocol(1);
            server->SetLocal(serverAddr);
            receiver->AddApplication(server);
            server->TraceConnect(
                "Rx",
                "/NodeList/" + std::to_string(receiver->GetId()) + "/",
                MakeCallback(&SimulationEnvironment::ServerRxTrace, this));
            for (uint32_t j = 0; j < count; j++) {
                if (staCounter < nStas) {
                    PacketSocketAddress socketAddr;
                    Ptr<Node> sender = m_staNodes.Get(staCounter);
                    socketAddr.SetSingleDevice(sender->GetDevice(0)->GetIfIndex());
                    socketAddr.SetPhysicalAddress(apWifiDev->GetAddress());
                    socketAddr.SetProtocol(1);
                    if (timelineMode) {
                        Ptr<TrafficApp> app = CreateObject<TrafficApp>();
                        app->SetRemote(socketAddr, "ns3::PacketSocketFactory");
                        app->SetSize(Dist::Const(pSize));
                        app->SetInterval(Dist::Const(pktIntervalUs));
                        sender->AddApplication(app);
                        app->SetStartTime(Seconds(1.0)); app->SetStopTime(Seconds(duration));
                        staCounter++;
                        continue;
                    }
                    Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
                    client->SetRemote(socketAddr); sender->AddApplication(client);
                    client->SetAttribute("PacketSize", UintegerValue(pSize));
                    client->SetAttribute("Interval", TimeValue(MicroSeconds(pktIntervalUs)));
                    client->SetAttribute("MaxPackets", UintegerValue(99999999));
                    double startTime = 1.0; 
                    client->SetStartTime(Seconds(startTime)); client->SetStopTime(Seconds(duration));
                    staCounter++;
                }
            }
        }
    }

    // Config::Connect is fatal when nothing matches, so every path is gated by the topology
    if (!m_extended) {
        Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSocketClient/Tx", MakeCallback(&SimulationEnvironment::SocketSendTrace, this));
    }
    if (!IsMesh()) {
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxDrop", MakeCallback(&SimulationEnvironment::P2PRxDrop, this));
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback(&SimulationEnvironment::AssociationLog, this));
    }
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&SimulationEnvironment::WifiMacDropTrace, this));
    if (m_extended) {
        Config::Connect("/NodeList/*/ApplicationList/*/$ns3::NetworkProjectTrafficApp/Tx", MakeCallback(&SimulationEnvironment::SocketSendTrace, this));
        Config::Connect("/NodeList/*/ApplicationList/*/$ns3::NetworkProjectTrafficApp/NetemDrop", MakeCallback(&SimulationEnvironment::NetemDropTrace, this));
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/DroppedMpdu", MakeCallback(&SimulationEnvironment::DroppedMpduTrace, this));
        for (uint32_t i = 0; i < m_staNodes.GetN(); ++i) {
            LogValue("STA_CELL", m_staCell[i], m_staNodes.Get(i)->GetId());
        }
    }
    if (m_extended && !IsMesh()) {
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc", MakeCallback(&SimulationEnvironment::DeassociationLog, this));
    }
    if (IsMesh()) {
        const std::string mp = "/NodeList/*/DeviceList/*/$ns3::MeshPointDevice/";
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback(&SimulationEnvironment::MacTxTrace, this));
        Config::Connect(mp + "$ns3::dot11s::PeerManagementProtocol/LinkOpen", MakeCallback(&SimulationEnvironment::PeerLinkOpen, this));
        Config::Connect(mp + "$ns3::dot11s::PeerManagementProtocol/LinkClose", MakeCallback(&SimulationEnvironment::PeerLinkClose, this));
        Config::Connect(mp + "$ns3::dot11s::HwmpProtocol/RouteDiscoveryTime", MakeCallback(&SimulationEnvironment::RouteDiscoveryTrace, this));
        Config::Connect("/NodeList/*/$ns3::Ipv4L3Protocol/Drop", MakeCallback(&SimulationEnvironment::Ipv4DropTrace, this));
        Config::ConnectFailSafe("/NodeList/*/$ns3::TrafficControlLayer/RootQueueDiscList/*/Drop", MakeCallback(&SimulationEnvironment::QueueDiscDropTrace, this));
        Config::Connect("/NodeList/*/ApplicationList/*/$ns3::NetworkProjectTrafficApp/SendFailed", MakeCallback(&SimulationEnvironment::SendFailedTrace, this));
    }

    Simulator::Schedule(Seconds(1.0), &SimulationEnvironment::CalculateThroughput, this, 1.0);

    // Agent Orchestrator Setup
    AgentOrchestrator orchestrator;
    
    // Director Agent
    std::vector<DirectorAgent::Phase> scenarioPlan;
    if (scenario == "controlled") {
        scenarioPlan = {
            {0.0,   SimulationMode::NORMAL_OPERATION,  "Baseline"},
            {20.0,  SimulationMode::CONTROLLED_LOAD,   "Controlled_Load_Chaos"}
        };
    } else if (scenario == "failure") {
        scenarioPlan = {
            {0.0,   SimulationMode::NORMAL_OPERATION,  "Baseline"},
            {20.0,  SimulationMode::NETWORK_FAILURE,   "Network_Instability"}
        };
    } else if (scenario == "escalation") {
        scenarioPlan = {
            {0.0,   SimulationMode::NORMAL_OPERATION,  "Baseline"},
            {20.0,  SimulationMode::STRESS_ESCALATION, "Stress_Ramp"}
        };
    } else { // "normal" or default
        scenarioPlan = {
            {0.0,   SimulationMode::NORMAL_OPERATION,  "Baseline"}
        };
    }
    
    auto director = std::make_shared<DirectorAgent>(scenarioPlan);
    director->Initialize(this);
    orchestrator.RegisterAgent("director", director);

    // Targeted Loss Agent
    if (scenario == "targeted_loss") {
        auto targetedLoss = std::make_shared<TargetedLossAgent>(targetSta1, targetSta2, triggerTime, -80.0);
        targetedLoss->Initialize(this);
        orchestrator.RegisterAgent("targeted_loss", targetedLoss);
    }

    // Escalation Agent
    auto escalation = std::make_shared<LinearEscalationAgent>(
        5000.0, 0.1, pktIntervalUs, 500.0, 2.0, 15.0
    );
    escalation->Initialize(this);
    orchestrator.RegisterAgent("escalation", escalation);

    // Chaos Agent
    auto chaosAgent = std::make_shared<ChaosAgent>(0.01, 200000.0, 1.5, pktIntervalUs, 2.0);
    chaosAgent->Initialize(this);
    orchestrator.RegisterAgent("chaos", chaosAgent);

    // Telemetry Agent
    auto telemetry = std::make_shared<TelemetryAgent>();
    telemetry->Initialize(this);
    orchestrator.RegisterAgent("telemetry", telemetry);

    // Timeline Agent (schedules file commands; re-polls the file on every heartbeat)
    if (timelineMode) {
        auto timeline = std::make_shared<TimelineAgent>(m_timelinePath, &orchestrator);
        timeline->Initialize(this);
        orchestrator.RegisterAgent("timeline", timeline);
    }

    Simulator::Schedule(Seconds(1.0), &AgentOrchestrator::ExecuteHeartbeat, &orchestrator);

    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    if (IsMesh()) ReportHwmpDrops();
    Simulator::Destroy();
    return 0;
}

} // namespace ns3
