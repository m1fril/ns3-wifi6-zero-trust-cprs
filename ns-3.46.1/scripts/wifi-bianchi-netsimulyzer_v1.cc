
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
#include "ns3/point-to-point-module.h" // Added for Gateway links

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

std::ofstream cwTraceFile;      ///< File that traces CW over time
std::ofstream backoffTraceFile; ///< File that traces backoff over time
std::ofstream phyTxTraceFile;   ///< File that traces PHY transmissions  over time
std::ofstream macTxTraceFile;   ///< File that traces MAC transmissions  over time
std::ofstream macRxTraceFile;   ///< File that traces MAC receptions  over time
std::ofstream
    socketSendTraceFile; ///< File that traces packets transmitted by the application  over time

std::map<Mac48Address, uint64_t> packetsReceived; ///< Map that stores the total packets received
                                                  ///< per STA (and addressed to that STA)
std::map<Mac48Address, uint64_t>
    bytesReceived; ///< Map that stores the total bytes received per STA (and addressed to that STA)
std::map<Mac48Address, uint64_t>
    packetsTransmitted; ///< Map that stores the total packets transmitted per STA
std::map<Mac48Address, uint64_t>
    psduFailed; ///< Map that stores the total number of unsuccessfuly received PSDUS (for which
                ///< the PHY header was successfully received)  per STA (including PSDUs not
                ///< addressed to that STA)
std::map<Mac48Address, uint64_t>
    psduSucceeded; ///< Map that stores the total number of successfully received PSDUs per STA
                   ///< (including PSDUs not addressed to that STA)
std::map<Mac48Address, uint64_t> phyHeaderFailed; ///< Map that stores the total number of
                                                  ///< unsuccessfuly received PHY headers per STA
std::map<Mac48Address, uint64_t>
    rxEventWhileTxing; ///< Map that stores the number of reception events per STA that occurred
                       ///< while PHY was already transmitting a PPDU
std::map<Mac48Address, uint64_t>
    rxEventWhileRxing; ///< Map that stores the number of reception events per STA that occurred
                       ///< while PHY was already receiving a PPDU
std::map<Mac48Address, uint64_t>
    rxEventWhileDecodingPreamble; ///< Map that stores the number of reception events per STA that
                                  ///< occurred while PHY was already decoding a preamble
std::map<Mac48Address, uint64_t>
    rxEventAbortedByTx; ///< Map that stores the number of reception events aborted per STA because
                        ///< the PHY has started to transmit

std::map<Mac48Address, Time>
    timeFirstReceived; ///< Map that stores the time at which the first packet was received per STA
                       ///< (and the packet is addressed to that STA)
std::map<Mac48Address, Time>
    timeLastReceived; ///< Map that stores the time at which the last packet was received per STA
                      ///< (and the packet is addressed to that STA)
std::map<Mac48Address, Time> timeFirstTransmitted; ///< Map that stores the time at which the first
                                                   ///< packet was transmitted per STA
std::map<Mac48Address, Time> timeLastTransmitted;  ///< Map that stores the time at which the last
                                                   ///< packet was transmitted per STA

std::set<uint32_t> associated; ///< Contains the IDs of the STAs that successfully associated to the
                               ///< access point (in infrastructure mode only)

bool tracing = false;    ///< Flag to enable/disable generation of tracing files
uint32_t pktSize = 1500; ///< packet size used for the simulation (in bytes)
uint8_t maxMpdus = 0;    ///< The maximum number of MPDUs in A-MPDUs (0 to disable MPDU aggregation)
bool useTxHelper = false; ///< Flag to get MPDU statistics with WifiTxStatsHelper
bool useRxHelper = false; ///< Flag to get PPDU statistics with WifiPhyRxTraceHelper

#ifdef HAS_NETSIMULYZER
// Configuration parameters
bool enableVisualization = true;
double guiResolution = 200; // refresh time in ms

// Visualizer components
Ptr<netsimulyzer::Orchestrator> orchestrator;
Ptr<netsimulyzer::LogStream> applicationLog;

