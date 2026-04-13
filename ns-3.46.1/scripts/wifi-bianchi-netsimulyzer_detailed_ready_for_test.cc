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
#include "ns3/error-model.h"

#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <sstream>

#ifdef HAS_NETSIMULYZER
#include "ns3/netsimulyzer-module.h"
#include "ns3/pointer.h"
#endif

#define PI 3.1415926535

NS_LOG_COMPONENT_DEFINE("WifiBianchi");

using namespace ns3;

// --- Global Variables ---
std::ofstream cwTraceFile;     
std::ofstream backoffTraceFile; 
std::ofstream phyTxTraceFile;   
std::ofstream macTxTraceFile;   
std::ofstream macRxTraceFile;   
std::ofstream socketSendTraceFile; 

std::map<Mac48Address, uint64_t> packetsReceived; 
std::map<Mac48Address, uint64_t> bytesReceived; 
std::map<Mac48Address, uint64_t> packetsTransmitted; 
std::set<uint32_t> associated; 

bool tracing = false;    
uint32_t pktSize = 1500; 
uint8_t maxMpdus = 0;    
bool useTxHelper = false; 
bool useRxHelper = false; 

#ifdef HAS_NETSIMULYZER
bool enableVisualization = true;
double guiResolution = 100; 
Ptr<netsimulyzer::Orchestrator> orchestrator;
Ptr<netsimulyzer::LogStream> applicationLog; 

std::map<uint32_t, Ptr<netsimulyzer::XYSeries>> cwTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::ThroughputSink>> macTxTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::ThroughputSink>> macRxTraceSeries;
Ptr<netsimulyzer::ThroughputSink> macRxTotalTraceSeries;
Ptr<netsimulyzer::XYSeries> associatedSeries;
#endif

// --- Helper Functions ---
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
        
        #ifdef HAS_NETSIMULYZER
        if (enableVisualization && applicationLog) {
             std::stringstream ss;
             ss << Simulator::Now().GetSeconds() << "s: [RECV] Node " << ContextToNodeId(context) 
                << " from " << hdr.GetAddr2();
             *applicationLog << ss.str(); 
        }
        #endif
        std::cout << Simulator::Now().GetSeconds() << "s | [WIFI RECV] | Node: " << ContextToNodeId(context) << std::endl;
    }
}

void P2PRxDrop(std::string context, Ptr<const Packet> p) {
    std::stringstream ss;
    ss << Simulator::Now().GetSeconds() << "s: [!!! PACKET LOST !!!] Wired Link Drop at Node " << ContextToNodeId(context);
    
    #ifdef HAS_NETSIMULYZER
    if (enableVisualization && applicationLog) {
         *applicationLog << ss.str(); 
    }
    #endif
    std::cout << "\033[1;31m" << ss.str() << "\033[0m" << std::endl; 
}

void SocketSendTrace(std::string context, Ptr<const Packet> p, const Address& addr) {
    std::stringstream ss;
    ss << Simulator::Now().GetSeconds() << "s: [SEND] App Node " << ContextToNodeId(context) << " -> " << addr;

    #ifdef HAS_NETSIMULYZER
    if (enableVisualization && applicationLog) {
         *applicationLog << ss.str();
    }
    #endif
    std::cout << ss.str() << std::endl;
}

void MacTxTrace(std::string context, Ptr<const Packet> p) {
#ifdef HAS_NETSIMULYZER
    if (enableVisualization) {
        const auto it = macTxTraceSeries.find(ContextToNodeId(context));
        if (it != macTxTraceSeries.end()) it->second->AddPacketSize(p->GetSize());
    }
#endif
}

