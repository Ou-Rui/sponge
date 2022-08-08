#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
  // syn & isn
  if (seg.header().syn) {
    if (_isn != nullopt && seg.header().seqno != _isn) {
      cout << "segment_received(): conflict isn, old = " << _isn.value()
          << ", new = " << seg.header().seqno << endl;
      return;
    }
    if (_isn == nullopt) {
      _isn = seg.header().seqno;
      cout << "segment_received(): set isn = " << _isn.value() << endl;
    }
  }


}

optional<WrappingInt32> TCPReceiver::ackno() const {
  if (_isn == nullopt) {
    return nullopt;
  }
  uint64_t index = _reassembler.get_first_unassembled();
  auto ackno = wrap(index, _isn.value());
  cout << "ackno(): index = " << index << ", ackno = " << ackno << endl;
  return ackno;
}

size_t TCPReceiver::window_size() const {
  // number of bytes that ByteStream holds (unread)
  uint64_t bs_size = _reassembler.stream_out().buffer_size();
  uint64_t window = _capacity - bs_size;
  cout << "ackno(): bs_size = " << bs_size
      << ", window = " << window << endl;
  return window;
}
