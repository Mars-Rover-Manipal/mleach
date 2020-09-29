#include "ns3/core-module.h"
#include "ns3/double.h"
#include "ns3/flow-monitor.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "leach-helper.h"
#include "leach-routing-protocol.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/uinteger.h"
#include "wsn-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/vector.h"
#include "LeachPacket.h"
#include "ns3/udp-header.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

#include <cstdio>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <iomanip>


using namespace ns3;

uint16_t port = 9;
uint32_t packetsGenerated = 0;
uint32_t packetsDropped = 0;

NS_LOG_COMPONENT_DEFINE ("LeachProposal");


/// Trace function for remaining energy at node.
void
RemainingEnergy (double oldValue, double remainingEnergy)
{
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds ()
                   << "s Current remaining energy = " << remainingEnergy << "J");
}

/// Trace function for total energy consumption at node.
void
TotalEnergy (double oldValue, double totalEnergy)
{
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds ()
                   << "s Total energy consumed by radio = " << totalEnergy << "J");
}

/// record packet counts
void
TotalPackets (uint32_t oldValue, uint32_t newValue)
{
    packetsGenerated += (newValue - oldValue);
}

/// dropped packets from LeachRoutingProtocol
void
CountDroppedPkt (uint32_t oldValue, uint32_t newValue)
{
    packetsDropped += (newValue - oldValue);
}

bool
cmp (struct ns3::leach::msmt a, struct ns3::leach::msmt b)
{
    return (a.begin < b.begin);
}

class LeachProposal
{
public:
    LeachProposal ();
    ~LeachProposal ();
    void CaseRun (uint32_t nWifis,
                  uint32_t nSinks,
                  double totalTime,
                  std::string rate,
                  std::string phyMode,
                  uint32_t periodicUpdateInterval,
                  double dataStart,
                  double lambda,
                  bool verbose,
                  bool tracing,
                  bool netAnim);

private:
    uint32_t m_nWifis;
    uint32_t m_nSinks;
    double m_totalTime;
    std::string m_rate;
    std::string m_phyMode;
    uint32_t m_periodicUpdateInterval;
    double m_dataStart;
    uint32_t bytesTotal;
    uint32_t packetsReceived;
    uint32_t packetsReceivedYetExpired;
    uint32_t packetsDecompressed;
    Vector positions[205];
    double m_lambda;
    bool m_verbose;
    bool m_tracing;
    bool m_netAnim;
    std::vector<struct ns3::leach::msmt>* m_timeline;
    std::vector<Time>* m_txtime;

    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    EnergySourceContainer sources;

private:
    void CreateNodes ();
    void CreateDevices ();
    void InstallInternetStack (std::string tr_name);
    void InstallApplications ();
    void SetupMobility ();
    void SetupEnergyModel ();
    void ReceivePacket (Ptr <Socket> );
    Ptr <Socket> SetupPacketReceive (Ipv4Address, Ptr <Node> );

};

int main (int argc, char **argv)
{
    uint32_t nWifis = 50;
    uint32_t nSinks = 1;
    double totalTime = 2.0;
    std::string rate ("2048bps");
    std::string phyMode ("DsssRate11Mbps");
    uint32_t periodicUpdateInterval = 5;
    double dataStart = 0.0;
    double lambda = 1.0;
    bool verbose = true;
    bool tracing = true;
    bool netAnim = false;

    CommandLine cmd;
    cmd.AddValue ("nWifis",                 "Number of WiFi nodes",     nWifis);
    cmd.AddValue ("nSinks",                 "Number of Base Stations",  nSinks);
    cmd.AddValue ("totalTime",              "Total Simulation time",    totalTime);
    cmd.AddValue ("phyMode",                "Wifi Phy mode",            phyMode);
    cmd.AddValue ("rate",                   "CBR traffic rate",         rate);
    cmd.AddValue ("periodicUpdateInterval", "Periodic Interval Time",   periodicUpdateInterval);
    cmd.AddValue ("dataStart",              "Time at which nodes start to transmit data", dataStart);
    cmd.AddValue ("verbose",                "Enable Logging",           verbose);
    cmd.AddValue ("tracing",                "Enable PCAP and ASCII Tracing", tracing);
    cmd.AddValue ("netAnim",                "GUI Animation Interface",  netAnim);
    cmd.Parse (argc, argv);

    SeedManager::SetSeed (12345);
    std::cout << "Seed Set\n";

    //Config::SetDefault ("ns3::leach::WsnApplication::PacketSize", UintegerValue(64));
    //Config::SetDefault ("ns3::leach::WsnApplication::DataRate", DataRateValue (rate));
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2000"));

    //test = LeachProposal ();
    LeachProposal *test = new LeachProposal;
    test->CaseRun (nWifis, nSinks, totalTime, rate, phyMode, periodicUpdateInterval, dataStart, lambda, verbose, tracing, netAnim);
    //delete test;

    return 0;
}

