#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <string>
#include "buffer/buffer.h"
#include "spdlog/spdlog.h"

void testDynamicBuffer() {
    // 创建临时文件
    const char* filename = "../logs/tempfile.txt";
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    assert(fd >= 0);

    // 写入初始数据到文件
    const char* initial_data = "Hello, this is a test.";
    size_t initial_data_len = strlen(initial_data);
    ssize_t written = write(fd, initial_data, initial_data_len);
    assert(written == initial_data_len);

    // 重置文件偏移量到文件开头
    lseek(fd, 0, SEEK_SET);

    // 测试 DynamicBuffer
    DynamicBuffer buffer;
    int err;

    // 从文件描述符读取数据到缓冲区
    ssize_t read_bytes = buffer.read_fd(fd, &err);
    assert(read_bytes == initial_data_len);
    assert(buffer.readable_bytes() == initial_data_len);

    // 验证读取的数据
    ssize_t write_bytes = buffer.write_fd(1, &err);
    assert(write_bytes == read_bytes);

    // 写入新数据到文件描述符
    const char* new_data = " More data.";
    size_t new_data_len = strlen(new_data);
    buffer.append(new_data, new_data_len);
    ssize_t ret = buffer.write_fd(fd, &err);
    assert(ret == new_data_len);

    // 读取文件内容验证写入结果
    lseek(fd, 0, SEEK_SET);
    char final_data[1024] = {0};
    ssize_t total_bytes = read(fd, final_data, sizeof(final_data));
    assert(total_bytes == initial_data_len + new_data_len);
    assert(std::memcmp(final_data, "Hello, this is a test. More data.", total_bytes) == 0);

    // 关闭并删除临时文件
    close(fd);
    remove(filename);

    spdlog::info("Tests passed!");
}
