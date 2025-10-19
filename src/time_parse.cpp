#include "time_parser.h"

sys_time<milliseconds>
parse8601_full(std::string const& input)
{
    std::istringstream in;
    sys_time<milliseconds> tp;

    if (input.back() == 'Z') {
        std::string modified = input.substr(0, input.size() - 1) + "+00:00";
        in.str(modified);
        in.clear();
        in.exceptions(std::ios::failbit);
        try {
            in >> parse("%FT%T%Ez", tp);
            return tp;
        }
        catch (...) {
            in.clear();
            in.str(modified);
            in >> parse("%FT%T%Ez", tp);
            if (!in.fail()) return tp;
        }
    }

    in.str(input);
    in.clear();
    in.exceptions(std::ios::failbit);
    try {
        in >> parse("%FT%T%Ez", tp);
        return tp;
    }
    catch (...) {}

    in.clear();
    in.str(input);
    try {
        in >> parse("%FT%T%Ez", tp);
        if (!in.fail()) return tp;
    }
    catch (...) {}

    in.clear();
    in.str(input);
    try {
        in >> parse("%FT%T%Ez", tp);
        if (!in.fail()) return tp;
    }
    catch (...) {}

    in.clear();
    in.str(input);
    try {
        in >> parse("%FT%T", tp);
        if (!in.fail()) return tp;
    }
    catch (...) {}

    in.clear();
    in.str(input);
    try {
        in >> parse("%FT%T", tp);
        if (!in.fail()) return tp;
    }
    catch (...) {}

    std::string temp = input;
    size_t t_pos = temp.find('T');
    if (t_pos == std::string::npos) {
        t_pos = temp.find(' ');
        if (t_pos != std::string::npos) {
            temp.replace(t_pos, 1, "T");
            in.clear();
            in.str(temp);
            try {
                in >> parse("%FT%T%Ez", tp);
                if (!in.fail()) return tp;
            }
            catch (...) {}
        }
    }

    throw std::runtime_error("Unable to parse time: " + input);
}