#include <iostream>
#include <cmath>
#include <vector>

#include "leach-routing-protocol.h"
#include "ns3/assert.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/vector.h"
#include "ns3/udp-header.h"

//#define DA
//#define DA_PROP
//#define DA_OPT
//#define DA_CL
//#define DA_SF

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("LeachRoutingProtocol");

namespace leach {

NS_OBJECT_ENSURE_REGISTERED(RoutingProtocol);

/// UDP Port for LEACH control traffic
const uint32_t RoutingProtocol::LEACH_PORT = 269;

double max(double a, double b) {
    return (a>b)?a:b;
}


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
RoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream,Time::Unit unit) const
{
    *stream->GetStream() << "Node: "     << m_ipv4->GetObject<Node> ()->GetId()           << ", "
                         << "Time: "     << Now().As(unit)                            << ", "
                         << "Local Time" << GetObject<Node>()->GetLocalTime().As(unit) << ", "
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
        m_periodicUpdateTimer.SetFunction (&RoutingProtocol::PeriodicUpdate, this);
        m_broadcastClusterHeadTimer.SetFunction (&RoutingProtocol::SendBroadcast, this);
        m_respondToClusterHeadTimer.SetFunction (&RoutingProtocol::RespondToClusterHead, this);
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
    else
    {
        route->SetSource(j->second.GetLocal());
    }
    NS_ASSERT_MSG(route->GetSource() != Ipv4Address(), "Valid Leach source address not found");
    route->SetGateway(Ipv4Address("127.0.0.1"));
    route->SetOutputDevice(m_lo);
    return route;
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

#ifdef DA
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

void
RoutingProtocol::EnqueueForNoDA(UnicastForwardCallback ucb, Ptr<Ipv4Route> route, Ptr<const Packet> p, const Ipv4Header &header)
{
    struct DeferredPack tmp;
    tmp.ucb = ucb;
    tmp.route = route;
    tmp.p = p;
    tmp.header = header;
    DeferredQueue.push_back(tmp);
    Simulator::Schedule (MilliSeconds (100), &RoutingProtocol::AutoDequeueNoDA, this);
}

void
RoutingProtocol::AutoDequeueNoDA()
{
    while(DeferredQueue.size())
    {
        struct DeferredPack tmp = DeferredQueue.front();
        tmp.ucb(tmp.route, tmp.p, tmp.header);
        DeferredQueue.erase(DeferredQueue.begin());
    }
}
  
void
RoutingProtocol::RecvLeach (Ptr<Socket> socket)
{
    Address sourceAddress;
    Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
    InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
    Ipv4Address sender = inetSourceAddr.GetIpv4 ();
    Ipv4Address receiver = m_socketAddress[socket].GetLocal ();
    double dist, dx, dy;
    LeachHeader leachHeader;
    Vector senderPosition;
  
    // maintain list of received advertisements
    // always choose the closest CH to join in
    // if itself is CH, pass this phase
    packet->RemoveHeader(leachHeader);
    
    /*
    NS_LOG_DEBUG(leachHeader.GetAddress());
    NS_LOG_DEBUG(isSink);
    NS_LOG_DEBUG(m_mainAddress);
    */
  
    if(isSink) return;
    if(leachHeader.GetAddress() == Ipv4Address("255.255.255.255")) 
    {
        NS_LOG_DEBUG("Recv broadcast from CH: " << sender);
        // Need to update a new route
        RoutingTableEntry newEntry ( socket->GetBoundNetDevice(), /*device*/
                                     m_sinkAddress, /*dst (sink)*/
                                     m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0), /*iface*/
                                     sender); /*next hop*/
      
        senderPosition = leachHeader.GetPosition();
        dx = senderPosition.x - m_position.x;
        dy = senderPosition.y - m_position.y;
        dist = dx*dx + dy*dy;
        NS_LOG_DEBUG("dist = " << dist << ", m_dist = " << m_dist);
      
        if(dist < m_dist) 
        {
            m_dist = dist;
            m_targetAddress = sender;
            m_bestRoute = newEntry;
            NS_LOG_DEBUG(sender);
        }
    }
    else 
    {
        // Record cluster member
        m_clusterMember.push_back(leachHeader.GetAddress());
    }
}

