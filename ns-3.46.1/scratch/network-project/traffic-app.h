#ifndef TRAFFIC_APP_H
#define TRAFFIC_APP_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/traced-callback.h"

#include <string>

namespace ns3 {

// Random distribution clipped to [lo, hi].
// Spec: const:V | uniform:MIN:MAX | normal:MEAN:STD[:MIN:MAX] | exp:MEAN[:MAX]
struct Dist {
    Ptr<RandomVariableStream> rv;
    double lo = 0.0;
    double hi = 1e18;

    static bool Parse(const std::string& spec, Dist& out);
    static Dist Const(double v);
    double Sample() const;
};

// Uplink traffic source used in timeline and mesh mode. Infrastructure mode sends raw L2
// frames over a packet socket to the AP; mesh mode sends UDP datagrams to a gateway that
// may be several hops away. Per-packet size/interval are sampled from Dist, bursts override
// the interval for N packets, and a netem-style egress stage applies delay, jitter, loss
// and isolation before the packet enters the stack.
class TrafficApp : public Application {
public:
    static TypeId GetTypeId();
    TrafficApp();

    // factory: ns3::PacketSocketFactory (peer = PacketSocketAddress) or
    // ns3::UdpSocketFactory (peer = InetSocketAddress, payload capped at maxPayload)
    void SetRemote(const Address& peer, const std::string& factory, uint32_t maxPayload = 0);
    void SetSize(const Dist& d) { m_size = d; }
    void SetInterval(const Dist& d) { m_interval = d; }
    void Burst(uint32_t count, double gapUs);
    void SetNetem(double delayMs, double jitterMs, double loss);
    void SetIsolated(bool on) { m_isolated = on; }
    void SetAppOn(bool on);   // service switch (APP command)
    void SetNodeUp(bool up);  // node liveness (NODE command)
    bool IsNodeUp() const { return m_nodeUp; }

private:
    void StartApplication() override;
    void StopApplication() override;
    void Refresh();
    void SendOne();
    void Transmit(Ptr<Packet> p);
    bool Active() const { return m_running && m_appOn && m_nodeUp; }

    Ptr<Socket> m_socket;
    Address m_peer;
    std::string m_factory = "ns3::PacketSocketFactory";
    EventId m_event;
    uint32_t m_mtu = 1500;
    Dist m_size;
    Dist m_interval;
    uint32_t m_burstLeft = 0;
    double m_burstGapUs = 0.0;
    double m_delayMs = 0.0;
    double m_jitterMs = 0.0;
    double m_loss = 0.0;
    bool m_isolated = false;
    bool m_running = false;
    bool m_appOn = true;
    bool m_nodeUp = true;
    Ptr<UniformRandomVariable> m_lossRv;
    Ptr<NormalRandomVariable> m_jitterRv;
    TracedCallback<Ptr<const Packet>, const Address&> m_txTrace;
    TracedCallback<Ptr<const Packet>> m_netemDropTrace;
    TracedCallback<Ptr<const Packet>> m_sendFailedTrace;
};

} // namespace ns3

#endif // TRAFFIC_APP_H
