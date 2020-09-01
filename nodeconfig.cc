#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"


using namespace ns3;
uint32_t nadhoc = 30;
int nodeSpeed = 20; //in m/s
int nodePause = 0; //in s
double TotalTime = 200.0;

CommandLine cmd;

cmd.AddValue("nadhoc", "Number adhocnodes", nadhoc);
cmd.Parse(argc,argv);
int main(int argc, char *argv[])
{

NodeContainer adhocNodes;
adhocNodes.Create(nadhoc);
WifiHelper wifi;
wifi.SetStandard(WIFI_PHY_STANDARD_80211ax_5GHZ);

YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default();
YansWifiChannelHelper wifiChannel;

wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
wifiPhy.SetChannel (wifiChannel.Create ());


WifiMacHelper wifiMac;


wifiMac.SetType("ns3::AdhocWifiMac");
NetDeviceContainer adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);


phy.SetChannel(channel.Create());
WifiHelper wifi;


NS_LOG_INFO("Install Internetv6 stack.");
InternetStackHelper internetv6;
internetv6.SetIpv4StackInstall(false);
internetv6.Install(nadhoc);

NS_LOG_INFO ("Assign addresses.");
Ipv6AddressHelper ipv6;
ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
Ipv6InterfaceContainer i = ipv6.Assign(adhocDevices);



Simulator::Stop (Seconds (TotalTime));
Simulator::Run ();
Simulator::Destroy ();
}