void
RoutingProtocol::RespondToClusterHead()
{
    Ptr<Socket> socket = FindSocketWithAddress(m_mainAddress);
    Ptr<Packet> packet = Create<Packet> ();
    LeachHeader leachHeader;
    Ipv4Address ipv4;
    OutputStreamWrapper temp = OutputStreamWrapper(&std::cout);

    
    // Add routing to routingTable
    if(m_targetAddress != ipv4) 
    {
        RoutingTableEntry newEntry, entry2;
        newEntry.Copy(m_bestRoute);
        entry2.Copy(m_bestRoute);
        Ptr<Ipv4Route> newRoute = newEntry.GetRoute();
        newRoute->SetDestination(m_targetAddress);
        newEntry.SetRoute(newRoute);

        if(m_bestRoute.GetInterface().GetLocal() != ipv4) m_routingTable.AddRoute (entry2);
        if(newEntry.GetInterface().GetLocal() != ipv4) m_routingTable.AddRoute (newEntry);

        // m_routingTable.Print(&temp);
      
        leachHeader.SetAddress(m_mainAddress);
        packet->AddHeader (leachHeader);
        socket->SendTo (packet, 0, InetSocketAddress (m_targetAddress, LEACH_PORT));
    }
}

void
RoutingProtocol::SendBroadcast ()
{
    Ptr<Socket> socket = FindSocketWithAddress (m_mainAddress);
    Ptr<Packet> packet = Create<Packet> ();
    LeachHeader leachHeader;
    Ipv4Address destination = Ipv4Address ("10.1.1.255");;

    socket->SetAllowBroadcast (true);

    leachHeader.SetPosition (m_position);
    packet->AddHeader (leachHeader);
    socket->SendTo (packet, 0, InetSocketAddress (destination, LEACH_PORT));
    
    RoutingTableEntry newEntry (
        /*device=*/    socket->GetBoundNetDevice(), 
        /*dst (sink)*/ m_sinkAddress,
        /*iface=*/     m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (m_mainAddress), 0),
        /*next hop=*/  m_sinkAddress);
    m_routingTable.AddRoute (newEntry);
}
  
void
RoutingProtocol::PeriodicUpdate ()
{
    double prob = m_uniformRandomVariable->GetValue (0,1);
    // 10 round a cycle, 100/10=10 cluster heads per round
    int n = 10;
    double p = 1.0/n;
    double t = p/(1-p*(Round%n));
    
    NS_LOG_DEBUG("PeriodicUpdate!!");
    //  NS_LOG_DEBUG("prob = " << prob << ", t = " << t);

    m_routingTable.DeleteRoute(m_targetAddress);
    m_routingTable.DeleteRoute(m_sinkAddress);
    /*
      OutputStreamWrapper temp = OutputStreamWrapper(&std::cout);
      m_routingTable.Print(&temp);
    */
    if(Round%n == 0) valid = 1;
    Round++;
    m_dist = 1e100;
    clusterHeadThisRound = 0;
    m_clusterMember.clear();
    m_bestRoute.Reset();
    m_targetAddress = Ipv4Address();
    
    if(prob < t && valid) 
    {
        // become cluster head
        // broadcast info
        NS_LOG_DEBUG(m_mainAddress << " becomes cluster head");
        valid = 0;
        clusterHeadThisRound = 1;
        m_targetAddress = m_sinkAddress;
        m_broadcastClusterHeadTimer.Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (10000,50000)));
    }
    else 
    {
        m_respondToClusterHeadTimer.Schedule (MilliSeconds(100) + MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)));
    }
    m_periodicUpdateTimer.Schedule (m_periodicUpdateInterval + MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)));
}