LeachProposal::LeachProposal ()
  : bytesTotal (0),
    packetsReceived (0),
    packetsReceivedYetExpired (0),
    packetsDecompressed (0)
{
}

LeachProposal::~LeachProposal()
{
}

void
LeachProposal::ReceivePacket (Ptr <Socket> socket)
{
    Ptr <Packet> packet;
    uint32_t packetSize = 0;
    uint32_t packetCount = 0;

    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << " Received one packet!");

    while ((packet = socket->Recv ()))
    {
        leach::LeachHeader leachHeader;
      
        bytesTotal += packet->GetSize();
        packetSize += packet->GetSize();
        //NS_LOG_UNCOND("packet size: " << packet->GetSize());
        //packet->Print(std::cout);

        while(packet->GetSize() >= 56) 
        {
            packet->RemoveHeader(leachHeader);
            packet->RemoveAtStart(16);
            //NS_LOG_UNCOND(leachHeader);
        
            if(leachHeader.GetDeadline() > Simulator::Now()) packetsDecompressed++;
            else packetsReceivedYetExpired++;
            packetCount++;
        }
        packetsReceived++;
    }
    NS_LOG_UNCOND ("packet size = " << packetSize << ", packetCount = " << packetCount);
    NS_LOG_UNCOND ("packet size/packet count = " << packetSize/(double)packetCount);
    //std::cout << "packet size = " << packetSize << ", packetCount = " << packetCount << std::endl;
    //std::cout << "packet size/packet count = " << packetSize/(double)packetCount << std::endl;
}

Ptr <Socket>
LeachProposal::SetupPacketReceive (Ipv4Address addr, Ptr <Node> node)
{

    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    Ptr <Socket> sink = Socket::CreateSocket (node, tid);
    InetSocketAddress local = InetSocketAddress (addr, port);

    sink->Bind (local);
    sink->SetRecvCallback (MakeCallback ( &LeachProposal::ReceivePacket, this));

    return sink;
}