// Utility function to automatically add time to log messages
void
WriteApplicationLog(std::string message)
{
    *applicationLog << "At " << Simulator::Now().GetSeconds() << " " << message;
}

// Define graphs to visualize */
std::map<uint32_t, Ptr<netsimulyzer::XYSeries>> cwTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::XYSeries>> backoffTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::ThroughputSink>> macTxTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::ThroughputSink>> macRxTraceSeries;
std::map<uint32_t, Ptr<netsimulyzer::SeriesCollection>> macCollections;

Ptr<netsimulyzer::ThroughputSink> macRxTotalTraceSeries;

Ptr<netsimulyzer::XYSeries> associatedSeries;

#endif

// ... (Bianchi Results Maps omitted for brevity, logic remains same) ...

/**
 * Parse context strings of the form "/NodeList/x/DeviceList/x/..." to extract the NodeId integer
 *
 * @param context The context to parse.
 * @return the NodeId
 */
uint32_t
ContextToNodeId(std::string context)
{
    std::string sub = context.substr(10);
    uint32_t pos = sub.find("/Device");
    return std::stoi(sub.substr(0, pos));
}

/**
 * Parse context strings of the form "/NodeList/x/DeviceList/x/..." and fetch the Mac address
 *
 * @param context The context to parse.
 * @return the device MAC address
 */
Mac48Address
ContextToMac(std::string context)
{
    std::string sub = context.substr(10);
    uint32_t pos = sub.find("/Device");
    uint32_t nodeId = std::stoi(sub.substr(0, pos));
    Ptr<Node> n = NodeList::GetNode(nodeId);
    Ptr<WifiNetDevice> d;
    for (uint32_t i = 0; i < n->GetNDevices(); i++)
    {
        d = n->GetDevice(i)->GetObject<WifiNetDevice>();
        if (d)
        {
            break;
        }
    }
    return Mac48Address::ConvertFrom(d->GetAddress());
}

// Functions for tracing.

/**
 * Increment the counter for a given address.
 *
 * @param [out] counter The counter to increment.
 * @param addr The address to increment the counter for.
 * @param increment The increment (1 if omitted).
 */
void
IncrementCounter(std::map<Mac48Address, uint64_t>& counter,
                 Mac48Address addr,
                 uint64_t increment = 1)
{
    auto it = counter.find(addr);
    if (it != counter.end())
    {
        it->second += increment;
    }
    else
    {
        counter.insert(std::make_pair(addr, increment));
    }
}

/**
 * Trace a packet reception.
 *
 * @param context The context.
 * @param p The packet.
 * @param channelFreqMhz The channel frequqncy.
 * @param txVector The TX vector.
 * @param aMpdu The AMPDU.
 * @param signalNoise The signal and noise dBm.
 * @param staId The STA ID.
 */
void
TracePacketReception(std::string context,
                     Ptr<const Packet> p,
                     uint16_t channelFreqMhz,
                     WifiTxVector txVector,
                     MpduInfo aMpdu,
                     SignalNoiseDbm signalNoise,
                     uint16_t staId)
{
    Ptr<Packet> packet = p->Copy();
    if (txVector.IsAggregation())
    {
        AmpduSubframeHeader subHdr;
        uint32_t extractedLength;
        packet->RemoveHeader(subHdr);
        extractedLength = subHdr.GetLength();
        packet = packet->CreateFragment(0, static_cast<uint32_t>(extractedLength));
    }
    WifiMacHeader hdr;
    packet->PeekHeader(hdr);
    // hdr.GetAddr1() is the receiving MAC address
    if (hdr.GetAddr1() != ContextToMac(context))
    {
        return;
    }
    // hdr.GetAddr2() is the sending MAC address
    if (packet->GetSize() >= pktSize) // ignore non-data frames
    {
        IncrementCounter(packetsReceived, hdr.GetAddr2());
        IncrementCounter(bytesReceived, hdr.GetAddr2(), pktSize);
        auto itTimeFirstReceived = timeFirstReceived.find(hdr.GetAddr2());
        if (itTimeFirstReceived == timeFirstReceived.end())
        {
            timeFirstReceived.insert(std::make_pair(hdr.GetAddr2(), Simulator::Now()));
        }
        auto itTimeLastReceived = timeLastReceived.find(hdr.GetAddr2());
        if (itTimeLastReceived != timeLastReceived.end())
        {
            itTimeLastReceived->second = Simulator::Now();
        }
        else
        {
            timeLastReceived.insert(std::make_pair(hdr.GetAddr2(), Simulator::Now()));
        }
    }
}

