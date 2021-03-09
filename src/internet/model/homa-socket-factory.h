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

#ifndef HOMA_SOCKET_FACTORY_H
#define HOMA_SOCKET_FACTORY_H

#include "ns3/socket-factory.h"
#include "ns3/ptr.h"

namespace ns3 {

class Socket;
class HomaL4Protocol;

/**
 * \ingroup socket
 * \ingroup homa
 *
 * \brief API to create HOMA socket instances 
 *
 * This abstract class defines the API for HOMA socket factory.
 * All HOMA implementations must provide an implementation of CreateSocket
 * below.
 * 
 */
class HomaSocketFactory : public SocketFactory
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  
  HomaSocketFactory ();
  virtual ~HomaSocketFactory ();

  /**
   * \brief Set the associated HOMA L4 protocol.
   * \param homa the HOMA L4 protocol
   */
  void SetHoma (Ptr<HomaL4Protocol> homa);

  /**
   * \brief Implements a method to create a Homa-based socket and return
   * a base class smart pointer to the socket.
   *
   * \return smart pointer to Socket
   */
  virtual Ptr<Socket> CreateSocket (void);

protected:
  virtual void DoDispose (void);
private:
  Ptr<HomaL4Protocol> m_homa; //!< the associated HOMA L4 protocol
};
    
} // namespace ns3

#endif /* HOMA_SOCKET_FACTORY_H */