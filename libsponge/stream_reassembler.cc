#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _output(capacity), _capacity(capacity) {}

StreamReassembler::block_t StreamReassembler::get_block(const std::string &data, const size_t index) const {
  block_t block;
  // remove suffix
  if (index + data.size() > _first_unaccepted) {
    block.data = data.substr(0, _first_unaccepted - index);
  } else {
    block.data = data;
  }
  // remove prefix
  if (index >= _first_unassembled) {
    block.index = index;
  } else {
    block.index = _first_unassembled;
    block.data = block.data.substr(_first_unassembled - index);
  }
  block.len = block.data.size();
//  cout << "get_block(): done, index = " << block.index << ", len = " << block.len << endl;
  return block;
}

bool StreamReassembler::merge_block(block_t& b1, const block_t& b2) {
  // reorder, guarantee: x < y
  block_t x, y;
  x = b1 < b2 ? b1 : b2;
  y = b1 < b2 ? b2 : b1;
//  cout << "merge_block(): x.index = " << x.index << ", x.len = " << x.len
//      << ", y.index = " << y.index << ", y.len = " << y.len << endl;
  if (x.index + x.len < y.index) {
//    cout << "merge_block(): cannot merge" << endl;
    return false;
  }
  if (x.index + x.len >= y.index + y.len) {
    // x contains y
    b1 = x;
  } else {
    b1.index = x.index;
    b1.data = x.data + y.data.substr(x.index + x.len - y.index);
    b1.len = b1.data.size();
  }

//  cout << "merge_block(): done, b1.index = " << b1.index << ", b1.len = " << b1.len << endl;
  return true;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
  // update unread & unaccepted when recv new string
  _first_unread = _output.bytes_read();
  _first_unaccepted = _first_unread + _capacity;
//  cout << "push_substring(): index = " << index << ", len = " << data.length()
//      << ", eof = " << eof << endl;
  // left - 1 ----> right + 1
  if (eof && index + data.size() < _first_unaccepted + 1) {
    _is_end = true;
  }
  if (index + data.size() < _first_unassembled + 1) {
    if (_set.empty() && _is_end) {
      stream_out().end_input();
    }
//    cout << "push_substring(): no use.. return." << endl;
    return;
  }
  if (index >= _first_unaccepted) {
//    cout << "push_substring(): out of capacity.. return. _first_unaccepted = " << _first_unaccepted
//         << ", index = " << index << endl;
    return;
  }
  block_t block = get_block(data, index);
  // empty block should not be added into set, but need to deal with eof
  if (block.len == 0) {
    if (_set.empty() && _is_end) {
      stream_out().end_input();
    }
    return;
  }

  // merge next
  auto iter = _set.lower_bound(block);
//  if (iter == _set.end()) {
//    cout << "push_substring(): lower_bound not found" << endl;
//  } else {
//    cout << "push_substring(): find lower_bound, index = " << iter->index
//         << ", len = " << iter->len << endl;
//  }
  while (iter != _set.end() && merge_block(block, *iter)) {
    _unassembled_bytes -= iter->len;
    _set.erase(iter);
    iter = _set.lower_bound(block);
  }
  // merge prev
  if (iter != _set.begin()) {
    iter--;
    while (merge_block(block, *iter)) {
      _unassembled_bytes -= iter->len;
      _set.erase(iter);
      iter = _set.lower_bound(block);
      if (iter == _set.begin()) {
        break;
      }
      iter--;
    }
  }
  _unassembled_bytes += block.len;
  _set.insert(block);

  // reassemble
  iter = _set.begin();
  while (!_set.empty() && iter->index == _first_unassembled) {
    stream_out().write(iter->data);
    _first_unassembled += iter->len;
    _unassembled_bytes -= iter->len;
    _set.erase(iter);
    iter = _set.begin();
  }
  if (_set.empty() && _is_end) {
    stream_out().end_input();
  }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
