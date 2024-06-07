#include "mysql/mysql.h"
#include "spdlog/spdlog.h"
#include <string>
#include <queue>
#include <condition_variable>
#include <mutex>

class SqlPool {
public:
    static SqlPool& getInstance(const std::string& host, const std::string& user, const std::string& passwd,
            const std::string& dbname, int port, int max_conn = 10) {
        static SqlPool instance(host, user, passwd, dbname, port, max_conn);
        return instance;
    }

    std::shared_ptr<MYSQL> getConnection() {
        std::unique_lock<std::mutex> lock(m_quemtx);
        m_cond.wait(lock, [this] {
            return !this->m_connque.empty();
        });
        auto conn = m_connque.front();
        m_connque.pop();
        return std::shared_ptr<MYSQL>(conn, [this](MYSQL* conn) {
            std::unique_lock<std::mutex> lock(this->m_quemtx);
            this->m_connque.push(conn);
            this->m_cond.notify_one();
        });
    }
    

private:
    SqlPool(const std::string& host, const std::string& user, const std::string& passwd,
            const std::string& dbname, int port, int max_conn = 10) {
        for (int i = 0; i < max_conn; ++i) {
            MYSQL* conn = mysql_init(nullptr);
            if (!conn) {
                spdlog::error("The {}-th sql initialize failure!", i);
            }
            if (!mysql_real_connect(conn, host.c_str(), user.c_str(), passwd.c_str(), dbname.c_str(), port, nullptr, 0)) {
                mysql_close(conn);
                spdlog::error("The {}-th sql connect failure!", i);
            }
            m_connque.push(conn);
        }
    }

    ~SqlPool() {
        while (!m_connque.empty()) {
            mysql_close(m_connque.front());
            m_connque.pop();
        }
    }

    SqlPool(const SqlPool&) = delete;
    SqlPool& operator=(const SqlPool&) = delete;

    std::queue<MYSQL *> m_connque;
    std::mutex m_quemtx;
    std::condition_variable m_cond;
};