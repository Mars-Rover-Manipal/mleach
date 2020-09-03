#include <algorithm>
#include <bits/stdint-uintn.h>
#include <functional>
#include <vector>

#include "leach-routing-queue.h"
#include "ns3/ipv4-route.h"
#include "ns3/log-macros-disabled.h"
#include "ns3/socket.h"
#include "ns3/log.h"

namespace ns3 {
    
NS_LOG_COMPONENT_DEFINE("LeachPacketQueue");

namespace leach {

uint32_t
PacketQueue::GetSize()
{
    return m_queue.size();
}

bool
PacketQueue::Enqueue(QueueEntry &entry)
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
PacketQueue::Drop(uint32_t idx)
{
    m_queue.erase(m_queue.begin()+idx);
}

bool
PacketQueue::Dequeue(Ipv4Address dst, QueueEntry &entry)
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
PacketQueue::Find(Ipv4Address dst)
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
PacketQueue::GetCountForPacketsWithDst(Ipv4Address dst)
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

} /* namespace leach */
} /* namespace ns3 */