/**
 * Contention window trace.
 *
 * @param context The context.
 * @param cw The contention window.
 */
void
CwTrace(std::string context, uint32_t cw, uint8_t /* linkId */)
{
    // Logging CW Trace (Logic same as original)
    if (tracing)
    {
        cwTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " " << cw
                    << std::endl;
    }
}

/**
 * Backoff trace.
 *
 * @param context The context.
 * @param newVal The backoff value.
 */
void
BackoffTrace(std::string context, uint32_t newVal, uint8_t /* linkId */)
{
    if (tracing)
    {
        backoffTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " "
                         << newVal << std::endl;
    }
}

/**
 * PHY Rx trace.
 *
 * @param context The context.
 * @param p The packet.
 * @param power The Rx power.
 */
void
PhyRxTrace(std::string context, Ptr<const Packet> p, RxPowerWattPerChannelBand power)
{
    // Minimal log
}

/**
 * PHY Rx trace.
 *
 * @param context The context.
 * @param txVector The TX vector.
 * @param psduDuration The PDSU diration.
 */
void
PhyRxPayloadTrace(std::string context, WifiTxVector txVector, Time psduDuration)
{
   // Minimal Log
}

/**
 * PHY Drop trace.
 *
 * @param context The context.
 * @param p The packet.
 * @param reason The drop reason.
 */
void
PhyRxDropTrace(std::string context, Ptr<const Packet> p, WifiPhyRxfailureReason reason)
{
    Mac48Address addr = ContextToMac(context);
    switch (reason)
    {
    case UNSUPPORTED_SETTINGS:
        break;
    case CHANNEL_SWITCHING:
        break;
    case BUSY_DECODING_PREAMBLE: {
        if (p->GetSize() >= pktSize) // ignore non-data frames
        {
            IncrementCounter(rxEventWhileDecodingPreamble, addr);
        }
        break;
    }
    case RXING: {
        if (p->GetSize() >= pktSize) // ignore non-data frames
        {
            IncrementCounter(rxEventWhileRxing, addr);
        }
        break;
    }
    case TXING: {
        if (p->GetSize() >= pktSize) // ignore non-data frames
        {
            IncrementCounter(rxEventWhileTxing, addr);
        }
        break;
    }
    case SLEEPING:
        break;
    case PREAMBLE_DETECT_FAILURE:
        break;
    case RECEPTION_ABORTED_BY_TX: {
        if (p->GetSize() >= pktSize) // ignore non-data frames
        {
            IncrementCounter(rxEventAbortedByTx, addr);
        }
        break;
    }
    case L_SIG_FAILURE: {
        if (p->GetSize() >= pktSize) // ignore non-data frames
        {
            IncrementCounter(phyHeaderFailed, addr);
        }
        break;
    }
    default:
        break;
    }
}

/**
 * PHY RX end trace
 *
 * @param context The context.
 * @param p The packet.
 */
void
PhyRxDoneTrace(std::string context, Ptr<const Packet> p)
{
}

/**
 * PHY successful RX trace
 *
 * @param context The context.
 * @param p The packet.
 * @param snr The SNR.
 * @param mode The WiFi mode.
 * @param preamble The preamble.
 */
