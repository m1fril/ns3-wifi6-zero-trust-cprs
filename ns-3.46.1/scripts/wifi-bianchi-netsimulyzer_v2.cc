#include "ns3/ampdu-subframe-header.h"
#include "ns3/application-container.h"
#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/gnuplot.h"
#include "ns3/integer.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-list.h"
#include "ns3/packet-socket-client.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-server.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/queue-size.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-mac.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-rx-trace-helper.h"
#include "ns3/wifi-tx-stats-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/point-to-point-module.h" 

#include <fstream>
#include <iomanip>
#include <vector>

#ifdef HAS_NETSIMULYZER
#include "ns3/netsimulyzer-module.h"
#include "ns3/pointer.h"
#endif

/// Avoid std::numbers::pi because it's C++20
#define PI 3.1415926535

NS_LOG_COMPONENT_DEFINE("WifiBianchi");

using namespace ns3;

// --- Global Trace/Stats Variables ---
std::ofstream cwTraceFile;     
std::ofstream backoffTraceFile; 
std::ofstream phyTxTraceFile;   
std::ofstream macTxTraceFile;   
std::ofstream macRxTraceFile;   
std::ofstream socketSendTraceFile; 

std::map<Mac48Address, uint64_t> packetsReceived; 
std::map<Mac48Address, uint64_t> bytesReceived; 
std::map<Mac48Address, uint64_t> packetsTransmitted; 
std::map<Mac48Address, uint64_t> psduFailed; 
std::map<Mac48Address, uint64_t> psduSucceeded; 
std::map<Mac48Address, uint64_t> phyHeaderFailed; 
std::map<Mac48Address, uint64_t> rxEventWhileTxing; 
std::map<Mac48Address, uint64_t> rxEventWhileRxing; 
std::map<Mac48Address, uint64_t> rxEventWhileDecodingPreamble; 
std::map<Mac48Address, uint64_t> rxEventAbortedByTx; 
std::map<Mac48Address, Time> timeFirstReceived; 
std::map<Mac48Address, Time> timeLastReceived; 
std::map<Mac48Address, Time> timeFirstTransmitted; 
std::map<Mac48Address, Time> timeLastTransmitted;  
std::set<uint32_t> associated; 

bool tracing = false;    
uint32_t pktSize = 1500; 
uint8_t maxMpdus = 0;    
bool useTxHelper = false; 
bool useRxHelper = false; 

#ifdef HAS_NETSIMULYZER
// Configuration parameters
bool enableVisualization = true;
double guiResolution = 100; // Smoother refresh (100ms)

// Visualizer components
Ptr<netsimulyzer::Orchestrator> orchestrator;
Ptr<netsimulyzer::LogStream> applicationLog;

// Define graphs to visualize */
std::map<uint32_t, Ptr<netsimulyzer::XYSeries>> cwTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::XYSeries>> backoffTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::ThroughputSink>> macTxTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::ThroughputSink>> macRxTraceSeries;
Ptr<netsimulyzer::ThroughputSink> macRxTotalTraceSeries;
Ptr<netsimulyzer::XYSeries> associatedSeries;
#endif

// --- Helper Functions (Context Parsing & Tracing) ---

uint32_t ContextToNodeId(std::string context) {
    std::string sub = context.substr(10);
    uint32_t pos = sub.find("/Device");
    return std::stoi(sub.substr(0, pos));
}

Mac48Address ContextToMac(std::string context) {
    std::string sub = context.substr(10);
    uint32_t pos = sub.find("/Device");
    uint32_t nodeId = std::stoi(sub.substr(0, pos));
    Ptr<Node> n = NodeList::GetNode(nodeId);
    Ptr<WifiNetDevice> d;
    for (uint32_t i = 0; i < n->GetNDevices(); i++) {
        d = n->GetDevice(i)->GetObject<WifiNetDevice>();
        if (d) break;
    }
    return Mac48Address::ConvertFrom(d->GetAddress());
}

