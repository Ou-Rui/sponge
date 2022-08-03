#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _map(), _map_size(0), _first_unassembled(0), _first_unaccepted(0), _is_end(false),
    _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
  cout << "push_substring(): index = " << index << ", eof = " << eof << endl;
  if (eof) {
    _is_end = true;
  }
  size_t str_index = getFirstIndex(data, index);
  cout << "getFirstIndex(): str_index = " << str_index << endl;
  if (str_index >= data.size()) {
    cout << "useless segment..." << endl;
    if (_is_end && empty()) {
      cout << "push_substring(): ALL BYTES ASSEMBLED" << endl;
      _output.end_input();
    }
    return;
  }
  loadString(data, index, str_index);
}

//! \brief Get index-in-str of the first (possibly) useful byte
//! \note return_index is the index inside the input segment, not the entire ByteStream
size_t StreamReassembler::getFirstIndex(const std::string &data, const uint64_t index) const {
  const uint64_t last_index = index + data.size() - 1;
  // all useful
  if (_first_unassembled <= index) {
    return 0;
  }
  // useless
  if (_first_unassembled > last_index) {
    return data.size();
  }
  // part useful
  return _first_unassembled - index;
}

//! \brief Load input string into map
//! \param str_index: index-in-str of the first (possibly) useful byte
//! \return could be reassembled?
void StreamReassembler::loadString(const std::string &data, const uint64_t index, const size_t str_index) {
  // sequential load
  size_t i;
  for (i = str_index; i < data.size(); ++i) {
    if (index + i < _first_unassembled)
      continue;
    if (full()) {
      if (empty() || index + i >= _first_unaccepted) {
        // no space or no need
        break;
      }
      evict();
    }
    _map.insert_or_assign(index + i, data[i]);
    _first_unaccepted = max(_first_unaccepted, index + i + 1);
    reassemble();
  }
  cout << "loadString(): _first_unaccepted = " << _first_unaccepted
      << ", _first_unassembled = " << _first_unassembled << endl;
}

//! \brief evict the last byte, and update _first_unaccepted
void StreamReassembler::evict() {
  int i = _first_unaccepted - 1;
  // find the last byte in _map
  while (i >= 0 && _map.find(i) == _map.end()) {
    i--;
  }
  // evict
  cout << "evict(): evict index = " << i << endl;
  _map.erase(i);
  // find the last byte in _map
  while (i >= 0 && _map.find(i) == _map.end()) {
    i--;
  }
  // _first_unaccepted should be the "index of last byte" + 1
  if (_map.find(i) != _map.end()) {
    _first_unaccepted = i + 1;
  } else {
    _first_unaccepted = 0;
  }
  cout << "evict(): done, _first_unaccepted = " << _first_unaccepted
      << ", map_size = " << _map.size() << endl;
  _is_end = false;
}


//! \brief push the assembled bytes into the _output ByteStream
void StreamReassembler::reassemble() {
  cout << "reassemble(): start map_size = " << _map.size() << endl;
  for (size_t i = _first_unassembled; _map.find(i) != _map.end(); ++i) {
    string str;
    str += _map[i];
    _output.write(str);
    _map.erase(i);
    _first_unassembled = i + 1;
  }
  cout << "reassemble(): end map_size = " << _map.size()
      << ", _first_unassembled = " << _first_unassembled << endl;
  if (_is_end && empty()) {
    cout << "reassemble(): ALL BYTES ASSEMBLED" << endl;
    _output.end_input();
  }
}

size_t StreamReassembler::unassembled_bytes() const { return _map.size(); }

bool StreamReassembler::empty() const { return _map.empty(); }

bool StreamReassembler::full() const {
  size_t m_sz = _map.size();
  size_t out_sz = _output.buffer_size();
  return m_sz + out_sz >= _capacity;
}