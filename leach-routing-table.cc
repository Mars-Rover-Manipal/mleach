#include <bits/stdint-uintn.h>
#include <iomanip>
#include <utility>

#include "leach-routing-table.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/net-device.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("LeachRoutingTable");

namespace leach {

RoutingTableEntry::RoutingTableEntry (Ptr<NetDevice> device,
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

RoutingTableEntry::~RoutingTableEntry()
{
}

RoutingTable::RoutingTable ()
{
}

bool
RoutingTable::LookupRoute (Ipv4Address id,
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
RoutingTable::LookupRoute (Ipv4Address id,
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
RoutingTable::DeleteRoute (Ipv4Address dstAddr)
{
    if (m_ipv4AddressEntry.erase(dstAddr) != 0)
    {
        return true;
    }
    return false;
}

uint32_t
RoutingTable::RoutingTableSize ()
{
    return m_ipv4AddressEntry.size();
}

bool 
RoutingTable::AddRoute(RoutingTableEntry &routingTableEntry)
{
    std::pair<std::map<Ipv4Address, RoutingTableEntry>::iterator, bool> result = m_ipv4AddressEntry.insert (std::make_pair (routingTableEntry.GetDestination(), routingTableEntry));

    return result.second;
}

bool
RoutingTable::Update (RoutingTableEntry &rtEntry)
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
RoutingTable::DeleteAllRouteFromInterface(Ipv4InterfaceAddress iface)
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
RoutingTable::GetListOfAllRoutes(std::map<Ipv4Address, RoutingTableEntry> &allRoutes)
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
RoutingTable::GetListOfDestinationWithNextHop(Ipv4Address nextHop, std::map<Ipv4Address, RoutingTableEntry> &dstList)
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

void
RoutingTableEntry::Print(Ptr<OutputStreamWrapper> stream) const
{
    *stream->GetStream() << std::setiosflags(std::ios::fixed) << m_ipv4Route->GetDestination() 
        << "\t\t" << m_ipv4Route->GetGateway() << "\t\t" << m_iface.GetLocal() << "\n";
}

void
RoutingTable::Print(Ptr<OutputStreamWrapper> stream) const
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
RoutingTable::AddIpv4Event(Ipv4Address address, EventId id)
{
    std::pair<std::map<Ipv4Address, EventId>::iterator, bool> result = m_ipv4Events.insert(std::make_pair(address, id));
    return result.second;
}

bool
RoutingTable::AnyRunningEvent(Ipv4Address address)
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
RoutingTable::ForceDeleteIpv4Event(Ipv4Address address)
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
RoutingTable::DeleteIpv4Event(Ipv4Address address)
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
RoutingTable::GetEventId(Ipv4Address address)
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

} /* namespace leach */
} /* namespace ns3 */