void
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
    NS_ASSERT (ipv4 != 0);
    NS_ASSERT (m_ipv4 == 0);
    m_ipv4 = ipv4;
    // Create lo route. It is asserted that the only one interface up for now is loopback
    NS_ASSERT (m_ipv4->GetNInterfaces () == 1 && m_ipv4->GetAddress (0, 0).GetLocal () == Ipv4Address ("127.0.0.1"));
    m_lo = m_ipv4->GetNetDevice (0);
    NS_ASSERT (m_lo != 0);
    // Remember lo route
    RoutingTableEntry rt (
      /*device=*/   m_lo,  
      /*dst=*/      Ipv4Address::GetLoopback (),
      /*iface=*/    Ipv4InterfaceAddress (Ipv4Address::GetLoopback (),Ipv4Mask ("255.0.0.0")),
      /*next hop=*/ Ipv4Address::GetLoopback ());
    rt.SetFlag (INVALID);
    m_routingTable.AddRoute (rt);
    Simulator::ScheduleNow (&RoutingProtocol::Start,this);
}

void
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
    NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ()
                          << " interface is up");
    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
    Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
    if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    {
        return;
    }
    // Create a socket to listen only on this interface
    Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
    NS_ASSERT (socket != 0);
    socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach,this));
    socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), LEACH_PORT));
    socket->BindToNetDevice (l3->GetNetDevice (i));
    socket->SetAllowBroadcast (true);
    socket->SetAttribute ("IpTtl",UintegerValue (1));
    m_socketAddress.insert (std::make_pair (socket,iface));
    // Add local broadcast record to the routing table
    Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
    RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (),/*iface=*/ iface, /*next hop=*/ iface.GetBroadcast ());
    m_routingTable.AddRoute (rt);
    if (m_mainAddress == Ipv4Address ())
    {
        m_mainAddress = iface.GetLocal ();
    }
    NS_ASSERT (m_mainAddress != Ipv4Address ());
}

void
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
    Ptr<NetDevice> dev = l3->GetNetDevice (i);
    Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (i,0));
    NS_ASSERT (socket);
    socket->Close ();
    m_socketAddress.erase (socket);
    if (m_socketAddress.empty ())
    {
      NS_LOG_LOGIC ("No leach interfaces");
      m_routingTable.Clear ();
      return;
    }
    m_routingTable.DeleteAllRouteFromInterface (m_ipv4->GetAddress (i,0));
}

void
RoutingProtocol::NotifyAddAddress (uint32_t i,
                                   Ipv4InterfaceAddress address)
{
    NS_LOG_FUNCTION (this << " interface " << i << " address " << address);
    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
    if (!l3->IsUp (i))
    {
        return;
    }
    Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
    Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
    if (!socket)
    {
        if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
        {
            return;
        }
        Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
        NS_ASSERT (socket != 0);
        socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach,this));
        // Bind to any IP address so that broadcasts can be received
        socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), LEACH_PORT));
        socket->BindToNetDevice (l3->GetNetDevice (i));
        socket->SetAllowBroadcast (true);
        m_socketAddress.insert (std::make_pair (socket,iface));
        Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
        RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*iface=*/ iface, /*next hop=*/ iface.GetBroadcast ());
        m_routingTable.AddRoute (rt);
    }
}

void
RoutingProtocol::NotifyRemoveAddress (uint32_t i,
                                      Ipv4InterfaceAddress address)
{
    Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
    if (socket)
    {
        m_socketAddress.erase (socket);
        Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
        if (l3->GetNAddresses(i))
        {
            Ipv4InterfaceAddress iface = l3->GetAddress (i,0);
            // Create a socket to listen only on this interface
            Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
            NS_ASSERT (socket != 0);
            socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvLeach,this));
            // Bind to any IP address so that broadcasts can be received
            socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), LEACH_PORT));
            socket->SetAllowBroadcast (true);
            m_socketAddress.insert (std::make_pair (socket,iface));
        }
    }
}

Ptr<Socket>
RoutingProtocol::FindSocketWithAddress (Ipv4Address addr) const
{
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddress.begin (); 
            j != m_socketAddress.end (); ++j)
    {
        Ptr<Socket> socket = j->first;
        Ipv4Address iface = j->second.GetLocal();
        if (iface == addr)
        {
            return socket;
        }
    }
  return NULL;
}

Ptr<Socket>
RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr) const
{
    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddress.begin (); 
            j != m_socketAddress.end (); ++j)
    {
        Ptr<Socket> socket = j->first;
        Ipv4InterfaceAddress iface = j->second;
        if (iface == addr)
        {
            return socket;
        }
    }
  return NULL;
}