void IncrementCounter(std::map<Mac48Address, uint64_t>& counter, Mac48Address addr, uint64_t increment = 1) {
    auto it = counter.find(addr);
    if (it != counter.end()) it->second += increment;
    else counter.insert(std::make_pair(addr, increment));
}

// --- Tracing Callbacks ---

void TracePacketReception(std::string context, Ptr<const Packet> p, uint16_t channelFreqMhz,
                     WifiTxVector txVector, MpduInfo aMpdu, SignalNoiseDbm signalNoise, uint16_t staId) {
    Ptr<Packet> packet = p->Copy();
    if (txVector.IsAggregation()) {
        AmpduSubframeHeader subHdr;
        packet->RemoveHeader(subHdr);
        packet = packet->CreateFragment(0, static_cast<uint32_t>(subHdr.GetLength()));
    }
    WifiMacHeader hdr;
    packet->PeekHeader(hdr);
    if (hdr.GetAddr1() != ContextToMac(context)) return;

    if (packet->GetSize() >= pktSize) {
        IncrementCounter(packetsReceived, hdr.GetAddr2());
        IncrementCounter(bytesReceived, hdr.GetAddr2(), pktSize);
    }
}

void CwTrace(std::string context, uint32_t cw, uint8_t) {
    if (tracing) cwTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " " << cw << std::endl;
}

void BackoffTrace(std::string context, uint32_t newVal, uint8_t) {
    if (tracing) backoffTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " " << newVal << std::endl;
}

void PhyTxTrace(std::string context, Ptr<const Packet> p, double txPowerW) {
    if (tracing) phyTxTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " size=" << p->GetSize() << " " << txPowerW << std::endl;
    if (p->GetSize() >= pktSize) IncrementCounter(packetsTransmitted, ContextToMac(context));
}

void MacTxTrace(std::string context, Ptr<const Packet> p) {
    if (tracing) macTxTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " " << p->GetSize() << std::endl;
#ifdef HAS_NETSIMULYZER
    if (enableVisualization) {
        const auto it = macTxTraceSeries.find(ContextToNodeId(context));
        if (it != macTxTraceSeries.end()) it->second->AddPacketSize(p->GetSize());
    }
#endif
}

void MacRxTrace(std::string context, Ptr<const Packet> p) {
    if (tracing) macRxTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " " << p->GetSize() << std::endl;
#ifdef HAS_NETSIMULYZER
    if (enableVisualization) {
        const auto it = macRxTraceSeries.find(ContextToNodeId(context));
        if (it != macRxTraceSeries.end()) {
             it->second->AddPacketSize(p->GetSize());
             macRxTotalTraceSeries->AddPacketSize(p->GetSize());
        }
    }
#endif
}

void SocketSendTrace(std::string context, Ptr<const Packet> p, const Address& addr) {
    if (tracing) socketSendTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " " << p->GetSize() << " " << addr << std::endl;
}

void AssociationLog(std::string context, Mac48Address address) {
    uint32_t nodeId = ContextToNodeId(context);
    auto it = associated.find(nodeId);
    if (it == associated.end()) {
        associated.insert(it, nodeId);
    }
#ifdef HAS_NETSIMULYZER
    if (enableVisualization && associatedSeries) {
        associatedSeries->Append(Simulator::Now().GetSeconds(), static_cast<double>(associated.size()));
    }
#endif
}

void RestartCalc() {
    bytesReceived.clear();
    packetsReceived.clear();
    packetsTransmitted.clear();
    associated.clear();
}

// --- Experiment Class ---

class Experiment {
  public:
    Experiment();
    int Run(const WifiHelper& wifi, const YansWifiPhyHelper& wifiPhy,
            const WifiMacHelper& wifiMac, const YansWifiChannelHelper& wifiChannel,
            uint32_t trialNumber, uint32_t networkSize, uint32_t nAps,
            Time duration, bool pcap, bool infra, uint16_t guardIntervalNs,
            meter_u distance, dBm_u apTxPower, dBm_u staTxPower, Time pktInterval);
};

