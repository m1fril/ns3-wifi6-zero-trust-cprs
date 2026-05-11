#include "simulation-environment.h"
#include "agent-orchestrator.h"
#include "director-agent.h"
#include "chaos-agent.h"
#include "linear-escalation-agent.h"
#include "telemetry-agent.h"

#include "ns3/ampdu-subframe-header.h"
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
        if (app) app->SetAttribute("Interval", TimeValue(MicroSeconds(intervalUs)));
    }
}

void SimulationEnvironment::AdjustAllStaSpeeds(double speed) {
    for (uint32_t i = 0; i < m_staNodes.GetN(); ++i) {
        Ptr<MobilityModel> mob = m_staNodes.Get(i)->GetObject<MobilityModel>();
        if (mob) {
            std::string speedStr = "ns3::ConstantRandomVariable[Constant=" + std::to_string(speed) + "]";
            mob->SetAttribute("Speed", StringValue(speedStr));
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
        Ptr<NetDevice> dev = m_staNodes.Get(staIdx)->GetDevice(0);
        Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
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
        for (const auto& [nodeId, bytes] : m_totalRxBytes) {
            double mbps = (bytes * 8.0) / (timeDiff * 1e6);
            LogEvent("THROUGHPUT_MBPS", mbps, nodeId);
        }
    }
    m_totalRxBytes.clear();
    m_lastThroughputCalcTime = now;
    Simulator::Schedule(Seconds(intervalSeconds), &SimulationEnvironment::CalculateThroughput, this, intervalSeconds);
}

void SimulationEnvironment::TracePacketReception(std::string context, Ptr<const Packet> p, uint16_t, WifiTxVector, MpduInfo, SignalNoiseDbm, uint16_t) {
    // Only use for aggregate channel monitoring if needed, otherwise skip to avoid confusion
    // uint32_t nId = ContextToNodeId(context);
    // LogEvent("RECV_SNIFFER", 0, nId);
}

void SimulationEnvironment::ServerRxTrace(std::string context, Ptr<const Packet> p) {
    uint32_t nId = ContextToNodeId(context);
    if (nId != 1) return; // Only AP 1 reception

    TimestampTag tag;
    if (p->PeekPacketTag(tag)) {
        LogEvent("RECV", p->GetUid(), tag.GetSenderId());
    }
}

void SimulationEnvironment::WifiMacDropTrace(std::string context, Ptr<const Packet> p) {
    TimestampTag tag;
    if (p->PeekPacketTag(tag)) {
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
    LogEvent("SEND", p->GetUid(), nId);
}

void SimulationEnvironment::AssociationLog(std::string context, Mac48Address address) {
    uint32_t nodeId = ContextToNodeId(context);
    if (m_associated.find(nodeId) == m_associated.end()) {
        m_associated.insert(nodeId);
        LogEvent("ASSOC", 0, nodeId);
        LogEvent("JOINED", 0, nodeId);
    }
}

int SimulationEnvironment::Run(uint32_t nAps, uint32_t nStas, uint32_t queueSize, uint32_t pktIntervalUs, double errorRate, bool visual, double duration, double roomSize, std::string wifiRate, uint32_t pSize, std::string scenario, uint32_t targetSta1, uint32_t targetSta2, double triggerTime) {
    m_duration = duration;
    m_associated.clear();
    m_totalRxBytes.clear();
    m_lastThroughputCalcTime = 0;

    NodeContainer gatewayNode; gatewayNode.Create(1);
    m_apNodes.Create(nAps);
    m_staNodes.Create(nStas);

    m_nodeTypes.clear();
    m_nodeTypes[gatewayNode.Get(0)->GetId()] = "Gateway";
    for (uint32_t i = 0; i < m_apNodes.GetN(); i++) m_nodeTypes[m_apNodes.Get(i)->GetId()] = "AP";
    for (uint32_t i = 0; i < m_staNodes.GetN(); i++) m_nodeTypes[m_staNodes.Get(i)->GetId()] = "STA";

    NodeContainer allNodes; allNodes.Add(gatewayNode); allNodes.Add(m_apNodes); allNodes.Add(m_staNodes);

    std::stringstream csvName; csvName << "Bianchi-" << nAps << "AP-" << nStas << "STA-" << queueSize << "Q-Events.csv";
    m_csvLogFile.open(csvName.str(), std::ios::out);
    m_csvLogFile << "Time_s,NodeID,NodeType,Action,Value\n";

    PointToPointHelper p2p; p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps")); p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
    errorModel->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
    errorModel->SetAttribute("ErrorRate", DoubleValue(errorRate));
    p2p.SetDeviceAttribute("ReceiveErrorModel", PointerValue(errorModel));

    NetDeviceContainer p2pDevices;
    for (uint32_t i = 0; i < nAps; i++) p2pDevices.Add(p2p.Install(gatewayNode.Get(0), m_apNodes.Get(i)));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy; phy.SetChannel(channel.Create());
    WifiHelper wifi; wifi.SetStandard(WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager("ns3::IdealWifiManager");

    WifiMacHelper mac;
    uint32_t macStaCounter = 0;
    uint32_t macStasPerAp = nStas / nAps;

    for (uint32_t i = 0; i < nAps; i++) {
        std::string ssidStr = "ns3-80211n-" + std::to_string(i);
        Ssid ssid = Ssid(ssidStr);
        
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        wifi.Install(phy, mac, m_apNodes.Get(i));

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(true));
        uint32_t count = macStasPerAp + (i < (nStas % nAps) ? 1 : 0);
        NodeContainer currentApStas;
        for (uint32_t j = 0; j < count; j++) currentApStas.Add(m_staNodes.Get(macStaCounter++));
        wifi.Install(phy, mac, currentApStas);
    }

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> gatewayAlloc = CreateObject<ListPositionAllocator>();
    gatewayAlloc->Add(Vector(-20.0, 0.0, 0.0));
    mobility.SetPositionAllocator(gatewayAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gatewayNode);

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

    PacketSocketHelper packetSocket; packetSocket.Install(allNodes);
    uint32_t staCounter = 0;
    for (uint32_t i = 0; i < nAps; i++) {
        uint32_t count = macStasPerAp + (i < (nStas % nAps) ? 1 : 0);
        for (uint32_t j = 0; j < count; j++) {
            if (staCounter < nStas) {
                PacketSocketAddress socketAddr;
                Ptr<Node> sender = m_staNodes.Get(staCounter); Ptr<Node> receiver = m_apNodes.Get(i);
                Ptr<NetDevice> apWifiDev = receiver->GetDevice(1);
                socketAddr.SetSingleDevice(sender->GetDevice(0)->GetIfIndex());
                socketAddr.SetPhysicalAddress(apWifiDev->GetAddress());
                socketAddr.SetProtocol(1);
                Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
                client->SetRemote(socketAddr); sender->AddApplication(client);
                client->SetAttribute("PacketSize", UintegerValue(pSize));
                client->SetAttribute("Interval", TimeValue(MicroSeconds(pktIntervalUs)));
                client->SetAttribute("MaxPackets", UintegerValue(99999999));
                double startTime = 1.0; 
                client->SetStartTime(Seconds(startTime)); client->SetStopTime(Seconds(duration));
                Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
                server->SetLocal(socketAddr); receiver->AddApplication(server);
                staCounter++;
            }
        }
    }

    Config::Connect("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/PhyRxEnd", MakeCallback(&SimulationEnvironment::ServerRxTrace, this));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSocketClient/Tx", MakeCallback(&SimulationEnvironment::SocketSendTrace, this));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxDrop", MakeCallback(&SimulationEnvironment::P2PRxDrop, this));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback(&SimulationEnvironment::AssociationLog, this));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&SimulationEnvironment::WifiMacDropTrace, this));

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
    orchestrator.RegisterAgent(director);

    // Targeted Loss Agent
    if (scenario == "targeted_loss") {
        auto targetedLoss = std::make_shared<TargetedLossAgent>(targetSta1, targetSta2, triggerTime, -80.0);
        targetedLoss->Initialize(this);
        orchestrator.RegisterAgent(targetedLoss);
    }

    // Escalation Agent
    auto escalation = std::make_shared<LinearEscalationAgent>(
        5000.0, 0.1, pktIntervalUs, 500.0, 2.0, 15.0
    );
    escalation->Initialize(this);
    orchestrator.RegisterAgent(escalation);

    // Chaos Agent
    auto chaosAgent = std::make_shared<ChaosAgent>(0.01, 200000.0, 1.5, pktIntervalUs, 2.0);
    chaosAgent->Initialize(this);
    orchestrator.RegisterAgent(chaosAgent);

    // Telemetry Agent
    auto telemetry = std::make_shared<TelemetryAgent>();
    telemetry->Initialize(this);
    orchestrator.RegisterAgent(telemetry);

    Simulator::Schedule(Seconds(1.0), &AgentOrchestrator::ExecuteHeartbeat, &orchestrator);

    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

} // namespace ns3