void
LeachProposal::CaseRun (uint32_t nWifis, uint32_t nSinks, double totalTime, std::string rate,
                           std::string phyMode, uint32_t periodicUpdateInterval, double dataStart, 
                           double lambda, bool verbose, bool tracing, bool netAnim)
{
    m_nWifis = nWifis;
    m_nSinks = nSinks;
    m_totalTime = totalTime;
    m_rate = rate;
    m_phyMode = phyMode;
    m_periodicUpdateInterval = periodicUpdateInterval;
    m_dataStart = dataStart;
    m_lambda = lambda;
    m_verbose = verbose;
    m_tracing = tracing;
    m_netAnim = netAnim;

    std::stringstream ss;
    ss << m_nWifis;
    std::string t_nodes = ss.str ();

    std::stringstream ss3;
    ss3 << m_totalTime;
    std::string sTotalTime = ss3.str ();

    std::string tr_name = "Leach_Manet_" + t_nodes + "Nodes_" + sTotalTime + "SimTime";

    CreateNodes ();
    CreateDevices ();
    SetupMobility ();
    SetupEnergyModel();
    InstallInternetStack (tr_name);
    InstallApplications ();

    std::cout << "\nStarting simulation for " << m_totalTime << " s ...\n\n";
    //if (m_netAnim)
    //{
    std::cout << "Generating xml file for NetAnim..." << std::endl;
    AnimationInterface anim ("anim/leach-animation.xml"); // Mandatory
    anim.UpdateNodeDescription (nodes.Get (0), "sink"); // Optional
    anim.UpdateNodeColor (nodes.Get (0), 0, 0, 255); // Optional
    anim.UpdateNodeSize ( 0, 20.0, 20.0); // Optional
    //anim.GetNodeEnergyFraction(nodes.Get(0)); // Optional
    for (uint32_t i = 1; i < m_nWifis; ++i)
    {
        anim.UpdateNodeDescription (nodes.Get (i), "node"); // Optional
        anim.UpdateNodeColor (nodes.Get (i), 255, 0, 0); // Optional
        anim.UpdateNodeSize ( i, 5.0, 5.0); // Optional
        //anim.GetNodeEnergyFraction (nodes.Get(i)); // Optional
    }

    anim.EnablePacketMetadata (); // Optional
    anim.EnableIpv4RouteTracking ("anim/routingtable-leach.xml", Seconds (0), Seconds (5), Seconds (0.25)); //Optional
    anim.EnableWifiMacCounters (Seconds (0), Seconds (50)); //Optional
    anim.EnableWifiPhyCounters (Seconds (0), Seconds (50)); //Optional
    std::cout << "Finished generating xml file for NetAnim.\n" << std::endl;
    //}

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop (Seconds (m_totalTime));
    Simulator::Run ();

    flowMonitor->SerializeToXmlFile("anim/flowMonitor-leach.flowmon", true, true);

    double avgIdle = 0.0, avgTx = 0.0, avgRx = 0.0;
    double energyTx = 0.0, energyRx = 0.0;
#if 0
    char file_name[20];
    FILE* pfile, *p2file;

    snprintf(file_name, 19, "timeline%d-%d", m_nWifis, (int)lambda);
    pfile = fopen(file_name, "w");
    snprintf(file_name, 19, "txtime%d-%d", m_nWifis, (int)lambda);
    p2file = fopen(file_name, "w");
#endif

    std::cout << "Total bytes received: " << bytesTotal << "\n";
    std::cout << "Total packets received: " << packetsReceived << "\n"
              << "Total packets decompressed: " << packetsDecompressed << "\n"
              << "Total packets received yet expired+dropped: " << packetsReceivedYetExpired + packetsDropped << "\n"
              << "Total packets generated:" << packetsGenerated << "\n";

    for (uint32_t i=0; i<m_nWifis; i++)
    {
        Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource> (sources.Get (i));
        Ptr<DeviceEnergyModel> basicRadioModelPtr = basicSourcePtr->FindDeviceEnergyModels ("ns3::WifiRadioEnergyModel").Get (0);
        Ptr<WifiRadioEnergyModel> ptr = DynamicCast<WifiRadioEnergyModel> (basicRadioModelPtr);
        NS_ASSERT (basicRadioModelPtr != NULL);

        avgIdle += ptr->GetIdleTime().ToDouble(Time::MS);
        avgTx += ptr->GetTxTime().ToDouble(Time::MS);
        avgRx += ptr->GetRxTime().ToDouble(Time::MS);
        energyTx += ptr->GetTxTime().ToDouble(Time::MS) * ptr->GetTxCurrentA();
        energyRx += ptr->GetRxTime().ToDouble(Time::MS) * ptr->GetTxCurrentA();
        //NS_LOG_UNCOND("Idle time: " << ptr->GetIdleTime() << ", Tx Time: " << ptr->GetTxTime() << ", Rx Time: " << ptr->GetRxTime());
    }

    std::cout << "Avg Idle time(ms):  " << avgIdle/m_nWifis << "\n"
              << "Avg Tx Time(ms):  "   << avgTx/m_nWifis   << "\n"
              << "Avg Rx Time(ms): "   <<  avgRx/m_nWifis   << "\n";

    std::cout << "Avg Tx energy(mJ): " << energyTx/m_nWifis << "\n"
              << "Avg Rx energy(mJ): " << energyRx/m_nWifis << "\n";

    std::cout << "\nGenerating Trace File." << std::endl;
    Ptr<leach::RoutingProtocol> leachTracer = DynamicCast<leach::RoutingProtocol> ((nodes.Get(m_nWifis/2))->GetObject<Ipv4> ()->GetRoutingProtocol());
    m_timeline = leachTracer->getTimeline();
    m_txtime = leachTracer->getTxTime();

    //std::cout << m_timeline << std::endl;
    //std::cout << m_txtime << std::endl;

#if 0 //Seg Fault
    sort(m_timeline->begin(), m_timeline->end(), cmp);
    for (std::vector<struct ns3::leach::msmt>::iterator it=m_timeline->begin(); it!=m_timeline->end(); ++it)
    {
        fprintf(pfile, "%.6f, %.6f\n", it->begin.GetSeconds(), it->end.GetSeconds());
    }
    //sort(tx_time->begin(), tx_time->end());
    for (std::vector<Time>::iterator it=m_txtime->begin(); it!=m_txtime->end(); ++it)
    {
        fprintf(p2file, "%.6f\n", it->GetSeconds());
    }
    std::cout << "Here" << std::endl;
  
    fclose(pfile);
    fclose(p2file);
#endif
    Simulator::Destroy ();
}

void
LeachProposal::CreateNodes ()
{
    std::cout << "Creating " << (unsigned) m_nWifis << " nodes.\n";
    nodes.Create (m_nWifis);
    NS_ASSERT_MSG (m_nWifis > m_nSinks, "Sinks must be less or equal to the number of nodes in network");
    std::cout << "Finished creating " << (unsigned) m_nWifis << " nodes.\n";
}

void
LeachProposal::CreateDevices ()
{
    std::cout << "Creating " << (unsigned) m_nWifis << " devices.\n";
    WifiMacHelper wifiMac;
    wifiMac.SetType ("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(1000));
    wifiPhy.SetChannel (wifiChannel.Create ());
    WifiHelper wifi;
    if (m_verbose)
    {
        wifi.EnableLogComponents();
    }
    // TODO: Change Standard to WIFI_PHY_STANDARD_80211ah
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue (m_phyMode), "ControlMode",
                                StringValue (m_phyMode));
    devices = wifi.Install (wifiPhy, wifiMac, nodes);

    if (m_tracing)
    {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll (ascii.CreateFileStream("trace/Leach-Manet.mob"));
        wifiPhy.EnablePcapAll ("trace/pcap/Leach-Manet");
    }
    std::cout << "Finished creating " << (unsigned) m_nWifis << " devices.\n";
}

