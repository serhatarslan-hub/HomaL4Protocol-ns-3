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

/*
 * The heade implementation is partly based on the one provided by 
 * https://github.com/PlatformLab/HomaModule/blob/master/homa_impl.h
 * However it does not completely follow the way Home Linux Kernel is
 * implemented for the sake of easier management
 */

#include <stdint.h>
#include <iostream>

#include "homa-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("HomaHeader");
    
NS_OBJECT_ENSURE_REGISTERED (HomaHeader);

/* The magic values below are used only for debugging.
 * They can be used to easily detect memory corruption
 * problems so you can see the patterns in memory.
 */
HomaHeader::HomaHeader ()
  : m_srcPort (0xfffd),
    m_dstPort (0xfffd),
    m_txMsgId (0),
    m_flags (0),
    m_prio (0xff),
    m_msgSizeBytes (0),
    m_pktOffset (0),
    m_grantOffset (0),
    m_payloadSize (0),
    m_generation (1)
{
}

HomaHeader::~HomaHeader ()
{
}
    
TypeId 
HomaHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaHeader")
    .SetParent<Header> ()
    .SetGroupName ("Internet")
    .AddConstructor<HomaHeader> ()
  ;
  return tid;
}
TypeId 
HomaHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
    
void 
HomaHeader::Print (std::ostream &os) const
{
  os << "length: " << m_payloadSize + GetSerializedSize ()
     << " " << m_srcPort << " > " << m_dstPort
     << " txMsgId: " << m_txMsgId
     << " prio: " << (uint16_t)m_prio
     << " gen: " << m_generation
     << " msgSize: " << m_msgSizeBytes
     << " pktOffset: " << m_pktOffset
     << " grantOffset: " << m_grantOffset
     << " " << FlagsToString (m_flags)
  ;
}

uint32_t 
HomaHeader::GetSerializedSize (void) const
{
  /* Note: The original Homa implementation has a slighly different packet 
   *       header format for every type of Homa packet.
   */
  return 20; 
  // TODO: If the above value is updated, update the default payload size
  //       in the declaration of Homa nanoPU implementation.
}
    
std::string
HomaHeader::FlagsToString (uint8_t flags, const std::string& delimiter)
{
  static const char* flagNames[8] = {
    "DATA",
    "GRANT",
    "RESEND",
    "ACK",
    "BUSY",
    "CUTOFFS",
    "FREEZE",
    "BOGUS"
  };
  std::string flagsDescription = "";
  for (uint8_t i = 0; i < 8; ++i)
    {
      if (flags & (1 << i))
        {
          if (flagsDescription.length () > 0)
            {
              flagsDescription += delimiter;
            }
          flagsDescription.append (flagNames[i]);
        }
    }
  return flagsDescription;
}
    
void
HomaHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  i.WriteHtonU16 (m_srcPort);
  i.WriteHtonU16 (m_dstPort);
  i.WriteHtonU16 (m_txMsgId);
  i.WriteU8 (m_flags);
  i.WriteU8 (m_prio);
  i.WriteHtonU32 (m_msgSizeBytes);
  i.WriteHtonU16 (m_pktOffset);
  i.WriteHtonU16 (m_grantOffset);
  i.WriteHtonU16 (m_payloadSize);
  i.WriteHtonU16 (m_generation);
}
uint32_t
HomaHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_srcPort = i.ReadNtohU16 ();
  m_dstPort = i.ReadNtohU16 ();
  m_txMsgId = i.ReadNtohU16 ();
  m_flags = i.ReadU8 ();
  m_prio = i.ReadU8 ();
  m_msgSizeBytes = i.ReadNtohU32 ();
  m_pktOffset = i.ReadNtohU16 ();
  m_grantOffset = i.ReadNtohU16 ();
  m_payloadSize = i.ReadNtohU16 ();
  m_generation = i.ReadNtohU16 ();

  return GetSerializedSize ();
}
    
void 
HomaHeader::SetSrcPort (uint16_t port)
{
  m_srcPort = port;
}
uint16_t 
HomaHeader::GetSrcPort (void) const
{
  return m_srcPort;
}
    
void 
HomaHeader::SetDstPort (uint16_t port)
{
  m_dstPort = port;
}
uint16_t 
HomaHeader::GetDstPort (void) const
{
  return m_dstPort;
}
    
void 
HomaHeader::SetTxMsgId (uint16_t txMsgId)
{
  m_txMsgId = txMsgId;
}
uint16_t 
HomaHeader::GetTxMsgId (void) const
{
  return m_txMsgId;
}

void
HomaHeader::SetFlags (uint8_t flags)
{
  m_flags = flags;
}
uint8_t
HomaHeader::GetFlags (void) const
{
  return m_flags;
}
    
void
HomaHeader::SetPrio (uint8_t prio)
{
  m_prio = prio;
}
uint8_t
HomaHeader::GetPrio (void) const
{
  return m_prio;
}
    
void 
HomaHeader::SetMsgSize (uint32_t msgSizeBytes)
{
  m_msgSizeBytes = msgSizeBytes;
}
uint32_t 
HomaHeader::GetMsgSize (void) const
{
  return m_msgSizeBytes;
}
    
void 
HomaHeader::SetPktOffset (uint16_t pktOffset)
{
  m_pktOffset = pktOffset;
}
uint16_t 
HomaHeader::GetPktOffset (void) const
{
  return m_pktOffset;
}
    
void 
HomaHeader::SetGrantOffset (uint16_t grantOffset)
{
  m_grantOffset = grantOffset;
}
uint16_t 
HomaHeader::GetGrantOffset (void) const
{
  return m_grantOffset;
}
    
void 
HomaHeader::SetPayloadSize (uint16_t payloadSize)
{
  m_payloadSize = payloadSize;
}
uint16_t 
HomaHeader::GetPayloadSize (void) const
{
  return m_payloadSize;
}

// TODO: Currently there is no way for the application to
//       signal the generation information to the nanopu-archt.
//       NanoPuAppHeader wouldn't work because "generation"
//       information is not generic enough to be baked into the
//       the architecture. Maybe applications insert this info
//       as the first couple of bytes of the payload.
    
void 
HomaHeader::SetGeneration (uint16_t generation)
{
  m_generation = generation;
}
uint16_t 
HomaHeader::GetGeneration (void) const
{
  return m_generation;
}
    
} // namespace ns3