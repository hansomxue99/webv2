#pragma once

#include <vector>
#include <sys/epoll.h>
#include <unistd.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 512) : m_epoll_fd(epoll_create(512)), m_events(maxEvent){}
    ~Epoller() {
        close(m_epoll_fd);
    }

    bool add_fd(int fd, uint32_t events) {
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = events;
        return 0 == epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    }

    bool modify_fd(int fd, uint32_t events) {
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = events;
        return 0 == epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
    }

    bool delete_fd(int fd) {
        return 0 == epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, 0);
    }

    int wait(int timeOutMs) {
        return epoll_wait(m_epoll_fd, m_events.data(), m_events.size(), timeOutMs);
    }

    int get_fd(size_t i) const {
        return m_events[i].data.fd;
    }

    int get_event(size_t i) const {
        return m_events[i].events;
    }

private:
    int m_epoll_fd;
    std::vector<struct epoll_event> m_events;
};