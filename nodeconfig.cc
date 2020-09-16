#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/leach-routing-protocol.h" //changes
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"



class confignode
{
public:
  confignode ();
  void Configure (int argc, char ** argv);
  // run rest
  // returns test status
  int Run ();
private:
  uint32_t port; //changes
  uint32_t bytesTotal; //c
  uint32_t packetsReceived;//c
 
  //std::string m_CSVfileName
  int m_nSinks;//c
  int nadhoc;
  int nodeSpeed;
  int nodePause;
  // int numPackets;
  // int interval;
  double totaltime;
  // uint32_t packetSize;
  double txpower;
  bool verbose;
  bool m_pcap;
  bool m_ascii;
  NodeContainer adhocNodes;
  NetDeviceContainer adhocDevices;
  Ipv4InterfaceContainer interfaces;
  WifiHelper wifi;
private:
  Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node); // changes
  void ReceivePacket (Ptr<Socket> socket); // changes
  void CheckThroughput ();  // changes
  /// Create nodes and setup their mobility
  void CreateNodes ();
  /// Install internet m_stack on nodes
  void InstallInternetStack ();
  /// Install applications
  void InstallApplication ();
  /// Print adhoc devices diagnostics
  void Report ();
};
  

confignode::confignode():
  nadhoc (50),
  nodeSpeed (7), //in m/s
  nodePause (0), //in s
  totaltime (200.0),
  packetSize (512),
  verbose (false),
  //interval (1),
  tracemobility (false),
  //numPackets (1),
  m_pcap (false), //c
  m_ascii (false), //c
  port (9),//c
  bytesTotal (0),//c
  packetsReceived (0),//c
  //m_CSVfileName ("manet-routing.output.csv"),//c
  //txpower (27)
{

}// c whole
static inline std::string
PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress)
{
  std::ostringstream oss;
 
  oss << Simulator::Now ().GetSeconds () << " " << socket->GetNode ()->GetId ();
 
  if (InetSocketAddress::IsMatchingType (senderAddress))
    {
      InetSocketAddress addr = InetSocketAddress::ConvertFrom (senderAddress);
      oss << " received one packet from " << addr.GetIpv4 ();
    }
  else
    {
      oss << " received one packet!";
    }
  return oss.str ();
}
 
void
confignode::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket->RecvFrom (senderAddress)))
    {
      bytesTotal += packet->GetSize ();
      packetsReceived += 1;
      NS_LOG_UNCOND (PrintReceivedPacket (socket, packet, senderAddress));
    }
}
 
void
confignode::CheckThroughput ()
{
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;
 
  std::ofstream out (m_CSVfileName.c_str (), std::ios::app);
 
  out << (Simulator::Now ()).GetSeconds () << ","
      << kbs << ","
      << packetsReceived << ","
      << m_nSinks << ","
      << m_protocolName << ","
      << m_txp << ""
      << std::endl;
 
  out.close ();
  packetsReceived = 0;
  Simulator::Schedule (Seconds (1.0), &confignode::CheckThroughput, this);
}
 
Ptr<Socket>
confignode::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback (&confignode::ReceivePacket, this));
 
  return sink;
}
//  c end
void
confignode::Configure (int argc, char *argv[])
{
CommandLine cmd;

cmd.AddValue ("nadhoc", "Number adhocnodes", nadhoc);
cmd.AddValue ("time",  "Simulation time (sec)", totaltime);
cmd.AddValue ("pcap",   "Enable PCAP traces on interfaces", m_pcap);
cmd.AddValue ("ascii",   "Enable Ascii traces on interfaces", m_ascii);
//cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
//cmd.AddValue ("numPackets", "number of packets generated", numPackets);
//cmd.AddValue ("interval", "interval (seconds) between packets", interval);
cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
//cmd.AddValue ("txPower", "Transmitted Power", txpower);
cmd.AddValue ("traceMobility", "Enable mobility tracing", m_traceMobility);
cmd.Parse(argc,argv)
//check ascii and pcap
//tx power not used numpacket packet sixe interval
if (m_ascii)
    {
      PacketMetadata::Enable ();
    }
}
void
confignode::CreateNodes (int nSinks) // function argu changed
{
std::string rate ("2048bps");
std::string phyMode ("DsssRate11Mbps");
Config::SetDefault  ("ns3::OnOffApplication::PacketSize",StringValue ("64"));
Config::SetDefault ("ns3::OnOffApplication::DataRate",  StringValue (rate));
 //Set Non-unicastMode rate to unicast mode
Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",StringValue (phyMode));
adhocNodes.Create (nadhoc);
if(verbose)
  {
    wifi.EnableLogComponents ();  // Turn on all Wifi logging
  }
wifi.SetStandard (WIFI_PHY_STANDARD_80211ah); //changes

YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default();
YansWifiChannelHelper wifiChannel;

wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
wifiPhy.SetChannel (wifiChannel.Create ());
wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                              "DataMode",StringValue (phyMode),
                              "ControlMode",StringValue (phyMode));
 
