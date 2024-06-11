#include "sqlpool/sqlpool.h"
#include "spdlog/spdlog.h"

void test_sqlpool() {
    SqlPool::init("localhost", "debian-sys-maint", "MQd50c16m0khn17B", "web", 3306, 10);
    SqlPool& sqlpool = SqlPool::getInstance();
    auto conn = sqlpool.getConnection();
    // Use the connection
    if (mysql_query(conn.get(), "SELECT * FROM user")) {
        spdlog::error("SELECT error: {}!", mysql_error(conn.get()));
    }

    // Process the result
    MYSQL_RES* res = mysql_store_result(conn.get());
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            spdlog::info("==> {} : {}", row[0], row[1]);
        }
        mysql_free_result(res);
    }
}