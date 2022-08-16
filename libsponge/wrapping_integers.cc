#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
//  cout << "wrap(): n = " << n << ", isn = " << isn << endl;
  uint64_t max_32 = (1ll << 32);
  uint32_t tmp = n % max_32;
//  cout << "wrap(): max_32 = " << max_32 << ", tmp = " << tmp << endl;
  if (tmp < max_32 - isn.raw_value()) {
    return isn + tmp;
  }
  return isn - (max_32 - tmp);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
  //cout << "unwrap(): n = " << n << ", isn = " << isn << ", cp = " << checkpoint << endl;
  uint64_t max_32 = (1ll << 32);
  int32_t dist = n - isn;
  uint64_t tmp = checkpoint >> 32;
  // can1 < cp; can2 > cp
  uint64_t can1, can2;
  can1 = (tmp << 32) + dist;
  can2 = (tmp << 32) + dist + max_32;
  //cout << "unwrap(): can1 = " << can1 << ", can2 = " << can2 << ", cp = " << checkpoint << endl;
  return get_dist(checkpoint, can1) < get_dist(checkpoint, can2) ? can1 : can2;
}

uint64_t get_dist(uint64_t a, uint64_t b) {
  if (a > b) {
    return a - b;
  }
  return b - a;
}