void
LeachProposal::SetupMobility ()
{
    std::cout << "Setting Mobility Model for " << (unsigned) m_nWifis << " nodes.\n";
    MobilityHelper mobility;

    int64_t streamIndex = 0; // used to get consistent mobility across scenarios
    int nodeSpeed = 5;  //m/s
    int nodePause = 0;

    ObjectFactory pos;
    pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
    pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=7500.0]"));
    pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=7500.0]"));

    Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
    streamIndex += taPositionAlloc->AssignStreams (streamIndex);

    std::stringstream ssSpeed;
    ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
    std::stringstream ssPause;
    ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                    "Speed", StringValue (ssSpeed.str ()),
                                    "Pause", StringValue (ssPause.str ()),
                                    "PositionAllocator", PointerValue (taPositionAlloc));
    mobility.SetPositionAllocator (taPositionAlloc);
    mobility.Install (nodes);
    streamIndex += mobility.AssignStreams (nodes, streamIndex);
    NS_UNUSED (streamIndex); // From this point, streamIndex is unused

}

void
LeachProposal::SetupEnergyModel()
{
    std::cout << "Setting Energy Model for " << (unsigned) m_nWifis << " nodes.\n";
    /** Energy Model **/
    /***************************************************************************/
    /* energy source */
    BasicEnergySourceHelper basicSourceHelper;
    // configure energy source
    basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (100));
    // install source
    /*EnergySourceContainer */sources = basicSourceHelper.Install (nodes);
    /* device energy model */
    WifiRadioEnergyModelHelper radioEnergyHelper;
    // install device model
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, sources);
    /***************************************************************************/

    
    for (uint32_t i=0; i<m_nWifis; i++)
    {
        Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource> (sources.Get (i));
        basicSourcePtr->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
        // device energy model
        Ptr<DeviceEnergyModel> basicRadioModelPtr = basicSourcePtr->FindDeviceEnergyModels ("ns3::WifiRadioEnergyModel").Get (0);
        NS_ASSERT (basicRadioModelPtr != NULL);
        basicRadioModelPtr->TraceConnectWithoutContext ("TotalEnergyConsumption", MakeCallback (&TotalEnergy));
    }
    
    std::cout << "Finished setting Energy Model for " << (unsigned) m_nWifis << " nodes.\n";
}


#if 1
void
LeachProposal::InstallInternetStack (std::string tr_name)
{
    std::cout << "Installing Internet Stack for " << (unsigned) m_nWifis << " nodes.\n";
    LeachHelper leach;
    //std::cout << m_lambda << std::endl;
    //leach.Set ("Lambda", DoubleValue (m_lambda));
    //leach.Set ("PeriodicUpdateInterval", TimeValue (Seconds (m_periodicUpdateInterval)));
    InternetStackHelper stack;
#if 1
    //uint32_t count = 0;
    stack.Install (nodes); 
    int j=0;
    for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i, ++j)
    {
        //leach.Set("Position", Vector4DValue(positions[count++]));
        stack.SetRoutingHelper (leach); // has effect on the next Install ()
        //stack.Install (*i);
        Ptr<leach::RoutingProtocol> leachTracer = DynamicCast<leach::RoutingProtocol> ((*i)->GetObject<Ipv4> ()->GetRoutingProtocol());
        //TODO: Fix next line
        //leachTracer->TraceConnectWithoutContext ("DroppedCount", MakeCallback (&CountDroppedPkt));
        if (0)
        {
            Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (("trace/" + tr_name + ".routes"), std::ios::out);
            leach.PrintRoutingTableAt(Seconds(m_periodicUpdateInterval), *i, routingStream);
        }
    }
#endif
    //stack.Install (nodes);        // should give change to leach protocol on the position property
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.252.0");
    interfaces = address.Assign (devices);
    std::cout << "Finished installing Internet Stack for " << (unsigned) m_nWifis << " nodes.\n";
}