//wifiPhy.Set ("TxPowerStart",DoubleValue (txpower));
//wifiPhy.Set ("TxPowerEnd", DoubleValue (txpower));

WifiMacHelper wifiMac;


wifiMac.SetType ("ns3::AdhocWifiMac");
NetDeviceContainer adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);
MobilityHelper mobilityAdhoc;
int64_t streamIndex = 0; // used to get consistent mobility across scenarios
 
ObjectFactory pos;
pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1500.0]"));
 
Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
streamIndex += taPositionAlloc->AssignStreams (streamIndex);
 
std::stringstream ssSpeed;
ssSpeed << "ns3::UniformRandomVariable[Min=0.0|Max=" << nodeSpeed << "]";
std::stringstream ssPause;
ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                "Speed", StringValue (ssSpeed.str ()),
                                "Pause", StringValue (ssPause.str ()),
                                "PositionAllocator", PointerValue (taPositionAlloc));
mobilityAdhoc.SetPositionAllocator (taPositionAlloc);
mobilityAdhoc.Install (adhocNodes);
streamIndex += mobilityAdhoc.AssignStreams (adhocNodes, streamIndex);
NS_UNUSED (streamIndex); // From this point, streamIndex is unused
//protocol changes
RoutingProtocol leach;
Ipv4ListRoutingHelper list;
list.Add (leach, 100);
InternetStackHelper internet;
internet.SetRoutingHelper (list);
internet.Install (adhocNodes);

//phy.SetChannel (channel.Create());
if (m_pcap)
    wifiPhy.EnablePcapAll (std::string ("mp-"));
  if (m_ascii)
    {
      //AsciiTraceHelper ascii;
      //wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("manet.tr"));
      std::stringstream ss;
      ss << nadhoc;
      std::string nodes = ss.str ();
 
      std::stringstream ss2;
      ss2 << nodeSpeed;
      std::string sNodeSpeed = ss2.str ();
 
      std::stringstream ss3;
      ss3 << nodePause;
      std::string sNodePause = ss3.str ();
 
      std::stringstream ss4;
      ss4 << rate;
      std::string sRate = ss4.str ();
 
   //NS_LOG_INFO ("Configure Tracing.");
   //tr_name = tr_name + "_" + m_protocolName +"_" + nodes + "nodes_" + sNodeSpeed + "speed_" + sNodePause + "pause_" + sRate + "rate";
 
   //AsciiTraceHelper ascii;
   //Ptr<OutputStreamWrapper> osw = ascii.CreateFileStream ( (tr_name + ".tr").c_str());
   //wifiPhy.EnableAsciiAll (osw);
      AsciiTraceHelper ascii;
      MobilityHelper::EnableAsciiAll (ascii.CreateFileStream (tr_name + ".mob"));
 
   //Ptr<FlowMonitor> flowmon;
   //FlowMonitorHelper flowmonHelper;
   //flowmon = flowmonHelper.InstallAll ();
    }

}
void
confignode::InstallInternetStack ()
{
  InternetStackHelper internetStack;
  internetStack.Install (adhocNodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign (adhocDevices);
}
void
confignode::InstallApplication ()
{
  OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ());
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  //changes
  for (int i = 0; i < nSinks; i++)
  {
    Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress (i), adhocNodes.Get (i));
 
    AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (i), port));
    onoff1.SetAttribute ("Remote", remoteAddress);
 
    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
    ApplicationContainer temp = onoff1.Install (adhocNodes.Get (i + nSinks));
    temp.Start (Seconds (var->GetValue (100.0,101.0)));
    temp.Stop (Seconds (TotalTime));
  }
}

int
confignode::Run ()
{
  CreateNodes ();
  InstallInternetStack ();
  InstallApplication ();
  Simulator::Schedule (Seconds (totaltime), &nodeconfig::Report, this);
  Simulator::Stop (Seconds (totaltime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
void
confignode::Report ()
{
  // unsigned n (0);
  // for (NetDeviceContainer::Iterator i = adhocDevices.Begin (); i != adhocDevices.End (); ++i, ++n)
  //   {
  //     std::ostringstream os;
  //     os << "mp-report-" << n << ".xml";
  //     std::cerr << "Printing ahoc point device #" << n << " diagnostics to " << os.str () << "\n";
  //     std::ofstream of;
  //     of.open (os.str ().c_str ());
  //     if (!of.is_open ())
  //       {
  //         std::cerr << "Error: Can't open file " << os.str () << "\n";
  //         return;
  //       }
  //     adhoc.Report (*i, of);
  //     of.close ();
  //   }
}


int 
main(int argc, char *argv[])
{
  int nSinks =1;
  confignode a;
  a.Configure (argc, argv);
  return a.Run ();

}

