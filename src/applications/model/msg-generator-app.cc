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

#include "msg-generator-app.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/callback.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/double.h"

#include "ns3/udp-socket-factory.h"
#include "ns3/homa-socket-factory.h"
#include "ns3/point-to-point-net-device.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MsgGeneratorApp");

NS_OBJECT_ENSURE_REGISTERED (MsgGeneratorApp);
    
TypeId
MsgGeneratorApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MsgGeneratorApp")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddAttribute ("Protocol", "The type of protocol to use. This should be "
                    "a subclass of ns3::SocketFactory",
                    TypeIdValue (HomaSocketFactory::GetTypeId ()),
                    MakeTypeIdAccessor (&MsgGeneratorApp::m_tid),
                    // This should check for SocketFactory as a parent
                    MakeTypeIdChecker ())
    .AddAttribute ("MaxMsg", 
                   "The total number of messages to send. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&MsgGeneratorApp::m_maxMsgs),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PayloadSize", 
                   "MTU for the network interface excluding the header sizes",
                   UintegerValue (1400),
                   MakeUintegerAccessor (&MsgGeneratorApp::m_maxPayloadSize),
                   MakeUintegerChecker<uint32_t> ())
  ;
  return tid;
}
    
MsgGeneratorApp::MsgGeneratorApp(Ipv4Address localIp, uint16_t localPort)
  : m_socket (0),
    m_interMsgTime (0),
    m_msgSizePkts (0),
    m_remoteClient (0),
    m_totMsgCnt (0)
{
  NS_LOG_FUNCTION (this << localIp << localPort);
   
  m_localIp = localIp;
  m_localPort = localPort;
}
    
MsgGeneratorApp::~MsgGeneratorApp()
{
  NS_LOG_FUNCTION (this);
}
    
void MsgGeneratorApp::Install (Ptr<Node> node, 
                               std::vector<InetSocketAddress> remoteClients)
{
  NS_LOG_FUNCTION(this << node);
   
  node->AddApplication (this);
  
  m_socket = Socket::CreateSocket (node, m_tid);
  m_socket->Bind (InetSocketAddress(m_localIp, m_localPort));
  m_socket->SetRecvCallback (MakeCallback (&MsgGeneratorApp::ReceiveMessage, this));
    
  for (std::size_t i = 0; i < remoteClients.size(); i++)
  {
    if (remoteClients[i].GetIpv4() != m_localIp ||
        remoteClients[i].GetPort() != m_localPort)
    {
      m_remoteClients.push_back(remoteClients[i]);
    }
    else
    {
      // Remove the local address from the client addresses list
      NS_LOG_LOGIC("MsgGeneratorApp (" << this << 
                   ") removes address " << remoteClients[i].GetIpv4() <<
                   ":" << remoteClients[i].GetPort() <<
                   " from remote clients because it is the local address.");
    }
  }
    
  m_remoteClient = CreateObject<UniformRandomVariable> ();
  m_remoteClient->SetAttribute ("Min", DoubleValue (0));
  m_remoteClient->SetAttribute ("Max", DoubleValue (m_remoteClients.size()));
}
    
void MsgGeneratorApp::SetWorkload (double load, 
                                   std::map<double,int> msgSizeCDF, 
                                   double avgMsgSizePkts)
{
  NS_LOG_FUNCTION(this << avgMsgSizePkts);
    
  load = std::max(0.0, std::min(load, 1.0));
    
  Ptr<NetDevice> netDevice = GetNode ()->GetDevice (0); 
  uint32_t mtu = netDevice->GetMtu ();
    
  PointToPointNetDevice* p2pNetDevice = dynamic_cast<PointToPointNetDevice*>(&(*(netDevice))); 
  uint64_t txRate = p2pNetDevice->GetDataRate ().GetBitRate ();
    
  double avgPktLoadBytes = (double)(mtu + 64); // Account for the ctrl pkts each data pkt induce
  double avgInterMsgTime = (avgMsgSizePkts * avgPktLoadBytes * 8.0 ) / (((double)txRate) * load);
    
  m_interMsgTime = CreateObject<ExponentialRandomVariable> ();
  m_interMsgTime->SetAttribute ("Mean", DoubleValue (avgInterMsgTime));
    
  m_msgSizeCDF = msgSizeCDF;
    
  m_msgSizePkts = CreateObject<UniformRandomVariable> ();
  m_msgSizePkts->SetAttribute ("Min", DoubleValue (0));
  m_msgSizePkts->SetAttribute ("Max", DoubleValue (1));
}
    
