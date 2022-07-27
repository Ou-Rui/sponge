#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _queue(), _capacity(capacity), _w_cnt(0), _r_cnt(0), _end(false), _error(false) {}

size_t ByteStream::write(const string &data) {
    if (_error) {
        return 0;
    }
    size_t size = min(data.size(), remaining_capacity());
    for (size_t i = 0; i < size; i++) {
        _queue.emplace_back(data[i]);
        _w_cnt++;
    }
    return size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (_error) {
        return "";
    }

    string res = "";
    const size_t size = min(len, _queue.size());
    for (size_t i = 0; i < size; i++) {
        res += _queue.at(i);
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (_error) {
        return;
    }

    const size_t size = min(len, _queue.size());
    for (size_t i = 0; i < size; i++) {
        _queue.pop_front();
        _r_cnt++;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (_error) {
        return "";
    }

    string res = "";
    size_t size = min(len, _queue.size());
    for (size_t i = 0; i < size; i++) {
        res += _queue.front();
        _queue.pop_front();
        _r_cnt++;
    }
    return res;
}

void ByteStream::end_input() { _end = true; }

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return _queue.size(); }

bool ByteStream::buffer_empty() const { return _queue.empty(); }

bool ByteStream::eof() const { return _end && _queue.empty(); }

size_t ByteStream::bytes_written() const { return _w_cnt; }

size_t ByteStream::bytes_read() const { return _r_cnt; }

size_t ByteStream::remaining_capacity() const { return _capacity - _queue.size(); }
