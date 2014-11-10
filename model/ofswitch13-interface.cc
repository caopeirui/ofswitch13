/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */
#ifdef NS3_OFSWITCH13

#include "ofswitch13-interface.h"
#include "ofswitch13-net-device.h"
#include "ofswitch13-controller.h"

NS_LOG_COMPONENT_DEFINE ("OFSwitch13Interface");

namespace ns3 {
namespace ofs {

ofpbuf*
BufferFromPacket (Ptr<const Packet> packet, size_t bodyRoom,
                  size_t headRoom)
{
  NS_LOG_FUNCTION_NOARGS ();

  uint32_t pktSize = packet->GetSize ();
  NS_ASSERT (pktSize <= bodyRoom);

  ofpbuf *buffer = ofpbuf_new_with_headroom (bodyRoom, headRoom);
  packet->CopyData ((uint8_t*)ofpbuf_put_uninit (buffer, pktSize), pktSize);
  return buffer;
}

ofpbuf*
BufferFromMsg (ofl_msg_header *msg, uint32_t xid, ofl_exp *exp)
{
  NS_LOG_FUNCTION_NOARGS ();

  int error;
  uint8_t *buf;
  size_t buf_size;
  ofpbuf *ofpbuf = ofpbuf_new (0);

  // Pack message into ofpbuf using wire format
  error = ofl_msg_pack (msg, xid, &buf, &buf_size, exp);
  if (error)
    {
      NS_LOG_ERROR ("Error packing message.");
    }
  ofpbuf_use (ofpbuf, buf, buf_size);
  ofpbuf_put_uninit (ofpbuf, buf_size);

  return ofpbuf;
}

Ptr<Packet>
PacketFromMsg (ofl_msg_header *msg, uint32_t xid)
{
  return PacketFromBufferAndFree (BufferFromMsg (msg, xid));
}

Ptr<Packet>
PacketFromBufferAndFree (ofpbuf* buffer)
{
  NS_LOG_FUNCTION_NOARGS ();
  Ptr<Packet> packet = Create<Packet> ((uint8_t*)buffer->data, buffer->size);
  ofpbuf_delete (buffer);
  return packet;
}

Ptr<Packet>
PacketFromBuffer (ofpbuf* buffer)
{
  NS_LOG_FUNCTION_NOARGS ();
  Ptr<Packet> packet = Create<Packet> ((uint8_t*)buffer->data, buffer->size);
  return packet;
}


Ptr<Packet>
PacketFromInternalPacket (packet *pkt)
{
  NS_LOG_FUNCTION_NOARGS ();
  ofpbuf *buffer = pkt->buffer;
  Ptr<Packet> packet = Create<Packet> ((uint8_t*)buffer->data, buffer->size);
  return packet;
}

} // namespace ofs
} // namespace ns3

using namespace ns3;

Ptr<OFSwitch13NetDevice> GetDatapathDevice (uint64_t id);

/**
 * Overriding ofsoftswitch13 time_now weak function from lib/timeval.c.
 * \return The current simulation time, in seconds.
 */
time_t
time_now (void)
{
  return (time_t) Simulator::Now ().ToInteger (Time::S);
}

/**
 * Overriding ofsoftswitch13 time_msec weak function from lib/timeval.c.
 * \return The current simulation time, in ms.
 */
long long int
time_msec (void)
{
  return (long long int)Simulator::Now ().GetMilliSeconds ();
}

/**
 * Overriding ofsoftswitch13 send_openflow_buffer_to_remote weak function from
 * udatapath/datapath.c. Sends the given OFLib buffer message to the controller
 * associated with remote connection structure.
 * \internal This function relies on the global map that stores ofpenflow
 * devices to call the method on the correct object (\see
 * ofswitch13-net-device.cc).
 * \param buffer The message buffer to send.
 * \param remote The controller connection information.
 * \return 0 if everything's ok, error number otherwise.
 */
int
send_openflow_buffer_to_remote (struct ofpbuf *buffer, struct remote *remote)
{
  int error = 0;

  Ptr<OFSwitch13NetDevice> dev = GetDatapathDevice (remote->dp->id);
  error = dev->SendToController (buffer, remote);
  if (error)
    {
      NS_LOG_WARN ("There was an error sending the message!");
      return error;
    }
  return 0;
}

/**
 * Overriding ofsoftswitch13 dp_ports_output weak function from
 * udatapath/dp_ports.c. Outputs a datapath packet on the port.
 * \internal This function relies on the global map that stores ofpenflow
 * devices to call the method on the correct object (\see
 * ofswitch13-net-device.cc).
 * \param dp The datapath.
 * \param buffer The packet buffer.
 * \param out_port The port number.
 * \param queue_id The queue to use.
 */
void
dp_ports_output (struct datapath *dp, struct ofpbuf *buffer,
                 uint32_t out_port, uint32_t queue_id)
{
  Ptr<OFSwitch13NetDevice> dev = GetDatapathDevice (dp->id);
  dev->SendToSwitchPort (buffer, out_port, queue_id);
}

/**
 * Overriding ofsoftswitch13 dpctl_send_and_print weak function from
 * utilities/dpctl.c. Send a message from controller to switch.
 * \param vconn The SwitchInfo pointer, sent from controller to
 * dpctl_exec_ns3_command function and get back here to proper identify the
 * controller object.
 * \param msg The OFLib message to send.
 */
void
dpctl_send_and_print (struct vconn *vconn, struct ofl_msg_header *msg)
{
  SwitchInfo *sw = (SwitchInfo*)vconn;
  sw->ctrl->SendToSwitch (sw, msg, 0);
}

/**
 * Overriding ofsoftswitch13 dpctl_transact_and_print weak function from
 * utilities/dpctl.c. Send a message from controller to switch.
 * \internal Different from ofsoftswitch13 dpctl, this transaction doesn't
 * wait for a reply, as ns3 socket library doesn't provide blocking sockets. So,
 * we send the request and return. The reply will came later, using the ns3
 * callback mechanism.
 * \param vconn The SwitchInfo pointer, sent from controller to
 * dpctl_exec_ns3_command function and get back here to proper identify the
 * controller object.
 * \param msg The OFLib request to send.
 * \param repl The OFLib reply message (not used by ns3).
 */
void
dpctl_transact_and_print (struct vconn *vconn, struct ofl_msg_header *req,
                          struct ofl_msg_header **repl)
{
  dpctl_send_and_print (vconn, req);
}

#endif // NS3_OFSWITCH13
