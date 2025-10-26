#include "webserver/handlers.h"

#include <crow.h>
#include <spdlog/spdlog.h>

class CrowSpdlogLoggerAdapter : public crow::ILogHandler {
private:
    static auto crow_level_to_spdlog_level(crow::LogLevel level) {
        return static_cast<spdlog::level::level_enum>(static_cast<int>(level) + 1);
    }

public:
    CrowSpdlogLoggerAdapter() {}
    void log(const std::string& message, crow::LogLevel level) {
        auto spdlog_level {crow_level_to_spdlog_level(level)};
        spdlog::log(spdlog_level, message);
    }
};

int main() {
    // Output Crow log with spdlog
    CrowSpdlogLoggerAdapter logger;
    crow::logger::setHandler(&logger);

    crow::SimpleApp app;

    spdlog::set_level(spdlog::level::debug);

    CROW_ROUTE(app, "/metrics")
        .methods(crow::HTTPMethod::GET)
        (app::webserver::handle_metrics);

    CROW_ROUTE(app, "/transaction")
        .methods(crow::HTTPMethod::POST)
        (app::webserver::handle_transaction);

    app.port(8080).multithreaded().run();
}
