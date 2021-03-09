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

#include <limits>

#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/boolean.h"
#include "ns3/trace-source-accessor.h"
#include "homa-socket.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/homa-socket-factory.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "homa-l4-protocol.h"
#include "ipv4-end-point.h"

#include "ns3/homa-header.h"

namespace ns3 {
    
NS_LOG_COMPONENT_DEFINE ("HomaSocket");

NS_OBJECT_ENSURE_REGISTERED (HomaSocket);

static const uint32_t MAX_IPV4_HOMA_MESSAGE_SIZE = std::numeric_limits<uint32_t>::max(); //!< Maximum HOMA message size

TypeId
HomaSocket::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaSocket")
    .SetParent<Socket> ()
    .SetGroupName ("Internet")
    .AddAttribute ("RcvBufSize",
                   "HomaSocket maximum receive buffer size (bytes)",
                   UintegerValue (MAX_IPV4_HOMA_MESSAGE_SIZE),
                   MakeUintegerAccessor (&HomaSocket::GetRcvBufSize,
                                         &HomaSocket::SetRcvBufSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("IpTtl",
                   "socket-specific TTL for unicast IP packets (if non-zero)",
                   UintegerValue (0),
                   MakeUintegerAccessor (&HomaSocket::GetIpTtl,
                                         &HomaSocket::SetIpTtl),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("IpMulticastTtl",
                   "socket-specific TTL for multicast IP packets (if non-zero)",
                   UintegerValue (0),
                   MakeUintegerAccessor (&HomaSocket::GetIpMulticastTtl,
                                         &HomaSocket::SetIpMulticastTtl),
                   MakeUintegerChecker<uint8_t> ())
    .AddAttribute ("IpMulticastIf",
                   "interface index for outgoing multicast on this socket; -1 indicates to use default interface",
                   IntegerValue (-1),
                   MakeIntegerAccessor (&HomaSocket::GetIpMulticastIf,
                                        &HomaSocket::SetIpMulticastIf),
                   MakeIntegerChecker<int32_t> ())
    .AddAttribute ("IpMulticastLoop",
                   "whether outgoing multicast sent also to loopback interface",
                   BooleanValue (false),
                   MakeBooleanAccessor (&HomaSocket::GetIpMulticastLoop,
                                        &HomaSocket::SetIpMulticastLoop),
                   MakeBooleanChecker ())
    .AddAttribute ("MtuDiscover", "If enabled, every outgoing ip packet will have the DF flag set.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&HomaSocket::SetMtuDiscover,
                                        &HomaSocket::GetMtuDiscover),
                   MakeBooleanChecker ())
    .AddAttribute ("IcmpCallback", "Callback invoked whenever an icmp error is received on this socket.",
                   CallbackValue (),
                   MakeCallbackAccessor (&HomaSocket::m_icmpCallback),
                   MakeCallbackChecker ())
    .AddTraceSource ("Drop", "Drop HOMA packet due to receive buffer overflow",
                     MakeTraceSourceAccessor (&HomaSocket::m_dropTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

HomaSocket::HomaSocket ()
  : m_endPoint (0),
    m_node (0),
    m_homa (0),
    m_errno (ERROR_NOTERROR),
    m_shutdownSend (false),
    m_shutdownRecv (false),
    m_connected (false),
    m_rxAvailable (0)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_allowBroadcast = false;
}

HomaSocket::~HomaSocket ()
{
  NS_LOG_FUNCTION_NOARGS ();

  // TODO: leave any multicast groups that have been joined
  m_node = 0;
  /**
   * Note: actually this function is called AFTER
   * HomaSocket::Destroy so the code below is unnecessary in normal operations
   */
  if (m_endPoint != 0)
    {
      NS_ASSERT (m_homa != 0);
      /**
       * Note that this piece of code is a bit tricky:
       * when DeAllocate is called, it will call into
       * Ipv4EndPointDemux::Deallocate which triggers
       * a delete of the associated endPoint which triggers
       * in turn a call to the method HomaSocket::Destroy below
       * will will zero the m_endPoint field.
       */
      NS_ASSERT (m_endPoint != 0);
      m_homa->DeAllocate (m_endPoint);
      NS_ASSERT (m_endPoint == 0);
    }
  m_homa = 0;
}
    
void 
HomaSocket::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_node = node;

}
    
void 
HomaSocket::SetHoma (Ptr<HomaL4Protocol> homa)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_homa = homa;
}
    
enum Socket::SocketErrno
HomaSocket::GetErrno (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_errno;
}
    
enum Socket::SocketType
HomaSocket::GetSocketType (void) const
{
  return NS3_SOCK_SEQPACKET;
}
    
Ptr<Node>
HomaSocket::GetNode (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_node;
}
    
void 
HomaSocket::Destroy (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_endPoint = 0;
}
    
/* Deallocate the end point and cancel all the timers */
void
HomaSocket::DeallocateEndPoint (void)
{
  if (m_endPoint != 0)
    {
      m_endPoint->SetDestroyCallback (MakeNullCallback<void> ());
      m_homa->DeAllocate (m_endPoint);
      m_endPoint = 0;
    }
}
    
int
HomaSocket::FinishBind (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  bool done = false;
  if (m_endPoint != 0)
    {
      m_endPoint->SetRxCallback (MakeCallback (&HomaSocket::ForwardUp, Ptr<HomaSocket> (this)));
      m_endPoint->SetIcmpCallback (MakeCallback (&HomaSocket::ForwardIcmp, Ptr<HomaSocket> (this)));
      m_endPoint->SetDestroyCallback (MakeCallback (&HomaSocket::Destroy, Ptr<HomaSocket> (this)));
      done = true;
    }
  if (done)
    {
      return 0;
    }
  return -1;
}
    
int
HomaSocket::Bind (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_endPoint = m_homa->Allocate ();
  if (m_boundnetdevice)
    {
      m_endPoint->BindToNetDevice (m_boundnetdevice);
    }
  return FinishBind ();
}
    
int
HomaSocket::Bind6 (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_FATAL_ERROR_CONT("HomaSocket currently doesn't support IPv6. Use IPv4 instead.");
  return -1;
}
    
int 
HomaSocket::Bind (const Address &address)
{
  NS_LOG_FUNCTION (this << address);

  if (InetSocketAddress::IsMatchingType (address))
    {
      NS_ASSERT_MSG (m_endPoint == 0, "Endpoint already allocated.");

      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      Ipv4Address ipv4 = transport.GetIpv4 ();
      uint16_t port = transport.GetPort ();
      SetIpTos (transport.GetTos ());
      if (ipv4 == Ipv4Address::GetAny () && port == 0)
        {
          m_endPoint = m_homa->Allocate ();
        }
      else if (ipv4 == Ipv4Address::GetAny () && port != 0)
        {
          m_endPoint = m_homa->Allocate (GetBoundNetDevice (), port);
        }
      else if (ipv4 != Ipv4Address::GetAny () && port == 0)
        {
          m_endPoint = m_homa->Allocate (ipv4);
        }
      else if (ipv4 != Ipv4Address::GetAny () && port != 0)
        {
          m_endPoint = m_homa->Allocate (GetBoundNetDevice (), ipv4, port);
        }
      if (0 == m_endPoint)
        {
          m_errno = port ? ERROR_ADDRINUSE : ERROR_ADDRNOTAVAIL;
          return -1;
        }
      if (m_boundnetdevice)
        {
          m_endPoint->BindToNetDevice (m_boundnetdevice);
        }

    }
  else
    {
      NS_LOG_ERROR ("Not IsMatchingType");
      m_errno = ERROR_INVAL;
      return -1;
    }

  return FinishBind ();
}
    
int 
HomaSocket::ShutdownSend (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_shutdownSend = true;
  return 0;
}
    
int 
HomaSocket::ShutdownRecv (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_shutdownRecv = true;
  if (m_endPoint)
    {
      m_endPoint->SetRxEnabled (false);
    }
  return 0;
}
    
int
HomaSocket::Close (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (m_shutdownRecv == true && m_shutdownSend == true)
    {
      m_errno = Socket::ERROR_BADF;
      return -1;
    }
  m_shutdownRecv = true;
  m_shutdownSend = true;
  DeallocateEndPoint ();
  return 0;
}
    
int
HomaSocket::Connect (const Address & address)
{
  NS_LOG_FUNCTION (this << address);
  if (InetSocketAddress::IsMatchingType(address) == true)
    {
      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      m_defaultAddress = Address(transport.GetIpv4 ());
      m_defaultPort = transport.GetPort ();
      SetIpTos (transport.GetTos ());
      m_connected = true;
      NotifyConnectionSucceeded ();
    }
  else
    {
      NotifyConnectionFailed ();
      return -1;
    }

  return 0;
}
    
int 
HomaSocket::Listen (void)
{
  // TODO: Implement a connection based listener
  m_errno = Socket::ERROR_OPNOTSUPP;
  return -1;
}
    
// maximum segment size for HOMA broadcast is limited by MTU
// size of underlying link; we are not checking that now.
// TODO: Check MTU size of underlying link
uint32_t
HomaSocket::GetTxAvailable (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  // No finite send buffer is modelled, but we shall respect
  // the maximum size of an uint32_t type variable.
  return MAX_IPV4_HOMA_MESSAGE_SIZE;
}
    
    
int 
HomaSocket::Send (Ptr<Packet> msg, uint32_t flags)
{
  NS_LOG_FUNCTION (this << msg);

  if (!m_connected)
    {
      m_errno = ERROR_NOTCONN;
      return -1;
    }

  return DoSend (msg);
}
    
int 
HomaSocket::DoSend (Ptr<Packet> msg)
{
  NS_LOG_FUNCTION (this << msg);
  if ((m_endPoint == 0) && (Ipv4Address::IsMatchingType(m_defaultAddress) == true))
    {
      if (Bind () == -1)
        {
          NS_ASSERT (m_endPoint == 0);
          return -1;
        }
      NS_ASSERT (m_endPoint != 0);
    }
  if (m_shutdownSend)
    {
      m_errno = ERROR_SHUTDOWN;
      return -1;
    } 

  if (Ipv4Address::IsMatchingType (m_defaultAddress))
    {
      return DoSendTo (msg, Ipv4Address::ConvertFrom (m_defaultAddress), m_defaultPort, GetIpTos ());
    }

  m_errno = ERROR_AFNOSUPPORT;
  return(-1);
}
    
int 
HomaSocket::SendTo (Ptr<Packet> msg, uint32_t flags, const Address &address)
{
  NS_LOG_FUNCTION (this << msg << address);
  if (InetSocketAddress::IsMatchingType (address))
    {
      InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
      Ipv4Address ipv4 = transport.GetIpv4 ();
      uint16_t port = transport.GetPort ();
      uint8_t tos = transport.GetTos ();
      return DoSendTo (msg, ipv4, port, tos);
    }
  return -1;
}
    
int
HomaSocket::DoSendTo (Ptr<Packet> msg, Ipv4Address dest, uint16_t port, uint8_t tos)
{
  NS_LOG_FUNCTION (this << msg << dest << port << (uint16_t) tos);
  if (m_boundnetdevice)
    {
      NS_LOG_LOGIC ("Bound interface number " << m_boundnetdevice->GetIfIndex ());
    }
  if (m_endPoint == 0)
    {
      if (Bind () == -1)
        {
          NS_ASSERT (m_endPoint == 0);
          return -1;
        }
      NS_ASSERT (m_endPoint != 0);
    }
  if (m_shutdownSend)
    {
      m_errno = ERROR_SHUTDOWN;
      return -1;
    }

  if (msg->GetSize () > GetTxAvailable () )
    {
      m_errno = ERROR_MSGSIZE;
      return -1;
    }

  uint8_t priority = GetPriority ();
  if (tos)
    {
      SocketIpTosTag ipTosTag;
      ipTosTag.SetTos (tos);
      // This packet may already have a SocketIpTosTag (see BUG 2440)
      msg->ReplacePacketTag (ipTosTag);
      priority = IpTos2Priority (tos);
    }

  if (priority)
    {
      SocketPriorityTag priorityTag;
      priorityTag.SetPriority (priority);
      msg->ReplacePacketTag (priorityTag);
    }

  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();

  // Locally override the IP TTL for this socket
  // We cannot directly modify the TTL at this stage, so we set a Packet tag
  // The destination can be either multicast, unicast/anycast, or
  // either all-hosts broadcast or limited (subnet-directed) broadcast.
  // For the latter two broadcast types, the TTL will later be set to one
  // irrespective of what is set in these socket options.  So, this tagging
  // may end up setting the TTL of a limited broadcast packet to be
  // the same as a unicast, but it will be fixed further down the stack
  if (m_ipMulticastTtl != 0 && dest.IsMulticast ())
    {
      SocketIpTtlTag tag;
      tag.SetTtl (m_ipMulticastTtl);
      msg->AddPacketTag (tag);
    }
  else if (IsManualIpTtl () && GetIpTtl () != 0 && !dest.IsMulticast () && !dest.IsBroadcast ())
    {
      SocketIpTtlTag tag;
      tag.SetTtl (GetIpTtl ());
      msg->AddPacketTag (tag);
    }
  {
    SocketSetDontFragmentTag tag;
    bool found = msg->RemovePacketTag (tag);
    if (!found)
      {
        if (m_mtuDiscover)
          {
            tag.Enable ();
          }
        else
          {
            tag.Disable ();
          }
        msg->AddPacketTag (tag);
      }
  }

  // Note that some systems will only send limited broadcast packets
  // out of the "default" interface; here we send it out all interfaces
  if (dest.IsBroadcast ())
    {
      if (!m_allowBroadcast)
        {
          m_errno = ERROR_OPNOTSUPP;
          return -1;
        }
      NS_LOG_LOGIC ("Limited broadcast start.");
      for (uint32_t i = 0; i < ipv4->GetNInterfaces (); i++ )
        {
          // Get the primary address
          Ipv4InterfaceAddress iaddr = ipv4->GetAddress (i, 0);
          Ipv4Address addri = iaddr.GetLocal ();
          if (addri == Ipv4Address ("127.0.0.1"))
            continue;
          // Check if interface-bound socket
          if (m_boundnetdevice) 
            {
              if (ipv4->GetNetDevice (i) != m_boundnetdevice)
                continue;
            }
          NS_LOG_LOGIC ("Sending one copy from " << addri << " to " << dest);
          m_homa->Send (msg->Copy (), addri, dest,
                       m_endPoint->GetLocalPort (), port);
          NotifyDataSent (msg->GetSize ());
          NotifySend (GetTxAvailable ());
        }
      NS_LOG_LOGIC ("Limited broadcast end.");
      return msg->GetSize ();
    }
  else if (m_endPoint->GetLocalAddress () != Ipv4Address::GetAny ())
    {
      m_homa->Send (msg->Copy (), m_endPoint->GetLocalAddress (), dest,
                   m_endPoint->GetLocalPort (), port, 0);
      NotifyDataSent (msg->GetSize ());
      NotifySend (GetTxAvailable ());
      return msg->GetSize ();
    }
  else if (ipv4->GetRoutingProtocol () != 0)
    {
      Ipv4Header header;
      header.SetDestination (dest);
      header.SetProtocol (HomaL4Protocol::PROT_NUMBER);
      Socket::SocketErrno errno_;
      Ptr<Ipv4Route> route;
      Ptr<NetDevice> oif = m_boundnetdevice; //specify non-zero if bound to a specific device
      // TBD-- we could cache the route and just check its validity
      route = ipv4->GetRoutingProtocol ()->RouteOutput (msg, header, oif, errno_); 
      if (route != 0)
        {
          NS_LOG_LOGIC ("Route exists");
          if (!m_allowBroadcast)
            {
              // Here we try to route subnet-directed broadcasts
              uint32_t outputIfIndex = ipv4->GetInterfaceForDevice (route->GetOutputDevice ());
              uint32_t ifNAddr = ipv4->GetNAddresses (outputIfIndex);
              for (uint32_t addrI = 0; addrI < ifNAddr; ++addrI)
                {
                  Ipv4InterfaceAddress ifAddr = ipv4->GetAddress (outputIfIndex, addrI);
                  if (dest == ifAddr.GetBroadcast ())
                    {
                      m_errno = ERROR_OPNOTSUPP;
                      return -1;
                    }
                }
            }

          header.SetSource (route->GetSource ());
          m_homa->Send (msg->Copy (), header.GetSource (), header.GetDestination (),
                       m_endPoint->GetLocalPort (), port, route);
          NotifyDataSent (msg->GetSize ());
          return msg->GetSize ();
        }
      else 
        {
          NS_LOG_LOGIC ("No route to destination");
          NS_LOG_ERROR (errno_);
          m_errno = errno_;
          return -1;
        }
    }
  else
    {
      NS_LOG_ERROR ("ERROR_NOROUTETOHOST");
      m_errno = ERROR_NOROUTETOHOST;
      return -1;
    }

  return 0;
}
    
uint32_t
HomaSocket::GetRxAvailable (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  // We separately maintain this state to avoid walking the queue 
  // every time this might be called
  return m_rxAvailable;
}
    
Ptr<Packet>
HomaSocket::Recv (uint32_t maxSize, uint32_t flags)
{
  NS_LOG_FUNCTION (this << maxSize << flags);

  Address fromAddress;
  Ptr<Packet> message = RecvFrom (maxSize, flags, fromAddress);
  return message;
}
    
Ptr<Packet>
HomaSocket::RecvFrom (uint32_t maxSize, uint32_t flags, 
                         Address &fromAddress)
{
  NS_LOG_FUNCTION (this << maxSize << flags);

  if (m_deliveryQueue.empty () )
    {
      m_errno = ERROR_AGAIN;
      return 0;
    }
  Ptr<Packet> msg = m_deliveryQueue.front ().first;
  fromAddress = m_deliveryQueue.front ().second;

  if (msg->GetSize () <= maxSize)
    {
      m_deliveryQueue.pop ();
      m_rxAvailable -= msg->GetSize ();
    }
  else
    {
      msg = 0;
    }
  return msg;
}
    
int
HomaSocket::GetSockName (Address &address) const
{
  NS_LOG_FUNCTION_NOARGS ();
  if (m_endPoint != 0)
    {
      address = InetSocketAddress (m_endPoint->GetLocalAddress (), m_endPoint->GetLocalPort ());
    }
  else
    { // It is possible to call this method on a socket without a name
      // in which case, behavior is unspecified
      // Should this return an InetSocketAddress or an Inet6SocketAddress?
      address = InetSocketAddress (Ipv4Address::GetZero (), 0);
    }
  return 0;
}
    
int
HomaSocket::GetPeerName (Address &address) const
{
  NS_LOG_FUNCTION (this << address);

  if (!m_connected)
    {
      m_errno = ERROR_NOTCONN;
      return -1;
    }

  if (Ipv4Address::IsMatchingType (m_defaultAddress))
    {
      Ipv4Address addr = Ipv4Address::ConvertFrom (m_defaultAddress);
      InetSocketAddress inet (addr, m_defaultPort);
      inet.SetTos (GetIpTos ());
      address = inet;
    }
  else
    {
      NS_ASSERT_MSG (false, "unexpected address type");
    }

  return 0;
}
    
int 
HomaSocket::MulticastJoinGroup (uint32_t interface, const Address &groupAddress)
{
  NS_LOG_FUNCTION (interface << groupAddress);
  /*
   1) sanity check interface
   2) sanity check that it has not been called yet on this interface/group
   3) determine address family of groupAddress
   4) locally store a list of (interface, groupAddress)
   5) call ipv4->MulticastJoinGroup () or Ipv6->MulticastJoinGroup ()
  */
  return 0;
}
    
int 
HomaSocket::MulticastLeaveGroup (uint32_t interface, const Address &groupAddress) 
{
  NS_LOG_FUNCTION (interface << groupAddress);
  /*
   1) sanity check interface
   2) determine address family of groupAddress
   3) delete from local list of (interface, groupAddress); raise a LOG_WARN
      if not already present (but return 0) 
   5) call ipv4->MulticastLeaveGroup () or Ipv6->MulticastLeaveGroup ()
  */
  return 0;
}
    
void
HomaSocket::BindToNetDevice (Ptr<NetDevice> netdevice)
{
  NS_LOG_FUNCTION (netdevice);

  Socket::BindToNetDevice (netdevice); // Includes sanity check
  if (m_endPoint != 0)
    {
      m_endPoint->BindToNetDevice (netdevice);
    }

  return;
}
    
void 
HomaSocket::ForwardUp (Ptr<Packet> msg, Ipv4Header header, uint16_t port,
                          Ptr<Ipv4Interface> incomingInterface)
{
  NS_LOG_FUNCTION (this << msg << header << port);

  if (m_shutdownRecv)
    {
      return;
    }

  // Should check via getsockopt ()..
  if (IsRecvPktInfo ())
    {
      Ipv4PacketInfoTag tag;
      msg->RemovePacketTag (tag);
      tag.SetRecvIf (incomingInterface->GetDevice ()->GetIfIndex ());
      msg->AddPacketTag (tag);
    }

  //Check only version 4 options
  if (IsIpRecvTos ())
    {
      SocketIpTosTag ipTosTag;
      ipTosTag.SetTos (header.GetTos ());
      msg->ReplacePacketTag (ipTosTag);
    }

  if (IsIpRecvTtl ())
    {
      SocketIpTtlTag ipTtlTag;
      ipTtlTag.SetTtl (header.GetTtl ());
      msg->AddPacketTag (ipTtlTag);
    }

  // in case the packet still has a priority tag attached, remove it
  SocketPriorityTag priorityTag;
  msg->RemovePacketTag (priorityTag);

  if ((m_rxAvailable + msg->GetSize ()) <= m_rcvBufSize)
    {
      Address address = InetSocketAddress (header.GetSource (), port);
      m_deliveryQueue.push (std::make_pair (msg, address));
      m_rxAvailable += msg->GetSize ();
      NotifyDataRecv ();
    }
  else
    {
      // In general, this case should not occur unless the
      // receiving application reads data from this socket slowly
      // in comparison to the arrival rate
      //
      // drop and trace packet
      NS_LOG_WARN ("No receive buffer space available.  Drop.");
      m_dropTrace (msg);
    }
}
    
void
HomaSocket::ForwardIcmp (Ipv4Address icmpSource, uint8_t icmpTtl, 
                            uint8_t icmpType, uint8_t icmpCode,
                            uint32_t icmpInfo)
{
  NS_LOG_FUNCTION (this << icmpSource << (uint32_t)icmpTtl << (uint32_t)icmpType <<
                   (uint32_t)icmpCode << icmpInfo);
  if (!m_icmpCallback.IsNull ())
    {
      m_icmpCallback (icmpSource, icmpTtl, icmpType, icmpCode, icmpInfo);
    }
}
    
void 
HomaSocket::SetRcvBufSize (uint32_t size)
{
  m_rcvBufSize = size;
}

uint32_t 
HomaSocket::GetRcvBufSize (void) const
{
  return m_rcvBufSize;
}

void 
HomaSocket::SetIpMulticastTtl (uint8_t ipTtl)
{
  m_ipMulticastTtl = ipTtl;
}

uint8_t 
HomaSocket::GetIpMulticastTtl (void) const
{
  return m_ipMulticastTtl;
}

void 
HomaSocket::SetIpMulticastIf (int32_t ipIf)
{
  m_ipMulticastIf = ipIf;
}

int32_t 
HomaSocket::GetIpMulticastIf (void) const
{
  return m_ipMulticastIf;
}

void 
HomaSocket::SetIpMulticastLoop (bool loop)
{
  m_ipMulticastLoop = loop;
}

bool 
HomaSocket::GetIpMulticastLoop (void) const
{
  return m_ipMulticastLoop;
}

void 
HomaSocket::SetMtuDiscover (bool discover)
{
  m_mtuDiscover = discover;
}
bool 
HomaSocket::GetMtuDiscover (void) const
{
  return m_mtuDiscover;
}

bool
HomaSocket::SetAllowBroadcast (bool allowBroadcast)
{
  m_allowBroadcast = allowBroadcast;
  return true;
}

bool
HomaSocket::GetAllowBroadcast () const
{
  return m_allowBroadcast;
}
    
} // namespace ns3