/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 Stanford University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Serhat Arslan <sarslan@stanford.edu>
 */

#ifndef HOMA_SOCKET_H
#define HOMA_SOCKET_H

#include <stdint.h>
#include <queue>

#include "ns3/socket.h"
#include "ns3/traced-callback.h"
#include "ns3/callback.h"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface.h"
#include "icmpv4.h"

namespace ns3 {
    
class Node;
class Packet;
class Ipv4EndPoint;
class Ipv6EndPoint;
class HomaL4Protocol;

/**
 * \ingroup socket
 * \ingroup homa
 *
 * \brief A sockets interface to HOMA
 * 
 * This class provides a socket interface
 * to ns3's implementation of Homa.
 *
 * For IPv4 packets, the TOS is set according to the following rules:
 * - if the socket is connected, the TOS set for the socket is used
 * - if the socket is not connected, the TOS specified in the destination address
 *   passed to SendTo is used, while the TOS set for the socket is ignored
 * In both cases, a SocketIpTos tag is only added to the packet if the resulting
 * TOS is non-null. The Bind and Connect operations set the TOS for the
 * socket to the value specified in the provided address.
 * If the TOS determined for a packet (as described above) is not null, the
 * packet is assigned a priority based on that TOS value (according to the
 * Socket::IpTos2Priority function). Otherwise, the priority set for the
 * socket is assigned to the packet. Setting a TOS for a socket also sets a
 * priority for the socket (according to the Socket::IpTos2Priority function).
 * A SocketPriority tag is only added to the packet if the resulting priority
 * is non-null.
 */
class HomaSocket : public Socket
{
public:
  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  
  HomaSocket (void);
  virtual ~HomaSocket (void);

  /**
   * \brief Set the associated node.
   * \param node the node
   */
  void SetNode (Ptr<Node> node);
  /**
   * \brief Set the associated HOMA L4 protocol.
   * \param homa the HOMA L4 protocol
   */
  void SetHoma (Ptr<HomaL4Protocol> homa);

  virtual enum SocketErrno GetErrno (void) const;
  virtual enum SocketType GetSocketType (void) const;
  
  virtual Ptr<Node> GetNode (void) const;
  
  virtual int Bind (void);
  virtual int Bind6 (void); // Bind for IPv6 endpoints (not supported)
  virtual int Bind (const Address &address);
  
  virtual int ShutdownSend (void);
  virtual int ShutdownRecv (void);
  
  virtual int Close (void);
  
  virtual int Connect (const Address &address);
  
  virtual int Listen (void);
  
  virtual uint32_t GetTxAvailable (void) const;
  virtual int Send (Ptr<Packet> p, uint32_t flags);
  virtual int SendTo (Ptr<Packet> p, uint32_t flags, const Address &address);
  
  virtual uint32_t GetRxAvailable (void) const;
  virtual Ptr<Packet> Recv (uint32_t maxSize, uint32_t flags);
  virtual Ptr<Packet> RecvFrom (uint32_t maxSize, uint32_t flags,
                                Address &fromAddress);
                                
  virtual int GetSockName (Address &address) const; 
  virtual int GetPeerName (Address &address) const;
  
  /**
   * \brief Corresponds to socket option MCAST_JOIN_GROUP
   *
   * \param interface interface number, or 0
   * \param groupAddress multicast group address
   * \returns on success, zero is returned.  On error, -1 is returned,
   *          and errno is set appropriately
   *
   * Enable reception of multicast datagrams for this socket on the
   * interface number specified.  If zero is specified as
   * the interface, then a single local interface is chosen by
   * system.  In the future, this function will generate trigger IGMP
   * joins as necessary when IGMP is implemented, but for now, this
   * just enables multicast datagram reception in the system if not already
   * enabled for this interface/groupAddress combination.
   *
   * \attention IGMP is not yet implemented in ns-3
   *
   * This function may be called repeatedly on a given socket but each
   * join must be for a different multicast address, or for the same
   * multicast address but on a different interface from previous joins.
   * This enables host multihoming, and the ability to join the same 
   * group on different interfaces.
   */
  virtual int MulticastJoinGroup (uint32_t interface, const Address &groupAddress);
  /**
   * \brief Corresponds to socket option MCAST_LEAVE_GROUP
   *
   * \param interface interface number, or 0
   * \param groupAddress multicast group address
   * \returns on success, zero is returned.  On error, -1 is returned,
   *          and errno is set appropriately
   *
   * Disable reception of multicast datagrams for this socket on the
   * interface number specified.  If zero is specified as
   * the interfaceIndex, then a single local interface is chosen by
   * system.  In the future, this function will generate trigger IGMP
   * leaves as necessary when IGMP is implemented, but for now, this
   * just disables multicast datagram reception in the system if this
   * socket is the last for this interface/groupAddress combination.
   *
   * \attention IGMP is not yet implemented in ns-3
   */
  virtual int MulticastLeaveGroup (uint32_t interface, const Address &groupAddress);
  
  virtual void BindToNetDevice (Ptr<NetDevice> netdevice);
  
  virtual bool SetAllowBroadcast (bool allowBroadcast);
  virtual bool GetAllowBroadcast () const;

private:
  /**
   * \brief Kill this socket by zeroing its attributes (IPv4)
   *
   * This is a callback function configured to m_endpoint in
   * SetupCallback(), invoked when the endpoint is destroyed.
   */
  void Destroy (void);

