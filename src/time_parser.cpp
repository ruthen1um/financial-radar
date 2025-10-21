#include "time_parser.h"
#include <sstream>
#include <stdexcept>

using namespace std::chrono;

date::sys_time<milliseconds> parse8601_full(const std::string& input)
{
    std::istringstream in;
    date::sys_time<milliseconds> tp;

    // helper to attempt parse with given stringstream content and format
    auto try_parse = [&](const std::string& s, const char* fmt) -> bool {
        in.clear();
        in.str(s);
        try {
            in.exceptions(std::ios::failbit);
            in >> date::parse(fmt, tp);
            return true;
        }
        catch (...) {
            // try again without exceptions to inspect failbit in fallback attempts
            in.clear();
            in.str(s);
            try {
                in >> date::parse(fmt, tp);
                return !in.fail();
            }
            catch (...) {
                return false;
            }
        }
        };

    // If input ends with 'Z' — convert to +00:00 for %Ez parsing
    if (!input.empty() && input.back() == 'Z') {
        std::string modified = input.substr(0, input.size() - 1) + "+00:00";
        if (try_parse(modified, "%FT%T%Ez")) return tp;
    }

    // Try full ISO with timezone
    if (try_parse(input, "%FT%T%Ez")) return tp;

    // Try variants (sometimes fractional seconds or missing timezone)
    // these attempts are intentionally permissive and repeated to handle minor differences
    if (try_parse(input, "%FT%T")) return tp;
    if (try_parse(input, "%F %T%Ez")) return tp;
    if (try_parse(input, "%F %T")) return tp;

    // If there is a space instead of 'T', try replacing first space with 'T' and parse
    std::string tmp = input;
    size_t pos = tmp.find(' ');
    if (pos != std::string::npos) {
        tmp.replace(pos, 1, "T");
        if (try_parse(tmp, "%FT%T%Ez")) return tp;
        if (try_parse(tmp, "%FT%T")) return tp;
    }

    throw std::runtime_error(std::string("Unable to parse time: ") + input);
}
