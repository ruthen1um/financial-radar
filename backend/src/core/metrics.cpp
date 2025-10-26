#include "metrics.h"

namespace app::core {

    IngestMetricsSingleton::IngestMetricsSingleton()
        : api_requests_(0), validation_errors_(0), enqueued_(0), enqueue_failures_(0) {}

    crow::json::wvalue IngestMetricsSingleton::get_json() const {
        return {
            {"api_requests", api_requests_.load()},
            {"validation_errors", validation_errors_.load()},
            {"enqueued", enqueued_.load()},
            {"enqueue_failures", enqueue_failures_.load()}
        };
    }

    void IngestMetricsSingleton::increment_api_requests() {
        api_requests_++;
    }

    void IngestMetricsSingleton::increment_validation_errors() {
        validation_errors_++;
    }

    void IngestMetricsSingleton::increment_enqueued() {
        enqueued_++;
    }

    void IngestMetricsSingleton::increment_enqueue_failures() {
        enqueue_failures_++;
    }

} // namespace app::core
