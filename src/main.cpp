#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "webserver/webserver.h"

void initialize_logger() {
    auto async_file = spdlog::rotating_logger_mt<spdlog::async_factory>("file_logger", "../logs/mylog.txt", 1024 * 1024 * 5, 100);
    spdlog::set_default_logger(async_file);
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
}

int main()
{
    initialize_logger();
    spdlog::info("initial...");
    spdlog::info("Now test Webserver");
    WebServer server(
        9006,
        "debian-sys-maint", "MQd50c16m0khn17B", "web", 3306, 12,
        "/home/xwk/Desktop/webv2/webfiles", 6
    );
    server.run();
    spdlog::drop_all();
    return 0;
}