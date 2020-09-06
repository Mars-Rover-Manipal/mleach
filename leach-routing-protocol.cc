#include <bits/stdint-intn.h>
#include <bits/stdint-uintn.h>
#include <cstddef>
#include <iostream>
#include <cmath>
#include <ostream>
#include <vector>

#include "leach-routing-protocol.h"
#include "ns3/assert.h"
#include "ns3/callback.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/log-macros-disabled.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/object-base.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/timer.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/type-id.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/vector.h"
#include "ns3/udp-header.h"
#include "scratch/leach-routing-table.h"
#include "src/core/model/boolean.h"
#include "src/core/model/vector.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE("LeachRoutingProtocol");

namespace leach {

NS_OBJECT_ENSURE_REGISTERED(RoutingProtocol);

TypeId 
RoutingProtocol::GetTypeId (void)
{
    static TypeId tid = TypeId("ns3::leach::RoutingProtocol")
        .SetParent<Ipv4RoutingProtocol> ()
        .SetGroupName("Leach")
        .AddConstructor<RoutingProtocol>()
        .AddAttribute ("PeriodicUpdateInterval", "Periodic interval between exchange of full routing table among nodes.",
                       TimeValue (Seconds(15)),
                       MakeTimeAccessor(&RoutingProtocol::m_periodicUpdateInterval),
                       MakeTimeChecker())
        .AddAttribute ("Position", "X and Y position of node",
                       Vector3DValue(),
                       MakeVectorAccessor(&RoutingProtocol::m_position),
                       MakeVectorChecker())
        .AddAttribute ("Acceleration", "X and Y acceleration of node",
                       Vector3DValue(),
                       MakeVectorAccessor(&RoutingProtocol::m_acceleration),
                       MakeVectorChecker())
        .AddAttribute ("PIR", "True or False for PIR sensor on node",
                       BooleanValue(),
                       MakeBooleanAccessor(&RoutingProtocol::m_PIR),
                       MakeBooleanChecker())
        .AddAttribute ("lambda", "Average packet generation rate",
                       DoubleValue(1.0),
                       MakeDoubleAccessor(&RoutingProtocol::m_lambda),
                       MakeDoubleChecker<double>())
        .AddTraceSource ("DroppedCount", "Total Packets dropped",
                       MakeTraceSourceAccessor(&RoutingProtocol::m_dropped),
                       "ns3::TracedValueCallback::Uint32")
        ;

    return tid;
}

void 
RoutingProtocol::SetPosition(Vector pos)
{
    m_position = pos;
}
Vector
RoutingProtocol::GetPosition() const
{
    return m_position;
}

void 
RoutingProtocol::SetAcceleration(Vector accel)
{
    m_acceleration = accel;
}
Vector
RoutingProtocol::GetAcceleration() const
{
    return m_acceleration;
}

void 
RoutingProtocol::SetPIR(BooleanValue pir)
{
    m_PIR = pir;
}
BooleanValue
RoutingProtocol::GetPIR() const
{
    return m_PIR;
}

std::vector<struct msmt>*
RoutingProtocol::getTimeline()
{
    return &timeline;
}
std::vector<Time>*
RoutingProtocol::getTxTime()
{
    return &tx_time;
}

int64_t
RoutingProtocol::AssignStreams(int64_t stream)
{
    try
    {
        NS_LOG_FUNCTION(this << stream);
        m_uniformRandomVariable -> SetStream(stream);
        return 1;
    }
    catch(...)
    {
        NS_LOG_ERROR("Cannot AssignStreams");
        return 0;
    }
}

RoutingProtocol::RoutingProtocol()
  : Round(0),
    isSink(0),
    m_dropped(0),
    m_lambda(4.0),
    timeline(),
    tx_time(),
    m_routingTable(),
    m_bestRoute(),
    m_queue(),
    m_periodicUpdateTimer(Timer::CANCEL_ON_DESTROY),
    m_broadcastClusterHeadTimer (Timer::CANCEL_ON_DESTROY),
    m_respondToClusterHeadTimer (Timer::CANCEL_ON_DESTROY)
    {
        m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
        for (int i = 0; i < 1021; i++) m_hash[i] = NULL;
    }

RoutingProtocol::~RoutingProtocol()
{
}

void
RoutingProtocol::DoDispose()
{
    m_ipv4 = 0;
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter = m_socketAddress.begin();
            iter != m_socketAddress.end(); iter++)
    {
        iter->first->Close();
    }
    m_socketAddress.clear();
    Ipv4RoutingProtocol::DoDispose();
}

