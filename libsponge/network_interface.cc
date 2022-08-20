#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
//    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
//         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
  // convert IP address of next hop to raw 32-bit representation (used in ARP header)
  const uint32_t next_hop_ip = next_hop.ipv4_numeric();
//  cout << "send_datagram(): next_hop_ip = " << next_hop_ip << endl;
  EthernetFrame frame;
  frame.header().type = EthernetHeader::TYPE_IPv4;
  frame.header().src = _ethernet_address;
  frame.payload() = dgram.serialize();
  if (_map.count(next_hop_ip) && _map[next_hop_ip].ttl >= _timer) {
    // valid cache
    frame.header().dst = _map[next_hop_ip].addr;
    _frames_out.push(move(frame));
//    cout << "send_datagram(): cached, done" << endl;
  } else {
    // send ARP Request
//    cout << "send_datagram(): not cached, send arp.." << endl;
    _waiting_queue.push({frame, next_hop_ip});
    _arp_queue.push(next_hop_ip);
    _send_arp_request();
  }

}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
  if (_mac_equal(frame.header().dst, _ethernet_address) ||
      _mac_equal(frame.header().dst, ETHERNET_BROADCAST)) {
//    cout<< "recv_frame(): recv my frame" << endl;
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
      InternetDatagram datagram;
      if (datagram.parse(frame.payload()) == ParseResult::NoError) {
        return datagram;
      }
//      cout<< "recv_frame(): ipv4 parse error.." << endl;
      return nullopt;
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
      ARPMessage msg;
      if (msg.parse(frame.payload()) == ParseResult::NoError) {
        _recv_arp_handler(msg);
      } else {
//        cout<< "recv_frame(): ARP parse error.." << endl;
      }
      return nullopt;
    }
//    cout<< "recv_frame(): unknown frame type.." << endl;
  }

  return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
  _timer += ms_since_last_tick;
  _arp_timer += ms_since_last_tick;
  _send_arp_request();
}

void NetworkInterface::_send_arp_request() {
  if (_arp_timer >= ARP_RETX_TIME) {
    uint32_t ip;
    while (!_arp_queue.empty()) {
      ip = _arp_queue.front();
      if (_map.count(ip) && _map[ip].ttl >= _timer) {
        _arp_queue.pop();
      } else {
        break;
      }
    }
    if (_arp_queue.empty()) {
//      cout << "_send_arp_request(): nothing need to request" << endl;
      return;
    }
//    cout << "_send_arp_request(): timeout, send arp_request for ip = " << ip << endl;
    // construct ARPMessage
    ARPMessage msg;
    msg.opcode = ARPMessage::OPCODE_REQUEST;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.sender_ethernet_address = _ethernet_address;
    msg.target_ip_address = ip;
    // construct EthernetFrame
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.header().src = _ethernet_address;
    frame.header().dst = ETHERNET_BROADCAST;
    frame.payload() = msg.serialize();
    // send
    _frames_out.push(move(frame));
    _arp_timer = 0;
  }
}

bool NetworkInterface::_mac_equal(const EthernetAddress& addr1, const EthernetAddress& addr2) const {
  for (int i = 0; i < 6; ++i) {
    if (addr1[i] != addr2[i])
      return false;
  }
  return true;
}

void NetworkInterface::_recv_arp_handler(const ARPMessage& msg) {
//  cout << "_recv_arp_handler(): cache ip = " << msg.sender_ip_address << endl;
  _map[msg.sender_ip_address] = {msg.sender_ethernet_address, _timer + CACHE_TTL};
  // if recv ARP Request, send ARP Reply
  if (msg.opcode == ARPMessage::OPCODE_REQUEST &&
      msg.target_ip_address == _ip_address.ipv4_numeric()) {
    // construct ARPMessage
    ARPMessage reply;
    reply.opcode = ARPMessage::OPCODE_REPLY;
    reply.sender_ip_address = _ip_address.ipv4_numeric();
    reply.sender_ethernet_address = _ethernet_address;
    reply.target_ip_address = msg.sender_ip_address;
    reply.target_ethernet_address = msg.sender_ethernet_address;
    // construct EthernetFrame
    EthernetFrame frame;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.header().src = _ethernet_address;
    frame.header().dst = msg.sender_ethernet_address;
    frame.payload() = reply.serialize();
    // send
//    cout << "_recv_arp_handler(): recv ARP Request, send Reply to ip = " << reply.target_ip_address << endl;
    _frames_out.push(move(frame));
  }
  // check waiting queue
  while (!_waiting_queue.empty()) {
    uint32_t dst_ip = _waiting_queue.front().dst_ip;
    if (_map.count(dst_ip) && _map[dst_ip].ttl >= _timer) {
//      cout << "_recv_arp_handler(): waiting for ip = " << dst_ip << " satisfied, send IPv4 Frame" << endl;
      // ip found, send ipv4 frame
      EthernetFrame frame = _waiting_queue.front().frame;
      frame.header().dst = _map[dst_ip].addr;
      _frames_out.push(move(frame));
      _waiting_queue.pop();
      // enable ARP Request tx
      _arp_timer = ARP_RETX_TIME;
    } else {
//      cout << "_recv_arp_handler(): waiting for ip = " << dst_ip << " not satisfied.. break" << endl;
      break;
    }
  }
}