/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2014 University of Washington
 *               2015 Universita' degli Studi di Napoli Federico II
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
 * Author:   Serhat Arslan <sarslan@stanford.edu>
 *
 * This document has been created by modifying pfifo-fast-queue-disc class.
 * The authors of the original document are given below.
 *
 * Authors:  Stefano Avallone <stavallo@unina.it>
 *           Tom Henderson <tomhend@u.washington.edu>
 */

#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/socket.h"
#include "pfifo-homa-queue-disc.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PfifoHomaQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (PfifoHomaQueueDisc);

TypeId PfifoHomaQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PfifoHomaQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName ("TrafficControl")
    .AddConstructor<PfifoHomaQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc.",
                   QueueSizeValue (QueueSize ("1000p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ("NumBands",
                   "The number of priorities the queue disc has.",
                   UintegerValue (4),
                   MakeUintegerAccessor (&PfifoHomaQueueDisc::m_numBands),
                   MakeUintegerChecker<uint8_t> ())
  ;
  return tid;
}

PfifoHomaQueueDisc::PfifoHomaQueueDisc ()
  : QueueDisc (QueueDiscSizePolicy::MULTIPLE_QUEUES, QueueSizeUnit::PACKETS)
{
  NS_LOG_FUNCTION (this);
}

PfifoHomaQueueDisc::~PfifoHomaQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

bool
PfifoHomaQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
//   NS_LOG_DEBUG("The received packet is: " << item->GetPacket ()->ToString ());

  if (GetCurrentSize () >= GetMaxSize ())
    {
      NS_LOG_LOGIC ("Queue disc limit exceeded -- dropping packet");
      DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
      return false;
    }

  uint8_t priority = 0;
  SocketIpTosTag priorityTag;
  if (item->GetPacket ()->PeekPacketTag (priorityTag))
    {
//       NS_LOG_DEBUG("Found priority tag on the packet: " << 
//                    (uint32_t)priorityTag.GetTos ());
      priority = priorityTag.GetTos ();
    }

  uint32_t band = (uint32_t)priority;
  if (band > m_numBands)
      band = m_numBands;

  bool retval = GetInternalQueue (band)->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
  // internal queue because QueueDisc::AddInternalQueue sets the trace callback

  if (!retval)
    {
      NS_LOG_WARN ("Packet enqueue failed. Check the size of the internal queues");
    }

  NS_LOG_LOGIC ("Number packets band " << band << ": " << GetInternalQueue (band)->GetNPackets ());

  return retval;
}

Ptr<QueueDiscItem>
PfifoHomaQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Dequeue ()) != 0)
        {
          NS_LOG_LOGIC ("Popped from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }
  
  NS_LOG_LOGIC ("Queue empty");
  return item;
}

Ptr<const QueueDiscItem>
PfifoHomaQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  Ptr<const QueueDiscItem> item;

  for (uint32_t i = 0; i < GetNInternalQueues (); i++)
    {
      if ((item = GetInternalQueue (i)->Peek ()) != 0)
        {
          NS_LOG_LOGIC ("Peeked from band " << i << ": " << item);
          NS_LOG_LOGIC ("Number packets band " << i << ": " << GetInternalQueue (i)->GetNPackets ());
          return item;
        }
    }

  NS_LOG_LOGIC ("Queue empty");
  return item;
}

bool
PfifoHomaQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("PfifoHomaQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () != 0)
    {
      NS_LOG_ERROR ("PfifoHomaQueueDisc needs no packet filter");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // create m_numBands DropTail queues with GetLimit() packets each
      ObjectFactory factory;
      factory.SetTypeId ("ns3::DropTailQueue<QueueDiscItem>");
      factory.Set ("MaxSize", QueueSizeValue (GetMaxSize ()));
      for (uint8_t i = 0; i < m_numBands; i++)
      {
        AddInternalQueue (factory.Create<InternalQueue> ());
      }
    }

  if (GetNInternalQueues () != m_numBands)
    {
      NS_LOG_ERROR ("PfifoHomaQueueDisc needs "<< m_numBands <<
                    " internal queues");
      return false;
    }

  for (uint8_t i = 0; i < m_numBands; i++)
    {
      if (GetInternalQueue (i)-> GetMaxSize ().GetUnit () != QueueSizeUnit::PACKETS)
        {
          NS_LOG_ERROR ("PfifoHomaQueueDisc needs internal queues operating in packet mode");
          return false;
        }
    }

  for (uint8_t i = 0; i < m_numBands; i++)
    {
      if (GetInternalQueue (i)->GetMaxSize () < GetMaxSize ())
        {
          NS_LOG_ERROR ("The capacity of some internal queue(s) is less than the queue disc capacity");
          return false;
        }
    }

  return true;
}

void
PfifoHomaQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
}

} // namespace ns3
