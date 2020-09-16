#ifndef LEACH_HELPER_H
#define LEACH_HELPER_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"

namespace ns3 {
/**
 * \ingroup leach
 * \brief Helper Class that adds leach routing to nodes
 */
class LeachHelper : public Ipv4RoutingHelper
{
public:
    LeachHelper();
    ~LeachHelper();
    /**
     * \returns pointer to clone of this Helper
     *
     * This method is for internal use by other helpers
     * Clients are expected to free dynamic memory allocated by this method
     */
    LeachHelper* Copy (void) const;

    /**
     * \param node the node on which the routing protocol will run
     * \returns a newly-created routing protocol
     *
     * This method will be called by ns3::InternetStackHelper::Install
     */
    virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

    /**
     * \param name the name of the attribute to set
     * \param value the value of the attribute to set.
     *
     * This method controls the attributes of ns3::leach::RoutingProtocol
     */
    void Set (std::string name, const AttributeValue &value);

private:
    ObjectFactory m_agentFactory;
};
} /* namespace ns3 */

#endif /* LEACH_HELPER_H */
