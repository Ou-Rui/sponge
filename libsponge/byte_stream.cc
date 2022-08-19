#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
  if (_error) {
    return 0;
  }
  size_t size = min(data.size(), remaining_capacity());
  _buffers.append(BufferList(data.substr(0, size)));
  _w_cnt += size;
  return size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
  if (_error) {
    return "";
  }
  const size_t size = min(len, _buffers.size());
  string str = _buffers.concatenate();
  return str.substr(0, size);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
  if (_error) {
    return;
  }

  const size_t size = min(len, _buffers.size());
  _buffers.remove_prefix(size);
  _r_cnt += size;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
  if (_error) {
    return "";
  }
  string str = peek_output(len);
  pop_output(len);
  return str;
}

void ByteStream::end_input() { _end = true; }

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return _buffers.size(); }

bool ByteStream::buffer_empty() const { return _buffers.size() == 0; }

bool ByteStream::eof() const { return _end && _buffers.size() == 0; }

size_t ByteStream::bytes_written() const { return _w_cnt; }

size_t ByteStream::bytes_read() const { return _r_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffers.size(); }
