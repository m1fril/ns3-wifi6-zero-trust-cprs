#include "traffic-app.h"

#include "ns3/double.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/packet-socket-address.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(TrafficApp);

bool Dist::Parse(const std::string& spec, Dist& out) {
    std::vector<std::string> f;
    std::stringstream ss(spec);
    for (std::string part; std::getline(ss, part, ':');) f.push_back(part);
    if (f.empty()) return false;
    std::vector<double> v;
    try {
        for (size_t i = 1; i < f.size(); ++i) v.push_back(std::stod(f[i]));
    } catch (...) {
        return false;
    }
    Dist d;
    if (f[0] == "const" && v.size() == 1) {
        d = Const(v[0]);
    } else if (f[0] == "uniform" && v.size() == 2 && v[0] <= v[1]) {
        auto rv = CreateObject<UniformRandomVariable>();
        rv->SetAttribute("Min", DoubleValue(v[0]));
        rv->SetAttribute("Max", DoubleValue(v[1]));
        d.rv = rv; d.lo = v[0]; d.hi = v[1];
    } else if (f[0] == "normal" && (v.size() == 2 || v.size() == 4)) {
        auto rv = CreateObject<NormalRandomVariable>();
        rv->SetAttribute("Mean", DoubleValue(v[0]));
        rv->SetAttribute("Variance", DoubleValue(v[1] * v[1]));
        d.rv = rv;
        if (v.size() == 4) { d.lo = v[2]; d.hi = v[3]; }
    } else if (f[0] == "exp" && (v.size() == 1 || v.size() == 2)) {
        auto rv = CreateObject<ExponentialRandomVariable>();
        rv->SetAttribute("Mean", DoubleValue(v[0]));
        if (v.size() == 2) rv->SetAttribute("Bound", DoubleValue(v[1]));
        d.rv = rv;
    } else {
        return false;
    }
    out = d;
    return true;
}

Dist Dist::Const(double v) {
    Dist d;
    auto rv = CreateObject<ConstantRandomVariable>();
    rv->SetAttribute("Constant", DoubleValue(v));
    d.rv = rv; d.lo = v; d.hi = v;
    return d;
}

double Dist::Sample() const {
    return std::clamp(rv->GetValue(), lo, hi);
}

TypeId TrafficApp::GetTypeId() {
    static TypeId tid = TypeId("ns3::NetworkProjectTrafficApp")
                            .SetParent<Application>()
                            .AddConstructor<TrafficApp>()
                            .AddTraceSource("Tx", "Packet generated (before netem stage)",
                                            MakeTraceSourceAccessor(&TrafficApp::m_txTrace),
                                            "ns3::Packet::AddressTracedCallback")
                            .AddTraceSource("NetemDrop", "Packet dropped by netem loss or isolation",
                                            MakeTraceSourceAccessor(&TrafficApp::m_netemDropTrace),
                                            "ns3::Packet::TracedCallback")
                            .AddTraceSource("SendFailed", "Socket refused the packet",
                                            MakeTraceSourceAccessor(&TrafficApp::m_sendFailedTrace),
                                            "ns3::Packet::TracedCallback");
    return tid;
}

TrafficApp::TrafficApp()
    : m_size(Dist::Const(1500)), m_interval(Dist::Const(1200)),
      m_lossRv(CreateObject<UniformRandomVariable>()),
      m_jitterRv(CreateObject<NormalRandomVariable>()) {}

void TrafficApp::SetRemote(const Address& peer, const std::string& factory, uint32_t maxPayload) {
    m_peer = peer;
    m_factory = factory;
    if (maxPayload) m_mtu = maxPayload;
}

void TrafficApp::Burst(uint32_t count, double gapUs) {
    if (!Active()) return;  // a node that is down or switched off cannot burst
    m_burstLeft = count;
    m_burstGapUs = gapUs;
    Simulator::Cancel(m_event);
    Refresh();
}

void TrafficApp::SetNetem(double delayMs, double jitterMs, double loss) {
    m_delayMs = std::max(0.0, delayMs);
    m_jitterMs = std::max(0.0, jitterMs);
    m_loss = std::clamp(loss, 0.0, 1.0);
}

void TrafficApp::SetAppOn(bool on) { m_appOn = on; Refresh(); }
void TrafficApp::SetNodeUp(bool up) { m_nodeUp = up; Refresh(); }

// Arms the send loop when active, cancels it otherwise.
void TrafficApp::Refresh() {
    if (!Active()) {
        Simulator::Cancel(m_event);
    } else if (!m_event.IsPending()) {
        double gapUs = 0.0;  // a burst starts right away
        if (m_burstLeft > 0) --m_burstLeft; else gapUs = m_interval.Sample();
        m_event = Simulator::Schedule(MicroSeconds(static_cast<uint64_t>(gapUs)), &TrafficApp::SendOne, this);
    }
}

void TrafficApp::StartApplication() {
    if (!m_socket) {
        m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName(m_factory));
        if (PacketSocketAddress::IsMatchingType(m_peer)) {
            m_socket->Bind(m_peer);
            m_mtu = GetNode()->GetDevice(PacketSocketAddress::ConvertFrom(m_peer).GetSingleDevice())->GetMtu();
        } else {
            m_socket->Bind();
        }
        m_socket->Connect(m_peer);
    }
    m_running = true;
    Refresh();
}

void TrafficApp::StopApplication() {
    m_running = false;
    Simulator::Cancel(m_event);
    if (m_socket) m_socket->Close();
    m_socket = nullptr;
}

void TrafficApp::SendOne() {
    auto size = static_cast<uint32_t>(std::clamp(m_size.Sample(), 1.0, static_cast<double>(m_mtu)));
    Ptr<Packet> p = Create<Packet>(size);
    m_txTrace(p, m_peer);  // timestamp tag is attached here, so latency includes netem delay

    if (m_isolated || (m_loss > 0.0 && m_lossRv->GetValue() < m_loss)) {
        m_netemDropTrace(p);
    } else {
        double delayMs = m_delayMs + (m_jitterMs > 0.0 ? m_jitterMs * m_jitterRv->GetValue() : 0.0);
        if (delayMs > 0.0) {
            Simulator::Schedule(Seconds(delayMs / 1000.0), &TrafficApp::Transmit, this, p);
        } else {
            Transmit(p);
        }
    }

    double gapUs = m_interval.Sample();
    if (m_burstLeft > 0) {
        --m_burstLeft;
        gapUs = m_burstGapUs;
    }
    m_event = Simulator::Schedule(MicroSeconds(static_cast<uint64_t>(std::max(1.0, gapUs))), &TrafficApp::SendOne, this);
}

void TrafficApp::Transmit(Ptr<Packet> p) {
    // socket is gone if the app stopped while the packet was delayed
    if (m_socket && m_socket->Send(p) < 0) m_sendFailedTrace(p);
}

} // namespace ns3
