#ifndef WEBSERVER_HANDLERS_H
#define WEBSERVER_HANDLERS_H

#include <crow.h>

namespace app::webserver {

crow::response handle_transaction(const crow::request& req);
crow::response handle_metrics();

} // namespace app::webserver

#endif // WEBSERVER_HANDLERS_H
