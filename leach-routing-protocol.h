#ifndef LEACH_ROUTING_PROTOCOL_H
#define LEACH_ROUTING_PROTOCOL_H
 
#include <vector>

#include "leach-routing-queue.h"
#include "leach-routing-table.h"
#include "LeachPacket.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/vector.h"
#include "ns3/traced-value.h"

namespace ns3 {
namespace leach {

// Timeline
struct msmt 
{
    Time begin;
    Time end;
};

/**
 * \ingroup leach
 * \brief LEACH routing protocol
 */
class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
    static TypeId GetTypeId(void);
    static const uint32_t LEACH_PORT;

    // c-tor
    RoutingProtocol();
    virtual ~RoutingProtocol();
    virtual void DoDispose();

    /**
     * \brief Query routing cache for an existing route, for an outbound packet
     *
     * This lookup is used by transport protocols.  It does not cause any
     * packet to be forwarded, and is synchronous.  Can be used for
     * multicast or unicast.  The Linux equivalent is ip_route_output()
     *
     * The header input parameter may have an uninitialized value
     * for the source address, but the destination address should always be 
     * properly set by the caller.
     *
     * \param p packet to be routed.  Note that this method may modify the packet.
     *          Callers may also pass in a null pointer. 
     * \param header input parameter (used to form key to search for the route)
     * \param oif Output interface Netdevice.  May be zero, or may be bound via
     *            socket options to a particular output interface.
     * \param sockerr Output parameter; socket errno 
     *
     * \returns a code that indicates what happened in the lookup
     */
    Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);

    /**
     * \brief Route an input packet (to be forwarded or locally delivered)
     *
     * This lookup is used in the forwarding process.  The packet is
     * handed over to the Ipv4RoutingProtocol, and will get forwarded onward
     * by one of the callbacks.  The Linux equivalent is ip_route_input().
     * There are four valid outcomes, and a matching callbacks to handle each.
     *
     * \param p received packet
     * \param header input parameter used to form a search key for a route
     * \param idev Pointer to ingress network device
     * \param ucb Callback for the case in which the packet is to be forwarded
     *            as unicast
     * \param mcb Callback for the case in which the packet is to be forwarded
     *            as multicast
     * \param lcb Callback for the case in which the packet is to be locally
     *            delivered
     * \param ecb Callback to call if there is an error in forwarding
     * \returns true if the Ipv4RoutingProtocol takes responsibility for 
     *          forwarding or delivering the packet, false otherwise
     */ 
    bool RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev, 
                              UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                              LocalDeliverCallback lcb, ErrorCallback ecb);

    /**
     * \param interface the index of the interface we are being notified about
     *
     * Protocols are expected to implement this method to be notified of the state change of
     * an interface in a node.
     */
    virtual void NotifyInterfaceUp (uint32_t interface);

    /**
     * \param interface the index of the interface we are being notified about
     *
     * Protocols are expected to implement this method to be notified of the state change of
     * an interface in a node.
     */
    virtual void NotifyInterfaceDown (uint32_t interface);
  
    /**
     * \param interface the index of the interface we are being notified about
     * \param address a new address being added to an interface
     *
     * Protocols are expected to implement this method to be notified whenever
     * a new address is added to an interface. Typically used to add a 'network route' on an
     * interface. Can be invoked on an up or down interface.
     */
    virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  
    /**
     * \param interface the index of the interface we are being notified about
     * \param address a new address being added to an interface
     *
     * Protocols are expected to implement this method to be notified whenever
     * a new address is removed from an interface. Typically used to remove the 'network route' of an
     * interface. Can be invoked on an up or down interface.
     */
    virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  
    /**
     * \param ipv4 the ipv4 object this routing protocol is being associated with
     * 
     * Typically, invoked directly or indirectly from ns3::Ipv4::SetRoutingProtocol
     */
    virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  
    /**
     * \brief Print the Routing Table entries
     *
     * \param stream The ostream the Routing table is printed to
     */
    virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;

    // Methods to handle protocol parameters
    void SetPosition (Vector pos);
    Vector GetPosition () const;
    void SetAcceleration (Vector accel);
    Vector GetAcceleration () const;
    void SetPIR (BooleanValue pir);
    BooleanValue GetPIR () const;

    std::vector<struct msmt>* getTimeline()
    {
        return &timeline;
    }
    std::vector<Time>* getTxTime();

    /**
     * Assign a fixed random variable stream number to the random variables
     * used by this model.  Return the number of streams (possibly zero) that
     * have been assigned.
     *
     * \param stream first stream index to use
     * \return the number of stream indices assigned by this model
     */
    int64_t AssignStreams (int64_t stream);

