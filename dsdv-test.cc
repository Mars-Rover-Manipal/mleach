//
// Created by harshil on 11/04/21.
//

#include <iostream>
#include <cmath>
#include <ostream>
#include "ns3/assert.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/internet-module.h"
#include "ns3/ptr.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/energy-module.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

uint16_t port = 9;

class DsdvProposal
{
public:
  DsdvProposal();

  void CaseRun(uint32_t nWifis,
               uint32_t nSinks,
               double totalTime,
               std::string rate,
               std::string phyMode,
               uint32_t nodeSpeed,
               uint32_t periodicUpdateInterval,
               uint32_t settlingTime,
               double dataStart,
               bool printRoutes,
               std::string CSVfileName);

private:
  uint32_t m_nWifis;
  uint32_t m_nSinks;
  double m_totalTime;
  std::string m_rate;
  std::string m_phyMode;
  uint32_t m_nodeSpeed;
  uint32_t m_periodicUpdateInterval;
  uint32_t m_settlingTime;
  double m_dataStart;
  uint32_t bytesTotal;
  uint32_t packetsReceived;
  bool m_printRoutes;
  std::string m_CSVfileName;

  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  EnergySourceContainer sources;

private:
  void createNodes();
  void createDevices(std::string tr_name);
  void installInternetStack(std::string tr_name);
  void installApplications();
  void setupMobility();
  void setupEnergy();
  Ptr<Socket> setupReceivePacket(Ipv4Address addr, Ptr<Node> node);
  void receivePacket(Ptr<Socket> socket);
  void checkThroughput();
};


void RemainingEnergy (double oldValue, double remainingEnergy)
{
    //NS_LOG_UNCOND (Simulator::Now ().GetSeconds() << " " << remainingEnergy);
}

void TotalEnergy (double oldValue, double totalEnergy)
{
    //NS_LOG_UNCOND (Simulator::Now ().GetSeconds() << " " << totalEnergy);
}

int main(int argc, char** argv)
{
  DsdvProposal test;

  uint32_t nWifis = 50;
  uint32_t nSinks = 1;
  double totalTime = 70.0;
  std::string rate ("1024bps");
  std::string phyMode ("DsssRate11Mbps");
  uint32_t nodeSpeed = 10; // in m/s
  std::string appl = "all";
  uint32_t periodicUpdateInterval = 5;
  uint32_t settlingTime = 0;
  double dataStart = 0.0;
  bool printRoutingTable = true;
  std::string CSVfileName = "trace/DsdvManetExample.csv";

  std::ofstream out (CSVfileName.c_str());
  out <<  "SimulationSeconds," <<
          "ReceiveRate," <<
          "PacketsReceived," <<
          "NumberOfSinks," <<
  std::endl;
  out.close();

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue ("1000"));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (rate));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2000"));

  test = DsdvProposal();
  test.CaseRun (nWifis, nSinks, totalTime, rate, phyMode, nodeSpeed, periodicUpdateInterval,
                           settlingTime, dataStart, printRoutingTable, CSVfileName);

  return 0;
}

DsdvProposal::DsdvProposal ()
  : bytesTotal(0),
    packetsReceived(0)
{
}

void DsdvProposal::checkThroughput()
{
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;

  std::ofstream out (m_CSVfileName.c_str(), std::ios::app);
  out << (Simulator::Now()).GetSeconds() << "," << kbs << "," << packetsReceived << "," << m_nSinks << "," << std::endl;
  out.close();

  packetsReceived = 0;
  Simulator::Schedule(Seconds(1), &DsdvProposal::checkThroughput, this);
}

void DsdvProposal::CaseRun (uint32_t nWifis, uint32_t nSinks, double totalTime, std::string rate,
                       std::string phyMode, uint32_t nodeSpeed, uint32_t periodicUpdateInterval,
                       uint32_t settlingTime, double dataStart, bool printRoutes, std::string CSVfileName)
{
  m_nWifis = nWifis;
  m_nSinks = nSinks;
  m_totalTime = totalTime;
  m_rate = rate;
  m_phyMode = phyMode;
  m_nodeSpeed = nodeSpeed;
  m_periodicUpdateInterval = periodicUpdateInterval;
  m_settlingTime = settlingTime;
  m_dataStart = dataStart;
  m_printRoutes = printRoutes;
  m_CSVfileName = CSVfileName;

  std::stringstream ss;
  ss << m_nWifis;
  std::string t_nodes = ss.str();

  std::stringstream ss3;
  ss3 << m_totalTime;
  std::string sTotalTime = ss3.str();

  std::string tr_name = "Dsdv_Manet_" + t_nodes + "Nodes_" + sTotalTime + "Simtime";
  std::cout << "Trace file generated is " << tr_name << ".tr\n";

  createNodes();
  createDevices(tr_name);
  setupMobility();
  setupEnergy();
  installInternetStack(tr_name);
  installApplications();

  std::cout << "\nStarting simulation for " << m_totalTime << " seconds...\n";

  std::cout << "Generating xml file for NetAnim..." << std::endl;

  AnimationInterface anim("anim/dsdv-animation.xml");
  anim.UpdateNodeDescription (nodes.Get (0), "sink"); // Optional
  anim.UpdateNodeColor (nodes.Get (0), 0, 0, 255); // Optional
  anim.UpdateNodeSize ( 0, 20.0, 20.0); // Optional
  anim.SetMaxPktsPerTraceFile(999999999);
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

  std::cout << bytesTotal;

  Simulator::Stop(Seconds(m_totalTime));
  Simulator::Run();
  Simulator::Destroy();
}

