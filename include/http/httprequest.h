#pragma once

#include "buffer/buffer.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <string>
#include <regex>
#include <unordered_set>
#include <unordered_map>
#include <cassert>
#include "sqlpool/sqlpool.h"
#include <iostream>

class HttpRequest {
public:
    enum STATE { REQUEST_LINE = 0, HEADERS, BODY, FINISH };

    HttpRequest() : m_method(""), m_path(""), m_version(""), m_body(""), m_state(REQUEST_LINE) {
        m_headers.clear();
        m_post.clear();
    }
    ~HttpRequest() = default;

    bool parse_buffer(DynamicBuffer& buffer) {
        const char CRLF[] = "\r\n";
        if (buffer.readable_bytes() <= 0) {
            return false;
        }

        while (buffer.readable_bytes() && m_state != FINISH) {
            const char *line_end = std::search(buffer.begin_read(), buffer.begin_write(), CRLF, CRLF + 2);
            const char *line_start = buffer.begin_read();
            std::string aline(line_start, line_end);
            spdlog::debug(aline);
            switch(m_state) {
            case REQUEST_LINE:
                if (parse_requestline(aline) == false) {
                    return false;
                }
                break;

            case HEADERS:
                parse_headers(aline);
                if (buffer.readable_bytes() <= 2) {
                    m_state = FINISH;
                }
                break;
            
            case BODY:
                parse_body(aline);
                break;
            default:
                spdlog::error("The request content is undefined! see HttpRequest::parse_buffer()");
                break;
            }
            if (line_end == buffer.begin_write()) {
                break;
            }
            buffer.retrieveUtil(line_end + 2);
        }
        spdlog::debug("request success parsed: {}, {}, {}", m_method, m_path, m_version);
        return true;
    }

    // path interface for response
    const std::string& get_response_path() const {
        return m_path;
    }

    // keep alive interface for response
    bool is_response_alive() const {
        if (m_headers.count("Connection")) {
            return m_headers.find("Connection")->second == "keep-alive" && m_version == "1.1";
        }
        return false;
    }
private:
    void parse_httppath() {
        if (m_path == "/") {
            m_path = "/index.html";
        } else {
            // m_path += ".html";
            if (DEFAULT_HTML.find(m_path) != DEFAULT_HTML.end()) {
                m_path += ".html";
            }
        }
    }

    bool parse_requestline(const std::string& line) {
        // regex judge
        std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
        std::smatch sub_match;
        if (std::regex_match(line, sub_match, pattern)) {
            m_method = sub_match[1];
            m_path = sub_match[2];
            m_version = sub_match[3];
            m_state = HEADERS;
            parse_httppath();
            return true;
        }
        spdlog::error("Request Line Error! See HttpRequest::parse_requestline()");
        return false;
    }

    void parse_headers(const std::string& line) {
        std::regex pattern("^([^:]*): ?(.*)$");
        std::smatch sub_match;
        if (std::regex_match(line, sub_match, pattern)) {
            m_headers[sub_match[1]] = sub_match[2];
        } else {
            m_state = BODY;
        }
    }

    void parse_body(const std::string& line) {
        m_body = line;
        parse_post();
        m_state = FINISH;
        spdlog::debug("get or post content: {}", m_body);
    }

    void parse_post() {
        if (m_method == "POST" && m_headers["Content-Type"] == "application/x-www-form-urlencoded") {
            parse_formurl();
            if (DEFAULT_TAG.count(m_path)) {
                int tag = DEFAULT_TAG.find(m_path)->second;
                spdlog::info("This post TAG: {}", tag);
                if (tag == 0 || tag == 1) {
                    bool is_login = (tag == 1);
                    if (rlverify(m_post["username"], m_post["password"], true)) {
                        m_path = "/welcome.html";
                    } else {
                        m_path = "/error.html";
                    }
                }
            }
        }
    }

    bool rlverify(const std::string& name, const std::string& pwd, bool islogin) {
        if (name == "" || pwd == "") {
            return false;
        }

        spdlog::debug("Register or Login Verify. Name : {}, PWD: {}", name, pwd);

        char sql_commad[256] = {0};

        // mysql interaction
        SqlPool& sqlpool = SqlPool::getInstance();
        auto conn = sqlpool.getConnection();
        snprintf(sql_commad, 256, "SELECT * FROM user WHERE username='%s' LIMIT 1", name.c_str());
        int ret = mysql_query(conn.get(), sql_commad);
        
        
        if (ret) {
            if (islogin) {
                spdlog::error("Login Verify Failure! Unknown Name {} !", name.c_str());
                return false;
            }
            // register
            spdlog::debug("Now Register...");
            bzero(sql_commad, 256);
            snprintf(sql_commad, 256, "SELECT into user(username, passwd) VALUES('%s', '%s')", name.c_str(), pwd.c_str());
            if (mysql_query(conn.get(), sql_commad)) {
                spdlog::error("Register Failure! Insertion Failure!");
                return false;
            }
            spdlog::debug("Register Verifying Success!");
            return true;
        }

        if (!islogin) {
            spdlog::error("Register Verify Failure! Repeated Name {} !", name.c_str());
            return false;
        }

        // login
        spdlog::debug("Now Login...");
        MYSQL_RES* res = mysql_store_result(conn.get());
        while (MYSQL_ROW row = mysql_fetch_row(res)) {
            spdlog::info("==> mysql user-db {} : {}", row[0], row[1]);
            std::string passwd(row[1]);
            if (pwd != passwd) {
                spdlog::error("Login Verify Failure! Error Password !");
                return false;
            }
            spdlog::debug("Login Verifying Success!");
        }
        return true;
    }

    void parse_formurl() {
        if (m_body.size() == 0) {
            return;
        }
        std::string key, value;
        int left = 0, right = 0, num = 0;

        for (; right < m_body.size(); ++right) {
            char c = m_body[right];
            switch (c) {
            case '=':
                key = m_body.substr(left, right - left);
                left = right + 1;
                break;
            case '+':
                m_body[right] = ' ';
                break;
            case '&':
                // first k-v
                value = m_body.substr(left, right - left);
                left = right + 1;
                m_post[key] = value;
                spdlog::debug("The first key-value: {} = {}", key, value);
                break;
            default:
                break;
            }
        }
        assert(right >= left);
        if (m_post.count(key) == 0) {
            value = m_body.substr(left, right - left);
            m_post[key] = value;
            spdlog::debug("The second key-value: {} = {}", key, value);
        }
    }

    STATE m_state;
    std::string m_method;
    std::string m_path;
    std::string m_version;
    std::string m_body;

    std::unordered_map<std::string, std::string> m_headers;
    std::unordered_map<std::string, std::string> m_post;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_TAG;
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_TAG {
    { "/register.html", 0 }, { "/login.html", 1 }  
};

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/index", "/register", "/login",
    "/welcome", "/video", "/picture"
};