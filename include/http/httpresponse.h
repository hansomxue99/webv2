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
    HttpResponse() : m_status_message("") {}
    ~HttpResponse() {
        if (m_file_memory) {
            munmap(m_file_memory, m_sendfile_len);
            m_file_memory = nullptr;
        }
    }

    void init(const std::string& srcDir, const std::string& path, bool isalive, int code) {
        if (m_file_memory) {
            munmap(m_file_memory, m_sendfile_len);
            m_file_memory = nullptr;
        }
        m_status_code = -1;
        m_sendfile_len = 0;
        m_srcDir = srcDir;
        m_path = path;
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

        struct stat file_stat;
        std::string full_path = m_srcDir + m_path;
        spdlog::debug("Checking status, current full path is {}.", full_path);
        if (stat((full_path).c_str(), &file_stat) != 0) {
            m_status_message = get_status_message(404); // not found
            return;
        }
        if (!(file_stat.st_mode & S_IROTH)) {
            m_status_message = get_status_message(403); // forbidden
            return;
        }
        m_status_message = get_status_message(200); // ok
    }

    void append_status_line(DynamicBuffer& buff) {
        std::string s = "HTTP/1.1 " + std::to_string(m_status_code) + " " + m_status_message + "\r\n";
        spdlog::debug("Status line to be appended is {}.", s);
        buff.append(s.c_str(), s.size());
    }

    void append_header_line(DynamicBuffer& buff) {
        std::string s;
        s = "Connnection: ";
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

        s += "Connect-type: " + get_filetype() + "\r\n";
        spdlog::debug("Header lines to be appended are {}.", s);
        buff.append(s.c_str(), s.size());
    }

    void append_body_line(DynamicBuffer& buff) {
        std::string full_path = m_srcDir + m_path;
        int fd = open(full_path.data(), O_RDONLY);
        if (fd < 0) {
            spdlog::error("Error: add body line error! The file to be open is {}, Not found!", full_path);
            return;
        }
        spdlog::debug("Adding Body Line, current file path is {}.", full_path);
        struct stat file_stat;
        stat(full_path.data(), &file_stat);
        int *mmret = (int *)mmap(0, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (*mmret == -1) {
            spdlog::error("Error: add body line error! mmap Error, Please check!");
            return;
        }
        m_file_memory = (char *)mmret;
        close(fd);
        std::string body;
        m_sendfile_len = file_stat.st_size;
        body += "Content-length: " + std::to_string(file_stat.st_size) + "\r\n\r\n";
        buff.append(body.c_str(), body.size());
    }

    
    int m_status_code;
    std::string m_status_message;
    bool is_alive;

    std::string m_path;
    std::string m_srcDir;
    char *m_file_memory;
    int m_sendfile_len;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
};

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE {
    {".html", "text/html"}, {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"}, {".text", "text/plain"},
    {".png", "image/png"}, {".gif", "image/gif"},
    {".jpg", "image/jpg"}, {"jpeg", "image/jpeg"},
    {".avi", "video/x-msvideo"}
};