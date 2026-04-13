#include "ns3/ampdu-subframe-header.h"
#include "ns3/application-container.h"
#include "ns3/boolean.h"
#include "ns3/command-line.h"
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
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/queue-size.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/tag.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-rx-trace-helper.h"
#include "ns3/wifi-tx-stats-helper.h"
#include "ns3/yans-wifi-helper.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#ifdef HAS_NETSIMULYZER
#include "ns3/netsimulyzer-module.h"
#endif

NS_LOG_COMPONENT_DEFINE("WifiBianchiNetsimulyzer");

using namespace ns3;

class TimestampTag : public Tag
{
  public:
    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::WifiBianchiTimestampTag")
                                .SetParent<Tag>()
                                .SetGroupName("Wifi")
                                .AddConstructor<TimestampTag>();
        return tid;
    }

    virtual TypeId GetInstanceTypeId(void) const
    {
        return GetTypeId();
    }

    virtual uint32_t GetSerializedSize(void) const
    {
        return sizeof(Time);
    }

    virtual void Serialize(TagBuffer i) const
    {
        i.Write((const uint8_t*)&m_timestamp, sizeof(Time));
    }

    virtual void Deserialize(TagBuffer i)
    {
        i.Read((uint8_t*)&m_timestamp, sizeof(Time));
    }

    virtual void Print(std::ostream& os) const
    {
        os << "t=" << m_timestamp;
    }

    void SetTimestamp(Time time)
    {
        m_timestamp = time;
    }

    Time GetTimestamp(void) const
    {
        return m_timestamp;
    }

  private:
    Time m_timestamp;
};

class Experiment
{
  public:
    Experiment();
    ~Experiment();
    int Run(uint32_t nAps,
            uint32_t nStas,
            uint32_t queueSize,
            uint32_t pktIntervalUs,
            double errorRate,
            bool visual,
            double duration,
            double roomSize,
            std::string wifiRate,
            uint32_t pSize);

  private:
    std::ofstream m_csvLogFile;
    std::map<uint32_t, std::string> m_nodeTypes;
    std::map<uint32_t, uint64_t> m_totalRxBytes;
    double m_lastThroughputCalcTime;
    std::set<uint32_t> m_associated;

    NodeContainer m_staNodes;
    double m_duration;