Experiment::Experiment() {}

int Experiment::Run(const WifiHelper& helper, const YansWifiPhyHelper& wifiPhy,
                const WifiMacHelper& wifiMac, const YansWifiChannelHelper& wifiChannel,
                uint32_t trialNumber, uint32_t networkSize, uint32_t nAps,
                Time duration, bool pcap, bool infra, uint16_t guardIntervalNs,
                meter_u distance, dBm_u apTxPower, dBm_u staTxPower, Time pktInterval)
{
    RngSeedManager::SetSeed(10);
    RngSeedManager::SetRun(10);

    // 1. Topology Construction
    NodeContainer gatewayNode; gatewayNode.Create(1);
    NodeContainer apNodes;     apNodes.Create(nAps);
    NodeContainer staNodes;    staNodes.Create(networkSize);
    
    NodeContainer allNodes;
    allNodes.Add(gatewayNode);
    allNodes.Add(apNodes);
    allNodes.Add(staNodes);

    // 2. Wired Backhaul (Gateway <-> APs)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer p2pDevices;
    // Connect Gateway to each AP
    for(uint32_t i = 0; i < nAps; i++) {
        p2pDevices.Add(p2p.Install(gatewayNode.Get(0), apNodes.Get(i)));
    }

    // 3. Wireless Access (APs <-> STAs)
    YansWifiPhyHelper phy = wifiPhy;
    phy.SetErrorRateModel("ns3::NistErrorRateModel");
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac = wifiMac;
    const auto& wifi = helper;
    NetDeviceContainer wifiApDevices; 
    NetDeviceContainer wifiStaDevices;

    if (infra) {
        Ssid ssid = Ssid("wifi-bianchi");
        
        // AP Mode
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        phy.Set("TxPowerStart", DoubleValue(apTxPower));
        phy.Set("TxPowerEnd", DoubleValue(apTxPower));
        wifiApDevices = wifi.Install(phy, mac, apNodes);

        // STA Mode
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        phy.Set("TxPowerStart", DoubleValue(staTxPower));
        phy.Set("TxPowerEnd", DoubleValue(staTxPower));
        wifiStaDevices = wifi.Install(phy, mac, staNodes);
    } else {
        mac.SetType("ns3::AdhocWifiMac");
        wifi.Install(phy, mac, allNodes);
    }

    // 4. Mobility
    MobilityHelper mobility;
    
    // Gateway: Static at -20, 0
    Ptr<ListPositionAllocator> gatewayAlloc = CreateObject<ListPositionAllocator>();
    gatewayAlloc->Add(Vector(-20.0, 0.0, 0.0));
    mobility.SetPositionAllocator(gatewayAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gatewayNode);

    // APs: Static, spaced out
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator>();
    double apSpacing = 60.0;
    for(uint32_t i=0; i < nAps; i++) {
        apAlloc->Add(Vector(i * apSpacing, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(apAlloc);
    mobility.Install(apNodes);

    // STAs: Random Walk inside a disc around specific APs
    uint32_t stasPerAp = networkSize / nAps;
    uint32_t staCounter = 0;

    for (uint32_t i = 0; i < nAps; i++) {
        NodeContainer currentApStas;
        uint32_t count = stasPerAp + (i < (networkSize % nAps) ? 1 : 0);
        
        for (uint32_t j = 0; j < count; j++) {
            if(staCounter < networkSize) {
                currentApStas.Add(staNodes.Get(staCounter));
                staCounter++;
            }
        }
        
        if (currentApStas.GetN() > 0) {
            // Place them initially in a disc
            mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
                                           "X", DoubleValue (i * apSpacing),
                                           "Y", DoubleValue (0.0),
                                           "rho", DoubleValue (20.0)); 
            
            // Make them move!
            mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                       "Bounds", RectangleValue (Rectangle (i*apSpacing - 30, i*apSpacing + 30, -30, 30)),
                                       "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                                       "Mode", StringValue ("Distance"));
            mobility.Install(currentApStas);
        }
    }

    // 5. Traffic (PacketSocket)
    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodes);

    // Traffic Setup
    staCounter = 0;
    for (uint32_t i = 0; i < nAps; i++) {
        uint32_t count = stasPerAp + (i < (networkSize % nAps) ? 1 : 0);
        for (uint32_t j = 0; j < count; j++) {
            if(staCounter < networkSize) {
                PacketSocketAddress socketAddr;
                Ptr<Node> sender = staNodes.Get(staCounter);
                Ptr<Node> receiver = apNodes.Get(i); // Sending to AP
                
                // Wifi interface on AP is index 1 (0 is p2p), on STA is index 0
                Ptr<NetDevice> apWifiDev = receiver->GetDevice(1); 
                
                socketAddr.SetSingleDevice(sender->GetDevice(0)->GetIfIndex());
                socketAddr.SetPhysicalAddress(apWifiDev->GetAddress());
                socketAddr.SetProtocol(1);

                Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
                client->SetRemote(socketAddr);
                sender->AddApplication(client);
                client->SetAttribute("PacketSize", UintegerValue(pktSize));
                client->SetAttribute("Interval", TimeValue(pktInterval));
                client->SetStartTime(Seconds(0.5)); 

                Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
                server->SetLocal(socketAddr);
                receiver->AddApplication(server);
                
                staCounter++;
            }
        }
    }

    // Tracing Hooks
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/MonitorSnifferRx", MakeCallback(&TracePacketReception));
    if (infra) Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback(&AssociationLog));
    
    std::string txop = StaticCast<WifiNetDevice>(staNodes.Get(0)->GetDevice(0))->GetMac()->GetQosSupported() ? "BE_Txop" : "Txop";
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::WifiMac/" + txop + "/CwTrace", MakeCallback(&CwTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback(&MacTxTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&MacRxTrace));


    Simulator::Schedule(Seconds(10), &RestartCalc);
    Simulator::Stop(duration); 

#ifdef HAS_NETSIMULYZER
    if (enableVisualization)
    {
        NS_LOG_INFO("Configuring NetSimulyzer...");

        orchestrator = CreateObject<netsimulyzer::Orchestrator>("Bianchi-GatewayDemo.json");
        orchestrator->SetAttribute("MobilityPollInterval", TimeValue(MilliSeconds(guiResolution)));

        netsimulyzer::NodeConfigurationHelper nodeConfigHelper(orchestrator);

        // 1. Gateway Visuals
        nodeConfigHelper.Set("Model", netsimulyzer::models::SERVER_VALUE);
        nodeConfigHelper.Set("Scale", DoubleValue(0.5));
        nodeConfigHelper.Set("Name", StringValue("Gateway")); 
        nodeConfigHelper.Install(gatewayNode);

        // 2. AP Visuals
        nodeConfigHelper.Set("Model", netsimulyzer::models::ROUTER_VALUE);
        nodeConfigHelper.Set("Scale", DoubleValue(0.3));
        for(uint32_t i=0; i < nAps; i++) {
             std::string apName = "AP-" + std::to_string(i);
             nodeConfigHelper.Set("Name", StringValue(apName));
             nodeConfigHelper.Install(apNodes.Get(i));
        }

        // 3. STA Visuals
        nodeConfigHelper.Set("Model", netsimulyzer::models::SMARTPHONE_VALUE);
        nodeConfigHelper.Set("Scale", DoubleValue(0.1));
        for(uint32_t i=0; i < networkSize; i++) {
             nodeConfigHelper.Set("Name", StringValue("STA-" + std::to_string(i)));
             nodeConfigHelper.Install(staNodes.Get(i));
        }
        
        // 4. Wired Links Visualization
        netsimulyzer::LogicalLinkHelper linkHelper(orchestrator);
        linkHelper.Set("Color", netsimulyzer::Color3Value(0, 255, 0)); // Green Wired Links
        
        // Safe manual linking loop
        for(uint32_t i=0; i < nAps; i++) {
             linkHelper.Link(gatewayNode.Get(0), apNodes.Get(i));
        }

        // 5. Graphs setup
        applicationLog = CreateObject<netsimulyzer::LogStream>(orchestrator);
        associatedSeries = CreateObject<netsimulyzer::XYSeries>(orchestrator);
        associatedSeries->SetAttribute("Name", StringValue("Associated STAs"));
        macRxTotalTraceSeries = CreateObject<netsimulyzer::ThroughputSink>(orchestrator, "Total Network Throughput");

        // Per Node Graphs
        for(uint32_t i=0; i < allNodes.GetN(); ++i) {
             Ptr<Node> n = allNodes.Get(i);
             uint32_t id = n->GetId();
             Ptr<netsimulyzer::ThroughputSink> txSink = CreateObject<netsimulyzer::ThroughputSink>(orchestrator, "Tx Throughput");
             macTxTraceSeries.insert(std::pair<uint32_t, Ptr<netsimulyzer::ThroughputSink>>(id, txSink));
             Ptr<netsimulyzer::ThroughputSink> rxSink = CreateObject<netsimulyzer::ThroughputSink>(orchestrator, "Rx Throughput");
             macRxTraceSeries.insert(std::pair<uint32_t, Ptr<netsimulyzer::ThroughputSink>>(id, rxSink));
        }
    } 
#endif

    Simulator::Run();
    Simulator::Destroy();

    if (tracing) {
        cwTraceFile.close();
        backoffTraceFile.close();
        phyTxTraceFile.close();
        macTxTraceFile.close();
        macRxTraceFile.close();
        socketSendTraceFile.close();
    }
    return 0;
}

