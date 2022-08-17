#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! private helper functions
//!@{
void TCPConnection::set_ackno_win(TCPSegment &seg) const {
  auto ackno = _receiver.ackno();
  if (!ackno) {
    seg.header().ack = false;
  } else {
    seg.header().ack = true;
    seg.header().ackno = ackno.value();
    seg.header().win = _receiver.window_size();
  }
}

void TCPConnection::send_segments() {
  while (!_sender.segments_out().empty()) {
    TCPSegment seg = _sender.segments_out().front();
    // set ackno & window_size according to receiver
    set_ackno_win(seg);
    seg.print_segment("TCPConnection::send_segments", "new segment");
    _segments_out.push(seg);
    _sender.segments_out().pop();
  }
}

void TCPConnection::send_rst() {
  // send RST segment
  TCPSegment seg;
  seg.header().rst = true;
  _segments_out.push(seg);
  // RST myself
  _sender.stream_in().set_error();
  _receiver.stream_out().set_error();
  _linger_after_streams_finish = false;
}

void TCPConnection::check_clean_shutdown() {
  cout << "TCPConnection::check_clean_shutdown(): start" << endl;
  if (!active()) {
    cout << "TCPConnection::check_clean_shutdown(): already shutdown.." << endl;
    return;
  }
  // Prerequisite 1: inbound stream ended && all reassembled
  bool pre1 = pre1_inbound_eof_and_all_reassembled();
  // Prerequisite 2: outbound stream ended && all data transmitted (including FIN)
  // next_seqno_abs == data_size + 1(SYN) + 1(FIN)
  bool pre2 = pre2_outbound_eof_and_all_tx();
  // Prerequisite 3: outbound stream fully ACK
  bool pre3 = pre3_outbound_fully_ack();
  // Prerequisite 4: remote peer can be clean shutdown, approximation:
  //    a. no need to linger  OR
  //    b. already linger enough time
  bool pre4 = !_linger_after_streams_finish || _tick_since_last_rx >= 10 * _cfg.rt_timeout;
  cout << "TCPConnection::check_clean_shutdown(): pre1 = " << pre1 << ", pre2 = " << pre2
      << ", pre3 = " << pre3 << ", pre4 = " << pre4 << endl;
  if (pre1 && pre2 && pre3 && pre4) {
    cout << "TCPConnection::check_clean_shutdown(): clean shutdown!" << endl;
    _linger_after_streams_finish = false;
  }

}
//!@}

size_t TCPConnection::remaining_outbound_capacity() const {
  return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
  return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
  return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
  return _tick_since_last_rx;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
  if (!active()) {
    cout << "TCPConnection::segment_received(): not active.." << endl;
    return;
  }

  seg.print_segment("TCPConnection::segment_received", "new seg rx");
  _tick_since_last_rx = 0;
  // RST
  if (seg.header().rst) {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
    return;
  }
  // receiver
  _receiver.segment_received(seg);
  // passive close
  if (inbound_stream().eof() && !outbound_stream().eof()) {
    cout << "TCPConnection::segment_received(): passive close, inbound eof, unbound not eof" << endl;
    _linger_after_streams_finish = false;
  }
  // ACK
  if (seg.header().ack) {
    _sender.ack_received(seg.header().ackno, seg.header().win);
  }
  // answer
  if (seg.length_in_sequence_space() > 0) {
    _sender.fill_window();
    // if _sender has nothing to send, send an empty ACK
    if (_sender.segments_out().empty()) {
      TCPSegment ack_seg;
      ack_seg.header().seqno = _sender.next_seqno();
      set_ackno_win(ack_seg);
      ack_seg.print_segment("TCPConnection::segment_received", "sender has nothing to send, send an empty ACK..");
      _segments_out.push(ack_seg);
    }
  }

  // special case: send empty segment to keep alive
  if (seg.length_in_sequence_space() == 0 && _receiver.ackno().has_value()
      && seg.header().seqno == _receiver.ackno().value() - 1) {
    _sender.send_empty_segment();
  }
  // actually send segments
  send_segments();
}

bool TCPConnection::active() const {
  bool pre1 = pre1_inbound_eof_and_all_reassembled();
  bool pre2 = pre2_outbound_eof_and_all_tx();
  bool pre3 = pre3_outbound_fully_ack();
  cout << "TCPConnection::active(): pre1 = " << pre1 << ", pre2 = " << pre2
      << ", pre3 = " << pre3 << endl;

  bool inbound_done = _receiver.stream_out().error() || pre1;
  bool outbound_done = _sender.stream_in().error() || (pre2 && pre3);
  cout << "TCPConnection::active(): inbound_done = " << inbound_done
       << ", outbound_done = " << outbound_done << endl;

  return !inbound_done || !outbound_done || _linger_after_streams_finish;
}

size_t TCPConnection::write(const string &data) {
  if (!active()) {
    cout << "TCPConnection::write(): not active.." << endl;
    return 0;
  }

  size_t size = _sender.stream_in().write(data);
  cout << "TCPConnection::write(): data_size = " << data.size() << ", write_size = " << size
      << ", inbound_size = " << _sender.stream_in().buffer_size() << endl;

  _sender.fill_window();
  send_segments();

  return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
  cout << "TCPConnection::tick(): tick = " << ms_since_last_tick << "ms" << endl;
  if (!active()) {
    cout << "TCPConnection::tick(): not active.." << endl;
    return;
  }

  _tick_since_last_rx += ms_since_last_tick;

  _sender.tick(ms_since_last_tick);
  // check consecutive_retx counter
  if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
    send_rst();
    return;
  }
  // retx outstanding segments
  while (!_sender.segments_out().empty()) {
    TCPSegment seg = _sender.segments_out().front();
    set_ackno_win(seg);
    _segments_out.push(seg);
    seg.print_segment("TCPConnection::tick", "retx outstanding segment");
    _sender.segments_out().pop();
  }

  check_clean_shutdown();
}

void TCPConnection::end_input_stream() {
  cout << "TCPConnection::end_input_stream(): outbound ByteStream ended" << endl;
  _sender.stream_in().end_input();

  if (_receiver.stream_out().eof()) {
    cout << "TCPConnection::end_input_stream(): inbound already EOF, linger = false" << endl;
    _linger_after_streams_finish = false;
  }

  _sender.fill_window();
  while (!_sender.segments_out().empty()) {
    TCPSegment seg = _sender.segments_out().front();
    set_ackno_win(seg);
    _segments_out.push(seg);
    seg.print_segment("TCPConnection::end_input_stream", "send a seg");
    _sender.segments_out().pop();
  }
}

// send a syn segment
void TCPConnection::connect() {
  if (!active()) {
    cout << "TCPConnection::connect(): not active.." << endl;
    return;
  }

  _sender.fill_window();
  TCPSegment seg = _sender.segments_out().front();
  cout << "TCPConnection::connect(): _sender.segments_out().size() = " << _sender.segments_out().size()
      << ", SYN = " << seg.header().syn << ", ACK = " << seg.header().ack << endl;
  _segments_out.push(seg);
  _sender.segments_out().pop();
}

TCPConnection::~TCPConnection() {
  cout << "TCPConnection(): de-constructor" << std::endl;
  try {
      if (active()) {
        cerr << "Warning: Unclean shutdown of TCPConnection\n";
          // Your code here: need to send a RST segment to the peer
        send_rst();
      }
  } catch (const exception &e) {
      std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
  }
}