    void CalculateThroughput(double intervalSeconds);
    void ManageLoad(uint32_t activeStas, double currentIntervalUs);
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

Experiment::Experiment()
    : m_lastThroughputCalcTime(0)
{
}

Experiment::~Experiment()
{
    if (m_csvLogFile.is_open())
    {
        m_csvLogFile.close();
    }
}

uint32_t
Experiment::ContextToNodeId(std::string context)
{
    size_t first = context.find("/NodeList/");
    if (first != std::string::npos)
    {
        first += 10;
        size_t second = context.find("/", first);
        if (second != std::string::npos)
        {
            return std::stoi(context.substr(first, second - first));
        }
    }
    return 0;
}

std::string
Experiment::GetNodeType(uint32_t nodeId)
{
    if (m_nodeTypes.find(nodeId) != m_nodeTypes.end())
    {
        return m_nodeTypes[nodeId];
    }
    return "UNKNOWN";
}

void
Experiment::CalculateThroughput(double intervalSeconds)
{
    double now = Simulator::Now().GetSeconds();
    double timeDiff = now - m_lastThroughputCalcTime;

    if (timeDiff > 0 && m_csvLogFile.is_open())
    {
        for (const auto& [nodeId, bytes] : m_totalRxBytes)
        {
            double mbps = (bytes * 8.0) / (timeDiff * 1e6);
            m_csvLogFile << now << "," << nodeId << "," << GetNodeType(nodeId)
                         << ",THROUGHPUT_MBPS," << mbps << "\n";
        }
    }
    m_totalRxBytes.clear();
    m_lastThroughputCalcTime = now;
    Simulator::Schedule(Seconds(intervalSeconds),
                        &Experiment::CalculateThroughput,
                        this,
                        intervalSeconds);
}

void
Experiment::ManageLoad(uint32_t activeStas, double currentIntervalUs)
{
    std::cout << Simulator::Now().GetSeconds() << "s: [LOAD MANAGER] " << activeStas
              << " STAs active. Packet Interval decreased to " << currentIntervalUs << "us."
              << std::endl;

    for (uint32_t i = 0; i < m_staNodes.GetN(); ++i)
    {
        Ptr<Application> app = m_staNodes.Get(i)->GetApplication(0);
        if (app)
        {
            app->SetAttribute("Interval", TimeValue(MicroSeconds(currentIntervalUs)));
        }
    }

    double nextInterval = currentIntervalUs * 0.8;
    uint32_t nextStas = activeStas + 10;

    if (Simulator::Now().GetSeconds() + 100.0 < m_duration)
    {
        Simulator::Schedule(Seconds(100.0), &Experiment::ManageLoad, this, nextStas, nextInterval);
    }
}

void
Experiment::TracePacketReception(std::string context,
                                 Ptr<const Packet> p,
                                 uint16_t channelFreqMhz,
                                 WifiTxVector txVector,
                                 MpduInfo aMpdu,
                                 SignalNoiseDbm signalNoise,
                                 uint16_t staId)
{
    Ptr<Packet> packet = p->Copy();
    uint32_t nId = ContextToNodeId(context);

    if (txVector.IsAggregation())
    {
        AmpduSubframeHeader subHdr;
        packet->RemoveHeader(subHdr);
        packet = packet->CreateFragment(0, static_cast<uint32_t>(subHdr.GetLength()));
    }

    m_totalRxBytes[nId] += packet->GetSize();

    TimestampTag tag;
    if (packet->RemovePacketTag(tag))
    {
        double latencyMs = (Simulator::Now() - tag.GetTimestamp()).GetMilliSeconds();
        if (m_csvLogFile.is_open())
        {
            m_csvLogFile << Simulator::Now().GetSeconds() << "," << nId << "," << GetNodeType(nId)
                         << ",LATENCY_MS," << latencyMs << "\n";
        }
    }

    if (m_csvLogFile.is_open())
    {
        m_csvLogFile << Simulator::Now().GetSeconds() << "," << nId << "," << GetNodeType(nId)
                     << ",RECV,\n";
    }
}

void
Experiment::WifiMacDropTrace(std::string context, Ptr<const Packet> p)
{
    if (m_csvLogFile.is_open())
    {
        uint32_t nId = ContextToNodeId(context);
        m_csvLogFile << Simulator::Now().GetSeconds() << "," << nId << "," << GetNodeType(nId)
                     << ",WIFI_DROP,\n";
    }
}

void
Experiment::P2PRxDrop(std::string context, Ptr<const Packet> p)
{
    if (m_csvLogFile.is_open())
    {
        uint32_t nId = ContextToNodeId(context);
        m_csvLogFile << Simulator::Now().GetSeconds() << "," << nId << "," << GetNodeType(nId)
                     << ",WIRED_DROP,\n";
    }
}

void
Experiment::SocketSendTrace(std::string context, Ptr<const Packet> p, const Address& addr)
{
    TimestampTag tag;
    tag.SetTimestamp(Simulator::Now());
    p->AddPacketTag(tag);

    if (m_csvLogFile.is_open())
    {
        uint32_t nId = ContextToNodeId(context);
        m_csvLogFile << Simulator::Now().GetSeconds() << "," << nId << "," << GetNodeType(nId)
                     << ",SEND,\n";
    }
}

void
Experiment::AssociationLog(std::string context, Mac48Address address)
{
    uint32_t nodeId = ContextToNodeId(context);
    if (m_associated.find(nodeId) == m_associated.end())
    {
        m_associated.insert(nodeId);
        if (m_csvLogFile.is_open())
        {
            m_csvLogFile << Simulator::Now().GetSeconds() << "," << nodeId << ","
                         << GetNodeType(nodeId) << ",JOINED,\n";
        }
    }
}

int
Experiment::Run(uint32_t nAps,
                uint32_t nStas,
                uint32_t queueSize,
                uint32_t pktIntervalUs,
                double errorRate,
                bool visual,
                double duration,
                double roomSize,
                std::string wifiRate,
                uint32_t pSize)
{
    m_duration = duration;
    m_associated.clear();
    m_totalRxBytes.clear();
    m_lastThroughputCalcTime = 0;

    NodeContainer gatewayNode;
    gatewayNode.Create(1);
    NodeContainer apNodes;
    apNodes.Create(nAps);
    m_staNodes.Create(nStas);

    m_nodeTypes.clear();
    m_nodeTypes[gatewayNode.Get(0)->GetId()] = "Gateway";
    for (uint32_t i = 0; i < apNodes.GetN(); i++)
    {
        m_nodeTypes[apNodes.Get(i)->GetId()] = "AP";
    }
    for (uint32_t i = 0; i < m_staNodes.GetN(); i++)
    {
        m_nodeTypes[m_staNodes.Get(i)->GetId()] = "STA";
    }

    NodeContainer allNodes;
    allNodes.Add(gatewayNode);
    allNodes.Add(apNodes);
    allNodes.Add(m_staNodes);

    std::stringstream csvName;
    csvName << "Bianchi-" << nAps << "AP-" << nStas << "STA-" << queueSize << "Q-Events.csv";
    m_csvLogFile.open(csvName.str(), std::ios::out);
    m_csvLogFile << "Time_s,NodeID,NodeType,Action,Value\n";

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
    errorModel->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
    errorModel->SetAttribute("ErrorRate", DoubleValue(errorRate));
    p2p.SetDeviceAttribute("ReceiveErrorModel", PointerValue(errorModel));

    NetDeviceContainer p2pDevices;
    for (uint32_t i = 0; i < nAps; i++)
    {
        p2pDevices.Add(p2p.Install(gatewayNode.Get(0), apNodes.Get(i)));
    }

    YansWifiPhyHelper phy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    phy.SetErrorRateModel("ns3::NistErrorRateModel");
    phy.SetChannel(wifiChannel.Create());

    // --- ЗМІНА 1: МІКРОСТІЛЬНИКИ (Зниження потужності передавача) ---
    // Це створює "бульбашки" покриття і запобігає інтерференції AP один з одним
    phy.Set("TxPowerStart", DoubleValue(18.0));
    phy.Set("TxPowerEnd", DoubleValue(18.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(wifiRate),
                                 "ControlMode",
                                 StringValue(wifiRate));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-test");

    const uint16_t valid5GhzChannels[] = {36, 40, 44, 48, 149, 153, 157, 161, 165};
    const uint16_t numChannels = sizeof(valid5GhzChannels) / sizeof(valid5GhzChannels[0]);

    uint32_t macStaCounter = 0;
    uint32_t macStasPerAp = nStas / nAps;

    for (uint32_t i = 0; i < apNodes.GetN(); i++)
    {
        uint16_t assignedChannel = valid5GhzChannels[i % numChannels];
        std::string channelStr = "{" + std::to_string(assignedChannel) + ", 20, BAND_5GHZ, 0}";
        phy.Set("ChannelSettings", StringValue(channelStr));

        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        wifi.Install(phy, mac, apNodes.Get(i));

        uint32_t count = macStasPerAp + (i < (nStas % nAps) ? 1 : 0);
        NodeContainer currentApStas;
        for (uint32_t j = 0; j < count; j++)
        {
            currentApStas.Add(m_staNodes.Get(macStaCounter++));
        }

        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        wifi.Install(phy, mac, currentApStas);
    }

    MobilityHelper mobility;

    // Gateway Allocation
    Ptr<ListPositionAllocator> gatewayAlloc = CreateObject<ListPositionAllocator>();
    gatewayAlloc->Add(Vector(-20.0, 0.0, 0.0));
    mobility.SetPositionAllocator(gatewayAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gatewayNode);

    // --- ЗМІНА 2: СІТКОВА ТОПОЛОГІЯ (GridPositionAllocator) ---
    // Тепер AP розміщуються на ідеальній відстані один від одного як на шахівниці
    uint32_t gridWidth = std::ceil(std::sqrt(nAps));
    double spacing = roomSize / (gridWidth > 0 ? gridWidth : 1);

    Ptr<GridPositionAllocator> apAlloc = CreateObject<GridPositionAllocator>();
    apAlloc->SetMinX(spacing / 2.0);
    apAlloc->SetMinY(spacing / 2.0);
    apAlloc->SetDeltaX(spacing);
    apAlloc->SetDeltaY(spacing);

    // В ns-3 ширина сітки та тип розкладки задаються через систему атрибутів
    apAlloc->SetAttribute("GridWidth", UintegerValue(gridWidth));
    apAlloc->SetAttribute("LayoutType", StringValue("RowFirst"));

    mobility.SetPositionAllocator(apAlloc);
    mobility.Install(apNodes);

    // STA Allocation (залишається RandomWalk по всій площі)
    // --- ЗМІНА 3: ЗОНОВЕ РОЗМІЩЕННЯ КЛІЄНТІВ ---
    uint32_t staMobilityCounter = 0;
    for (uint32_t i = 0; i < nAps; i++)
    {
        // Отримуємо координати поточної точки доступу
        Ptr<MobilityModel> apMob = apNodes.Get(i)->GetObject<MobilityModel>();
        Vector apPos = apMob->GetPosition();

        uint32_t count = macStasPerAp + (i < (nStas % nAps) ? 1 : 0);
        NodeContainer currentApStas;
        for (uint32_t j = 0; j < count; j++)
        {
            currentApStas.Add(m_staNodes.Get(staMobilityCounter++));
        }

        // Створюємо зону 100х100 метрів навколо цієї AP
        double radius = 75.0;
        double minX = std::max(0.0, apPos.x - radius);
        double maxX = std::min(roomSize, apPos.x + radius);
        double minY = std::max(0.0, apPos.y - radius);
        double maxY = std::min(roomSize, apPos.y + radius);

        MobilityHelper staMobility;
        staMobility.SetPositionAllocator(
            "ns3::RandomRectanglePositionAllocator",
            "X",
            StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minX) +
                        "|Max=" + std::to_string(maxX) + "]"),
            "Y",
            StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minY) +
                        "|Max=" + std::to_string(maxY) + "]"));

        staMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                     "Bounds",
                                     RectangleValue(Rectangle(minX, maxX, minY, maxY)),
                                     "Speed",
                                     StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                                     "Mode",
                                     StringValue("Distance"));
        staMobility.Install(currentApStas);
    }

    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodes);

    uint32_t staCounter = 0;

    for (uint32_t i = 0; i < nAps; i++)
    {
        uint32_t count = macStasPerAp + (i < (nStas % nAps) ? 1 : 0);
        for (uint32_t j = 0; j < count; j++)
        {
            if (staCounter < nStas)
            {
                PacketSocketAddress socketAddr;
                Ptr<Node> sender = m_staNodes.Get(staCounter);
                Ptr<Node> receiver = apNodes.Get(i);
                Ptr<NetDevice> apWifiDev = receiver->GetDevice(1);

                socketAddr.SetSingleDevice(sender->GetDevice(0)->GetIfIndex());
                socketAddr.SetPhysicalAddress(apWifiDev->GetAddress());
                socketAddr.SetProtocol(1);

                Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
                client->SetRemote(socketAddr);
                sender->AddApplication(client);

                client->SetAttribute("PacketSize", UintegerValue(pSize));
                client->SetAttribute("Interval", TimeValue(MicroSeconds(pktIntervalUs)));
                client->SetAttribute("MaxPackets", UintegerValue(99999999));

                double startTime = (staCounter / 10) * 100.0 + 0.5;
                if (startTime >= duration)
                {
                    startTime = duration - 1.0;
                }

                client->SetStartTime(Seconds(startTime));
                client->SetStopTime(Seconds(duration));

                Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
                server->SetLocal(socketAddr);
                receiver->AddApplication(server);

                staCounter++;
            }
        }
    }

    Config::Connect(
        "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/MonitorSnifferRx",
        MakeCallback(&Experiment::TracePacketReception, this));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSocketClient/Tx",
                    MakeCallback(&Experiment::SocketSendTrace, this));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxDrop",
                    MakeCallback(&Experiment::P2PRxDrop, this));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                    MakeCallback(&Experiment::AssociationLog, this));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop",
                    MakeCallback(&Experiment::WifiMacDropTrace, this));

    Simulator::Schedule(Seconds(1.0), &Experiment::CalculateThroughput, this, 1.0);
    Simulator::Schedule(Seconds(100.0), &Experiment::ManageLoad, this, 21, pktIntervalUs * 0.8);

    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

int
main(int argc, char* argv[])
{
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
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMacQueue::MaxSize", StringValue(std::to_string(queueSize) + "p"));

    Experiment experiment;
    experiment.Run(nAps,
                   nStas,
                   queueSize,
                   pktInterval,
                   errorRate,
                   visual,
                   duration,
                   roomSize,
                   wifiRate,
                   pSize);

    return 0;
}