void
LeachProposal::InstallApplications ()
{
    std::cout << "Installing Applications for " << (unsigned) m_nWifis << " devices.\n";
    Ptr<Node> node = NodeList::GetNode (0);
    Ipv4Address nodeAddress = node->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
    Ptr<Socket> sink = SetupPacketReceive (nodeAddress, node);
    
    //TODO: Use WsnHelper
    //WsnHelper wsn1 ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (0), port)));
    OnOffHelper wsn1 ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (0), port)));
    //wsn1.SetAttribute ("PktGenRate", DoubleValue(m_lambda));
    //// 0 for periodic, 1 for Poisson
    //wsn1.SetAttribute ("PktGenPattern", IntegerValue(0));
    //wsn1.SetAttribute ("PacketDeadlineLen", IntegerValue(3000000000));  // default
    //wsn1.SetAttribute ("PacketDeadlineMin", IntegerValue(5000000000));  // default
    
    for (uint32_t clientNode = 1; clientNode <= m_nWifis - 1; clientNode++ )
    {
        ApplicationContainer apps1 = wsn1.Install (nodes.Get (clientNode));
        Ptr<WsnApplication> wsnapp = DynamicCast<WsnApplication> (apps1.Get (0));
        Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();

        apps1.Start (Seconds (var->GetValue (m_dataStart, m_dataStart + 1)));
        apps1.Stop (Seconds (m_totalTime));
        //TODO: Fix next line
        //wsnapp->TraceConnectWithoutContext ("PktCount", MakeCallback (&TotalPackets));
    }
    std::cout << "Finished installing Applications on " << (unsigned) m_nWifis << " devices.\n";
}
#endif

/*leach-packet.cc*/
/*****************************************************************************/

#if 1
//LeachHeader::LeachHeader (BooleanValue PIR, Vector position, Vector acceleration, Ipv4Address address, Time m)
leach::LeachHeader::LeachHeader (BooleanValue PIR, Vector position, Vector acceleration, Ipv4Address address, Time m) :
    m_PIR (PIR),
    m_position (position),
    m_acceleration (acceleration),
    m_address (address),
    m_deadline (m)
{
}
#endif

leach::LeachHeader::~LeachHeader ()
{
}

TypeId 
leach::LeachHeader::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::leach::LeachHeader")
        .SetParent<Header> ()
        .SetGroupName("Leach")
        .AddConstructor<LeachHeader>();
    return tid;
}

TypeId 
leach::LeachHeader::GetInstanceTypeId () const
{
    return GetTypeId ();
}

uint32_t
leach::LeachHeader::GetSerializedSize () const
{
    return sizeof(m_PIR)+sizeof(m_position)+sizeof(m_acceleration)+sizeof(m_address)+sizeof(m_deadline);
}

void 
leach::LeachHeader::Serialize (Buffer::Iterator i) const
{
    i.Write ((const uint8_t*)&m_PIR,            sizeof(m_PIR));
    i.Write ((const uint8_t*)&m_acceleration,   sizeof(m_acceleration));
    i.Write ((const uint8_t*)&m_position,       sizeof(m_position));
    i.Write ((const uint8_t*)&m_address,        sizeof(m_address));
    i.Write ((const uint8_t*)&m_address+4,      sizeof(m_address));
    i.Write ((const uint8_t*)&m_deadline,       sizeof(m_deadline));
}

uint32_t
leach::LeachHeader::Deserialize (Buffer::Iterator start)
{
    Buffer::Iterator i = start;

    i.Read ((uint8_t*)&m_PIR,           sizeof(m_PIR));
    i.Read ((uint8_t*)&m_position,      sizeof(m_position));
    i.Read ((uint8_t*)&m_acceleration,  sizeof(m_acceleration));
    i.Read ((uint8_t*)&m_address,       sizeof(m_address));
    i.ReadU32();
    i.Read ((uint8_t*)&m_deadline,      sizeof(m_deadline));

    uint32_t dist = i.GetDistanceFrom(start);
    NS_ASSERT (dist == GetSerializedSize());
    return dist;
}

void 
leach::LeachHeader::Print(std::ostream &os) const
{
  os << " PIR: "            << m_PIR
     << " Position: "       << m_position 
     << " Acceleration: "   << m_acceleration 
     << ", IP: "            << m_address 
     << ", Deadline:"       << m_deadline 
     << "\n";
}

/*leach-header.cc*/
/*****************************************************************************/

LeachHelper::~LeachHelper ()
{
}

LeachHelper::LeachHelper () : Ipv4RoutingHelper ()
{
    m_agentFactory.SetTypeId ("ns3::leach::RoutingProtocol");
}