void DsdvProposal::createNodes ()
{
  std::cout << "Creating " << m_nWifis << " nodes.\n";
  nodes.Create(m_nWifis);
  NS_ASSERT_MSG(m_nWifis > m_nSinks, "Sinks must be less or equal to number of nodes.");
}

void DsdvProposal::createDevices (std::string tr_name)
{
  int range = 200;
  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(range));
  wifiPhy.SetChannel(wifiChannel.Create());
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(m_phyMode),
                                "ControlMode", StringValue(m_phyMode));
  devices = wifi.Install(wifiPhy, wifiMac, nodes);

  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll(ascii.CreateFileStream("trace/" +tr_name + ".tr"));
  wifiPhy.EnablePcapAll("trace/pcap/" + tr_name);
}

void DsdvProposal::installInternetStack (std::string tr_name)
{
  DsdvHelper dsdv;
  dsdv.Set("PeriodicUpdateInterval", TimeValue(Seconds(m_periodicUpdateInterval)));
  dsdv.Set("SettlingTime", TimeValue(Seconds(m_settlingTime)));
  InternetStackHelper stack;
  stack.SetRoutingHelper(dsdv);
  stack.Install(nodes);
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign(devices);
  if (m_printRoutes)
  {
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(("trace/" + tr_name + ".routes"), std::ios::out);
    dsdv.PrintRoutingTableAllAt(Seconds(m_periodicUpdateInterval), routingStream);
  }
}

void DsdvProposal::installApplications ()
{
//  for (uint32_t i = 0; i <= m_nSinks; i++)
//    {
//      Ptr<Node> node = NodeList::GetNode(i);
//      Ipv4Address nodeAddress = node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
//      Ptr<Socket> sink = setupReceivePacket(nodeAddress, node);
//    }
  Ptr<Node> node = NodeList::GetNode(0);
  Ipv4Address nodeAddress = node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
  Ptr<Socket> sink = setupReceivePacket(nodeAddress, node);

  for (uint32_t clientNode = 1; clientNode <= m_nWifis-1; clientNode++)
  {
    OnOffHelper onoff1("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(0), port)));
//      onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
//      onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    ApplicationContainer apps1 = onoff1.Install(nodes.Get(clientNode));
    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
    apps1.Start(Seconds(var->GetValue(m_dataStart, m_dataStart+1)));
    apps1.Stop(Seconds(m_totalTime));
  }
}


void DsdvProposal::setupMobility ()
{
  std::cout << "Setting Mobility Model for " << (unsigned) m_nWifis << " nodes.\n";
  MobilityHelper mobility;

  int64_t streamIndex = 0; // used to get consistent mobility across scenarios
  int nodeSpeed = 5;  //m/s
  int nodePause = 0;

  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));

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

void DsdvProposal::setupEnergy ()
{
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100));
  sources = basicSourceHelper.Install(nodes);
  WifiRadioEnergyModelHelper radioEnergyHelper;
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

  for (uint32_t i=0; i<m_nWifis; i++)
  {
    Ptr<BasicEnergySource> basicSourcePtr = DynamicCast<BasicEnergySource>(sources.Get(i));
    basicSourcePtr->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergy));
    Ptr<DeviceEnergyModel> basicRadioModelPtr = basicSourcePtr->FindDeviceEnergyModels("ns3::WifiRadioEnergyModel").Get(0);
    NS_ASSERT(basicRadioModelPtr != NULL);
    basicRadioModelPtr->TraceConnectWithoutContext("TotalEnergyConsumption", MakeCallback(&TotalEnergy));
  }
}

Ptr<Socket> DsdvProposal::setupReceivePacket (Ipv4Address addr, Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket(node, tid);
  InetSocketAddress local = InetSocketAddress(addr, port);
  sink->Bind(local);
  sink->SetRecvCallback(MakeCallback(&DsdvProposal::receivePacket, this));

  return sink;
}

void DsdvProposal::receivePacket (Ptr<Socket> socket)
{
  //NS_LOG_UNCOND(Simulator::Now().As(Time::S) << " Received one packet!");
  Ptr<Packet> packet;
  while ((packet = socket->Recv()))
  {
    bytesTotal += packet->GetSize();
    std::cout << Simulator::Now().GetSeconds() << " " << bytesTotal << std::endl;
    packetsReceived += 1;
  }
}
