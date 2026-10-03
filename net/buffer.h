#pragma once

#include "net/endian.h"

#include <assert.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace xvorin::evix::net {

class Buffer {
public:
    static constexpr size_t kCheapPrepend = 8;

    static constexpr size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , reader_index_(kCheapPrepend)
        , writer_index_(kCheapPrepend)
    {
        assert(readable_bytes() == 0);
        assert(writable_bytes() == initialSize);
        assert(prependable_bytes() == kCheapPrepend);
    }

    // implicit copy-ctor, move-ctor, dtor and assignment are fine
    // NOTE: implicit move-ctor is added in g++ 4.6

    void swap(Buffer& rhs)
    {
        buffer_.swap(rhs.buffer_);
        std::swap(reader_index_, rhs.reader_index_);
        std::swap(writer_index_, rhs.writer_index_);
    }

    size_t readable_bytes() const
    {
        return writer_index_ - reader_index_;
    }

    size_t writable_bytes() const
    {
        return buffer_.size() - writer_index_;
    }

    size_t prependable_bytes() const
    {
        return reader_index_;
    }

    const char* peek() const
    {
        return begin() + reader_index_;
    }

    const char* find_crlf() const
    {
        // FIXME: replace with memmem()?
        const char* crlf = std::search(peek(), begin_write(), kCRLF, kCRLF + 2);
        return crlf == begin_write() ? NULL : crlf;
    }

    const char* find_crlf(const char* start) const
    {
        assert(peek() <= start);
        assert(start <= begin_write());
        // FIXME: replace with memmem()?
        const char* crlf = std::search(start, begin_write(), kCRLF, kCRLF + 2);
        return crlf == begin_write() ? NULL : crlf;
    }

    const char* find_eol() const
    {
        const void* eol = memchr(peek(), '\n', readable_bytes());
        return static_cast<const char*>(eol);
    }

    const char* find_eol(const char* start) const
    {
        assert(peek() <= start);
        assert(start <= begin_write());
        const void* eol = memchr(start, '\n', begin_write() - start);
        return static_cast<const char*>(eol);
    }

    // retrieve returns void, to prevent
    // string str(retrieve(readable_bytes()), readable_bytes());
    // the evaluation of two functions are unspecified

    void retrieve(size_t len)
    {
        assert(len <= readable_bytes());
        if (len < readable_bytes()) {
            reader_index_ += len;
        } else {
            retrieve_all();
        }
    }

    void retrieve_until(const char* end)
    {
        assert(peek() <= end);
        assert(end <= begin_write());
        retrieve(end - peek());
    }

    void retrieve_int64()
    {
        retrieve(sizeof(int64_t));
    }

    void retrieve_int32()
    {
        retrieve(sizeof(int32_t));
    }

    void retrieve_int16()
    {
        retrieve(sizeof(int16_t));
    }

    void retrieve_int8()
    {
        retrieve(sizeof(int8_t));
    }

    void retrieve_all()
    {
        reader_index_ = kCheapPrepend;
        writer_index_ = kCheapPrepend;
    }

    std::string retrieve_all_as_string()
    {
        return retrieve_as_string(readable_bytes());
    }