void
RoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const
{
    *stream->GetStream() << "Node: "     << m_ipv4->GetObject<Node> ()->GetId()           << ", "
                         << "Time: "     << Now().As(Time::S)                            << ", "
                         << "Local Time" << GetObject<Node>()->GetLocalTime().As(Time::S) << ", "
                         << "LEACH Routing Table" << std::endl;
    m_routingTable.Print(stream);
    *stream->GetStream() << std::endl;
}

void
RoutingProtocol::Start ()
{
    m_selfCallback  = MakeCallback(&RoutingProtocol::Send, this);
    m_errorCallback = MakeCallback(&RoutingProtocol::Drop, this);
    m_sinkAddress = Ipv4Address ("10.1.1.1");
    ns3::PacketMetadata::Enable();
    ns3::Packet::EnablePrinting();

    if (m_mainAddress == m_sinkAddress)
    {
        isSink = 1;
    }
    else 
    {
        Round = 0;
        m_routingTable.SetHoldDownTime (Time (m_periodicUpdateInterval));
        m_periodicUpdateTimer.SetFunction (&RoutingProtocol::PeriodicUpdate(), this);
        m_broadcastClusterHeadTimer.SetFunction (&RoutingProtocol::SendBroadcast(), this);
        m_respondToClusterHeadTimer.SetFunction (&RoutingProtocol::RespondToClusterHead(), this);
        m_periodicUpdateTimer.Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger(10,1000)));
    }
}

Ptr<Ipv4Route>
RoutingProtocol::LoopbackRoute(const Ipv4Header &header, Ptr<NetDevice> oif) const
{
    NS_ASSERT (m_lo != 0);
    Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    route->SetDestination(header.GetDestination());

    std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddress.begin();
    if (oif)
    {
        // Iterate to find addressof oif device
        for (j = m_socketAddress.begin(); j != m_socketAddress.end(); ++j)
        {
            Ipv4Address addr = j->second.GetLocal();
            int32_t interface = m_ipv4->GetInterfaceForAddress(addr);
            if (oif == m_ipv4->GetNetDevice(static_cast<uint32_t>(interface)))
            {
                route->SetSource(addr);
                break;
            }
        }
    }
}

