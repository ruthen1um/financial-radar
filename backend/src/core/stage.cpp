#include "stage.h"

#include <string>

namespace app::core {

    std::string to_string(Stage c) {
        switch (c) {
            case Stage::INGEST: return "ingest";
            case Stage::QUEUE: return "queue";
            case Stage::RULES: return "rules";
            case Stage::NOTIFY: return "notify";
        }
    }

} // namespace app::core