LeachHelper*
LeachHelper::Copy (void) const
{
    return new LeachHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
LeachHelper::Create (Ptr<Node> node) const
{
    Ptr<leach::RoutingProtocol> agent = m_agentFactory.Create<leach::RoutingProtocol> ();
    node->AggregateObject (agent);
    return agent;
}

void
LeachHelper::Set (std::string name, const AttributeValue &v)
{
    m_agentFactory.Set (name, v);
}

/*leach-routing-protocol.cc*/
/*****************************************************************************/

/// UDP Port for LEACH control traffic
const uint32_t leach::RoutingProtocol::LEACH_PORT = 269;

double max(double a, double b) {
    return (a>b)?a:b;
}


TypeId 
leach::RoutingProtocol::GetTypeId (void)
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
leach::RoutingProtocol::SetPosition(Vector pos)
{
    m_position = pos;
}
Vector
leach::RoutingProtocol::GetPosition() const
{
    return m_position;
}

void 
leach::RoutingProtocol::SetAcceleration(Vector accel)
{
    m_acceleration = accel;
}
Vector
leach::RoutingProtocol::GetAcceleration() const
{
    return m_acceleration;
}

void 
leach::RoutingProtocol::SetPIR(BooleanValue pir)
{
    m_PIR = pir;
}
BooleanValue
leach::RoutingProtocol::GetPIR() const
{
    return m_PIR;
}

#if 0
std::vector<struct msmt>*
leach::RoutingProtocol::getTimeline()
{
    return &timeline;
}
#endif

std::vector<Time>*
leach::RoutingProtocol::getTxTime()
{
    return &tx_time;
}

int64_t
leach::RoutingProtocol::AssignStreams(int64_t stream)
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

leach::RoutingProtocol::RoutingProtocol()
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

leach::RoutingProtocol::~RoutingProtocol()
{
}

void
leach::RoutingProtocol::DoDispose()
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
leach::RoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream,Time::Unit unit) const
{
    *stream->GetStream() << "Node: "     << m_ipv4->GetObject<Node> ()->GetId()           << ", "
                         << "Time: "     << Now().As(unit)                            << ", "
                         << "Local Time" << GetObject<Node>()->GetLocalTime().As(unit) << ", "
                         << "LEACH Routing Table" << std::endl;
    m_routingTable.Print(stream);
    *stream->GetStream() << std::endl;
}

void
leach::RoutingProtocol::Start ()
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
leach::RoutingProtocol::LoopbackRoute(const Ipv4Header &header, Ptr<NetDevice> oif) const
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
leach::RoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb, MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
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
leach::RoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
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
leach::RoutingProtocol::EnqueueForNoDA(UnicastForwardCallback ucb, Ptr<Ipv4Route> route, Ptr<const Packet> p, const Ipv4Header &header)
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
leach::RoutingProtocol::AutoDequeueNoDA()
{
    while(DeferredQueue.size())
    {
        struct DeferredPack tmp = DeferredQueue.front();
        tmp.ucb(tmp.route, tmp.p, tmp.header);
        DeferredQueue.erase(DeferredQueue.begin());
    }
}
  
