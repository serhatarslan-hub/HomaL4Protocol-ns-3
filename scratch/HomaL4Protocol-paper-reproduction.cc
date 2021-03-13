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

// The topology used in this simulation is provided in Homa paper [1] in detail.
//
// The topology consists of 144 hosts divided among 9 racks with a 2-level switching 
// fabric. Host links operate at 10Gbps and TOR-aggregation links operate at 40 Gbps.
//
// [1] Behnam Montazeri, Yilong Li, Mohammad Alizadeh, and John Ousterhout.  
//     2018. Homa: a receiver-driven low-latency transport protocol using  
//     network priorities. In Proceedings of the 2018 Conference of the ACM  
//     Special Interest Group on Data Communication (SIGCOMM '18). Association  
//     for Computing Machinery, New York, NY, USA, 221–235. 
//     DOI:https://doi.org/10.1145/3230543.3230564

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HomaL4ProtocolPaperReproduction");

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

static void
BytesInQueueDiscTrace (Ptr<OutputStreamWrapper> stream, int hostIdx, 
                       uint32_t oldval, uint32_t newval)
{
  NS_LOG_DEBUG (Simulator::Now ().GetNanoSeconds () <<
               " Queue Disc size from " << oldval << " to " << newval);
    
  *stream->GetStream () << Simulator::Now ().GetNanoSeconds ()
                        << " HostIdx=" << hostIdx
                        << " NewQueueSize=" << newval << std::endl;
}

void TraceDataPktArrival (Ptr<OutputStreamWrapper> stream,
                          Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                          uint16_t sport, uint16_t dport, int txMsgId,
                          uint16_t pktOffset, uint8_t prio)
{
  NS_LOG_DEBUG("- " << Simulator::Now ().GetNanoSeconds () 
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << " " << pktOffset << " " << (uint16_t)prio);
    
  *stream->GetStream () << "- "  <<Simulator::Now ().GetNanoSeconds () 
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << " " << pktOffset << " " << (uint16_t)prio << std::endl;
}
void TraceDataPktDeparture (Ptr<OutputStreamWrapper> stream,
                            Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                            uint16_t sport, uint16_t dport, int txMsgId,
                            uint16_t pktOffset, uint16_t prio)
{
  NS_LOG_DEBUG("+ " << Simulator::Now ().GetNanoSeconds () 
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << " " << pktOffset);// << " " << (uint16_t)prio);
    
  *stream->GetStream () << "+ "  <<Simulator::Now ().GetNanoSeconds () 
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << txMsgId << " " << pktOffset << " " << std::endl;
//       << " " << txMsgId << " " << pktOffset << " " << (uint16_t)prio << std::endl;
}
void TraceCtrlPktArrival (Ptr<OutputStreamWrapper> stream,
                          Ptr<const Packet> msg, Ipv4Address saddr, Ipv4Address daddr, 
                          uint16_t sport, uint16_t dport, uint8_t flag,
                          uint16_t grantOffset, uint8_t prio)
{
  NS_LOG_DEBUG("- " << Simulator::Now ().GetNanoSeconds () 
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << HomaHeader::FlagsToString(flag) << " " << grantOffset 
      << " " << (uint16_t)prio);
    
  *stream->GetStream () << "- "  <<Simulator::Now ().GetNanoSeconds () 
      << " " << saddr << ":" << sport << " "  << daddr << ":" << dport 
      << " " << HomaHeader::FlagsToString(flag) << " " << grantOffset 
      << " " << (uint16_t)prio << std::endl;
}

