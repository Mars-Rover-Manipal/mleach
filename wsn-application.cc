#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"
#include "wsn-application.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "LeachPacket.h"

#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("WsnApplication");

NS_OBJECT_ENSURE_REGISTERED (WsnApplication);
  
TypeId
WsnApplication::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::WsnApplication")
        .SetParent<Application> ()
        .SetGroupName("Applications")
        .AddConstructor<WsnApplication> ()
        .AddAttribute ("DataRate", "The data rate in on state.",
                       DataRateValue (DataRate ("500kb/s")),
                       MakeDataRateAccessor (&WsnApplication::m_cbrRate),
                       MakeDataRateChecker ())
        .AddAttribute ("PacketSize", "The size of packets sent in on state",
                       UintegerValue (512),
                       MakeUintegerAccessor (&WsnApplication::m_pktSize),
                       MakeUintegerChecker<uint32_t> (1))
        .AddAttribute ("PacketDeadlineLen", "The deadline range of packets",
                       IntegerValue (3),
                       MakeIntegerAccessor (&WsnApplication::m_pktDeadlineLen),
                       MakeIntegerChecker<int64_t> (1))
        .AddAttribute ("PacketDeadlineMin", "The minimum deadline of packets",
                       IntegerValue (5),
                       MakeIntegerAccessor (&WsnApplication::m_pktDeadlineMin),
                       MakeIntegerChecker<int64_t> (1))
        .AddAttribute ("Remote", "The address of the destination",
                       AddressValue (),
                       MakeAddressAccessor (&WsnApplication::m_peer),
                       MakeAddressChecker ())
        .AddAttribute ("PktGenRate", "Packet generation rate",
                       DoubleValue (1.0),
                       MakeDoubleAccessor (&WsnApplication::m_pktGenRate),
                       MakeDoubleChecker <double>())
        .AddAttribute ("PktGenPattern", "Packet generation distribution model",
                       IntegerValue (0),
                       MakeIntegerAccessor (&WsnApplication::m_pktGenPattern),
                       MakeIntegerChecker <int>())
        .AddAttribute ("MaxBytes", 
                       "The total number of bytes to send. Once these bytes are sent, "
                       "no packet is sent again, even in on state. The value zero means "
                       "that there is no limit.",
                       UintegerValue (0),
                       MakeUintegerAccessor (&WsnApplication::m_maxBytes),
                       MakeUintegerChecker<uint64_t> ())
        .AddAttribute ("Protocol", "The type of protocol to use.",
                       TypeIdValue (UdpSocketFactory::GetTypeId ()),
                       MakeTypeIdAccessor (&WsnApplication::m_tid),
                       MakeTypeIdChecker ())
        .AddTraceSource ("Tx", "A new packet is created and is sent",
                         MakeTraceSourceAccessor (&WsnApplication::m_txTrace),
                         "ns3::Packet::TracedCallback")
        .AddTraceSource ("PktCount", "Total packets count",
                         MakeTraceSourceAccessor (&WsnApplication::m_pktCount),
                         "ns3::TracedValueCallback::Uint32")
    ;
    return tid;
}


WsnApplication::WsnApplication ()
  : m_socket (0),
    m_connected (false),
    m_residualBits (0),
    m_lastStartTime (Seconds (0)),
    m_totBytes (0),
    m_pktCount (0)
{
    NS_LOG_FUNCTION (this);
}

WsnApplication::~WsnApplication()
{
    NS_LOG_FUNCTION (this);
    //NS_LOG_UNCOND(m_pktCount);
}

void 
WsnApplication::SetMaxBytes (uint64_t maxBytes)
{
    NS_LOG_FUNCTION (this << maxBytes);
    m_maxBytes = maxBytes;
}

Ptr<Socket>
WsnApplication::GetSocket (void) const
{
    NS_LOG_FUNCTION (this);
    return m_socket;
}

void
WsnApplication::DoDispose (void)
{
    NS_LOG_FUNCTION (this);

    m_socket = 0;
    // chain up
    Application::DoDispose ();
}

// Application Methods
void WsnApplication::StartApplication () // Called at time specified by Start
{
    NS_LOG_FUNCTION (this);

    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket (GetNode (), m_tid);
        if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
            m_socket->Bind6 ();
        }
        else if (InetSocketAddress::IsMatchingType (m_peer) ||
                 PacketSocketAddress::IsMatchingType (m_peer))
        {
            m_socket->Bind ();
        }
        m_socket->Connect (m_peer);
        m_socket->SetAllowBroadcast (true);
        m_socket->ShutdownRecv ();

        m_socket->SetConnectCallback (
            MakeCallback (&WsnApplication::ConnectionSucceeded, this),
            MakeCallback (&WsnApplication::ConnectionFailed, this));
    }
    m_cbrRateFailSafe = m_cbrRate;

    // Insure no pending event
    CancelEvents ();
    // If we are not yet connected, there is nothing to do here
    // The ConnectionComplete upcall will start timers at that time
    //if (!m_connected) return;
    //ScheduleStartEvent ();
    StartSending();
}

