#ifndef TIME_PARSER_H
#define TIME_PARSER_H

#include "date/date.h"
#include <chrono>
#include <string>
#include <sstream>
#include <stdexcept>

using namespace std::chrono;
using namespace date;

date::sys_time<std::chrono::milliseconds>
parse8601_full(std::string const& input);

#endif
