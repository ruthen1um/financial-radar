#ifndef CORE_METRICS_H
#define CORE_METRICS_H

#include <crow/json.h>

#include <atomic>

namespace app::core {

    template <typename T>
    class MetricsSingleton {
    public:
        static T& get_instance() {
            static T instance;
            return instance;
        }

        virtual crow::json::wvalue get_json() const = 0;
    };

    class IngestMetricsSingleton : public MetricsSingleton<IngestMetricsSingleton> {
    private:
        std::atomic<std::size_t> api_requests_;
        std::atomic<std::size_t> validation_errors_;
        std::atomic<std::size_t> enqueued_;
        std::atomic<std::size_t> enqueue_failures_;

    public:
        IngestMetricsSingleton();
        crow::json::wvalue get_json() const override;

        void increment_api_requests();
        void increment_validation_errors();
        void increment_enqueued();
        void increment_enqueue_failures();
    };

} // namespace app::core

#endif // CORE_METRICS_H