void MacRxTrace(std::string context, Ptr<const Packet> p) {
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

void AssociationLog(std::string context, Mac48Address address) {
    uint32_t nodeId = ContextToNodeId(context);
    auto it = associated.find(nodeId);
    if (it == associated.end()) {
        associated.insert(it, nodeId);
        std::stringstream ss;
        ss << Simulator::Now().GetSeconds() << "s: [ASSOC] Node " << nodeId << " Joined.";
        #ifdef HAS_NETSIMULYZER
        if (enableVisualization && applicationLog) *applicationLog << ss.str();
        #endif
        std::cout << ss.str() << std::endl;
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
            meter_u distance, dBm_u apTxPower, dBm_u staTxPower, uint32_t pktIntervalUs,
            double errorRate);
};

Experiment::Experiment() {}

int Experiment::Run(const WifiHelper& helper, const YansWifiPhyHelper& wifiPhy,
                const WifiMacHelper& wifiMac, const YansWifiChannelHelper& wifiChannel,
                uint32_t trialNumber, uint32_t networkSize, uint32_t nAps,
                Time duration, bool pcap, bool infra, uint16_t guardIntervalNs,
                meter_u distance, dBm_u apTxPower, dBm_u staTxPower, uint32_t pktIntervalUs,
                double errorRate)
{
    RngSeedManager::SetSeed(10);
    RngSeedManager::SetRun(10);

    NodeContainer gatewayNode; gatewayNode.Create(1);
    NodeContainer apNodes;     apNodes.Create(nAps);
    NodeContainer staNodes;    staNodes.Create(networkSize);
    
    NodeContainer allNodes;
    allNodes.Add(gatewayNode);
    allNodes.Add(apNodes);
    allNodes.Add(staNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel> ();
    errorModel->SetAttribute ("ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
    errorModel->SetAttribute ("ErrorRate", DoubleValue (errorRate));
    p2p.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (errorModel));

    NetDeviceContainer p2pDevices;
    for(uint32_t i = 0; i < nAps; i++) {
        p2pDevices.Add(p2p.Install(gatewayNode.Get(0), apNodes.Get(i)));
    }

    YansWifiPhyHelper phy = wifiPhy;
    phy.SetErrorRateModel("ns3::NistErrorRateModel");
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac = wifiMac;
    const auto& wifi = helper;
    NetDeviceContainer wifiApDevices; 
    NetDeviceContainer wifiStaDevices;

    Ssid ssid = Ssid("wifi-bianchi");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    phy.Set("TxPowerStart", DoubleValue(apTxPower));
    phy.Set("TxPowerEnd", DoubleValue(apTxPower));
    wifiApDevices = wifi.Install(phy, mac, apNodes);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    phy.Set("TxPowerStart", DoubleValue(staTxPower));
    phy.Set("TxPowerEnd", DoubleValue(staTxPower));
    wifiStaDevices = wifi.Install(phy, mac, staNodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> gatewayAlloc = CreateObject<ListPositionAllocator>();
    gatewayAlloc->Add(Vector(-20.0, 0.0, 0.0));
    mobility.SetPositionAllocator(gatewayAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gatewayNode);

    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator>();
    double apSpacing = 60.0;
    for(uint32_t i=0; i < nAps; i++) {
        apAlloc->Add(Vector(i * apSpacing, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(apAlloc);
    mobility.Install(apNodes);

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
            mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator",
                                           "X", DoubleValue (i * apSpacing),
                                           "Y", DoubleValue (0.0),
                                           "rho", DoubleValue (20.0)); 
            mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                       "Bounds", RectangleValue (Rectangle (i*apSpacing - 30, i*apSpacing + 30, -30, 30)),
                                       "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                                       "Mode", StringValue ("Distance"));
            mobility.Install(currentApStas);
        }
    }

    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodes);

    staCounter = 0;
    for (uint32_t i = 0; i < nAps; i++) {
        uint32_t count = stasPerAp + (i < (networkSize % nAps) ? 1 : 0);
        for (uint32_t j = 0; j < count; j++) {
            if(staCounter < networkSize) {
                PacketSocketAddress socketAddr;
                Ptr<Node> sender = staNodes.Get(staCounter);
                Ptr<Node> receiver = apNodes.Get(i); 
                Ptr<NetDevice> apWifiDev = receiver->GetDevice(1); 
                socketAddr.SetSingleDevice(sender->GetDevice(0)->GetIfIndex());
                socketAddr.SetPhysicalAddress(apWifiDev->GetAddress());
                socketAddr.SetProtocol(1);

                Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
                client->SetRemote(socketAddr);
                sender->AddApplication(client);
                client->SetAttribute("PacketSize", UintegerValue(pktSize));
                client->SetAttribute("Interval", TimeValue(MicroSeconds(pktIntervalUs)));
                client->SetStartTime(Seconds(0.5)); 

                Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
                server->SetLocal(socketAddr);
                receiver->AddApplication(server);
                
                staCounter++;
            }
        }
    }

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/MonitorSnifferRx", MakeCallback(&TracePacketReception));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSocketClient/Tx", MakeCallback(&SocketSendTrace)); 
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxDrop", MakeCallback(&P2PRxDrop)); 
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", MakeCallback(&AssociationLog));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback(&MacTxTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&MacRxTrace));

    Simulator::Schedule(Seconds(10), &RestartCalc);
    Simulator::Stop(duration); 

#ifdef HAS_NETSIMULYZER
    if (enableVisualization)
    {
        // CREATE DYNAMIC JSON FILENAME BASED ON PARAMETERS
        std::stringstream jsonName;
        jsonName << "Bianchi-" << nAps << "APs-" << networkSize << "STAs-" << (errorRate * 100) << "Err-" << pktIntervalUs << "Int.json";
        
        std::cout << "\n[INFO] Generating NetSimulyzer Trace File: " << jsonName.str() << std::endl;
        
        orchestrator = CreateObject<netsimulyzer::Orchestrator>(jsonName.str());
        orchestrator->SetAttribute("MobilityPollInterval", TimeValue(MilliSeconds(guiResolution)));

        netsimulyzer::NodeConfigurationHelper nodeConfigHelper(orchestrator);

        nodeConfigHelper.Set("Model", netsimulyzer::models::SERVER_VALUE);
        nodeConfigHelper.Set("Scale", DoubleValue(0.5));
        nodeConfigHelper.Set("Name", StringValue("Gateway")); 
        nodeConfigHelper.Install(gatewayNode);

        nodeConfigHelper.Set("Model", netsimulyzer::models::ROUTER_VALUE);
        nodeConfigHelper.Set("Scale", DoubleValue(0.3));
        for(uint32_t i=0; i < nAps; i++) {
             std::string apName = "AP-" + std::to_string(i);
             nodeConfigHelper.Set("Name", StringValue(apName));
             nodeConfigHelper.Install(apNodes.Get(i));
        }

        nodeConfigHelper.Set("Model", netsimulyzer::models::SMARTPHONE_VALUE);
        nodeConfigHelper.Set("Scale", DoubleValue(0.1));
        for(uint32_t i=0; i < networkSize; i++) {
             nodeConfigHelper.Set("Name", StringValue("STA-" + std::to_string(i)));
             nodeConfigHelper.Install(staNodes.Get(i));
        }
        
        netsimulyzer::LogicalLinkHelper linkHelper(orchestrator);
        linkHelper.Set("Color", netsimulyzer::Color3Value(0, 255, 0)); 
        for(uint32_t i=0; i < nAps; i++) {
             linkHelper.Link(gatewayNode.Get(0), apNodes.Get(i));
        }

        applicationLog = CreateObject<netsimulyzer::LogStream>(orchestrator);
        
        associatedSeries = CreateObject<netsimulyzer::XYSeries>(orchestrator);
        associatedSeries->SetAttribute("Name", StringValue("Associated STAs"));
        macRxTotalTraceSeries = CreateObject<netsimulyzer::ThroughputSink>(orchestrator, "Total Network Throughput");

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

    return 0;
}

int main(int argc, char* argv[])
{
    // UPDATED DEFAULTS: 1 Router, 10 Clients
    uint32_t nMinStas = 10; 
    uint32_t nMaxStas = 10; 
    uint32_t nStepSize = 5;
    uint32_t nAps = 1;      
    double duration = 30.0; 
    bool pcap = false;      
    bool infra = true;      
    std::string phyMode = "OfdmRate54Mbps";
    uint16_t guardIntervalNs = 800;
    
    // EXPOSED TO COMMAND LINE:
    uint32_t pktInterval = 2000; // Microseconds between packets
    double errorRate = 0.0;      // 0.0 to 1.0 (0% to 100%)

    meter_u distance = 0.001;
    dBm_u apTxPower{16};
    dBm_u staTxPower{16};

    CommandLine cmd(__FILE__);
    cmd.AddValue("nMinStas", "Minimum number of stations", nMinStas);
    cmd.AddValue("nMaxStas", "Maximum number of stations", nMaxStas);
    cmd.AddValue("nAps", "Number of Access Points", nAps);
    cmd.AddValue("pktInterval", "Microseconds between packets (lower = higher load)", pktInterval);
    cmd.AddValue("errorRate", "Wired link packet error rate (0.0 to 1.0)", errorRate);
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
                       distance, apTxPower, staTxPower, pktInterval,
                       errorRate);
    }
    return 0;
}