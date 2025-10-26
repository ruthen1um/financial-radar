#include "logging.h"
#include "stage.h"

#include <crow/json.h>
#include <spdlog/spdlog.h>

#include <string>

namespace app::core {

    void log_processing(
        spdlog::level::level_enum level,
        Stage stage,
        const std::string& correlation_id,
        const std::string& message
    ) {
        crow::json::wvalue data {
            {"stage", to_string(stage)},
            {"correlation_id", correlation_id},
            {"message", message}
        };

        spdlog::log(level, data.dump());
    }

    void log_junk(
        spdlog::level::level_enum level,
        const std::string& correlation_id,
        const std::string& error
    ) {
        crow::json::wvalue data {
            {"correlation_id", correlation_id},
            {"error", error}
        };

        spdlog::log(level, data.dump());
    }

} // namespace app::core