void
PhyRxOkTrace(std::string context,
             Ptr<const Packet> p,
             double snr,
             WifiMode mode,
             WifiPreamble preamble)
{
    if (p->GetSize() >= pktSize) // ignore non-data frames
    {
        Mac48Address addr = ContextToMac(context);
        IncrementCounter(psduSucceeded, addr);
    }
}

/**
 * PHY RX error trace
 *
 * @param context The context.
 * @param p The packet.
 * @param snr The SNR.
 */
void
PhyRxErrorTrace(std::string context, Ptr<const Packet> p, double snr)
{
    if (p->GetSize() >= pktSize) // ignore non-data frames
    {
        Mac48Address addr = ContextToMac(context);
        IncrementCounter(psduFailed, addr);
    }
}

/**
 * PHY TX trace
 *
 * @param context The context.
 * @param p The packet.
 * @param txPowerW The TX power.
 */
void
PhyTxTrace(std::string context, Ptr<const Packet> p, double txPowerW)
{
    if (tracing)
    {
        phyTxTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context)
                       << " size=" << p->GetSize() << " " << txPowerW << std::endl;
    }
    if (p->GetSize() >= pktSize) // ignore non-data frames
    {
        Mac48Address addr = ContextToMac(context);
        IncrementCounter(packetsTransmitted, addr);
    }
}

/**
 * PHY TX end trace.
 *
 * @param context The context.
 * @param p The packet.
 */
void
PhyTxDoneTrace(std::string context, Ptr<const Packet> p)
{
}

/**
 * MAC TX trace.
 *
 * @param context The context.
 * @param p The packet.
 */
void
MacTxTrace(std::string context, Ptr<const Packet> p)
{
    if (tracing)
    {
        macTxTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " "
                       << p->GetSize() << std::endl;
    }

#ifdef HAS_NETSIMULYZER
    if (enableVisualization)
    {
        const auto it = macTxTraceSeries.find(ContextToNodeId(context));
        if (it != macTxTraceSeries.end())
            it->second->AddPacketSize(p->GetSize());
    }
#endif
}

/**
 * MAC RX trace.
 *
 * @param context The context.
 * @param p The packet.
 */
void
MacRxTrace(std::string context, Ptr<const Packet> p)
{
    if (tracing)
    {
        macRxTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context) << " "
                       << p->GetSize() << std::endl;
    }
#ifdef HAS_NETSIMULYZER
    if (enableVisualization)
    {
        const auto it = macRxTraceSeries.find(ContextToNodeId(context));
        if (it != macRxTraceSeries.end()) {
             it->second->AddPacketSize(p->GetSize());
             macRxTotalTraceSeries->AddPacketSize(p->GetSize());
        }
    }
#endif
}

/**
 * Socket send trace.
 *
 * @param context The context.
 * @param p The packet.
 * @param addr destination address.
 */
void
SocketSendTrace(std::string context, Ptr<const Packet> p, const Address& addr)
{
    if (tracing)
    {
        socketSendTraceFile << Simulator::Now().GetSeconds() << " " << ContextToNodeId(context)
                            << " " << p->GetSize() << " " << addr << std::endl;
    }
}

/**
 * Association log trace.
 *
 * @param context The context.
 * @param address The MAC address.
 */
void
AssociationLog(std::string context, Mac48Address address)
{
    uint32_t nodeId = ContextToNodeId(context);
    auto it = associated.find(nodeId);
    if (it == associated.end())
    {
        NS_LOG_DEBUG("Association: time=" << Simulator::Now() << " node=" << nodeId);
        associated.insert(it, nodeId);
    }
#ifdef HAS_NETSIMULYZER
    if (enableVisualization && associatedSeries)
    {
        associatedSeries->Append(Simulator::Now().GetSeconds(), static_cast<double>(associated.size()));
    }
#endif
}

/**
 * Deassociation log trace.
 *
 * @param context The context.
 * @param address The MAC address.
 */
void
DisassociationLog(std::string context, Mac48Address address)
{
}

/**
 * Reset the stats.
 */
