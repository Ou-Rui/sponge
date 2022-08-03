#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <iostream>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    // key: index; val: byte
    std::unordered_map<size_t, char> _map;
    size_t _map_size;
    //! \note _first_unassembled/_first_unaccepted must be the index of "non-received" Byte
    size_t _first_unassembled;
    size_t _first_unaccepted;
    bool _is_end;

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

    //! \brief Get index-in-str of the first (possibly) useful byte
    //! \note return_index is the index inside the input segment, not the entire ByteStream
    size_t getFirstIndex(const std::string &data, const uint64_t index) const;

    //! \brief Load input string into map
    //! \param str_index: index-in-str of the first (possibly) useful byte
    //! \return new_first_unassembled index
    void loadString(const std::string &data, const uint64_t index, const size_t str_index);

    //! \brief push the assembled bytes into the _output ByteStream
    void reassemble();

    bool full() const;
    //! \brief evict the last byte, and update _first_unaccepted
    void evict();

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
