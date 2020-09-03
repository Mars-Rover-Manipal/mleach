#ifndef LEACH_ROUTING_QUEUE_H
#define LEACH_ROUTING_QUEUE_H

#include <cstddef>
#include <vector>
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "LeachPacket.h"

#include <iostream>

namespace ns3 {
namespace leach {

/**
 * \ingroup leach
 * \brief LEACH queue entry
 */
class QueueEntry
{
public:
    typedef Ipv4RoutingProtocol::UnicastForwardCallback UnicastForwardCallback;

    // Constructor
    QueueEntry (Ptr<Packet> packet=0, Ipv4Header const &h = Ipv4Header())
        : m_packet (packet),
          m_header (h)
    {
        if (packet != 0)
        {
            Packet a (*packet);
            LeachHeader leachHeader;
            a.RemoveHeader(leachHeader);
            m_deadline = leachHeader.GetDeadline();
        }
    }

    /**
     * Compare queue entries 
     * \return true if equal
     */
    bool 
    operator== (QueueEntry const &o) const
    {
        return ((m_packet == o.m_packet) && (m_header.GetDestination() == o.m_header.GetDestination()));
    }

    // Fields
    Ptr<Packet> GetPacket() const { return m_packet; }
    void SetPacket(Ptr<Packet> packet) { m_packet = packet;}
    Ipv4Header GetIpv4Header() const {return m_header;}
    void SetIpv4Header(Ipv4Header header) { m_header = header;}
    Time GetDeadline() const {return m_deadline;}
    void SetDeadline(Time deadline) {m_deadline = deadline;}

private:
    // Data Packet
    Ptr<Packet> m_packet;
    // Ipv4 Header
    Ipv4Header m_header;
    // Deadline
    Time m_deadline;
};


/**
 * \ingroup leach
 * \brief LEACH packet queue
 *
 * When route is not available, packets are queued. Every node can buffer upto 5 packets
 * per destination. We will drop the first queued packet if the buffer is full.
 */
class PacketQueue
{
public:
    PacketQueue()
    {
    }
    
    /// Push entry in queue, if there is no entry with same packet and destination address in queue
    bool Enqueue (QueueEntry &entry);
    /// Return earliest entry for given destination
    bool Dequeue (Ipv4Address dst, QueueEntry &entry);
    /// Find is packet with given destination address exists in queue
    bool Find (Ipv4Address dst);
    /// Drop idx-th entry
    void Drop (uint32_t idx);
    /// Get count of packets with destination address dst
    uint32_t GetCountForPacketsWithDst (Ipv4Address dst);
    /// Number of entries
    uint32_t GetSize();

    // Fields
    Time GetQueueTimeout() const {return m_queueTimeout;}
    void SetQueueTimeout(Time t) {m_queueTimeout = t;}
    QueueEntry& operator[] (size_t idx) {return m_queue[idx];}


private:
    std::vector<QueueEntry> m_queue;
    // Max period of time that a routing protocol is allowed to buffer a packet (seconds)
    Time m_queueTimeout;
    static bool 
    IsEqual (QueueEntry en, const Ipv4Address dst)
    {
        return (en.GetIpv4Header().GetDestination() == dst);
    }
};

} /* namespace leach */
} /* namespace ns3 */

#endif /* LEACH_ROUTING_QUEUE_H */
