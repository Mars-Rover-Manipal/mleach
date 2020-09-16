#ifndef WSN_APPLICATION_H
#define WSN_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"
#include "ns3/traced-value.h"
#include "LeachPacket.h"

namespace ns3 {

class Address;
class RandomVariableStream;
class Socket;
  
/**
 * \ingroup applications 
 * \defgroup WSN WsnApplication
 *
 * This traffic generator follows an On/Off pattern: after 
 * Application::StartApplication
 * is called, "On" and "Off" states alternate. The duration of each of
 * these states is determined with the onTime and the offTime random
 * variables. During the "Off" state, no traffic is generated.
 * During the "On" state, cbr traffic is generated. This cbr traffic is
 * characterized by the specified "data rate" and "packet size".
 */
/**
* \ingroup WSN
*
* \brief Generate traffic to a single destination according to an
*        WSN pattern.
*
* This traffic generator follows an On/Off pattern: after
* Application::StartApplication
* is called, "On" and "Off" states alternate. The duration of each of
* these states is determined with the onTime and the offTime random
* variables. During the "Off" state, no traffic is generated.
* During the "On" state, cbr traffic is generated. This cbr traffic is
* characterized by the specified "data rate" and "packet size".
*
* Note:  When an application is started, the first packet transmission
* occurs _after_ a delay equal to (packet size/bit rate).  Note also,
* when an application transitions into an off state in between packet
* transmissions, the remaining time until when the next transmission
* would have occurred is cached and is used when the application starts
* up again.  Example:  packet size = 1000 bits, bit rate = 500 bits/sec.
* If the application is started at time 3 seconds, the first packet
* transmission will be scheduled for time 5 seconds (3 + 1000/500)
* and subsequent transmissions at 2 second intervals.  If the above
* application were instead stopped at time 4 seconds, and restarted at
* time 5.5 seconds, then the first packet would be sent at time 6.5 seconds,
* because when it was stopped at 4 seconds, there was only 1 second remaining
* until the originally scheduled transmission, and this time remaining
* information is cached and used to schedule the next transmission
* upon restarting.
*
* If the underlying socket type supports broadcast, this application
* will automatically enable the SetAllowBroadcast(true) socket option.
*/
class WsnApplication : public Application 
{
public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);
  
    WsnApplication ();
  
    virtual ~WsnApplication();
  
    /**
     * \brief Set the total number of bytes to send.
     *
     * Once these bytes are sent, no packet is sent again, even in on state.
     * The value zero means that there is no limit.
     *
     * \param maxBytes the total number of bytes to send
     */
    void SetMaxBytes (uint64_t maxBytes);
  
    /// Get total count of packet generated
    uint32_t GetPktCount() const;
  
    /**
     * \brief Return a pointer to associated socket.
     * \return pointer to associated socket
     */
    Ptr<Socket> GetSocket (void) const;

protected:
    virtual void DoDispose (void);
private:
    // inherited from Application base class.
    virtual void StartApplication (void);    // Called at time specified by Start
    virtual void StopApplication (void);     // Called at time specified by Stop
  
    //helpers
    /**
     * \brief Cancel all pending events.
     */
    void CancelEvents ();
  
    // Event handlers
    /**
     * \brief Start an On period
     */
    void StartSending ();
    /**
     * \brief Send a packet
     */
    void SendPacket ();
  
    Ptr<Socket>     m_socket;       //!< Associated socket
    Address         m_peer;         //!< Peer address
    bool            m_connected;    //!< True if connected
    DataRate        m_cbrRate;      //!< Rate that data is generated
    DataRate        m_cbrRateFailSafe;      //!< Rate that data is generated (check copy)
    uint32_t        m_pktSize;      //!< Size of packets
    uint32_t        m_residualBits; //!< Number of generated, but not sent, bits
    Time            m_lastStartTime; //!< Time last packet sent
    uint64_t        m_maxBytes;     //!< Limit total number of bytes sent
    uint64_t        m_totBytes;     //!< Total bytes sent so far
    EventId         m_startStopEvent;     //!< Event id for next start or stop event
    EventId         m_sendEvent;    //!< Event id of pending "send packet" event
    TypeId          m_tid;          //!< Type of the socket used
    int64_t         m_pktDeadlineMin;  //!< Packet Expired Time Min
    int64_t         m_pktDeadlineLen;  //!< Packet Expired Time Len
    double          m_pktGenRate;   //!< Packet generation rate
    int             m_pktGenPattern;   //!< Packet generation distribution model
  
    TracedValue<uint32_t>      m_pktCount;     //!< Total packet count
  
    /// Traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet> > m_txTrace;

private:
    /**
     * \brief Schedule the next packet transmission
     */
    void ScheduleNextTx ();
    /**
     * \brief Handle a Connection Succeed event
     * \param socket the connected socket
     */
    void ConnectionSucceeded (Ptr<Socket> socket);
    /**
     * \brief Handle a Connection Failed event
     * \param socket the not connected socket
     */
    void ConnectionFailed (Ptr<Socket> socket);
};

} /* namespace ns3 */

#endif /* WSN_APPLICATION_H */