void WsnApplication::StopApplication () // Called at time specified by Stop
{
    NS_LOG_FUNCTION (this);

    CancelEvents ();
    if(m_socket != 0)
    {
        m_socket->Close ();
    }
    else
    {
        NS_LOG_WARN ("WsnApplication found null socket to close in StopApplication");
    }
}

void WsnApplication::CancelEvents ()
{
    NS_LOG_FUNCTION (this);

    if (m_sendEvent.IsRunning () && m_cbrRateFailSafe == m_cbrRate )
    { 
        // Cancel the pending send packet event
        // Calculate residual bits since last packet sent
        Time delta (Simulator::Now () - m_lastStartTime);
        int64x64_t bits = delta.To (Time::S) * m_cbrRate.GetBitRate ();
        m_residualBits += bits.GetHigh ();
    }
    m_cbrRateFailSafe = m_cbrRate;
    Simulator::Cancel (m_sendEvent);
    Simulator::Cancel (m_startStopEvent);
}

// Event handlers
void WsnApplication::StartSending ()
{
    NS_LOG_FUNCTION (this);
    m_lastStartTime = Simulator::Now ();
    ScheduleNextTx ();  // Schedule the send packet event
}

// Private helpers
void WsnApplication::ScheduleNextTx ()
{
    NS_LOG_FUNCTION (this);

    if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
        uint32_t bits = m_pktSize * 8 - m_residualBits;
        NS_LOG_LOGIC ("bits = " << bits);
        Time nextTime (Seconds (bits /
                              static_cast<double>(m_cbrRate.GetBitRate ()))); // Time till next packet
      
        switch(m_pktGenPattern)
        {
            case 0:
                // suppose periodic model
                nextTime += Seconds(1.0/m_pktGenRate);
                break;
            case 1:
                // suppose Poisson model
            {
                Ptr<UniformRandomVariable> m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
                double p = m_uniformRandomVariable->GetValue (0,1);
                double poisson, expo = exp(-m_pktGenRate);
                int k;

                poisson = expo;
                for(k=1; poisson < p; k++)
                {
                    double temp = pow(m_pktGenRate, k);
                    for(int i=1; i<=k; i++) temp /= i;
                    poisson += temp*expo;
                }
                k--;
                if(k>0) nextTime += Seconds(1.0/k);
                else
                {
                    Simulator::Schedule (Seconds(1.0), &WsnApplication::ScheduleNextTx, this);
                    return;
                }

                break;
            }
          default:
                break;
        }
        NS_LOG_LOGIC ("nextTime = " << nextTime);
        m_sendEvent = Simulator::Schedule (nextTime, &WsnApplication::SendPacket, this);
    }
    else
    {
        // All done, cancel any pending events
        StopApplication ();
    }
}

void WsnApplication::SendPacket ()
{
    NS_LOG_FUNCTION (this);

    NS_ASSERT (m_sendEvent.IsExpired ());
    //leach::LeachHeader hdr(BooleanValue(false), Vector(0.0,0.0,0.0), Vector(0.0,0.0,0.0), Ipv4Address("255.255.255.255"), Time(0));
    leach::LeachHeader hdr;
    Ptr<Packet> packet = Create<Packet> (m_pktSize - sizeof(hdr));
    Ptr<UniformRandomVariable> m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
    int64_t temp = (m_uniformRandomVariable->GetInteger(0, m_pktDeadlineLen) + m_pktDeadlineMin) + Now ().ToInteger(Time::NS);
    
    m_pktCount++;
    hdr.SetDeadline(Time(temp));
    NS_LOG_INFO(temp << ", " << hdr.GetDeadline());
    packet->AddHeader(hdr);
    m_txTrace (packet);
    m_socket->Send (packet);
    m_totBytes += m_pktSize;
    if (InetSocketAddress::IsMatchingType (m_peer))
    {
        NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                    << "s on-off application sent "
                    <<  packet->GetSize () << " bytes to "
                    << InetSocketAddress::ConvertFrom(m_peer).GetIpv4 ()
                    << " port " << InetSocketAddress::ConvertFrom (m_peer).GetPort ()
                    << " total Tx " << m_totBytes << " bytes");
    }
    else if (Inet6SocketAddress::IsMatchingType (m_peer))
    {
        NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                    << "s on-off application sent "
                    <<  packet->GetSize () << " bytes to "
                    << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6 ()
                    << " port " << Inet6SocketAddress::ConvertFrom (m_peer).GetPort ()
                    << " total Tx " << m_totBytes << " bytes");
    }
    m_lastStartTime = Simulator::Now ();
    m_residualBits = 0;
    ScheduleNextTx ();
}

void WsnApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    m_connected = true;
}

void WsnApplication::ConnectionFailed (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
}

} // Namespace ns3

