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

#include "ns3/object.h"
#include "homa-socket-factory.h"
#include "homa-l4-protocol.h"
#include "ns3/socket.h"
#include "ns3/assert.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (HomaSocketFactory);

TypeId HomaSocketFactory::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::HomaSocketFactory")
    .SetParent<SocketFactory> ()
    .SetGroupName ("Internet")
  ;
  return tid;
}
    
HomaSocketFactory::HomaSocketFactory ()
  : m_homa (0)
{
}
HomaSocketFactory::~HomaSocketFactory ()
{
  NS_ASSERT (m_homa == 0);
}
    
void
HomaSocketFactory::SetHoma (Ptr<HomaL4Protocol> homa)
{
  m_homa = homa;
}

Ptr<Socket>
HomaSocketFactory::CreateSocket (void)
{
  return m_homa->CreateSocket ();
}

void 
HomaSocketFactory::DoDispose (void)
{
  m_homa = 0;
  Object::DoDispose ();
}

} // namespace ns3