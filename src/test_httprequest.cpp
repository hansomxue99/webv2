#include "http/httprequest.h"
#include "buffer/buffer.h"

void test_httprequest_get() {
    // 创建临时文件
    const char* filename = "../logs/tempfile.txt";
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    assert(fd >= 0);

    // 写入初始数据到文件
    const char* initial_data = "GET / HTTP/1.1\r\nDate: Wed, 06 Jun 2024 12:00:00 GMT\r\nContent-Type: text/html;\r\nContent-Length: 137\r\nConnection: close\r\n\r\nhelloword";
    size_t initial_data_len = strlen(initial_data);
    ssize_t written = write(fd, initial_data, initial_data_len);

    // 重置文件偏移量到文件开头
    lseek(fd, 0, SEEK_SET);

    // 测试 DynamicBuffer
    DynamicBuffer buffer;
    int err;

    // 从文件描述符读取数据到缓冲区
    ssize_t read_bytes = buffer.read_fd(fd, &err);

    // 验证读取的数据
    HttpRequest hr;
    hr.parse_buffer(buffer);
}

void test_httprequest_post() {
    // 创建临时文件
    const char* filename = "../logs/tempfile.txt";
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    assert(fd >= 0);

    // 写入初始数据到文件
    const char* initial_data = "POST /login HTTP/1.1\r\nHost:baidu.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 137\r\nConnection: close\r\n\r\nusername=name&password=passwd";
    size_t initial_data_len = strlen(initial_data);
    ssize_t written = write(fd, initial_data, initial_data_len);

    // 重置文件偏移量到文件开头
    lseek(fd, 0, SEEK_SET);

    // 测试 DynamicBuffer
    DynamicBuffer buffer;
    int err;

    // 从文件描述符读取数据到缓冲区
    ssize_t read_bytes = buffer.read_fd(fd, &err);

    // 验证读取的数据
    HttpRequest hr;
    hr.parse_buffer(buffer);
}