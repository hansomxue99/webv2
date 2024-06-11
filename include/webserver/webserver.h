#pragma once

#include "spdlog/spdlog.h"
#include "http/http.h"
#include "threadpool/threadpool.h"
#include "sqlpool/sqlpool.h"
#include "epoller/epoller.h"
#include "timer/timer.h"

class WebServer {
public:
    WebServer(
        int user_port, 
        const std::string& sql_user, const std::string& sql_pwd, const std::string& sql_dbName, 
        int sql_port, int sql_num,
        const std::string& web_srcDir, int thread_num) : 
        m_port(user_port),  m_close(false), m_webDir(web_srcDir), m_listen_fd(-1),
        m_timers(new HeapTimer()), m_threadpool(new ThreadPool(thread_num)), m_epoller(new Epoller()) {
        
        HttpInterface::userCount = 0;
        SqlPool::init("localhost", sql_user, sql_pwd, sql_dbName, sql_port, sql_num);
        init_trigger_mode();
        if (init_socket() == false) {
            m_close = true;
        }
    }

    ~WebServer() {
        if (m_listen_fd >= 0) {
            close(m_listen_fd);
        }
        m_close = true;
        m_users.clear();
    }

    void run() {
        int time_ms = -1;
        if (!m_close) {
            spdlog::info("WebServer is Starting...");
        }
        while (!m_close) {
            if (TIME_OUT > 0) {
                time_ms = m_timers->GetNextTick();
            }
            int event_cnt = m_epoller->wait(time_ms);
        
            if (event_cnt < 0) {
                spdlog::error("Epoll wait error! WebServer::run()");
                break;
            }

            for (int i = 0; i < event_cnt; ++i) {
                int fd = m_epoller->get_fd(i);
                uint32_t events = m_epoller->get_event(i);
                if (fd == m_listen_fd) {    // new connection
                    spdlog::info("Connection {} in. Create connection Now ...", fd);
                    handle_new_connection();
                } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {   // close connection
                    spdlog::info("Connection {} quit.", fd);
                    m_epoller->delete_fd(fd);
                    m_users.erase(fd);
                } else if (events & EPOLLIN) {
                    spdlog::info("Connection {} has read event. Reading Now ...", fd);
                    handle_read_event(fd);
                } else if (events & EPOLLOUT) {
                    spdlog::info("Connection {} has write event. Writing Now ...", fd);
                    handle_write_event(fd);
                }
            }
        }
    }

private:
    bool init_socket() {
        // create listen
        m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_listen_fd < 0) {
            spdlog::error("socket init error! WebServer::init_socket()");
            return false;
        }

        // set socket options
        int opt = 1;
        if (setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(int)) < 0) {
            spdlog::error("socketopt error! WebServer::init_socket()");
            close(m_listen_fd);
            return false;
        }

        // bind socket(ip : port)
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(m_port);
        if (bind(m_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            spdlog::error("socket bind error! WebServer::init_socket()");
            close(m_listen_fd);
            return false;
        }

        if (listen(m_listen_fd, 10) < 0) {
            spdlog::error("socket listen error! WebServer::init_socket()");
            close(m_listen_fd);
            return false;
        }

        if (m_epoller->add_fd(m_listen_fd, m_listen_event | EPOLLIN) == false) {
            spdlog::error("add listen fd to epoll error! WebServer::init_socket()");
            close(m_listen_fd);
            return false;
        }
        set_fd_nonblock(m_listen_fd);
        spdlog::info("Socket Init Success! Listen Port is {}.", m_port);
        return true;
    }

    void init_trigger_mode() {
        m_listen_event = EPOLLRDHUP | EPOLLET;
        m_conn_event = EPOLLONESHOT | EPOLLRDHUP | EPOLLET;
    }

    void handle_new_connection() {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        do {
            int conn_fd = accept(m_listen_fd, (struct sockaddr *)&addr, &len);
            if (conn_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                spdlog::error("New connnection Accept Error! WebServer::handle_new_connection()");
                return;
            }
            if (HttpInterface::userCount >= 65535) {
                spdlog::warn("New connnection Accept Busy! WebServer::handle_new_connection()");
                emergency_send(conn_fd, "WebServer is Busy Now.");
                return;
            }

            // add connection
            m_users[conn_fd].init(conn_fd, addr, m_webDir);
            set_fd_nonblock(conn_fd);
            m_epoller->add_fd(conn_fd, EPOLLIN | m_conn_event);

            // add timer
            if (TIME_OUT > 0) {
                m_timers->add(conn_fd, TIME_OUT, [this, conn_fd] () {
                    spdlog::info("Connection {} Time Out! This Connection quit.", conn_fd);
                    this->m_epoller->delete_fd(conn_fd);
                    this->m_users.erase(conn_fd);
                });
            }
        } while (true);

    }

    void handle_read_event(int fd) {
        if (TIME_OUT > 0) {
            m_timers->adjust(fd, TIME_OUT);
        }
        m_threadpool->submit([this, fd](){
            int readErrno = 0;
            int ret = -1;
            ret = this->m_users[fd].read(&readErrno);
            if (ret <= 0 && readErrno != EAGAIN) {
                spdlog::error("Http Connection read failure! WebServer::handle_read_event()");
                return;
            }
            if (this->m_users[fd].process()) {
                spdlog::info("Http Process has already finished!");
                this->m_epoller->modify_fd(fd, this->m_conn_event | EPOLLOUT);
            } else {
                this->m_epoller->modify_fd(fd, this->m_conn_event | EPOLLIN);
            }
        });
    }

    void handle_write_event(int fd) {
        if (TIME_OUT > 0) {
            m_timers->adjust(fd, TIME_OUT);
        }
        m_threadpool->submit([this, fd]() {
            int writeErrno = 0;
            int ret = -1;
            ret = this->m_users[fd].write(&writeErrno);
            if (this->m_users[fd].to_be_sent() == 0) {
                spdlog::info("Http Write has already finished!");
                if (this->m_users[fd].is_keep_alive()) {
                    this->m_epoller->modify_fd(fd, this->m_conn_event | EPOLLIN);
                    return;
                }
            } else if (ret < 0) {
                if (writeErrno == EAGAIN) {
                    this->m_epoller->modify_fd(fd, this->m_conn_event | EPOLLOUT);
                    return;
                }
            }
            this->m_epoller->delete_fd(fd);
            this->m_users.erase(fd);
        });
    }

    void emergency_send(int fd, const std::string& msg) {
        if (send(fd, msg.c_str(), msg.size(), 0) < 0) {
            spdlog::warn("emergency_send msg error! Please Check information! WebServer::emergency_send()");
        }
        close(fd);
    }

    void set_fd_nonblock(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    static const int MAX_FD = 65535;
    static const int TIME_OUT = 60000;

    int m_listen_fd;
    int m_port;
    bool m_close;
    std::string m_webDir;

    std::unique_ptr<Epoller> m_epoller;
    uint32_t m_listen_event;
    uint32_t m_conn_event;
    std::unique_ptr<HeapTimer> m_timers;
    std::unique_ptr<ThreadPool> m_threadpool;

    std::unordered_map<int, HttpInterface> m_users;
};