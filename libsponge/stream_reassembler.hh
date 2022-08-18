#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <set>
#include <iostream>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    struct block_t {
      uint64_t index{0};
      uint64_t len{0};
      std::string data = "";
      bool operator<(const block_t& t) const { return index < t.index; }
    };

    std::set<block_t> _set{};
    uint64_t _unassembled_bytes{0};
    //! \note _first_unassembled/_first_unaccepted must be the index of "non-received" Byte
    uint64_t _first_unread{0};
    uint64_t _first_unassembled{0};
    uint64_t _first_unaccepted{0};
    bool _is_end{false};

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

    block_t get_block(const std::string &data, const size_t index) const;
    //! \brief merge b2 into b1
    bool merge_block(block_t& b1, const block_t& b2);

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

    uint64_t get_first_unassembled() const { return _first_unassembled; }
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
