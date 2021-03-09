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

#ifndef HOMA_HEADER_H
#define HOMA_HEADER_H

#include "ns3/header.h"

namespace ns3 {
/**
 * \ingroup homa
 * \brief Packet header for Homa Transport packets
 *
 * This class has fields corresponding to those in a network Homa header
 * (transport protocol) as well as methods for serialization
 * to and deserialization from a byte buffer.
 */
class HomaHeader : public Header 
{
public:
  /**
   * \brief Constructor
   *
   * Creates a null header
   */
  HomaHeader ();
  ~HomaHeader ();
  
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  
  /**
   * \param port The source port for this HomaHeader
   */
  void SetSrcPort (uint16_t port);
  /**
   * \return The source port for this HomaHeader
   */
  uint16_t GetSrcPort (void) const;
  
  /**
   * \param port The destination port for this HomaHeader
   */
  void SetDstPort (uint16_t port);
  /**
   * \return the destination port for this HomaHeader
   */
  uint16_t GetDstPort (void) const;
  
  /**
   * \param txMsgId The TX message ID for this HomaHeader
   */
  void SetTxMsgId (uint16_t txMsgId);
  /**
   * \return The source port for this HomaHeader
   */
  uint16_t GetTxMsgId (void) const;
  
  /**
   * \brief Set flags of the header
   * \param flags the flags for this HomaHeader
   */
  void SetFlags (uint8_t flags);
  /**
   * \brief Get the flags
   * \return the flags for this HomaHeader
   */
  uint8_t GetFlags () const;
  
  /**
   * \brief Set priority for the response packets
   * \param prio Priority requested for the response packets
   */
  void SetPrio (uint8_t prio);
  /**
   * \brief Get the priority for the response packets
   * \return the priority requested for the response packets
   */
  uint8_t GetPrio () const;
  
//   /**
//    * \param icw The initial credit allowed for transmissions
//    */
//   void SetIncoming (uint16_t icw);
//   /**
//    * \return The initial credit allowed for transmissions
//    */
//   uint16_t GetIncoming (void) const;
  
  /**
   * \param msgSizeBytes The message size for this HomaHeader in bytes
   */
  void SetMsgSize (uint32_t msgSizeBytes);
  /**
   * \return The message size for this HomaHeader in bytes
   */
  uint32_t GetMsgSize (void) const;
  
  /**
   * \param pktOffset The packet identifier for this HomaHeader in number of packets 
   */
  void SetPktOffset (uint16_t pktOffset);
  /**
   * \return The packet identifier for this HomaHeader in number of packets 
   */
  uint16_t GetPktOffset (void) const;
  
  /**
   * \param grantOffset The grant identifier for this HomaHeader in number of packets 
   */
  void SetGrantOffset (uint16_t grantOffset);
  /**
   * \return The grant identifier for this HomaHeader in number of packets 
   */
  uint16_t GetGrantOffset (void) const;
  
  /**
   * \param payloadSize The payload size for this HomaHeader in bytes
   */
  void SetPayloadSize (uint16_t payloadSize);
  /**
   * \return The payload size for this HomaHeader in bytes
   */
  uint16_t GetPayloadSize (void) const;
  
  /**
   * \param generation The generation of the RPC this packet is associated with
   */
  void SetGeneration (uint16_t generation);
  /**
   * \return The generation of the RPC this packet is associated with
   */
  uint16_t GetGeneration (void) const;
  
  /**
   * \brief Converts an integer into a human readable list of Homa flags
   *
   * \param flags Bitfield of Homa flags to convert to a readable string
   * \param delimiter String to insert between flags
   *
   * \return the generated string
   **/
  static std::string FlagsToString (uint8_t flags, const std::string& delimiter = "|");
  
  /**
   * \brief Homa flag field values
   */
  typedef enum Flags_t
  {
    DATA = 1,     //!< DATA Packet
    GRANT = 2,    //!< GRANT
    RESEND = 4,   //!< RESEND (Only sent from senders)
    ACK = 8,      //!< ACK (sent once the msg is completely received)
    BUSY = 16,    //!< BUSY
    CUTOFFS = 32, //!< Priority cutoffs
    FREEZE = 64,  //!< 
    BOGUS = 128   //!< Used only in unit tests.
  } Flags_t;
  
  static const uint8_t PROT_NUMBER = 198; //!< Protocol number of HOMA to be used in IP packets
  
private:

  uint16_t m_srcPort;     //!< Source port
  uint16_t m_dstPort;     //!< Destination port
  uint16_t m_txMsgId;     //!< ID generated by the sender
  uint8_t m_flags;        //!< Flags
  uint8_t m_prio;         //!< Priority to be used for the response
//   Use grantOffset for the initial window size
//   uint16_t m_incoming;    //!< Initial window size (in packets)
  uint32_t m_msgSizeBytes;//!< Size of the message in bytes
  uint16_t m_pktOffset;   //!< Similar to seq number (in packets)
  uint16_t m_grantOffset; //!< Similar to ack number (in packets)
  uint16_t m_payloadSize; //!< Payload size
  uint16_t m_generation;  //!< The generation of the RPC
  
};
} // namespace ns3
#endif /* HOMA_HEADER */