private:
    
    // Protocol Parameters
    /**
     * Holdtime is multiplicative factor of PeriodicUpdateInterval for which the node waits sinec last update 
     * before flushing a route from the routing table. 
     * Example: If PeriodicUpdateInterval = 8s and Holdtime = 3s, the node waits for 24s since last update 
     * to flush this route from its routing table.
     */
    uint32_t Round;
    uint32_t valid;
    uint32_t clusterHeadThisRound;
    uint32_t isSink;
    TracedValue<uint32_t> m_dropped;
    // Packet generation rate
    double m_lambda;

    struct hash
    {
        uint32_t uid;
        Ptr<Packet> p;
        struct hash* next;
    }*m_hash[1021];

    std::vector<struct msmt> timeline;
    std::vector<Time> tx_time;
    
    /// PeriodicUpdateInterval specifies the periodic time interval between which a node broadcasts
    /// its entire routing table
    Time m_periodicUpdateInterval;
    /// Node IP address
    Ipv4Address m_mainAddress;
    /// Cluster Head/Sink address
    Ipv4Address m_targetAddress;
    /// Ultimate Sink/Base Station address
    Ipv4Address m_sinkAddress;
    /// Closest Distance node
    double m_dist;
    /// Cluster memeber list
    std::vector<Ipv4Address> m_clusterMember;
    /// IP protocol
    Ptr<Ipv4> m_ipv4;
    /// Raw socket per each IP interface, map socket -> iface address (IP + mask)
    std::map<Ptr<Socket>, Ipv4InterfaceAddress> m_socketAddress;
    /// Loopback device used to defer route requests until route is found 
    Ptr<NetDevice> m_lo;
    /// Main Routing Table for the node
    RoutingTable m_routingTable;
    /// From selecting CHs, best stores here
    RoutingTableEntry m_bestRoute;
    /// Node Position
    Vector m_position;
    /// Node Acceleration
    Vector m_acceleration;
    /// Node PIR
    BooleanValue m_PIR;
    /// Queue used by routing layer to buffer packets to which it does not have route
    PacketQueue m_queue;    
    /// Unicast call for own packet
    UnicastForwardCallback m_selfCallback;
    /// Error callback for own packet
    ErrorCallback m_errorCallback;

private:
    /// Start protocol
    void
    Start();
    /// Queue Packet till route is found
    void
    EnqueuePacket (Ptr<Packet> p, const Ipv4Header &header);
    /// Decide wether to send packet in buffer
    bool
    DataAggregation (Ptr<Packet> p);
    bool
    Proposal (Ptr<Packet> p);
    bool
    OptTM (Ptr<Packet> p);
    bool
    ControlLimit (Ptr<Packet> p);
    bool
    SelectiveForwarding (Ptr<Packet> p);

    /// De-Aggregate chunks of data
    bool
    DeAggregate (Ptr<Packet> in, Ptr<Packet> &out, LeachHeader&);

    /// Find Socket with local interface address iface
    Ptr<Socket>
    FindSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;
    /// Find Socket with local address iface
    Ptr<Socket>
    FindSocketWithAddress (Ipv4Address iface) const;

    ///Receive LeachControl packets
    ///Receive and process leach control packets
    void
    RecvLeach (Ptr<Socket> socket);

    void
    Send (Ptr<Ipv4Route>, Ptr<const Packet>, const Ipv4Header&);
    /// Create Loopback Route for given header
    Ptr<Ipv4Route>
    LoopbackRoute (const Ipv4Header &header, Ptr<NetDevice> oif) const;
    /// Triggered by timer, sent 1s after cluster head is elected
    void
    SendBroadcast();
    /// Select cluster head selection
    void
    PeriodicUpdate();
    /// Cluster members tell cluster head 
    void 
    RespondToClusterHead ();
#ifndef DA
    /// Deal with no DA
    void
    EnqueueForNoDA (UnicastForwardCallback ucb, Ptr<Ipv4Route> route, Ptr<const Packet> p, const Ipv4Header &header);
    void
    AutoDequeueNoDA();
    struct DeferredPack
    {
        UnicastForwardCallback ucb;
        Ptr<Ipv4Route> route;
        Ptr<const Packet> p;
        Ipv4Header header;
    };
    std::vector<struct DeferredPack> DeferredQueue;
#endif

    /// Notify if packet is dropped
    void 
    Drop (Ptr<const Packet>, const Ipv4Header &, Socket::SocketErrno);

    /// Timer to trigger periodic updates 
    Timer m_periodicUpdateTimer;
    /// Timer used by the trigger updates in case of Weighted Settling Time is used
    Timer m_broadcastClusterHeadTimer;
    /// Timer to feed cluster head its members
    Timer m_respondToClusterHeadTimer;
    /// Provide uniform random variables
    Ptr<UniformRandomVariable> m_uniformRandomVariable;
};
    

} /* namespace leach */
} /* namespace ns3 */

#endif /* LEACH_ROUTING_PROTOCOL_H */