void
leach::RoutingProtocol::RecvLeach (Ptr<Socket> socket)
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
leach::RoutingProtocol::RespondToClusterHead()
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
leach::RoutingProtocol::SendBroadcast ()
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
leach::RoutingProtocol::PeriodicUpdate ()
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
leach::RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
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
leach::RoutingProtocol::NotifyInterfaceUp (uint32_t i)
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
leach::RoutingProtocol::NotifyInterfaceDown (uint32_t i)
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
leach::RoutingProtocol::NotifyAddAddress (uint32_t i,
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
leach::RoutingProtocol::NotifyRemoveAddress (uint32_t i,
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
leach::RoutingProtocol::FindSocketWithAddress (Ipv4Address addr) const
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
leach::RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr) const
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
leach::RoutingProtocol::Send (Ptr<Ipv4Route> route, Ptr<const Packet> packet, const Ipv4Header & header)
{
    Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
    NS_ASSERT (l3 != 0);
    Ptr<Packet> p = packet->Copy ();
    l3->Send (p,route->GetSource (),header.GetDestination (),header.GetProtocol (),route);
}

void
leach::RoutingProtocol::Drop (Ptr<const Packet> packet,
                       const Ipv4Header & header,
                       Socket::SocketErrno err)
{
    NS_LOG_DEBUG (m_mainAddress << " drop packet " << packet->GetUid () << " to "
                                << header.GetDestination () << " from queue. Error " << err);
}

void
leach::RoutingProtocol::EnqueuePacket (Ptr<Packet> p,
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
leach::RoutingProtocol::DeAggregate (Ptr<Packet> in, Ptr<Packet>& out, LeachHeader& lhdr)
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
leach::RoutingProtocol::DataAggregation (Ptr<Packet> p)
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
leach::RoutingProtocol::Proposal (Ptr<Packet> p)
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
leach::RoutingProtocol::OptTM (Ptr<Packet> p)
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
leach::RoutingProtocol::ControlLimit (Ptr<Packet> p)
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
leach::RoutingProtocol::SelectiveForwarding (Ptr<Packet> p)
{
    return false;
}

/*wsn-helper.cc*/
/*****************************************************************************/
WsnHelper::WsnHelper (std::string protocol, Address address)
{
    m_factory.SetTypeId ("ns3::WsnApplication");
    m_factory.Set ("Protocol", StringValue (protocol));
    m_factory.Set ("Remote", AddressValue (address));
}

void 
WsnHelper::SetAttribute (std::string name, const AttributeValue &value)
{
    m_factory.Set (name, value);
}

ApplicationContainer
WsnHelper::Install (Ptr<Node> node) const
{
    return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
WsnHelper::Install (std::string nodeName) const
{
    Ptr<Node> node = Names::Find<Node> (nodeName);
    return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
WsnHelper::Install (NodeContainer c) const
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
        apps.Add (InstallPriv (*i));
    }

    return apps;
}

Ptr<Application>
WsnHelper::InstallPriv (Ptr<Node> node) const
{
    Ptr<Application> app = m_factory.Create<Application> ();
    node->AddApplication (app);

    return app;
}

void 
WsnHelper::SetConstantRate (DataRate dataRate, uint32_t packetSize)
{
    m_factory.Set ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1000]"));
    m_factory.Set ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    m_factory.Set ("DataRate", DataRateValue (dataRate));
    m_factory.Set ("PacketSize", UintegerValue (packetSize));
}

/*wsn-application.cc*/
/*****************************************************************************/

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


WsnApplication::WsnApplication () : 
    m_socket (0),
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

/*leach-routing-table.cc*/
/*****************************************************************************/

leach::RoutingTableEntry::RoutingTableEntry (Ptr<NetDevice> device,
                                      Ipv4Address dstAddr,
                                      Ipv4InterfaceAddress iface,
                                      Ipv4Address nextHop)
    : m_iface (iface),
      m_flag (VALID)
{
    m_ipv4Route = Create<Ipv4Route> ();
    m_ipv4Route -> SetDestination (dstAddr);
    m_ipv4Route -> SetGateway (nextHop);
    m_ipv4Route -> SetSource(m_iface.GetLocal());
    m_ipv4Route -> SetOutputDevice(device);
}

leach::RoutingTableEntry::~RoutingTableEntry()
{
}

leach::RoutingTable::RoutingTable ()
{
}

bool
leach::RoutingTable::LookupRoute (Ipv4Address id,
                           RoutingTableEntry &rtEntry)
{
    if (m_ipv4AddressEntry.empty())
    {
        return false;
    }
    std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.find (id);
    if (i == m_ipv4AddressEntry.end())
    {
        return false;
    }
    rtEntry = i->second;
    return true;
}

bool
leach::RoutingTable::LookupRoute (Ipv4Address id,
                           RoutingTableEntry &rtEntry,
                           bool forRouteInput)
{
    if (m_ipv4AddressEntry.empty())
    {
        return false;
    }
    std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.find (id);
    if (i == m_ipv4AddressEntry.end())
    {
        return false;
    }
    if (forRouteInput == true && id == i->second.GetInterface().GetBroadcast())
    {
        return false;
    }
    rtEntry = i->second;
    return true;
}

bool
leach::RoutingTable::DeleteRoute (Ipv4Address dstAddr)
{
    if (m_ipv4AddressEntry.erase(dstAddr) != 0)
    {
        return true;
    }
    return false;
}

uint32_t
leach::RoutingTable::RoutingTableSize ()
{
    return m_ipv4AddressEntry.size();
}

bool 
leach::RoutingTable::AddRoute(RoutingTableEntry &routingTableEntry)
{
    std::pair<std::map<Ipv4Address, RoutingTableEntry>::iterator, bool> result = m_ipv4AddressEntry.insert (std::make_pair (routingTableEntry.GetDestination(), routingTableEntry));

    return result.second;
}

bool
leach::RoutingTable::Update (RoutingTableEntry &rtEntry)
{
    std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.find (rtEntry.GetDestination());
    if (i == m_ipv4AddressEntry.end())
    {
        return false;
    }
    i->second = rtEntry;
    return true;
}

void
leach::RoutingTable::DeleteAllRouteFromInterface(Ipv4InterfaceAddress iface)
{
    if (m_ipv4AddressEntry.empty())
    {
        return;
    }
    for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end(); )
    {
        if (i->second.GetInterface() == iface)
        {
            std::map<Ipv4Address, RoutingTableEntry>::iterator tmp = i;
            ++i;
            m_ipv4AddressEntry.erase(tmp);
        }
        else 
        {
            ++i;
        }
    }
}

void
leach::RoutingTable::GetListOfAllRoutes(std::map<Ipv4Address, RoutingTableEntry> &allRoutes)
{
    for (std::map<Ipv4Address, RoutingTableEntry>::iterator i = m_ipv4AddressEntry.begin(); i != m_ipv4AddressEntry.end(); ++i)
    {
        if (i->second.GetDestination() != Ipv4Address("127.0.0.1") && i->second.GetFlag()==VALID)
        {
            allRoutes.insert(
                    std::make_pair(i->first, i->second));
        }
    }
}

void
leach::RoutingTable::GetListOfDestinationWithNextHop(Ipv4Address nextHop, std::map<Ipv4Address, RoutingTableEntry> &dstList)
{
    dstList.clear();
    for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.begin(); 
         i != m_ipv4AddressEntry.end(); ++i)
    {
        if (i->second.GetNextHop() == nextHop)
        {
            dstList.insert(std::make_pair (i->first, i->second));
        }
    }
}