std::map<double,int> ReadMsgSizeDist (std::string msgSizeDistFileName, double &avgMsgSizePkts)
{
  std::ifstream msgSizeDistFile;
  msgSizeDistFile.open (msgSizeDistFileName);
  NS_LOG_FUNCTION("Reading Msg Size Distribution From: " << msgSizeDistFileName);
    
  std::string line;
  std::istringstream lineBuffer;
  
  getline (msgSizeDistFile, line);
  lineBuffer.str (line);
  lineBuffer >> avgMsgSizePkts;
    
  std::map<double,int> msgSizeCDF;
  double prob;
  int msgSizePkts;
  while(getline (msgSizeDistFile, line)) 
  {
    lineBuffer.clear ();
    lineBuffer.str (line);
    lineBuffer >> msgSizePkts;
    lineBuffer >> prob;
      
    msgSizeCDF[prob] = msgSizePkts;
  }
  msgSizeDistFile.close();
    
  return msgSizeCDF;
}

int
main (int argc, char *argv[])
{
  AsciiTraceHelper asciiTraceHelper;
  double duration = 0.01;
  double networkLoad = 0.5;
  uint32_t simIdx = 0;
  bool traceQueues = false;
  bool disableRtx = false;
  bool debugMode = false;
  uint32_t initialCredit = 7; // in packets
  uint64_t inboundRtxTimeout = 1000; // in microseconds
  uint64_t outboundRtxTimeout = 10000; // in microseconds
    
  CommandLine cmd (__FILE__);
  cmd.AddValue ("duration", "The duration of the simulation in seconds.", duration);
  cmd.AddValue ("load", "The network load to simulate the network at, ie 0.5 for 50%.", networkLoad);
  cmd.AddValue ("simIdx", "The index of the simulation used to identify parallel runs.", simIdx);
  cmd.AddValue ("traceQueues", "Whether to trace the queue lengths during the simulation.", traceQueues);
  cmd.AddValue ("disableRtx", "Whether to disable rtx timers during the simulation.", disableRtx);
  cmd.AddValue ("debug", "Whether to enable detailed pkt traces for debugging", debugMode);
  cmd.AddValue ("bdpPkts", "RttBytes to use in the simulation.", initialCredit);
  cmd.AddValue ("inboundRtxTimeout", "Number of microseconds before an inbound msg expires.", inboundRtxTimeout);
  cmd.AddValue ("outboundRtxTimeout", "Number of microseconds before an outbound msg expires.", outboundRtxTimeout);
  cmd.Parse (argc, argv);
    
  if (debugMode)
  {
    NS_LOG_UNCOND("Running in DEBUG Mode!");
    SeedManager::SetRun (0);
  }
  else
    SeedManager::SetRun (simIdx);
    
  Time::SetResolution (Time::NS);
//   Packet::EnablePrinting ();
  LogComponentEnable ("HomaL4ProtocolPaperReproduction", LOG_LEVEL_DEBUG);  
  LogComponentEnable ("MsgGeneratorApp", LOG_LEVEL_WARN);  
  LogComponentEnable ("HomaSocket", LOG_LEVEL_WARN);
  LogComponentEnable ("HomaL4Protocol", LOG_LEVEL_WARN);
    
  std::string msgSizeDistFileName ("inputs/homa-paper-reproduction/DCTCP-MsgSizeDist.txt");
  std::string tracesFileName ("outputs/homa-paper-reproduction/MsgTraces");
  tracesFileName += "_W5";
  tracesFileName += "_load-" + std::to_string((int)(networkLoad*100)) + "p";
  if (debugMode)
    tracesFileName += "_debug";
  else
    tracesFileName += "_" + std::to_string(simIdx);
    
  std::string qStreamName = tracesFileName + ".qlen";
  std::string msgTracesFileName = tracesFileName + ".tr";
    
  int nHosts = 144;
  int nTors = 9;
  int nSpines = 4;
  
  /******** Create Nodes ********/
  NS_LOG_UNCOND("Creating Nodes...");
  NodeContainer hostNodes;
  hostNodes.Create (nHosts);
    
  NodeContainer torNodes;
  torNodes.Create (nTors);
    
  NodeContainer spineNodes;
  spineNodes.Create (nSpines);
    
  /******** Create Channels ********/
  NS_LOG_UNCOND("Configuring Channels...");
  PointToPointHelper hostLinks;
  hostLinks.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  hostLinks.SetChannelAttribute ("Delay", StringValue ("250ns"));
  hostLinks.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
    
  PointToPointHelper aggregationLinks;
  aggregationLinks.SetDeviceAttribute ("DataRate", StringValue ("40Gbps"));
  aggregationLinks.SetChannelAttribute ("Delay", StringValue ("250ns"));
  aggregationLinks.SetQueue ("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
    
  /******** Create NetDevices ********/
  NS_LOG_UNCOND("Creating NetDevices...");
  NetDeviceContainer hostTorDevices[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    hostTorDevices[i] = hostLinks.Install (hostNodes.Get(i), 
                                           torNodes.Get(i/(nHosts/nTors)));
  }
    
  NetDeviceContainer torSpineDevices[nTors*nSpines];
  for (int i = 0; i < nTors; i++)
  {
    for (int j = 0; j < nSpines; j++)
    {
      torSpineDevices[i*nSpines+j] = aggregationLinks.Install (torNodes.Get(i), 
                                                               spineNodes.Get(j));
    }
  }
    
  /******** Install Internet Stack ********/
  NS_LOG_UNCOND("Installing Internet Stack...");
    
  /* Set default BDP value in packets */
  Config::SetDefault("ns3::HomaL4Protocol::RttPackets", 
                     UintegerValue(initialCredit));
    
  /* Set default number of priority bands in the network */
  uint8_t numTotalPrioBands = 8;
  uint8_t numUnschedPrioBands = 2;
  if (disableRtx)
  {
    inboundRtxTimeout *= 1e9;
    outboundRtxTimeout *= 1e9;
  }
    
  NS_LOG_UNCOND("Deploying HomaL4Protocol Stack...");
  Config::SetDefault("ns3::HomaL4Protocol::NumTotalPrioBands", 
                     UintegerValue(numTotalPrioBands));
  Config::SetDefault("ns3::HomaL4Protocol::NumUnschedPrioBands", 
                     UintegerValue(numUnschedPrioBands));
  Config::SetDefault("ns3::HomaL4Protocol::InbndRtxTimeout", 
                     TimeValue (MicroSeconds (inboundRtxTimeout)));
  Config::SetDefault("ns3::HomaL4Protocol::OutbndRtxTimeout", 
                     TimeValue (MicroSeconds (outboundRtxTimeout)));
  
  InternetStackHelper stack;
  stack.Install (spineNodes);
    
  /* Enable multi-path routing */
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", 
                     EnumValue(Ipv4GlobalRouting::ECMP_RANDOM));
    
  stack.Install (torNodes);
  stack.Install (hostNodes);
    
  /* Link traffic control configuration for Homa compatibility */
  // TODO: The paper doesn't provide buffer sizes, so we set some large 
  //       value for rare overflows.
  TrafficControlHelper tchPfifoHoma;
  tchPfifoHoma.SetRootQueueDisc ("ns3::PfifoHomaQueueDisc",
                                 "MaxSize", StringValue("500p"),
                                 "NumBands", UintegerValue(numTotalPrioBands));
  QueueDiscContainer hostFacingTorQdiscs[nHosts];
  Ptr<OutputStreamWrapper> qStream;
  if (traceQueues)
    qStream = asciiTraceHelper.CreateFileStream (qStreamName);
    
  for (int i = 0; i < nHosts; i++)
  {
    hostFacingTorQdiscs[i] = tchPfifoHoma.Install (hostTorDevices[i].Get(1));
    if (traceQueues)
      hostFacingTorQdiscs[i].Get(0)->TraceConnectWithoutContext ("BytesInQueue", 
                                          MakeBoundCallback (&BytesInQueueDiscTrace, 
                                                             qStream, i));
  }
  for (int i = 0; i < nTors*nSpines; i++)
  {
    tchPfifoHoma.Install (torSpineDevices[i]);
  }
   
  /* Set IP addresses of the nodes in the network */
  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  std::vector<InetSocketAddress> clientAddresses;
    
  Ipv4InterfaceContainer hostTorIfs[nHosts];
  for (int i = 0; i < nHosts; i++)
  {
    hostTorIfs[i] = address.Assign (hostTorDevices[i]);
    clientAddresses.push_back(InetSocketAddress (hostTorIfs[i].GetAddress (0), 
                                                 1000+i));
    address.NewNetwork ();
  }
  
  Ipv4InterfaceContainer torSpineIfs[nTors*nSpines];
  for (int i = 0; i < nTors*nSpines; i++)
  {
    torSpineIfs[i] = address.Assign (torSpineDevices[i]);
    address.NewNetwork ();
  }
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    
  /******** Read the Workload Distribution From File ********/
  NS_LOG_UNCOND("Reading Msg Size Distribution...");
  double avgMsgSizePkts;
  std::map<double,int> msgSizeCDF = ReadMsgSizeDist(msgSizeDistFileName, avgMsgSizePkts);
    
  NS_LOG_LOGIC ("The CDF of message sizes is given below: ");
  for (auto it = msgSizeCDF.begin(); it != msgSizeCDF.end(); it++)
  {
    NS_LOG_LOGIC (it->second << " : " << it->first);
  }
  NS_LOG_LOGIC("Average Message Size is: " << avgMsgSizePkts);
    
  /******** Create Message Generator Apps on End-hosts ********/
  NS_LOG_UNCOND("Installing the Applications...");
  HomaHeader homah;
  Ipv4Header ipv4h;
  uint32_t payloadSize = hostTorDevices[0].Get (0)->GetMtu() 
                         - homah.GetSerializedSize ()
                         - ipv4h.GetSerializedSize ();
  Config::SetDefault("ns3::MsgGeneratorApp::PayloadSize", 
                     UintegerValue(payloadSize));
    
  for (int i = 0; i < nHosts; i++)
  {
    Ptr<MsgGeneratorApp> app = CreateObject<MsgGeneratorApp>(hostTorIfs[i].GetAddress (0),
                                                             1000 + i);
    app->Install (hostNodes.Get (i), clientAddresses);
    app->SetWorkload (networkLoad, msgSizeCDF, avgMsgSizePkts);
      
    app->Start(Seconds (3.0));
    app->Stop(Seconds (3.0 + duration));
  }
      
  /* Set the message traces for the Homa clients*/
  Ptr<OutputStreamWrapper> msgStream;
  msgStream = asciiTraceHelper.CreateFileStream (msgTracesFileName);
  Config::ConnectWithoutContext("/NodeList/*/$ns3::HomaL4Protocol/MsgBegin", 
                                MakeBoundCallback(&TraceMsgBegin, msgStream));
  Config::ConnectWithoutContext("/NodeList/*/$ns3::HomaL4Protocol/MsgFinish", 
                                MakeBoundCallback(&TraceMsgFinish, msgStream));
  
  if (debugMode)
  {
    Ptr<OutputStreamWrapper> pktStream;
    std::string pktTraceFileName ("outputs/homa-paper-reproduction/debug-pktTrace.tr"); 
    pktStream = asciiTraceHelper.CreateFileStream (pktTraceFileName);
      
    Config::ConnectWithoutContext("/NodeList/45/$ns3::HomaL4Protocol/DataPktDeparture", 
                                MakeBoundCallback(&TraceDataPktDeparture,pktStream));
    Config::ConnectWithoutContext("/NodeList/45/$ns3::HomaL4Protocol/DataPktArrival", 
                                MakeBoundCallback(&TraceDataPktArrival,pktStream));
    Config::ConnectWithoutContext("/NodeList/45/$ns3::HomaL4Protocol/CtrlPktArrival", 
                                MakeBoundCallback(&TraceCtrlPktArrival,pktStream));
  
//     std::string pcapFileName ("outputs/homa-paper-reproduction/pcaps/tor-spine");
//     aggregationLinks.EnablePcapAll (pcapFileName, false);
  }

  /******** Run the Actual Simulation ********/
  NS_LOG_UNCOND("Running the Simulation...");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}