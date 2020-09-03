#include <bits/stdint-intn.h>
#include <cstddef>
#include <iostream>
#include <cmath>
#include <ostream>
#include <vector>

#include "leach-routing-protocol.h"
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
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
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
    m_periodicUpdateInterval(Timer::CANCEL_ON_DESTROY)
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


} /* namespace leach */
} /* namespace ns3 */
