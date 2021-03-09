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

#ifndef PFIFO_HOMA_H
#define PFIFO_HOMA_H

#include "ns3/queue-disc.h"

namespace ns3 {

/**
 * \ingroup traffic-control
 *
 * Linux pfifo_fast is the default priority queue enabled on Linux
 * systems. Packets are enqueued in three FIFO droptail queues according
 * to given number of priority bands based on the packet priority.
 *
 * The system behaves similar to three ns3::DropTail queues operating
 * together, in which packets from higher priority bands are always
 * dequeued before a packet from a lower priority band is dequeued.
 *
 * The queue disc capacity, i.e., the maximum number of packets that can
 * be enqueued in the queue disc, is set through the limit attribute, which
 * plays the same role as txqueuelen in Linux. If no internal queue is
 * provided, given number of DropTail queues having each a capacity equal to limit 
 * are created by default. User is allowed to provide queues, but they must be
 * three, operate in packet mode and each have a capacity not less
 * than limit. No packet filter can be provided.
 */
class PfifoHomaQueueDisc : public QueueDisc {
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  /**
   * \brief PfifoHomaQueueDisc constructor
   *
   * Creates a queue with a depth of 1000 packets per band by default
   */
  PfifoHomaQueueDisc ();

  virtual ~PfifoHomaQueueDisc();

  // Reasons for dropping packets
  static constexpr const char* LIMIT_EXCEEDED_DROP = "Queue disc limit exceeded";  //!< Packet dropped due to queue disc limit exceeded

private:

  virtual bool DoEnqueue (Ptr<QueueDiscItem> item);
  virtual Ptr<QueueDiscItem> DoDequeue (void);
  virtual Ptr<const QueueDiscItem> DoPeek (void);
  virtual bool CheckConfig (void);
  virtual void InitializeParams (void);
    
  uint8_t m_numBands; //!< Number of bands to have
};

} // namespace ns3

#endif /* PFIFO_HOMA_H */
