#ifndef CORE_LOGGING_H
#define CORE_LOGGING_H

#include "stage.h"

#include <spdlog/spdlog.h>

#include <string>

namespace app::core {

    void log_processing(
        spdlog::level::level_enum level,
        Stage stage,
        const std::string& correlation_id,
        const std::string& message
    );

    void log_junk(
        spdlog::level::level_enum level,
        const std::string& correlation_id,
        const std::string& error
    );

} // namespace app::core

#endif // CORE_LOGGING_H