void
RoutingProtocol::Send (Ptr<Ipv4Route> route, Ptr<const Packet> packet, const Ipv4Header & header)
{
    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
    NS_ASSERT (l3 != 0);
    Ptr<Packet> p = packet->Copy ();
    l3->Send (p,route->GetSource (),header.GetDestination (),header.GetProtocol (),route);
}

void
RoutingProtocol::Drop (Ptr<const Packet> packet,
                       const Ipv4Header & header,
                       Socket::SocketErrno err)
{
    NS_LOG_DEBUG (m_mainAddress << " drop packet " << packet->GetUid () << " to "
                                << header.GetDestination () << " from queue. Error " << err);
}

void
RoutingProtocol::EnqueuePacket (Ptr<Packet> p,
                                const Ipv4Header & header)
{
    NS_LOG_FUNCTION (this << ", " << p << ", " << header);
    NS_ASSERT (p != 0 && p != Ptr<Packet> ());
    
    Ptr<Packet> out;
    UdpHeader uhdr;
    LeachHeader leachHeader;
    uint32_t slot = p->GetUid()%1021;
    struct hash* now = m_hash[slot];
    
    NS_LOG_DEBUG("IsDontFragement: " << header.IsDontFragment());
  
    if(header.GetFragmentOffset() == 0) p->RemoveHeader(uhdr);

    while (now != NULL)
    {
        if(now->uid == p->GetUid())
            break;
        now = now->next;
    }
    if(now != NULL)
    {
        NS_LOG_DEBUG("now->p size " << now->p->GetSize() << ", p size " << p->GetSize());
        now->p->AddAtEnd(p);
        p = now->p;
        NS_LOG_DEBUG("after p size " << p->GetSize());
    }
  
    while(DeAggregate(p, out, leachHeader))
    {
        QueueEntry newEntry (out,header);
        bool result = m_queue.Enqueue (newEntry);
        struct msmt temp;

        temp.begin = Simulator::Now();
        temp.end = leachHeader.GetDeadline();
        timeline.push_back(temp);
        if (result)
        {
            NS_LOG_DEBUG ("Added packet " << out->GetUid () << " to queue.");
        }
    }
}

bool
RoutingProtocol::DeAggregate (Ptr<Packet> in, Ptr<Packet>& out, LeachHeader& lhdr)
{
    if(in->GetSize() >= 56)
    {
        LeachHeader leachHeader;
        in->RemoveHeader(leachHeader);
        in->RemoveAtStart(16);

        lhdr = leachHeader;
        out = new Packet(16);
        out->AddHeader(leachHeader);
        NS_LOG_DEBUG("deadline" << leachHeader.GetDeadline());
        return true;
    }
    uint32_t slot = in->GetUid()%1021;
    struct hash* now = m_hash[slot];
    while(now != NULL)
    {
        if(now->uid == in->GetUid()) break;
        now = now->next;
    }
    if(now == NULL) 
    {
        now = new struct hash;
        now->uid = in->GetUid();
        now->next = m_hash[slot];
        m_hash[slot] = now;
    }
    now->p = in;
    NS_LOG_DEBUG("Size left " << in->GetSize() << ", on UID " << in->GetUid());
  
    return false;
}

bool
RoutingProtocol::DataAggregation (Ptr<Packet> p)
{
    // Implement data aggregation policy
    // and data addgregation function

#ifdef DA_PROP 
    return Proposal(p);
#endif
#ifdef DA_OPT
    return OptTM(p);
#endif
#ifdef DA_CL
    return ControlLimit(p);
#endif
#ifdef DA_SF
    return SelectiveForwarding(p);
#endif
  
  return true;
}

bool
RoutingProtocol::Proposal (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this);
  // pick up those selected entry and send
  int expired = 0, expected;
  //LeachHeader hdr;
  Time deadLine = Now();
  
  // 1.28 = 2*0.64, 0.064 = 64bytes/8kbps
  // average 10 cluster heads
  // average 10 members per cluster
  deadLine += Seconds(m_queue.GetSize()/m_lambda);
  if(!clusterHeadThisRound)
    // depend on average tx size from cluster member
    // depend on deadline setting
    // * average packet_size?
    deadLine += Seconds(0.064+1.0/m_lambda);

