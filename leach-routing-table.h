#ifndef LEACH_ROUTING_TABLE
#define LEACH_ROUTING_TABLE

#include <bits/stdint-uintn.h>
#include <cassert>
#include <map>
#include <sys/types.h>

#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-route.h"
#include "ns3/ptr.h"
#include "ns3/timer.h"
#include "ns3/net-device.h"
#include "ns3/output-stream-wrapper.h"


namespace ns3 {
namespace leach {

enum RouteFlags
{
    VALID = 0,
    INVALID = 1,
};

/**
 * \ingroup leach
 * \brief Routing table entry
 */
class RoutingTableEntry
{
public:
    RoutingTableEntry (Ptr<NetDevice> dev = 0, Ipv4Address dstAddr = Ipv4Address (),
                       Ipv4InterfaceAddress iface = Ipv4InterfaceAddress (), Ipv4Address nextHopAddr = Ipv4Address ());

    ~RoutingTableEntry ();

    void
    Reset ()
    {
        m_iface = Ipv4InterfaceAddress ();
        m_ipv4Route -> SetDestination (Ipv4Address ());
        m_ipv4Route -> SetGateway (Ipv4Address ());
        m_ipv4Route -> SetSource(m_iface.GetLocal());
        m_flag = VALID;
    }

    void
    Copy (RoutingTableEntry from)
    {
        ;
    }

    Ipv4Address
    GetDestination() const
    {
        return m_ipv4Route->GetDestination();
    }
    Ptr<Ipv4Route>
    GetRoute () const
    {
        return m_ipv4Route;
    }
    void 
    SetRoute (Ptr<Ipv4Route> route)
    {
        m_ipv4Route = route;
    }
    void
    SetNextHop (Ipv4Address nextHop)
    {
        m_ipv4Route -> SetGateway (nextHop);
    }
    Ipv4Address
    GetNextHop () const
    {
        return m_ipv4Route->GetGateway();
    }

    void 
    SetOutputDevice (Ptr<NetDevice> outputDevice)
    {
        m_ipv4Route->SetOutputDevice(outputDevice);
    }
    Ptr<NetDevice>
    GetOutputDevice () const
    {
        return m_ipv4Route->GetOutputDevice ();
    }
      Ipv4InterfaceAddress
    GetInterface () const
    {
        return m_iface;
    }
    void
    SetInterface (Ipv4InterfaceAddress iface)
    {
        m_iface = iface;
    }
    
    void
    SetFlag (RouteFlags flag)
    {
        m_flag = flag;
    }
    RouteFlags
    GetFlag () const
    {
        return m_flag;
    }
    /**
     * \brief Compare destination address
     * \return true if equal
     */
    bool
    operator== (Ipv4Address const destination) const
    {
        return (m_ipv4Route->GetDestination () == destination);
    }
    void
    Print (Ptr<OutputStreamWrapper> stream) const;

private:
    // Fields
    /**
     * Ip route, include
     *  - destination address
     *  - source address
     *  - next hop address (gateway)
     *  - output device
     */
    Ptr<Ipv4Route> m_ipv4Route;
    // Output interface address
    Ipv4InterfaceAddress m_iface;
    // Routing Flags: valid, invalid or searching
    RouteFlags m_flag;

};

class RoutingTable
{
public:
    RoutingTable ();

    /**
     * Add Routing table entry if it doesn't exist in routing table
     * \param routingTableEntry 
     * \return true in success
     */
    bool
    AddRoute (RoutingTableEntry &routingTableEntry);
   
    /**
    * Delete existing routing table entry for destination address
    * \param dstAddr destination address
    * \return true on success
    */
    bool
    DeleteRoute (Ipv4Address dstAddr);

    /**
     * Lookup routing table entry with destination address dstAddr
     * \param dstAddr destination address
     * \param rtEntry routing table entry
     * \return true on success
     */
    bool
    LookupRoute (Ipv4Address dstAddr, RoutingTableEntry &rtEntry);
    bool
    LookupRoute (Ipv4Address id, RoutingTableEntry &rtEntry, bool forRouteInput);

    /**
     * Update route table entry with route table entry rtEntry
     * \param rtEntry routing table entry
     * \return true on success
     */
    bool
    Update (RoutingTableEntry &rtEntry);

    /**
     * Lookup list of address for which nextHop is next hop address
     * \param nextHop is address for which we want list of destination
     * \param dstList is list holding all destination addresses
     */
    void
    GetListOfDestinationWithNextHop (Ipv4Address nextHop, std::map<Ipv4Address, RoutingTableEntry> &dstList);

    /**
     * Lookup list of address in routing table
     * \param allRoutes is the list holding addresses present in nodes routing table
     */
    void
    GetListOfAllRoutes (std::map<Ipv4Address, RoutingTableEntry> &allRoutes);

    /**
     * Delete all routes for interface
     * \param iface interface to delete routing table
     */
    void 
    DeleteAllRouteFromInterface (Ipv4InterfaceAddress iface);

    // Delete all entries from routing table
    void
    Clear ()
    {
        m_ipv4AddressEntry.clear();
    }

    /// Print Routing Table
    void
    Print (Ptr<OutputStreamWrapper> stream) const;

    /// Provide number of routes in that nodes routing table
    uint32_t
    RoutingTableSize ();

    /**
     * Add an event for a destination address so update that the update to for that destination is sent
     * after the event is completed
     * \param address destination address for which this event is running
     * \param id unique eventid that was generated
     * \return true on success
     */
    bool
    AddIpv4Event (Ipv4Address address, EventId id);

    /**
     * Clear up entry from map after event is complete
     * \param address destination for which event is running
     * \return true on success
     */
    bool 
    DeleteIpv4Event (Ipv4Address address);

    /**
     * Check if event is running
     * \param address destination address for which event is running
     * \return true on success
     */
    bool
    AnyRunningEvent (Ipv4Address address);

    /**
     * Force Delete an update waiting for settling time to complete as better update to 
     * same address was recieved
     * \param address destination address for which event is running
     * \return true on success
     */
    bool
    ForceDeleteIpv4Event (Ipv4Address address);

    /**
     * Get eventid associated with that address
     * \param address destination address for which event is running
     * \return EventId on finding out an event associated 
     *         else return NULL
     */
    EventId
    GetEventId (Ipv4Address address);

    // Handle life time of invalid route
    Time
    GetHoldDownTime () const
    {
        return m_holdDownTime;
    }
    void
    SetHoldDownTime (Time t)
    {
        m_holdDownTime = t;
    }


private:
    // Fields
    /// an entry in routing table
    std::map<Ipv4Address, RoutingTableEntry> m_ipv4AddressEntry;
    ///an entry in event table
    std::map<Ipv4Address, EventId> m_ipv4Events;

    Time m_holdDownTime;
    
};

} /* namespace leach */
} /* namespace ns3 */

#endif /* LEACH_ROUTING_TABLE */
