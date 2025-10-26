#ifndef CORE_STAGE_H
#define CORE_STAGE_H

#include <string>

namespace app::core {

    enum class Stage {
        INGEST,
        QUEUE,
        RULES,
        NOTIFY
    };

    std::string to_string(Stage c);

} // namespace app::core

#endif // CORE_STAGE_H
