#include <atomic>
#include <vector>
#include <unistd.h>
#include <sys/uio.h>
#include <errno.h>
#include <algorithm>

class DynamicBuffer {
public:
    DynamicBuffer(size_t initial_size = 1024) 
        : m_buffsize(initial_size), m_readpos(0), m_writepos(0) {
        m_buffer.resize(initial_size);
    }
    ~DynamicBuffer() = default;

    size_t writeable_bytes() const {
        return m_buffer.size() - m_writepos;
    }

    size_t readable_bytes() const {
        return m_writepos - m_readpos;
    }

    size_t prependable_bytes() const {
        return m_readpos;
    }

    char* begin_read() {
        return begin() + m_readpos;
    }

    char* begin_write() {
        return begin() + m_writepos;
    }

    // read bytes
    void retrieveAll() {
        m_readpos = 0;
        m_writepos = 0;
    }

    void retrieve(size_t len) {
        if (len <= readable_bytes()) {
            m_readpos += len;
        } else {
            retrieveAll();
        }
    }

    // write bytes
    void ensureWriteable(size_t len) {
        if (writeable_bytes() < len) {
            extendBuffer(len);
        }
    }

    void hasWritten(size_t len) {
        m_writepos += len;
    }

    void append(const char* data, size_t len) {
        ensureWriteable(len);
        std::copy(data, data + len, begin_write());
        hasWritten(len);
    }

    ssize_t read_fd(int fd, int* Errno) {
        char buf[65535];
        struct iovec iov[2];
        size_t writeable = writeable_bytes();
        iov[0].iov_base = begin_write();
        iov[0].iov_len = writeable;
        iov[1].iov_base = buf;
        iov[1].iov_len = sizeof(buf);

        ssize_t len = readv(fd, iov, 2);
        if (len < 0) {
            *Errno = errno;
        } else if (static_cast<size_t>(len) <= writeable) {
            m_writepos += len;
        } else {
            m_writepos = m_buffer.size();
            append(buf, static_cast<size_t>(len - writeable));
        }
        return len;
    }

    ssize_t write_fd(int fd, int *Errno) {
        ssize_t len = write(fd, begin_read(), readable_bytes());
        if (len < 0) {
            *Errno = errno;
            return len;
        }
        retrieve(len);
        return len;
    }

private:
    char* begin() {
        return m_buffer.data();
    }

    void extendBuffer(size_t len) {
        if (writeable_bytes() + prependable_bytes() < len) {
            m_buffer.resize(m_writepos + len);
        } else {
            size_t readable = readable_bytes();
            std::copy(begin_read(), begin_write(), begin());
            m_readpos = 0;
            m_writepos = readable;
        }
    }

    size_t m_buffsize;
    std::vector<char> m_buffer;
    std::atomic<std::size_t> m_readpos;
    std::atomic<std::size_t> m_writepos;
};