    std::string retrieve_as_string(size_t len)
    {
        assert(len <= readable_bytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    std::string_view to_string_piece() const
    {
        return std::string_view(peek(), readable_bytes());
    }

    void append(const std::string_view& str)
    {
        append(str.data(), str.size());
    }

    void append(const char* /*restrict*/ data, size_t len)
    {
        ensure_writable_bytes(len);
        std::copy(data, data + len, begin_write());
        has_written(len);
    }

    void append(const void* /*restrict*/ data, size_t len)
    {
        append(static_cast<const char*>(data), len);
    }

    void ensure_writable_bytes(size_t len)
    {
        if (writable_bytes() < len) {
            make_space(len);
        }
        assert(writable_bytes() >= len);
    }

    char* begin_write()
    {
        return begin() + writer_index_;
    }

    const char* begin_write() const
    {
        return begin() + writer_index_;
    }

    void has_written(size_t len)
    {
        assert(len <= writable_bytes());
        writer_index_ += len;
    }

    void unwrite(size_t len)
    {
        assert(len <= readable_bytes());
        writer_index_ -= len;
    }

    ///
    /// Append int64_t using network endian
    ///
    void append_int64(int64_t x)
    {
        int64_t be64 = sockets::hostToNetwork64(x);
        append(&be64, sizeof be64);
    }

    ///
    /// Append int32_t using network endian
    ///
    void append_int32(int32_t x)
    {
        int32_t be32 = sockets::hostToNetwork32(x);
        append(&be32, sizeof be32);
    }

    void append_int16(int16_t x)
    {
        int16_t be16 = sockets::hostToNetwork16(x);
        append(&be16, sizeof be16);
    }

    void append_int8(int8_t x)
    {
        append(&x, sizeof x);
    }

    ///
    /// Read int64_t from network endian
    ///
    /// Require: buf->readable_bytes() >= sizeof(int32_t)
    int64_t read_int64()
    {
        int64_t result = peek_int64();
        retrieve_int64();
        return result;
    }

    ///
    /// Read int32_t from network endian
    ///
    /// Require: buf->readable_bytes() >= sizeof(int32_t)
    int32_t read_int32()
    {
        int32_t result = peek_int32();
        retrieve_int32();
        return result;
    }

    int16_t read_int16()
    {
        int16_t result = peek_int16();
        retrieve_int16();
        return result;
    }

    int8_t read_int8()
    {
        int8_t result = peek_int8();
        retrieve_int8();
        return result;
    }

    ///
    /// Peek int64_t from network endian
    ///
    /// Require: buf->readable_bytes() >= sizeof(int64_t)
    int64_t peek_int64() const
    {
        assert(readable_bytes() >= sizeof(int64_t));
        int64_t be64 = 0;
        ::memcpy(&be64, peek(), sizeof be64);
        return sockets::networkToHost64(be64);
    }

    ///
    /// Peek int32_t from network endian
    ///
    /// Require: buf->readable_bytes() >= sizeof(int32_t)
    int32_t peek_int32() const
    {
        assert(readable_bytes() >= sizeof(int32_t));
        int32_t be32 = 0;
        ::memcpy(&be32, peek(), sizeof be32);
        return sockets::networkToHost32(be32);
    }

    int16_t peek_int16() const
    {
        assert(readable_bytes() >= sizeof(int16_t));
        int16_t be16 = 0;
        ::memcpy(&be16, peek(), sizeof be16);
        return sockets::networkToHost16(be16);
    }

    int8_t peek_int8() const
    {
        assert(readable_bytes() >= sizeof(int8_t));
        int8_t x = *peek();
        return x;
    }

    ///
    /// Prepend int64_t using network endian
    ///
    void prepend_int64(int64_t x)
    {
        int64_t be64 = sockets::hostToNetwork64(x);
        prepend(&be64, sizeof be64);
    }

    ///
    /// Prepend int32_t using network endian
    ///
    void prepend_int32(int32_t x)
    {
        int32_t be32 = sockets::hostToNetwork32(x);
        prepend(&be32, sizeof be32);
    }

    void prepend_int16(int16_t x)
    {
        int16_t be16 = sockets::hostToNetwork16(x);
        prepend(&be16, sizeof be16);
    }

    void prepend_int8(int8_t x)
    {
        prepend(&x, sizeof x);
    }

    void prepend(const void* /*restrict*/ data, size_t len)
    {
        assert(len <= prependable_bytes());
        reader_index_ -= len;
        const char* d = static_cast<const char*>(data);
        std::copy(d, d + len, begin() + reader_index_);
    }

    void shrink(size_t reserve)
    {
        // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
        Buffer other;
        other.ensure_writable_bytes(readable_bytes() + reserve);
        other.append(to_string_piece());
        swap(other);
    }

    size_t internal_capacity() const
    {
        return buffer_.capacity();
    }

    /// Read data directly into buffer.
    ///
    /// It may implement with readv(2)
    /// @return result of read(2), @c errno is saved
    ssize_t readFd(int fd, int* savedErrno);

private:
    char* begin()
    {
        return &*buffer_.begin();
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void make_space(size_t len)
    {
        if (writable_bytes() + prependable_bytes() < len + kCheapPrepend) {
            // FIXME: move readable data
            buffer_.resize(writer_index_ + len);
        } else {
            // move readable data to the front, make space inside buffer
            assert(kCheapPrepend < reader_index_);
            size_t readable = readable_bytes();
            std::copy(begin() + reader_index_, begin() + writer_index_, begin() + kCheapPrepend);
            reader_index_ = kCheapPrepend;
            writer_index_ = reader_index_ + readable;
            assert(readable == readable_bytes());
        }
    }

private:
    std::vector<char> buffer_;
    size_t reader_index_;
    size_t writer_index_;
    static const char kCRLF[];
};

} // namespace xvorin::evix::net