void MsgGeneratorApp::Start (Time start)
{
  NS_LOG_FUNCTION (this);
    
  SetStartTime(start);
  DoInitialize();
}
    
void MsgGeneratorApp::Stop (Time stop)
{
  NS_LOG_FUNCTION (this);
    
  SetStopTime(stop);
}
    
void MsgGeneratorApp::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  CancelNextEvent ();
  // chain up
  Application::DoDispose ();
}
    
void MsgGeneratorApp::StartApplication ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  NS_ASSERT_MSG(m_remoteClient && m_interMsgTime && m_msgSizePkts,
                "MsgGeneratorApp should be installed on a node and "
                "the workload should be set before starting the application!");
    
  ScheduleNextMessage ();
}
    
void MsgGeneratorApp::StopApplication ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  CancelNextEvent();
}
    
void MsgGeneratorApp::CancelNextEvent()
{
  NS_LOG_FUNCTION (this);
    
  if (!Simulator::IsExpired(m_nextSendEvent))
    Simulator::Cancel (m_nextSendEvent);
}
    
void MsgGeneratorApp::ScheduleNextMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  if (Simulator::IsExpired(m_nextSendEvent))
  {
    m_nextSendEvent = Simulator::Schedule (Seconds (m_interMsgTime->GetValue ()),
                                           &MsgGeneratorApp::SendMessage, this);
  }
  else
  {
    NS_LOG_WARN("MsgGeneratorApp (" << this <<
                ") tries to schedule the next msg before the previous one is sent!");
  }
}
    
uint32_t MsgGeneratorApp::GetNextMsgSizeFromDist ()
{
  NS_LOG_FUNCTION(this);
    
  int msgSizePkts = -1;
  double rndValue = m_msgSizePkts->GetValue();
  for (auto it = m_msgSizeCDF.begin(); it != m_msgSizeCDF.end(); it++)
  {
    if (rndValue <= it->first)
    {
      msgSizePkts = it->second;
      break;
    }
  }
    
  NS_ASSERT(msgSizePkts >= 0);
  // Homa header can't handle msgs larger than 0xffff pkts
  msgSizePkts = std::min(0xffff, msgSizePkts);
    
  if (m_maxPayloadSize > 0)
    return m_maxPayloadSize * (uint32_t)msgSizePkts;
  else
    return GetNode ()->GetDevice (0)->GetMtu () * (uint32_t)msgSizePkts;
    
  // NOTE: If maxPayloadSize is not set, the generated messages will be
  //       slightly larger than the intended number of packets due to
  //       the addition of the protocol headers.
}
    
void MsgGeneratorApp::SendMessage ()
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << this);
    
  /* Decide which remote client to send to */
  double rndValue = m_remoteClient->GetValue ();
  int remoteClientIdx = (int) std::floor(rndValue);
  InetSocketAddress receiverAddr = m_remoteClients[remoteClientIdx];
  
  /* Decide on the message size to send */
  uint32_t msgSizeBytes = GetNextMsgSizeFromDist (); 
  
  /* Create the message to send */
  Ptr<Packet> msg = Create<Packet> (msgSizeBytes);
  NS_LOG_LOGIC ("MsgGeneratorApp {" << this << ") generates a message of size: "
                << msgSizeBytes << " Bytes.");
    
  int sentBytes = m_socket->SendTo (msg, 0, receiverAddr);
   
  if (sentBytes > 0)
  {
    NS_LOG_INFO(sentBytes << " Bytes sent to " << receiverAddr);
    m_totMsgCnt++;
  }
    
  if (m_maxMsgs == 0 || m_totMsgCnt < m_maxMsgs)
  {
    ScheduleNextMessage ();
  }
}
    
void MsgGeneratorApp::ReceiveMessage (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this);
 
  Ptr<Packet> message;
  Address from;
  while ((message = socket->RecvFrom (from)))
  {
    NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () << 
                 " client received " << message->GetSize () << " bytes from " <<
                 InetSocketAddress::ConvertFrom (from).GetIpv4 () << ":" <<
                 InetSocketAddress::ConvertFrom (from).GetPort ());
  }
}
    
} // Namespace ns3