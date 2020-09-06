#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
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
  int nadhoc;
  int nodeSpeed;
  int nodePause;
  int numPackets;
  int interval;
  double totaltime;
  uint32_t packetSize;
  double txpower;
  bool verbose;
  bool m_pcap;
  bool m_ascii;
  NodeContainer adhocNodes;
  NetDeviceContainer adhocDevices;
  Ipv4InterfaceContainer interfaces;
  WifiHelper wifi;
private:
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
  nadhoc (3),
  nodeSpeed (20), //in m/s
  nodePause (0), //in s
  totaltime (200.0),
  packetSize (512),
  verbose (false),
  interval (1),
  numPackets (1),
  m_pcap (false),
  m_ascii (false),
  txpower (27)
{

}


void
confignode::Configure (int argc, char *argv[])
{
CommandLine cmd;

cmd.AddValue ("nadhoc", "Number adhocnodes", nadhoc);
cmd.AddValue ("time",  "Simulation time (sec)", totaltime);
cmd.AddValue ("pcap",   "Enable PCAP traces on interfaces", m_pcap);
cmd.AddValue ("ascii",   "Enable Ascii traces on interfaces", m_ascii);
cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
cmd.AddValue ("numPackets", "number of packets generated", numPackets);
cmd.AddValue ("interval", "interval (seconds) between packets", interval);
cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
cmd.AddValue ("txPower", "Transmitted Power", txpower);
cmd.Parse(argc,argv)
//check ascii and pcap
//tx power not used numpacket packet sixe interval
if (m_ascii)
    {
      PacketMetadata::Enable ();
    }
}
void
confignode::CreateNodes ()
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
wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_5GHZ);

YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default();
YansWifiChannelHelper wifiChannel;

wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
wifiPhy.SetChannel (wifiChannel.Create ());
wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                              "DataMode",StringValue (phyMode),
                              "ControlMode",StringValue (phyMode));
 
wifiPhy.Set ("TxPowerStart",DoubleValue (txpower));
wifiPhy.Set ("TxPowerEnd", DoubleValue (txpower));

WifiMacHelper wifiMac;


wifiMac.SetType ("ns3::AdhocWifiMac");
NetDeviceContainer adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);


phy.SetChannel (channel.Create());
if (m_pcap)
    wifiPhy.EnablePcapAll (std::string ("mp-"));
  if (m_ascii)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("manet.tr"));
    }

}
void
confignode::InstallInternetStack ()
{
  InternetStackHelper internetStack;
  internetStack.Install (adhocnodes);
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
  confignode a;
  a.Configure (argc, argv);
  return a.Run ();

}

