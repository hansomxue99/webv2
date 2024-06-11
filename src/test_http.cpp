#include "spdlog/spdlog.h"
#include "http/http.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"

void test_http() {
    // 创建临时文件
    const char* filename = "../logs/tempfile.txt";
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    assert(fd >= 0);

    // 写入初始数据到文件
    const char* initial_data = "GET / HTTP/1.1\r\nContent-Type: text/html;\r\nContent-Length: 137\r\nConnection: keep-alive\r\n\r\nhelloword";
    size_t initial_data_len = strlen(initial_data);
    ssize_t written = write(fd, initial_data, initial_data_len);

    // 重置文件偏移量到文件开头
    lseek(fd, 0, SEEK_SET);

    HttpInterface httpconn;
    struct sockaddr_in my_addr;
    my_addr.sin_family = AF_INET;     
    my_addr.sin_port = 3306;     
    my_addr.sin_addr.s_addr = inet_addr("192.168.0.1"); 
    httpconn.init(fd, my_addr, "../webfiles");

    int readErrno = 0;
    int writeErrno = 0;
    int ret = -1;
    ret = httpconn.read(&readErrno);
    if (ret < 0 && readErrno != EAGAIN) {
        spdlog::error("Http Connection read failure! test_http()");
        return;
    }

    httpconn.process();
    
    ret = httpconn.write(&writeErrno);
    if (ret < 0 && writeErrno != EAGAIN) {
        spdlog::error("Http Connection write failure! test_http()");
        return;
    }
    spdlog::debug("Http Test success!");
}