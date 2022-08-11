#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
  cout << "bytes_in_flight(): _next_seqno = " << _next_seqno
      << ", ackno = " << _abs_ackno << endl;
  return _next_seqno - _abs_ackno;
}

void TCPSender::fill_window() {
  unsigned int window_size = _window_size > 0 ? _window_size : 1;
  cout << "fill_window(): start, _next_seqno = " << _next_seqno
      << ", _abs_ackno = " << _abs_ackno << ", 'window_size' = " << window_size << endl;

  if (_fin) {
    cout << "fill_window(): fin, return" << endl;
    return;
  }

  // right bound = ackno + win
  while (_next_seqno < _abs_ackno + window_size) {

    TCPSegment seg = make_segment();

    if (seg.length_in_sequence_space() == 0) {
      cout << "fill_window(): stream empty.. _next_seqno = " << _next_seqno
          << ", _abs_ackno = " << _abs_ackno << ", 'window_size' = " << window_size << endl;
      return;
    }

    _next_seqno += seg.length_in_sequence_space();

    _segments_out.push(seg);
    _outstanding_segments.push(seg);
    _timer.on();
    cout << "fill_window(): make one segment, _next_seqno = " << _next_seqno
        << ", fin = " << seg.header().fin << endl;
    if (seg.header().fin) {
      _fin = true;
      break;
    }

  }
  cout << "fill_window(): done, _next_seqno = " << _next_seqno << endl;
}

TCPSegment TCPSender::make_segment() {

  TCPSegment seg;
  seg.header().seqno = wrap(_next_seqno, _isn);
  seg.header().syn = _next_seqno == 0;

  unsigned int window_size = _window_size > 0 ? _window_size : 1;
  uint64_t right_bound = _abs_ackno + window_size;
  uint64_t read_size = min(TCPConfig::MAX_PAYLOAD_SIZE, right_bound - _next_seqno);
  Buffer payload{_stream.read(read_size)};
  seg.payload() = payload;

  seg.header().fin = _stream.eof() && _stream.input_ended()
      && _next_seqno + seg.payload().size() < right_bound;      // check if still have space in window

  cout << "make_segment(): _next_seqno = " << _next_seqno
      << ", seqno = " << seg.header().seqno << ", syn = " << seg.header().syn
      << ", fin = " << seg.header().fin << ", payload_size = " << payload.size() << endl;
  return seg;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
  uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
  if (abs_ackno > _next_seqno) {
    cout << "ack_received(): ackno beyond next_seqno, ignore.. abs_ackno = " << abs_ackno
        << ", _next_seqno = " << _next_seqno << endl;
    return;
  }
  if (abs_ackno < _abs_ackno) {
    cout << "ack_received(): out-of-date ACK.. ignore abs_ackno = " << abs_ackno
         << ", _abs_ackno = " << _abs_ackno << endl;
    return;
  }

  if (abs_ackno > _abs_ackno) {
    _timer.reset(_initial_retransmission_timeout);
  }

  _abs_ackno = abs_ackno;
  cout << "ack_received(): ackno(Wrapped) = " << ackno
      << ", _abs_ackno = " << _abs_ackno
      << ", window_size = " << window_size << endl;

  _window_size = window_size;     // min_window_size = 1
  _cons_retx = 0;

  remove_outstanding();

  if (_outstanding_segments.empty()) {
    _timer.off();
  } else {
    _timer.on();
  }

}

void TCPSender::remove_outstanding() {
  cout << "remove_outstanding(): start, size = " << _outstanding_segments.size()
      << ", _abs_ackno = " << _abs_ackno << endl;
  while (!_outstanding_segments.empty()) {
    TCPSegment seg = _outstanding_segments.front();
    uint64_t abs_seqno_seg = unwrap(seg.header().seqno, _isn, _next_seqno);
    uint64_t last_seqno_seg = abs_seqno_seg + seg.length_in_sequence_space();
    if (last_seqno_seg <= _abs_ackno) {
      cout << "remove_outstanding(): pop one, abs_seqno = " << abs_seqno_seg
          << ", last_seqno_seg = " << last_seqno_seg << endl;
      _outstanding_segments.pop();
    } else {
      break;
    }
  }
  cout << "remove_outstanding(): done size = " << _outstanding_segments.size() << endl;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
  if (_timer.tick(ms_since_last_tick)) {
    // retx first segment, not pop()
    _segments_out.push(_outstanding_segments.front());
    if (_window_size != 0) {
      _cons_retx++;
      // double the rto and restart timeout counter
      _timer.push_off();
    }
    cout << "tick(): timeout, retx seg.seqno = " << _outstanding_segments.front().header().seqno
        << ", _window_size = " << _window_size << ", _cons_retx = " << _cons_retx << endl;
  }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _cons_retx; }

void TCPSender::send_empty_segment() {
  TCPSegment seg;
  seg.header().seqno = wrap(_next_seqno, _isn);
  _segments_out.push(seg);
  cout << "send_empty_segment(): _nex_seqno = " << _next_seqno
      << ", seqno = " << seg.header().seqno << endl;
}
