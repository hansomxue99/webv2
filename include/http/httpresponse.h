#pragma once

#include "spdlog/spdlog.h"
#include "buffer/buffer.h"
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unordered_map>
#include <functional>

class HttpResponse {
public:
    HttpResponse() : m_status_message(""), m_status_code(-1), is_alive(false) {}
    ~HttpResponse() {
        unmap_file();
    }

    void init(const std::string& srcDir, const std::string& path, bool isalive, int code) {
        unmap_file();
        m_status_code = code;
        mm_filestat = { 0 };
        m_srcDir = srcDir;
        m_path = path;
        is_alive = isalive;
        m_file_memory = nullptr;
    }

    void response(DynamicBuffer& buff) {
        check_status();

        // add status line
        append_status_line(buff);

        // add header lines
        append_header_line(buff);

        // add body line
        append_body_line(buff);
    }

    // interactions
    int get_file_len() const {
        return mm_filestat.st_size;
    }

    char* get_file() const {
        return m_file_memory;
    }

    void unmap_file() {
        if (m_file_memory) {
            munmap(m_file_memory, mm_filestat.st_size);
            m_file_memory = nullptr;
        }
    }
private:
    std::string get_status_message(int code) {
        switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        default:  return "Unknown";
        }
    }

    void check_status() {
        // bad request
        if (m_status_code == 400) {
            m_status_message = get_status_message(400); // bad request
            return;
        }

        std::string full_path = m_srcDir + m_path;
        spdlog::debug("Checking status, current full path is {}", full_path);
        if (stat((full_path).c_str(), &mm_filestat) < 0 || S_ISDIR(mm_filestat.st_mode)) {
            m_status_code = 404;
            m_status_message = get_status_message(404); // not found
            return;
        }
        if (!(mm_filestat.st_mode & S_IROTH)) {
            m_status_code = 403;
            m_status_message = get_status_message(403); // forbidden
            return;
        }
        m_status_code = 200;
        m_status_message = get_status_message(200); // ok
    }

    void append_status_line(DynamicBuffer& buff) {
        std::string s = "HTTP/1.1 " + std::to_string(m_status_code) + " " + m_status_message + "\r\n";
        spdlog::debug("Status line to be appended is\r\n{}", s);
        buff.append(s.data(), s.length());
    }

    void append_header_line(DynamicBuffer& buff) {
        std::string s;
        s = "Connection: ";
        if (is_alive) {
            s += "keep-alive\r\n";
            s += "keep-alive: max=6, timeout=120\r\n";
        } else {
            s += "close\r\n";
        }
        auto get_filetype = [this]() {
            std::string default_ret = "text/plain";
            std::string::size_type pos = this->m_path.find_last_of('.');
            if (pos == std::string::npos) {
                return default_ret;
            }

            std::string suffix = this->m_path.substr(pos);
            if (this->SUFFIX_TYPE.count(suffix)) {
                return this->SUFFIX_TYPE.find(suffix)->second;
            }
            return default_ret;
        };

        s += "Content-type: " + get_filetype() + "\r\n";
        spdlog::debug("Header lines to be appended are\r\n{}", s);
        buff.append(s.data(), s.length());
    }

    void send_error_content(DynamicBuffer& buff, const std::string& message) {
        std::string body;
        std::string prefix;
        body += "<html><title>Error</title>";
        body += "<body bgcolor=\"ffffff\">";
        body += std::to_string(m_status_code) + " : " + m_status_message  + "\n";
        body += "<p>" + message + "</p>";
        body += "<hr><em>TinyWebServer</em></body></html>";

        prefix = "Content-length: " + std::to_string(body.size()) + "\r\n\r\n";
        buff.append(prefix.data(), prefix.length());
        buff.append(body.data(), body.length());
    }

    void append_body_line(DynamicBuffer& buff) {
        std::string full_path = m_srcDir + m_path;
        int fd = open(full_path.data(), O_RDONLY);
        if (fd < 0) {
            spdlog::error("Error: add body line error! The file to be open is {}, Not found!", full_path);
            send_error_content(buff, "File Not Found");
            return;
        }
        spdlog::debug("Adding Body Line, current file path is {}", full_path);
        int *mmret = (int *)mmap(0, mm_filestat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (*mmret == -1) {
            spdlog::error("Error: add body line error! mmap Error, Please check!");
            send_error_content(buff, "File Not Found");
            close(fd);
            return;
        }
        m_file_memory = (char *)mmret;
        close(fd);
        std::string body;
        body += "Content-length: " + std::to_string(mm_filestat.st_size) + "\r\n\r\n";
        spdlog::debug("Body lines to be appended are\r\n{}Attention: file contents is mapped to memory.", body);
        buff.append(body.data(), body.length());
    }

    
    int m_status_code;
    std::string m_status_message;
    bool is_alive;

    std::string m_path;
    std::string m_srcDir;
    char *m_file_memory;
    struct stat mm_filestat;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
};

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};