void
RestartCalc()
{
    bytesReceived.clear();
    packetsReceived.clear();
    packetsTransmitted.clear();
    psduFailed.clear();
    psduSucceeded.clear();
    phyHeaderFailed.clear();
    timeFirstReceived.clear();
    timeLastReceived.clear();
    rxEventWhileDecodingPreamble.clear();
    rxEventWhileRxing.clear();
    rxEventWhileTxing.clear();
    rxEventAbortedByTx.clear();
}

/**
 * Class to configure and run an experiment.
 */
class Experiment
{
  public:
    Experiment();

    int Run(const WifiHelper& wifi,
            const YansWifiPhyHelper& wifiPhy,
            const WifiMacHelper& wifiMac,
            const YansWifiChannelHelper& wifiChannel,
            uint32_t trialNumber,
            uint32_t networkSize,
            uint32_t nAps, // Added: Number of APs
            Time duration,
            bool pcap,
            bool infra,
            uint16_t guardIntervalNs,
            meter_u distance,
            dBm_u apTxPower,
            dBm_u staTxPower,
            Time pktInterval);
};

Experiment::Experiment()
{
}

int
Experiment::Run(const WifiHelper& helper,
                const YansWifiPhyHelper& wifiPhy,
                const WifiMacHelper& wifiMac,
                const YansWifiChannelHelper& wifiChannel,
                uint32_t trialNumber,
                uint32_t networkSize,
                uint32_t nAps, // Added
                Time duration,
                bool pcap,
                bool infra,
                uint16_t guardIntervalNs,
                meter_u distance,
                dBm_u apTxPower,
                dBm_u staTxPower,
                Time pktInterval)
{
    RngSeedManager::SetSeed(10);
    RngSeedManager::SetRun(10);

    // ---------------------------------------------------------
    // 1. Topology Construction (Gateway, APs, STAs)
    // ---------------------------------------------------------
    
    // Gateway Node (The core network)
    NodeContainer gatewayNode;
    gatewayNode.Create(1);

    // Access Point Nodes
    NodeContainer apNodes;
    apNodes.Create(nAps);

    // Station Nodes
    NodeContainer staNodes;
    staNodes.Create(networkSize);

    // Container for all Wifi Devices (APs + STAs)
    NodeContainer wifiNodes;
    wifiNodes.Add(apNodes);
    wifiNodes.Add(staNodes);
    
    // Total Nodes
    NodeContainer allNodes;
    allNodes.Add(gatewayNode);
    allNodes.Add(apNodes);
    allNodes.Add(staNodes);

    // ---------------------------------------------------------
    // 2. Wired Backhaul (Gateway <-> APs)
    // ---------------------------------------------------------
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer p2pDevices;
    for(uint32_t i = 0; i < nAps; i++) {
        p2pDevices.Add(p2p.Install(gatewayNode.Get(0), apNodes.Get(i)));
    }

    // ---------------------------------------------------------
    // 3. Wireless Access (APs <-> STAs)
    // ---------------------------------------------------------
    YansWifiPhyHelper phy = wifiPhy;
    phy.SetErrorRateModel("ns3::NistErrorRateModel");
    phy.SetChannel(wifiChannel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiMacHelper mac = wifiMac;
    const auto& wifi = helper;
    NetDeviceContainer devices; // This will hold all Wifi devices (AP + STA)

    if (infra)
    {
        Ssid ssid = Ssid("wifi-bianchi");
        uint64_t beaconInterval = std::min<uint64_t>(
            (ceil((duration.GetSeconds() * 1000000) / 1024) * 1024),
            (65535 * 1024));

        // Install AP Mode
        mac.SetType("ns3::ApWifiMac",
                    "BeaconInterval",
                    TimeValue(MicroSeconds(beaconInterval)),
                    "Ssid",
                    SsidValue(ssid));
        phy.Set("TxPowerStart", DoubleValue(apTxPower));
        phy.Set("TxPowerEnd", DoubleValue(apTxPower));
        devices.Add(wifi.Install(phy, mac, apNodes));

        // Install STA Mode
        mac.SetType("ns3::StaWifiMac",
                    "MaxMissedBeacons",
                    UintegerValue(std::numeric_limits<uint32_t>::max()),
                    "Ssid",
                    SsidValue(ssid));
        phy.Set("TxPowerStart", DoubleValue(staTxPower));
        phy.Set("TxPowerEnd", DoubleValue(staTxPower));
        devices.Add(wifi.Install(phy, mac, staNodes));
    }
    else
    {
        mac.SetType("ns3::AdhocWifiMac");
        phy.Set("TxPowerStart", DoubleValue(staTxPower));
        phy.Set("TxPowerEnd", DoubleValue(staTxPower));
        devices = wifi.Install(phy, mac, wifiNodes);
    }

    WifiHelper::AssignStreams(devices, trialNumber);

    Config::Set(
        "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported",
        BooleanValue(guardIntervalNs == 400));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HeConfiguration/GuardInterval",
                TimeValue(NanoSeconds(guardIntervalNs)));

    // Configure aggregation
    for (uint32_t i = 0; i < wifiNodes.GetN(); ++i)
    {
        Ptr<NetDevice> dev = wifiNodes.Get(i)->GetDevice(0);
        Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice>(dev);
        if(wifi_dev) {
            wifi_dev->GetMac()->SetAttribute("BE_MaxAmpduSize",
                                             UintegerValue(maxMpdus * (pktSize + 50)));
        }
    }

    // ---------------------------------------------------------
    // 4. Mobility (Positioning)
    // ---------------------------------------------------------
    MobilityHelper mobility;
    
    // Gateway Position (Far left)
    Ptr<ListPositionAllocator> gatewayAlloc = CreateObject<ListPositionAllocator>();
    gatewayAlloc->Add(Vector(-20.0, 0.0, 0.0));
    mobility.SetPositionAllocator(gatewayAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gatewayNode);

    // AP Positions (Linearly spaced)
    Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator>();
    double apSpacing = 40.0;
    for(uint32_t i=0; i < nAps; i++) {
        apAlloc->Add(Vector(i * apSpacing, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(apAlloc);
    mobility.Install(apNodes);

    // STA Positions (Clustered around APs)
    // Distribute STAs among APs
    uint32_t stasPerAp = networkSize / nAps;
    uint32_t extraStas = networkSize % nAps;
    uint32_t staCounter = 0;

    for (uint32_t i = 0; i < nAps; i++) {
        NodeContainer currentApStas;
        uint32_t count = stasPerAp + (i < extraStas ? 1 : 0);
        
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
                                           "rho", DoubleValue (distance * 100)); // Radius
            mobility.Install(currentApStas);
        }
    }

    // ---------------------------------------------------------
    // 5. Traffic (Applications)
    // ---------------------------------------------------------
    PacketSocketHelper packetSocket;
    packetSocket.Install(allNodes);

    ApplicationContainer apps;
    Ptr<UniformRandomVariable> startTime = CreateObject<UniformRandomVariable>();
    startTime->SetAttribute("Stream", IntegerValue(trialNumber));
    startTime->SetAttribute("Max", DoubleValue(duration.GetSeconds() * 0.75));

    // STAs send to their nearest AP
    staCounter = 0;
    for (uint32_t i = 0; i < nAps; i++) {
        uint32_t count = stasPerAp + (i < extraStas ? 1 : 0);
        for (uint32_t j = 0; j < count; j++) {
            if(staCounter < networkSize) {
                PacketSocketAddress socketAddr;
                // STA is Sending
                Ptr<Node> sender = staNodes.Get(staCounter);
                // AP is Receiving
                Ptr<Node> receiver = apNodes.Get(i);
                
                // Get Wifi Interface indices
                // NOTE: APs have 2 devices (P2P + Wifi), Wifi is usually index 1
                // STAs have 1 device (Wifi), index 0
                uint32_t senderIfIndex = sender->GetDevice(0)->GetIfIndex(); 
                
                // IMPORTANT: We need the MAC address of the AP's Wifi interface
                // AP devices: [0]=P2P to Gateway, [1]=Wifi
                Ptr<NetDevice> apWifiDev = receiver->GetDevice(1); 
                
                socketAddr.SetSingleDevice(senderIfIndex);
                socketAddr.SetPhysicalAddress(apWifiDev->GetAddress());
                socketAddr.SetProtocol(1);

                Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient>();
                client->SetRemote(socketAddr);
                sender->AddApplication(client);
                client->SetAttribute("PacketSize", UintegerValue(pktSize));
                client->SetAttribute("MaxPackets", UintegerValue(0));
                client->SetAttribute("Interval", TimeValue(pktInterval));
                client->SetStartTime(Seconds(startTime->GetValue()));

                Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer>();
                server->SetLocal(socketAddr);
                receiver->AddApplication(server);
                
                staCounter++;
            }
        }
    }

    // Log packet receptions
    Config::Connect(
        "/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/MonitorSnifferRx",
        MakeCallback(&TracePacketReception));

    // Log association
    if (infra)
    {
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                        MakeCallback(&AssociationLog));
    }

    std::string txop =
        StaticCast<WifiNetDevice>(staNodes.Get(0)->GetDevice(0))->GetMac()->GetQosSupported()
            ? "BE_Txop"
            : "Txop";
    
    // Trace callbacks
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::WifiMac/" + txop + "/CwTrace", MakeCallback(&CwTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::WifiMac/" + txop + "/BackoffTrace", MakeCallback(&BackoffTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback(&MacTxTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&MacRxTrace));


    Simulator::Schedule(Seconds(10), &RestartCalc);
    Simulator::Stop(Seconds(10) + duration);

    if (pcap)
    {
        phy.EnablePcap("wifi_bianchi_pcap", devices);
    }

#ifdef HAS_NETSIMULYZER
    /* Define graphs to visualize */
    if (enableVisualization)
    {
        NS_LOG_INFO("Enabling visualization...");

        std::ostringstream oss;
        oss << "Bianchi-GatewayDemo.json";
        orchestrator = CreateObject<netsimulyzer::Orchestrator>(oss.str());
        orchestrator->SetAttribute("MobilityPollInterval", TimeValue(MilliSeconds(guiResolution)));

        netsimulyzer::NodeConfigurationHelper nodeConfigHelper(orchestrator);
        nodeConfigHelper.Set("Scale", DoubleValue(0.2));

        // 1. Configure Gateway
        nodeConfigHelper.Set("Model", netsimulyzer::models::SERVER_VALUE);
        nodeConfigHelper.Set("Name", StringValue("Gateway"));
        nodeConfigHelper.Install(gatewayNode);

        // 2. Configure APs
        nodeConfigHelper.Set("Model", netsimulyzer::models::ROUTER_VALUE);
        for(uint32_t i=0; i < nAps; i++) {
             nodeConfigHelper.Set("Name", StringValue("AP-" + std::to_string(i+1)));
             nodeConfigHelper.Install(apNodes.Get(i));
        }

        // 3. Configure STAs
        nodeConfigHelper.Set("Model", netsimulyzer::models::SMARTPHONE_VALUE);
        nodeConfigHelper.Set("Scale", DoubleValue(0.05));
        for(uint32_t i=0; i < networkSize; i++) {
             nodeConfigHelper.Set("Name", StringValue("Phone " + std::to_string(i+1)));
             nodeConfigHelper.Install(staNodes.Get(i));
        }
        
        // 4. Wired Links Visualization
        netsimulyzer::LogicalLinkHelper linkHelper(orchestrator);
        linkHelper.Set("Color", netsimulyzer::Color3Value(0, 255, 0)); // Green
        // REMOVED THE INVALID 'Width' ATTRIBUTE SETTING
        for(uint32_t i=0; i < nAps; i++) {
             linkHelper.Link(gatewayNode.Get(0), apNodes.Get(i));
        }

        // Log
        applicationLog = CreateObject<netsimulyzer::LogStream>(orchestrator);
        applicationLog->SetAttribute("Name", StringValue("Application log"));
        
        // Setup Charts
        associatedSeries = CreateObject<netsimulyzer::XYSeries>(orchestrator);
        associatedSeries->SetAttribute("Name", StringValue("Number of associated STAs"));

        // Setup per-node charts (Using Global Node ID)
        for(uint32_t i=0; i < allNodes.GetN(); ++i) {
             Ptr<Node> n = allNodes.Get(i);
             uint32_t id = n->GetId();
             
             Ptr<netsimulyzer::ThroughputSink> txSink = CreateObject<netsimulyzer::ThroughputSink>(orchestrator, "Tx");
             macTxTraceSeries.insert(std::pair<uint32_t, Ptr<netsimulyzer::ThroughputSink>>(id, txSink));
             
             Ptr<netsimulyzer::ThroughputSink> rxSink = CreateObject<netsimulyzer::ThroughputSink>(orchestrator, "Rx");
             macRxTraceSeries.insert(std::pair<uint32_t, Ptr<netsimulyzer::ThroughputSink>>(id, rxSink));
        }
        
        // Total Rx
        macRxTotalTraceSeries = CreateObject<netsimulyzer::ThroughputSink>(orchestrator, "Total MAC Rx");

    } 
#endif

    Simulator::Run();
    Simulator::Destroy();

    // Stats printing omitted for brevity, logic remains valid
    if (tracing)
    {
        cwTraceFile.close();
        backoffTraceFile.close();
        phyTxTraceFile.close();
        macTxTraceFile.close();
        macRxTraceFile.close();
        socketSendTraceFile.close();
    }

    return 0;
}

