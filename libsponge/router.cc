#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template<typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

bool Router::match(uint32_t target, route_t route) const {
  if (route.len == 0)
    return true;
  uint8_t shift = 32 - route.len;
  return (target >> shift) == (route.prefix >> shift);
//  uint32_t tmp = (route.len == 0) ? 0 : (0xffffffff << (32 - route.len));
//  return (target & tmp) == (route.prefix & tmp);
}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
       << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
  for (route_t& route : _route_tbl) {
    if (route.prefix == route_prefix && route.len == prefix_length) {
      cout << "add_route(): duplicate route, overwrite." << endl;
      route.next_hop = next_hop;
      route.index = interface_num;
      return;
    }
  }
  _route_tbl.push_back({route_prefix, prefix_length, next_hop, interface_num});
  cout << "add_route(): new route created." << endl;
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
  // Your code here.
  uint32_t ip = dgram.header().dst;
  cout << "route_one_datagram(): ip = " << ip << endl;
  if (dgram.header().ttl <= 1) {
    dgram.header().ttl = 0;
    cout << "route_one_datagram(): TTL end, drop datagram.." << endl;
    return;
  }
  dgram.header().ttl--;

  route_t target_route;
  bool found = false;
  for (const route_t& route : _route_tbl) {
    if ((!found || route.len > target_route.len) && match(ip, route)) {
      found = true;
      target_route = route;
    }
  }

  if (!found) {
    cout << "route_one_datagram(): no route matched.." << endl;
    return;
  }
  cout << "route_one_datagram(): route to interface " << target_route.index << endl;
  if (target_route.next_hop.has_value()) {
    interface(target_route.index).send_datagram(dgram, target_route.next_hop.value());
  } else {
    interface(target_route.index).send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
  }
}

void Router::route() {
  // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
  for (auto &interface: _interfaces) {
    auto &queue = interface.datagrams_out();
    while (not queue.empty()) {
      route_one_datagram(queue.front());
      queue.pop();
    }
  }
}
