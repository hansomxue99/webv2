#include "io_context.hpp"
#include "http_server.hpp"
#include "file_utils.hpp"
#include <unistd.h>

void server() {
    io_context ctx;
    chdir("../static");
    auto server = http_server::make();
    server->get_router().route("/", [](http_server::http_request &request) {
        std::string response = file_get_content("picture.html");
        request.write_response(200, response, "text/html");
    });

    server->get_router().route("/1", [](http_server::http_request &request) {
        std::string response = file_get_content("showpic.html");
        request.write_response(200, response, "text/html");
    });

    server->get_router().route("/x.png", [](http_server::http_request &request) {
        std::string response = file_get_content("test.png");
        request.write_response(200, response, "image/png");
    });
    // fmt::println("正在监听：http://127.0.0.1:8080");
    server->do_start("172.22.155.121", "3389");

    ctx.join();
}

int main() {
    // try {
        server();
    // } catch (std::system_error const &e) {
    //     fmt::println("{} ({}/{})", e.what(), e.code().category().name(),
    //                  e.code().value());
    // }
    return 0;
}