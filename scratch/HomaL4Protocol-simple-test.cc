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

// Simple sender-receiver topology to test basic functionality of HomaL4Protocol
// Default Network Topology
//
//     10.0.1.0/24        10.0.0.0/24       10.0.2.0/24
// n0 -------------- s0 -------------- s1 -------------- n1
//    point-to-point    point-to-point    point-to-point
//

#include <iostream>
#include <stdlib.h>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HomaL4ProtocolSimpleTest");

void
AppSendTo (Ptr<Socket> senderSocket, 
           Ptr<Packet> appMsg, 
           InetSocketAddress receiverAddr)
{
  NS_LOG_FUNCTION(Simulator::Now ().GetNanoSeconds () << 
                  "Sending an application message.");
    
  int sentBytes = senderSocket->SendTo (appMsg, 0, receiverAddr);
  NS_LOG_INFO(sentBytes << " Bytes sent to " << receiverAddr);
}

void
AppReceive (Ptr<Socket> receiverSocket)
{
  NS_LOG_FUNCTION (Simulator::Now ().GetNanoSeconds () << 
                   "Received an application message");
 
  Ptr<Packet> message;
  Address from;
  while ((message = receiverSocket->RecvFrom (from)))
  {
    NS_LOG_INFO (Simulator::Now ().GetNanoSeconds () << 
                 " client received " << message->GetSize () << " bytes from " <<
                 InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                 InetSocketAddress::ConvertFrom (from).GetPort ());
  }
}

void TraceMsgBegin (Ptr<OutputStreamWrapper> stream,
                    Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                    uint16_t sport, uint16_t dport, int txMsgId)
{
  NS_LOG_DEBUG("+ " << Simulator::Now ().GetNanoSeconds ()
                << " " << msg->GetSize()
                << " " << saddr << ":" << sport 
                << " "  << daddr << ":" << dport 
                << " " << txMsgId);
    
  *stream->GetStream () << "+ " << Simulator::Now ().GetNanoSeconds () 
      << " " << msg->GetSize()
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << std::endl;
}

void TraceMsgFinish (Ptr<OutputStreamWrapper> stream,
                     Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                     uint16_t sport, uint16_t dport, int txMsgId)
{
  NS_LOG_DEBUG("- " << Simulator::Now ().GetNanoSeconds () 
                << " " << msg->GetSize()
                << " " << saddr << ":" << sport 
                << " "  << daddr << ":" << dport 
                << " " << txMsgId);
    
  *stream->GetStream () << "- " << Simulator::Now ().GetNanoSeconds () 
      << " " << msg->GetSize()
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
    
  Packet::EnablePrinting ();
  Time::SetResolution (Time::NS);
  LogComponentEnable ("HomaSocket", LOG_LEVEL_WARN);
  LogComponentEnable ("HomaL4Protocol", LOG_LEVEL_WARN);
  LogComponentEnable ("HomaL4ProtocolSimpleTest", LOG_LEVEL_DEBUG);

  /******** Create Nodes ********/
  NodeContainer switches;
  switches.Create (2);
    
  NodeContainer sender2switch;
  sender2switch.Add (switches.Get (0));
  sender2switch.Create (1);
    
  NodeContainer receiver2switch;
  receiver2switch.Add (switches.Get (1));
  receiver2switch.Create (1);

  /******** Create Channels ********/
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1us"));
  pointToPoint.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));

  /******** Create NetDevices ********/
  NetDeviceContainer switchDevices;
  switchDevices = pointToPoint.Install (switches);
    
  NetDeviceContainer senderDevices;
  senderDevices = pointToPoint.Install (sender2switch);
    
  NetDeviceContainer receiveDevices;
  receiveDevices = pointToPoint.Install (receiver2switch);
    
  /******** Install Internet Stack ********/
    
  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_RANDOM));
  /* Set default BDP value in packets */
  Config::SetDefault("ns3::HomaL4Protocol::RttPackets", UintegerValue(10));
  /* Set default number of priority bands in the network */
  uint8_t numTotalPrioBands = 8;
  uint8_t numUnschedPrioBands = 2;
  Config::SetDefault("ns3::HomaL4Protocol::NumTotalPrioBands", 
                     UintegerValue(numTotalPrioBands));
  Config::SetDefault("ns3::HomaL4Protocol::NumUnschedPrioBands", 
                     UintegerValue(numUnschedPrioBands));
    
  InternetStackHelper stack;
  stack.InstallAll ();
    
  /* Bottleneck link traffic control configuration for Homa compatibility */
  TrafficControlHelper tchPfifo;
  tchPfifo.SetRootQueueDisc ("ns3::PfifoHomaQueueDisc", 
                             "MaxSize", StringValue("9p"),
                             "NumBands", UintegerValue(numTotalPrioBands));
  tchPfifo.Install (switchDevices);
  tchPfifo.Install (senderDevices);
  tchPfifo.Install (receiveDevices);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer switchIf = address.Assign (switchDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer senderIf = address.Assign (senderDevices);
  address.NewNetwork ();
  Ipv4InterfaceContainer receiverIf = address.Assign (receiveDevices);
    
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /******** Create and Bind Homa Sockets on End-hosts ********/
  Ptr<SocketFactory> senderSocketFactory = sender2switch.Get(1)->GetObject<HomaSocketFactory> ();
  Ptr<Socket> senderSocket = senderSocketFactory->CreateSocket ();
  InetSocketAddress senderAddr = InetSocketAddress (senderIf.GetAddress (1), 1010);
  senderSocket->Bind (senderAddr);
    
  Ptr<SocketFactory> receiverSocketFactory = receiver2switch.Get(1)->GetObject<HomaSocketFactory> ();
  Ptr<Socket> receiverSocket = receiverSocketFactory->CreateSocket ();
  InetSocketAddress receiverAddr = InetSocketAddress (receiverIf.GetAddress (1), 2020);
  receiverSocket->Bind (receiverAddr);
    
  /* Set the message traces for the Homa clients*/
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> qStream;
  qStream = asciiTraceHelper.CreateFileStream ("outputs/HomaL4Protocol-simple-test/HomaL4ProtocolSimpleTestMsgTraces.tr");
  Config::ConnectWithoutContext("/NodeList/*/$ns3::HomaL4Protocol/MsgBegin", 
                                MakeBoundCallback(&TraceMsgBegin, qStream));
  Config::ConnectWithoutContext("/NodeList/*/$ns3::HomaL4Protocol/MsgFinish", 
                                MakeBoundCallback(&TraceMsgFinish, qStream));
    
  /******** Create a Message and Schedule to be Sent ********/
  HomaHeader homah;
  Ipv4Header ipv4h;
    
  uint32_t payloadSize = senderDevices.Get (1)->GetMtu() 
                         - homah.GetSerializedSize ()
                         - ipv4h.GetSerializedSize ();
  Ptr<Packet> appMsg = Create<Packet> (payloadSize);
  
  Simulator::Schedule (Seconds (3.0), &AppSendTo, senderSocket, appMsg, receiverAddr);
  receiverSocket->SetRecvCallback (MakeCallback (&AppReceive));

  /******** Run the Actual Simulation ********/
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}