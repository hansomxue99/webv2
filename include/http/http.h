#pragma once

#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "buffer/buffer.h"
#include "spdlog/spdlog.h"
#include <arpa/inet.h>

class HttpInterface {
public:
    HttpInterface() : m_fd(-1), m_addr({0}),  m_isclose(true), m_iov_cnt(0) {};

    void init(int fd, const sockaddr_in& addr, const std::string& srcDir) {
        ++userCount;
        m_addr = addr;
        m_fd = fd;
        m_srcDir = srcDir;
        m_writeBuffer.retrieveAll();
        m_readBuffer.retrieveAll();
        m_isclose = false;
        spdlog::info("Client [{}]({} : {}) connected, now user count is {}", m_fd, get_ip(), get_port(), (int)userCount);
    }

    ~HttpInterface() {
        m_httpResponse.unmap_file();
        if (m_isclose == false) {
            m_isclose = true;
            --userCount;
            close(m_fd);
            spdlog::info("Client [{}]({} : {}) quit, now user count is {}", m_fd, get_ip(), get_port(), (int)userCount);
        }
    }
    ssize_t read(int *saveErrno) {
        if (m_fd < 0) {
            spdlog::error("Invalid fd! HttpInterface::read()");
            *saveErrno = EBADF;
            return -1;
        }
        ssize_t len = -1;
        do {
            len = m_readBuffer.read_fd(m_fd, saveErrno);
            if (len <= 0) {
                break;
            }
        } while (true); // ET Trigger
        spdlog::debug("has read bytes: {}, len = {}", m_readBuffer.readable_bytes(), len);
        return len;
    }

    bool process() {
        if (m_readBuffer.readable_bytes() <= 0) {
            spdlog::error("No data is read by HTTP! Http::process() Error!");
            return false;
        }

        if (m_httpRequest.parse_buffer(m_readBuffer)) {
            spdlog::debug("Http Request Parse Sucess! HttpInterface::process()");
            m_httpResponse.init(m_srcDir, m_httpRequest.get_response_path(), m_httpRequest.is_response_alive(), 200);
        } else {
            spdlog::debug("Http Request Parse Bad! HttpInterface::process()");
            m_httpResponse.init(m_srcDir, m_httpRequest.get_response_path(), false, 400);
        }

        m_httpResponse.response(m_writeBuffer);
        m_iov[0].iov_base = m_writeBuffer.begin_read();
        m_iov[0].iov_len = m_writeBuffer.readable_bytes();
        m_iov_cnt = 1;

        if (m_httpResponse.get_file_len() > 0 && m_httpResponse.get_file()) {
            m_iov[1].iov_base = m_httpResponse.get_file();
            m_iov[1].iov_len = m_httpResponse.get_file_len();
            m_iov_cnt = 2;
        }
        spdlog::debug("filesize: {}, {} to {}", m_httpResponse.get_file_len(), m_iov_cnt, to_be_sent());
        return true;
    }

    ssize_t write(int* saveErrno) {
        if (m_fd < 0) {
            spdlog::error("Invalid fd! HttpInterface::write()");
            *saveErrno = EBADF;
            return -1;
        }
        ssize_t len = -1;
        do {
            len = writev(m_fd, m_iov, m_iov_cnt);
            if (len <= 0) {
                *saveErrno = errno;
                break;
            }
            if (to_be_sent() == 0) {    // send finshed
                break;
            } else if (static_cast<size_t>(len) > m_iov[0].iov_len) {
                m_iov[1].iov_base = (uint8_t *) m_iov[1].iov_base + (len - m_iov[0].iov_len);
                m_iov[1].iov_len -= (len - m_iov[0].iov_len);
                if (m_iov[0].iov_len) {
                    m_writeBuffer.retrieveAll();
                    m_iov[0].iov_len = 0;
                }
            } else {
                m_iov[0].iov_base = (uint8_t *)m_iov[0].iov_base + len;
                m_iov[0].iov_len -= len;
                m_writeBuffer.retrieve(len);
            }
        } while (true); // ET Trigger
        return len;
    }
    
    int to_be_sent() const {
        return m_iov[0].iov_len + m_iov[1].iov_len;
    }

    bool is_keep_alive() const {
        return m_httpRequest.is_response_alive();
    }

    static std::atomic<int> userCount;
private:
    const char* get_ip() const {
        return inet_ntoa(m_addr.sin_addr);
    }

    int get_port() const {
        return ntohs(m_addr.sin_port);
    }

    HttpResponse m_httpResponse;
    HttpRequest m_httpRequest;

    DynamicBuffer m_readBuffer;
    DynamicBuffer m_writeBuffer;

    struct iovec m_iov[2];
    int m_iov_cnt;
    bool m_isclose;

    // interface
    int m_fd;
    std::string m_srcDir;
    struct sockaddr_in m_addr;
};

std::atomic<int> HttpInterface::userCount;