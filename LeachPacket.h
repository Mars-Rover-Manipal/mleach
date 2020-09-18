#ifndef LEACH_PACKET_H
#define LEACH_PACKET_H

#include <iostream>
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/vector.h"
#include "ns3/boolean.h"

namespace ns3 {
namespace leach {
/**
 * \ingroup leach
 * \brief LEACH Update Packet Format
 * \verbatim
 |       0       |       2       |       4       |       6       |
  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Boolean pir                         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Vector .x (Position)                |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Vector .y (Position)                |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Vector .z (Position)                |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Vector .x (Acceleration)            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Vector .y (Acceleration)            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Vector .z (Acceleration)            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |        Member IP to be        |                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                           Deadline                            |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */

class LeachHeader : public Header
{
public:
    LeachHeader (BooleanValue PIR = BooleanValue(false), Vector position = Vector(0.0, 0.0, 0.0), Vector acceleration = Vector(0.0, 0.0, 0.0), Ipv4Address address = Ipv4Address("255.255.255.255"), Time m = Time(0));
    virtual ~LeachHeader ();
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const;
    virtual uint32_t GetSerializedSize () const;
    virtual void Serialize (Buffer::Iterator start) const;
    virtual uint32_t Deserialize (Buffer::Iterator start);
    virtual void Print (std::ostream &os) const;

public:
    void
    SetPIR (BooleanValue PIR)
    {
        m_PIR = PIR;
    }
    BooleanValue
    GetPIR () const
    {
        return m_PIR;
    }

    void
    SetPosition (Vector position)
    {
        m_position = position;
    }
    Vector
    GetPosition () const
    {
        return m_position;
    }

    void
    SetAcceleration (Vector acceleration)
    {
        m_acceleration = acceleration;
    }
    Vector 
    GetAcceleration () const
    {
        return m_acceleration;
    }

    void
    SetAddress (Ipv4Address address)
    {
        m_address = address;
    }
    Ipv4Address
    GetAddress () const
    {
        return m_address;
    }

    void
    SetDeadline (Time t)
    {
        m_deadline = t;
    }
    Time
    GetDeadline () const
    {
        return m_deadline;
    }

    void
    TestLeachPacket () const
    {
        std::cout << "TestLeachPacket: " << &m_address << ", " << &m_PIR << &m_position << ", " << &m_acceleration << ", " << &m_deadline << ", " << sizeof(m_deadline) << ", " << sizeof(*this) << std::endl;
    }

private:
    BooleanValue m_PIR;     ///< (True/False)
    Vector m_position;      ///< (X, Y, Z) Position
    Vector m_acceleration;  ///< (X, Y< Z) Acceleration
    Ipv4Address m_address;
    Time m_deadline;
};

inline std::ostream & operator<<(std::ostream& os, const LeachHeader &packet)
{
    packet.Print(os);
    return os;
}

} /* namespace leach */
} /* namespace ns3   */

#endif /* LEACH_PACKET_H */
