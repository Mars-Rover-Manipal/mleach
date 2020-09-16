#include "leach-helper.h"
#include "leach-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"

namespace ns3 {
LeachHelper::~LeachHelper ()
{
}

LeachHelper::LeachHelper () : Ipv4RoutingHelper ()
{
    m_agentFactory.SetTypeId ("ns3::leach::RoutingProtocol");
}

LeachHelper*
LeachHelper::Copy (void) const
{
    return new LeachHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
LeachHelper::Create (Ptr<Node> node) const
{
    Ptr<leach::RoutingProtocol> agent = m_agentFactory.Create<leach::RoutingProtocol> ();
    node->AggregateObject (agent);
    return agent;
}

void
LeachHelper::Set (std::string name, const AttributeValue &value)
{
    m_agentFactory.Set (name, value);
}

} /* namespace ns3 */