/**
 * Get the Counter associated with a MAC address.
 */
uint64_t
GetCount(const std::map<Mac48Address, uint64_t>& counter, Mac48Address addr)
{
    uint64_t count = 0;
    auto it = counter.find(addr);
    if (it != counter.end())
    {
        count = it->second;
    }
    return count;
}

int
main(int argc, char* argv[])
{
    uint32_t nMinStas = 10;  ///< Minimum number of STAs to start with
    uint32_t nMaxStas = 10;  ///< Maximum number of STAs to end with
    uint32_t nStepSize = 5; ///< Number of stations to add at each step
    uint32_t nAps = 2;      ///< Number of Access Points
    // REMOVED unused verbose variable
    double duration = 10;   ///< duration
    // REMOVED unused trials variable
    bool pcap = false;      ///< PCAP
    bool infra = true;      ///< Infrastructure (Must be true for this Gateway demo)
    std::string phyMode = "OfdmRate54Mbps";
    std::string standard("11a");
    uint16_t guardIntervalNs = 800;
    uint16_t pktInterval = 1000;
    meter_u distance = 0.001;
    dBm_u apTxPower{16};
    dBm_u staTxPower{16};

    CommandLine cmd(__FILE__);
    cmd.AddValue("nMinStas", "Minimum number of stations", nMinStas);
    cmd.AddValue("nMaxStas", "Maximum number of stations", nMaxStas);
    cmd.AddValue("nAps", "Number of Access Points", nAps); // Added
    cmd.AddValue("visual", "Generate visualization traces", enableVisualization);
    cmd.Parse(argc, argv);

    // Initialize Log Files if tracing is enabled (omitted in this simplified view, flags are false by default)

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
    
    // Run Loop
    for (uint32_t n = nMinStas; n <= nMaxStas; n += nStepSize)
    {
        experiment.Run(wifi,
                       wifiPhy,
                       wifiMac,
                       wifiChannel,
                       0,
                       n,
                       nAps, // Pass new Arg
                       Seconds(duration),
                       pcap,
                       infra,
                       guardIntervalNs,
                       distance,
                       apTxPower,
                       staTxPower,
                       MicroSeconds(pktInterval));
    }

    return 0;
}