//  NS_LOG_UNCOND("Now: " << Now() << ", Deadline: " << deadLine);
  for (int i=0; i<(int)m_queue.GetSize(); i++)
    {
      NS_LOG_DEBUG("GetDeadline: " << m_queue[i].GetDeadline() << ", UID: " << m_queue[i].GetPacket()->GetUid() << ", Now: " << Now());
      if(m_queue[i].GetDeadline() < Now())
        {
          // drop it
          NS_LOG_DEBUG("Drop");
//          NS_LOG_DEBUG("GetLeachHeader: " << m_queue[i].GetDeadline() << ", Now: " << Now());
          m_queue.Drop (i);
          m_dropped++;
          i--;
        }
    }
  
  for(uint32_t i=0; i<m_queue.GetSize(); i++) {
//    NS_LOG_UNCOND("GetDeadline: " << m_queue[i].GetDeadline() << ", deadline: " << deadLine);
    if(m_queue[i].GetDeadline() < deadLine) {
      expired++;
    }
  }
  if(clusterHeadThisRound) {
    expected = 1+m_clusterMember.size();
  }
  else {
    expected = 1;
  }
  
//  NS_LOG_UNCOND("expired: " << expired << ", expected: " << expected);
  if(expired >= expected || Now() > Seconds(48.5)) {
    // merge data
    QueueEntry temp;
    
    while(m_queue.Dequeue(m_sinkAddress, temp)) {
      p->AddAtEnd(temp.GetPacket());
    }
    
    return true;
  }
  return false;
}

bool
RoutingProtocol::OptTM (Ptr<Packet> p)
{
    Time time = Now();
    uint32_t rewards[100], maxR = 0;
    uint32_t actions[100];
    static int step = 0;

    for(int i=0; i<100; i++)
    {
        actions[i] = 0;
        rewards[i] = 0;
        for(uint j=0; j<m_queue.GetSize(); j++)
        {
            if(m_queue[j].GetDeadline() >= time) rewards[i] += m_queue[j].GetDeadline().ToInteger(Time::MS) - time.ToInteger(Time::MS);
        }
        for(int j=1; j<i+step; j++)
        {
            rewards[i] += (j<8) ?30000-j*4000 :0;
        }
        time += Seconds(1/m_lambda);
    }
  
    for(int i=0; i<100; i++)
    {
        if(rewards[i] > maxR)
        {
            maxR = rewards[i];
        }
    }
  
    // wait=1, transmit=2
    for(int i=98; i>=0; i--)
    {
        if(rewards[i] < maxR)
            actions[i] = 1;
        else
        {
            double rb[100], rn[100];
            rb[99] = 1.0;
            rn[99] = 0.0;

            for(int k=98; k>i; k--)
            {
                rn[k] = max(0.0, i*rn[k+1]/(k+1) + rb[k+1]/(k+1));
                rb[k] = max(rewards[k], rn[k]);
            }

            if(rewards[i] >= (uint32_t)rb[i+1])
                actions[i] = 2;
            else
                actions[i] = 1;
        }
    }
    step++;
  
    if(actions[0] > 1 || Now() > Seconds(48.5))
    {
        QueueEntry temp;
        while(m_queue.Dequeue(m_sinkAddress, temp))
        {
            p->AddAtEnd(temp.GetPacket());
        }
      
        return true;
    }
  
    return false;
}


bool
RoutingProtocol::ControlLimit (Ptr<Packet> p)
{
    static uint32_t threshold = (1/(log(1/0.1)*(log(1/0.1)+m_lambda)))+2;
    for(int i=0; i<(int)m_queue.GetSize(); i++)
    {
        if(m_queue[i].GetDeadline() < Now())
        {
            m_queue.Drop(i);
            m_dropped++;
            i--;
        }
    }
    
    if(m_queue.GetSize() >= threshold || Now() > Seconds(48.5))
    {
        QueueEntry temp;
        while(m_queue.Dequeue(m_sinkAddress, temp)) 
        {
            p->AddAtEnd(temp.GetPacket());
        }
        return true;
    }
    return false;
}
  
bool
RoutingProtocol::SelectiveForwarding (Ptr<Packet> p)
{
    return false;
}


} /* namespace leach */
} /* namespace ns3 */
