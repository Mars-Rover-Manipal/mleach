#include "LeachPacket.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"
#include "ns3/vector.h"

namespace ns3 {
namespace leach {

NS_OBJECT_ENSURE_REGISTERED(LeachHeader);

#if 1
//LeachHeader::LeachHeader (BooleanValue PIR, Vector position, Vector acceleration, Ipv4Address address, Time m)
LeachHeader::LeachHeader (BooleanValue PIR, Vector position, Vector acceleration, Ipv4Address address, Time m) :
    m_PIR (PIR),
    m_position (position),
    m_acceleration (acceleration),
    m_address (address),
    m_deadline (m)
{
}
#endif

LeachHeader::~LeachHeader ()
{
}

TypeId 
LeachHeader::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::leach::LeachHeader")
        .SetParent<Header> ()
        .SetGroupName("Leach")
        .AddConstructor<LeachHeader>();
    return tid;
}

TypeId 
LeachHeader::GetInstanceTypeId () const
{
    return GetTypeId ();
}

uint32_t
LeachHeader::GetSerializedSize () const
{
    return sizeof(m_PIR)+sizeof(m_position)+sizeof(m_acceleration)+sizeof(m_address)+sizeof(m_deadline);
}

void 
LeachHeader::Serialize (Buffer::Iterator i) const
{
    i.Write ((const uint8_t*)&m_PIR,            sizeof(m_PIR));
    i.Write ((const uint8_t*)&m_acceleration,   sizeof(m_acceleration));
    i.Write ((const uint8_t*)&m_position,       sizeof(m_position));
    i.Write ((const uint8_t*)&m_address,        sizeof(m_address));
    i.Write ((const uint8_t*)&m_address+4,      sizeof(m_address));
    i.Write ((const uint8_t*)&m_deadline,       sizeof(m_deadline));
}

uint32_t
LeachHeader::Deserialize (Buffer::Iterator start)
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
LeachHeader::Print(std::ostream &os) const
{
  os << " PIR: "            << m_PIR
     << " Position: "       << m_position 
     << " Acceleration: "   << m_acceleration 
     << ", IP: "            << m_address 
     << ", Deadline:"       << m_deadline 
     << "\n";
}

}  /* namespace leach */
}  /* namespace ns3   */