int main(int argc, char* argv[])
{
    uint32_t nMinStas = 5; 
    uint32_t nMaxStas = 5; 
    uint32_t nStepSize = 5;
    uint32_t nAps = 2;      
    double duration = 30.0; // INCREASED Default Duration
    bool pcap = false;      
    bool infra = true;      
    std::string phyMode = "OfdmRate54Mbps";
    uint16_t guardIntervalNs = 800;
    uint16_t pktInterval = 2000; 
    meter_u distance = 0.001;
    dBm_u apTxPower{16};
    dBm_u staTxPower{16};

    CommandLine cmd(__FILE__);
    cmd.AddValue("nMinStas", "Minimum number of stations", nMinStas);
    cmd.AddValue("nMaxStas", "Maximum number of stations", nMaxStas);
    cmd.AddValue("nAps", "Number of Access Points", nAps);
    cmd.AddValue("visual", "Generate visualization traces", enableVisualization);
    cmd.AddValue("duration", "Simulation duration", duration);
    cmd.Parse(argc, argv);

    Experiment experiment;
    YansWifiPhyHelper wifiPhy;
    wifiPhy.DisablePreambleDetectionModel();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                                   "Exponent", DoubleValue(2.0),
                                   "ReferenceDistance", DoubleValue(1.0),
                                   "ReferenceLoss", DoubleValue(46.6777));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));
    WifiMacHelper wifiMac;
    
    for (uint32_t n = nMinStas; n <= nMaxStas; n += nStepSize) {
        experiment.Run(wifi, wifiPhy, wifiMac, wifiChannel, 0, n, nAps,
                       Seconds(duration), pcap, infra, guardIntervalNs,
                       distance, apTxPower, staTxPower, MicroSeconds(pktInterval));
    }
    return 0;
}