#include "util.h"

#include <crow/json.h>

namespace app::webserver {

    crow::json::wvalue get_error_json(
        const std::string& error,
        const std::string& message
    ) {
        return {
            {"error", error},
            {"message", message},
        };
    }

} // namespace app::webserver