#if 1
void
leach::RoutingTableEntry::Print(Ptr<OutputStreamWrapper> stream) const
{
    *stream->GetStream() << std::setiosflags(std::ios::fixed) << m_ipv4Route->GetDestination() 
        << "\t\t" << m_ipv4Route->GetGateway() << "\t\t" << m_iface.GetLocal() << "\n";
}
#endif

void
leach::RoutingTable::Print(Ptr<OutputStreamWrapper> stream) const
{
    *stream->GetStream() << "\nLEACH Routing table" << "DST\t\tDestination\t\tGateway\t\t\tInterface\n";
    for (std::map<Ipv4Address, RoutingTableEntry>::const_iterator i = m_ipv4AddressEntry.begin(); 
            i != m_ipv4AddressEntry.end(); ++i)
    {
        i->first.Print (*stream->GetStream());
        *stream->GetStream() << "\t\t";
        i->second.Print(stream);
    }
    *stream->GetStream() << "\n";
}

bool
leach::RoutingTable::AddIpv4Event(Ipv4Address address, EventId id)
{
    std::pair<std::map<Ipv4Address, EventId>::iterator, bool> result = m_ipv4Events.insert(std::make_pair(address, id));
    return result.second;
}

bool
leach::RoutingTable::AnyRunningEvent(Ipv4Address address)
{
    EventId event;
    std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find(address);
    if (m_ipv4Events.empty())
    {
        return false;
    }
    if (i == m_ipv4Events.end())
    {
        return true;
    }
    event = i->second;
    if (event.IsRunning())
    {
        return true;
    }
    else 
    {
        return false;
    }
}

bool
leach::RoutingTable::ForceDeleteIpv4Event(Ipv4Address address)
{
    EventId event;
    std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find(address);
    if (m_ipv4Events.empty() || i == m_ipv4Events.end())
    {
        return false;
    }
    event = i->second;
    Simulator::Cancel(event);
    m_ipv4Events.erase(address);
    return true;
}

bool
leach::RoutingTable::DeleteIpv4Event(Ipv4Address address)
{
    EventId event;
    std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find(address);
    if (m_ipv4Events.empty() || i == m_ipv4Events.end())
    {
        return false;
    }
    event = i->second;
    if (event.IsRunning())
    {
        return false;
    }
    if (event.IsExpired())
    {
        event.Cancel();
        m_ipv4Events.erase(address);
        return true;
    }
    else 
    {
        m_ipv4Events.erase(address);
        return true;
    }
}

EventId
leach::RoutingTable::GetEventId(Ipv4Address address)
{
    std::map<Ipv4Address, EventId>::const_iterator i = m_ipv4Events.find(address);
    if (m_ipv4Events.empty() || i == m_ipv4Events.end())
    {
        return EventId();
    }
    else
    {
        return i->second;
    }
}

/*leach-routing-queue.cc*/
/*****************************************************************************/

uint32_t
leach::PacketQueue::GetSize()
{
    return m_queue.size();
}

bool
leach::PacketQueue::Enqueue(QueueEntry &entry)
{
    try 
    {
        NS_LOG_FUNCTION("Enqueing packet destined for " << entry.GetIpv4Header().GetDestination());
        m_queue.push_back(entry);
        return true;
    }
    catch(...) 
    {
        NS_LOG_ERROR("ERROR: Enqueing packet destined for " << entry.GetIpv4Header().GetDestination());
        return false;
    }
}

void
leach::PacketQueue::Drop(uint32_t idx)
{
    m_queue.erase(m_queue.begin()+idx);
}

bool
leach::PacketQueue::Dequeue(Ipv4Address dst, QueueEntry &entry)
{
    NS_LOG_FUNCTION("Dequeueing packet destined for " << dst);
    for (std::vector<QueueEntry>::iterator i = m_queue.begin(); i != m_queue.end(); ++i)
    {
        if (i->GetIpv4Header().GetDestination() == dst)
        {
            entry = *i;
            m_queue.erase(i);
            return true;
        }
    }
    return false; 
}

bool
leach::PacketQueue::Find(Ipv4Address dst)
{
    for (std::vector<QueueEntry>::iterator i = m_queue.begin(); i!= m_queue.end(); ++i)
    {
        if (i->GetIpv4Header().GetDestination() == dst)
        {
            NS_LOG_DEBUG("Found");
            return true;
        }
    }
    return false;
}

uint32_t
leach::PacketQueue::GetCountForPacketsWithDst(Ipv4Address dst)
{
    uint32_t count = 0;
    for (std::vector<QueueEntry>::iterator i = m_queue.begin(); i != m_queue.end(); ++i)
    {
        if (i->GetIpv4Header().GetDestination() == dst)
        {
            count++;
        }
    }
    return count;
}
