#ifndef WEBSERVER_UTIL_H
#define WEBSERVER_UTIL_H

#include <crow/json.h>

#include <string>

namespace app::webserver {

    crow::json::wvalue get_error_json(
        const std::string& error,
        const std::string& message
    );

} // namespace app::webserver

#endif // WEBSERVER_UTIL_H
