#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "sqlpool/sqlpool.h"
#include <iostream>

void initialize_logger() {
    auto async_file = spdlog::rotating_logger_mt<spdlog::async_factory>("file_logger", "../logs/mylog.txt", 1024 * 1024 * 5, 100);
    spdlog::set_default_logger(async_file);
    spdlog::set_level(spdlog::level::debug);
}

void initilize_sqlpool() {
    SqlPool::init("localhost", "debian-sys-maint", "MQd50c16m0khn17B", "web", 3306, 10);
}

void print();
void test_threadpool();
void testDynamicBuffer();
void test_httprequest_get();
void test_httprequest_post();
void test_sqlpool();
void test_http();

int main()
{
    initialize_logger();
    initilize_sqlpool();
    spdlog::info("initial...");
    std::cout << "start..." << std::endl;
    // print();
    // test_threadpool();
    // testDynamicBuffer();
    // spdlog::info("Now test get");
    // test_httprequest_get();
    // spdlog::info("Now test post");
    // test_httprequest_post();
    // spdlog::info("Now test mysql");
    // test_sqlpool();
    spdlog::info("Now test Http");
    test_http();
    spdlog::drop_all();
    return 0;
}