bool
RoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
    NS_LOG_FUNCTION(m_mainAddress << " received packet " << p->GetUid ()
                                  << " from "            << header.GetSource ()
                                  << " on interface "    << idev->GetAddress ()
                                  << " to destination "  << header.GetDestination ());

    if (m_socketAddress.empty())
    {
        NS_LOG_DEBUG("No Leach Interface");
        return false;
    }

    NS_ASSERT (m_ipv4 != 0);
    // Check if input device supports IPv4
    NS_ASSERT (m_ipv4->GetInterfaceForDevice(idev) >= 0);
    int32_t iif = m_ipv4->GetInterfaceForDevice(idev);

    Ipv4Address dst = header.GetDestination();
    Ipv4Address origin = header.GetSource();
    
    // LEACH implementation is unicast
    if (dst.IsMulticast())
    {
        NS_LOG_ERROR("ERROR: Multicast destination found. Leach Implementation is not multicast.");
        return false;
    }

    // Deferred route request
    if (idev == m_lo)
    {
        NS_LOG_DEBUG("Loopback Route");
#ifdef DA
        Ptr<Packet> pa = new Packet(*p);
        EnqueuePacket (pa, header);
        return false;
#else
        RoutingTableEntry toDst;
        NS_LOG_DEBUG("Deferred: " << dst);

        if (m_routingTable.LookupRoute(dst, toDst))
            {
                Ptr<Ipv4Route> route = toDst.GetRoute();
                NS_LOG_DEBUG("Deferred forwarding");
                NS_LOG_DEBUG("Src: " << route->GetSource() << ", Dst: " << toDst.GetDestination() << ", Gateway: " << toDst.GetNextHop());
                ucb(route, p, header);
            }
        else 
        {
            NS_LOG_DEBUG("Route not found");
            Ptr<Ipv4Route> route = Create<Ipv4Route> ();
            route->SetDestination(dst);
            route->SetSource(origin);
            route->SetGateway(Ipv4Address ("127.0.0.1"));
            route->SetOutputDevice(m_lo);
            EnqueueForNoDA(ucb, route, p, header);
        }
        return true;
#endif
    }
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddress.begin();
            j != m_socketAddress.end(); ++j)
    {
        Ipv4InterfaceAddress iface = j->second;
        if (origin == iface.GetLocal())
        {
            return true;
        }
    }

    // Local Delivery to each leach interface
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddress.begin();
            j != m_socketAddress.end(); ++j)
    {
        Ipv4InterfaceAddress iface = j->second;
        if (m_ipv4->GetInterfaceForAddress(iface.GetLocal()) == iif)
        {
            // Ignore Broadcast
            if (dst == iface.GetBroadcast() || dst.IsBroadcast())
            {
                Ptr<Packet> packet = p->Copy();
                if (lcb.IsNull())
                {
                    NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
                    ecb (p, header, Socket::ERROR_NOROUTETOHOST);
                }
                else
                {
                      NS_LOG_LOGIC ("Broadcast local delivery to " << iface.GetLocal ());
                      lcb (p, header, iif);
                }
                if (header.GetTtl() > 1)
                {
                    NS_LOG_LOGIC("Forwarding Broadcast. TTL " << (uint16_t) header.GetTtl());
                    RoutingTableEntry toBroadcast;
                    if (m_routingTable.LookupRoute(dst, toBroadcast, true))
                    {
                        Ptr<Ipv4Route> route = toBroadcast.GetRoute();
                        ucb (route, packet, header);
                    }
                    else 
                    {
                        NS_LOG_DEBUG("No route to forward. Drop Packet " << p->GetUid());
                    }
                }
                return true;
            }
        }
    }

    // This means arrival
    if (m_ipv4->IsDestinationAddress(dst, iif))
    {
        if (lcb.IsNull())
        {
              NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
        else
        {
              NS_LOG_LOGIC ("Unicast local delivery to " << dst);
              lcb (p, header, iif);
        }
        return true;
    }

    // Check if input device supports IPv4 forwarding
    if (!m_ipv4->IsForwarding(iif))
    {
        NS_LOG_LOGIC("Forwarding Disabled for this interface.");
        ecb (p, header,  Socket::ERROR_NOROUTETOHOST);
        return true;
    }

    // Enqueue, not send
    RoutingTableEntry toDst;
    if (m_routingTable.LookupRoute(dst, toDst))
    {
        RoutingTableEntry ne;
        if (m_routingTable.LookupRoute(toDst.GetNextHop(), ne))
        {
            Ptr<Ipv4Route> route = ne.GetRoute();
            NS_LOG_LOGIC(m_mainAddress << " is forwarding packet "  << p->GetUid()
                                       << " to "                    << dst
                                       << " from "                  << header.GetSource()
                                       << " via nexthop neighbour " << toDst.GetNextHop());
#ifdef DA
            Ptr<Packet> pa = new Packet(*p);
            EnqueuePacket(pa, header);
            return false;
#else
            ucb (route, p, header);
            return true;
#endif
        }
    }
#ifndef DA
    NS_LOG_DEBUG("Route not found");

    Ptr<Ipv4Route> route = Create<Ipv4Route> ();
    route->SetDestination(dst);
    route->SetSource(origin);
    route->SetGateway(Ipv4Address ("127.0.0.1"));
    route->SetOutputDevice(m_lo);
    EnqueueForNoDA(ucb, route, p, header);
#endif
    return false;
}

Ptr<Ipv4Route>
RoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
    NS_LOG_FUNCTION(this << header << (oif ? oif->GetIfIndex() : 0));

    if (m_socketAddress.empty())
    {
        sockerr = Socket::ERROR_NOROUTETOHOST;
        NS_LOG_LOGIC("No Leach Interface");
        Ptr<Ipv4Route> route;
        return route;
    }

    Ipv4Address dst = header.GetDestination();
    RoutingTableEntry rt;
    NS_LOG_DEBUG("Packet Size: " << p->GetSize () << ", " << 
                 "Packet id: "   << p->GetUid ()  << ", " << 
                 "Destination address in Packet: " << dst);

#ifndef DA
    if (p->GetSize()%56 == 0)
    {
        if (DataAggregation(p))
        {
#endif           
            if (m_routingTable.LookupRoute(dst, rt))
            {
                tx_time.push_back(Simulator::Now());

                Ptr<Packet> packet = new Packet(*p);
                LeachHeader hdr;
                struct ns3::leach::msmt tmp;

                packet->RemoveHeader(hdr);
                tmp.begin = Simulator::Now();
                tmp.end = hdr.GetDeadline();
                timeline.push_back(tmp);

                return rt.GetRoute();
            }
#ifdef DA
        }
    }
    else if (m_routingTable.LookupRoute(dst, rt))
    {
        return rt.GetRoute();
    }
#endif

    return LoopbackRoute(header, oif);
}



} /* namespace leach */
} /* namespace ns3 */