  /**
   * \brief Deallocate m_endPoint and m_endPoint6
   */
  void DeallocateEndPoint (void);

  /**
   * Finish the binding process
   * \returns 0 on success, -1 on failure
   */
  int FinishBind (void);

  /**
   * \brief Send a packet
   * \param p packet
   * \returns 0 on success, -1 on failure
   */
  int DoSend (Ptr<Packet> msg);
  /**
   * \brief Send a packet to a specific destination and port (IPv4)
   * \param p packet
   * \param daddr destination address
   * \param dport destination port
   * \param tos ToS
   * \returns 0 on success, -1 on failure
   */
  int DoSendTo (Ptr<Packet> msg, Ipv4Address daddr, uint16_t dport, uint8_t tos);

  /**
   * \brief Called by the L3 protocol when it received a packet to pass on to TCP.
   *
   * \param packet the incoming packet
   * \param header the packet's IPv4 header
   * \param port the remote port
   * \param incomingInterface the incoming interface
   */
  void ForwardUp (Ptr<Packet> msg, Ipv4Header header, uint16_t port, Ptr<Ipv4Interface> incomingInterface);

  /**
   * \brief Called by the L3 protocol when it received an ICMP packet to pass on to TCP.
   *
   * \param icmpSource the ICMP source address
   * \param icmpTtl the ICMP Time to Live
   * \param icmpType the ICMP Type
   * \param icmpCode the ICMP Code
   * \param icmpInfo the ICMP Info
   */
  void ForwardIcmp (Ipv4Address icmpSource, uint8_t icmpTtl, uint8_t icmpType, uint8_t icmpCode, uint32_t icmpInfo);

  // Indirect the attribute setting and getting through private virtual methods
  /**
   * \brief Set the receiving buffer size
   * \param size the buffer size
   */
  virtual void SetRcvBufSize (uint32_t size);
  /**
   * \brief Get the receiving buffer size
   * \returns the buffer size
   */
  virtual uint32_t GetRcvBufSize (void) const;
  /**
   * \brief Set the IP multicast TTL
   * \param ipTtl the IP multicast TTL
   */
  virtual void SetIpMulticastTtl (uint8_t ipTtl);
  /**
   * \brief Get the IP multicast TTL
   * \returns the IP multicast TTL
   */
  virtual uint8_t GetIpMulticastTtl (void) const;
  /**
   * \brief Set the IP multicast interface
   * \param ipIf the IP multicast interface
   */
  virtual void SetIpMulticastIf (int32_t ipIf);
  /**
   * \brief Get the IP multicast interface
   * \returns the IP multicast interface
   */
  virtual int32_t GetIpMulticastIf (void) const;
  /**
   * \brief Set the IP multicast loop capability
   *
   * This means that the socket will receive the packets
   * sent by itself on a multicast address.
   * Equivalent to setsockopt  IP_MULTICAST_LOOP
   *
   * \param loop the IP multicast loop capability
   */
  virtual void SetIpMulticastLoop (bool loop);
  /**
   * \brief Get the IP multicast loop capability
   *
   * This means that the socket will receive the packets
   * sent by itself on a multicast address.
   * Equivalent to setsockopt  IP_MULTICAST_LOOP
   *
   * \returns the IP multicast loop capability
   */
  virtual bool GetIpMulticastLoop (void) const;
  /**
   * \brief Set the MTU discover capability
   *
   * \param discover the MTU discover capability
   */
  virtual void SetMtuDiscover (bool discover);
  /**
   * \brief Get the MTU discover capability
   *
   * \returns the MTU discover capability
   */
  virtual bool GetMtuDiscover (void) const;

  /**
   * \brief HomaSocketFactory friend class.
   * \relates HomaSocketFactory
   */
  friend class HomaSocketFactory;
  // invoked by Homa class

  // Connections to other layers of TCP/IP
  Ipv4EndPoint*        m_endPoint;   //!< the IPv4 endpoint
  Ptr<Node>            m_node;       //!< the associated node
  Ptr<HomaL4Protocol>  m_homa;       //!< the associated HOMA L4 protocol
  Callback<void, Ipv4Address,uint8_t,uint8_t,uint8_t,uint32_t> m_icmpCallback;  //!< ICMP callback

  Address m_defaultAddress; //!< Default peer address
  uint16_t m_defaultPort;   //!< Default peer port
  TracedCallback<Ptr<const Packet> > m_dropTrace; //!< Trace for dropped packets

  mutable enum SocketErrno m_errno;           //!< Socket error code
  bool                     m_shutdownSend;    //!< Send no longer allowed
  bool                     m_shutdownRecv;    //!< Receive no longer allowed
  bool                     m_connected;       //!< Connection established
  bool                     m_allowBroadcast;  //!< Allow send broadcast packets

  std::queue<std::pair<Ptr<Packet>, Address> > m_deliveryQueue; //!< Queue for incoming packets
  uint32_t m_rxAvailable;                   //!< Number of available bytes to be received

  // Socket attributes
  uint32_t m_rcvBufSize;    //!< Receive buffer size
  uint8_t m_ipMulticastTtl; //!< Multicast TTL
  int32_t m_ipMulticastIf;  //!< Multicast Interface
  bool m_ipMulticastLoop;   //!< Allow multicast loop
  bool m_mtuDiscover;       //!< Allow MTU discovery
};
    
} // namespace ns3

#endif /* HOMA_SOCKET_H */