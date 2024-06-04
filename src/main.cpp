#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"

void initialize_logger() {
    auto async_file = spdlog::rotating_logger_mt<spdlog::async_factory>("file_logger", "../logs/mylog.txt", 1024 * 1024 * 5, 100);
    spdlog::set_default_logger(async_file);
    spdlog::set_level(spdlog::level::info);
}

void print();
void test_threadpool();

int main()
{
    initialize_logger();
    spdlog::info("initial...");
    print();
    test_threadpool();
    spdlog::drop_all();
    return 0;
}