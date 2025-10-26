#include "handlers.h"
#include "util.h"
#include "../core/logging.h"
#include "../core/stage.h"
#include "../core/metrics.h"
#include "../core/transaction.h"

#include <crow.h>
#include <spdlog/spdlog.h>

namespace app::webserver {

/* /transaction handler */
crow::response handle_transaction(const crow::request& req) {
    try {
        auto& metrics {app::core::IngestMetricsSingleton::get_instance()};
        metrics.increment_api_requests();

        // Generate correlation_id to trace processing
        auto correlation_id {app::core::generate_correlation_id()};

        log_processing(
            spdlog::level::info,
            app::core::Stage::INGEST,
            correlation_id,
            "Entering ingest stage"
        );

        /* Check Content-Type */
        if (req.get_header_value("Content-Type") != "application/json") {
            metrics.increment_validation_errors();
            std::string message {"invalid content-type header"};
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                message
            );
            return crow::response{400, app::webserver::get_error_json(
                "invalid_content_type",
                std::move(message)
            )};
        }

        /* Check that body is non-empty */
        if (req.body.empty()) {
            metrics.increment_validation_errors();
            std::string message {"request has empty body"};
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                message
            );
            return crow::response{400, app::webserver::get_error_json(
                "empty_body",
                std::move(message)
            )};
        }

        auto data = crow::json::load(req.body);

        /* Check if data is a valid json */
        if (!data) {
            metrics.increment_validation_errors();
            std::string message {"request body has invalid json format"};
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                "Invalid json format"
            );
            return crow::response{400, app::webserver::get_error_json(
                "invalid_json",
                std::move(message)
            )};
        }

        app::core::Transaction t;

        /* Check if data has valid transaction_id */
        if (!data.has("transaction_id") ||
            data["transaction_id"].t() != crow::json::type::String ||
            !app::core::Validator::is_valid_transaction_id(data["transaction_id"].s())
        ) {
            std::string message {"transaction_id is missing or it has invalid format but is required"};
            metrics.increment_validation_errors();
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                message
            );
            return crow::response{400, app::webserver::get_error_json(
                "missing_or_invalid_transaction_id",
                std::move(message)
            )};
        }

        t.transaction_id = data["transaction_id"].s();

        /* Check if data has valid timestamp */
        if (!data.has("timestamp") ||
            data["timestamp"].t() != crow::json::type::String ||
            !app::core::Validator::is_valid_timestamp(data["timestamp"].s())
        ) {
            metrics.increment_validation_errors();
            std::string message {"timestamp is missing or it has invalid format"
                                 " (must exist and conform to the extended ISO 8601 standard with microseconds"
                                 " and timestamp should be within 2 days window in future or past from current timestamp)"};
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                message
            );
            return crow::response{400, app::webserver::get_error_json(
                "missing_or_invalid_timestamp",
                std::move(message)
            )};
        }

        t.timestamp = data["timestamp"].s();

        /* Check if data has valid sender_account */
        if (!data.has("sender_account") ||
            data["sender_account"].t() != crow::json::type::String ||
            !app::core::Validator::is_valid_sender_account(data["sender_account"].s())
        ) {
            metrics.increment_validation_errors();
            std::string message {"sender_account is missing or it has invalid format but is required"};
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                message
            );
            return crow::response{400, app::webserver::get_error_json(
                "missing_or_invalid_sender_account",
                std::move(message)
            )};
        }

        t.sender_account = data["sender_account"].s();

        /* Check if data has valid receiver_account */
        if (!data.has("receiver_account") ||
            data["receiver_account"].t() != crow::json::type::String ||
            !app::core::Validator::is_valid_receiver_account(data["receiver_account"].s())
        ) {
            metrics.increment_validation_errors();
            std::string message {"receiver_account is missing or it has invalid format but is required"};
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                message
            );
            return crow::response{400, app::webserver::get_error_json(
                "missing_or_invalid_receiver_account",
                std::move(message)
            )};
        }

        t.receiver_account = data["receiver_account"].s();

        if (!data.has("amount") ||
            data["amount"].t() != crow::json::type::Number ||
            !app::core::Validator::is_valid_amount(data["amount"].d())
        ) {
            metrics.increment_validation_errors();
            std::string message {"amount is missing or it has invalid format but is required"};
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                message
            );
            return crow::response{400, app::webserver::get_error_json(
                "missing_or_invalid_amount",
                std::move(message)
            )};
        }

        t.amount = data["amount"].d();

        if (!data.has("transaction_type") ||
            data["transaction_type"].t() != crow::json::type::String ||
            !app::core::Validator::is_valid_transaction_type(data["transaction_type"].s())
        ) {
            metrics.increment_validation_errors();
            std::string message {"transaction_type is missing or it has invalid format but is required"};
            log_processing(
                spdlog::level::err,
                app::core::Stage::INGEST,
                correlation_id,
                message
            );
            return crow::response{400, app::webserver::get_error_json(
                "missing_or_invalid_transaction_type",
                std::move(message)
            )};
        }

        t.transaction_type = data["transaction_type"].s();

        if (data.has("merchant_category") &&
            data["merchant_category"].t() == crow::json::type::String) {
            t.merchant_category = std::make_optional(data["merchant_category"].s());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "merchant_category is missing or it has invalid format"
            );
            t.merchant_category = std::nullopt;
        }

        if (data.has("location") &&
            data["location"].t() == crow::json::type::String) {
            t.location = std::make_optional(data["location"].s());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "location is missing or it has invalid format"
            );
            t.location = std::nullopt;
        }

        if (data.has("device_used") &&
            data["device_used"].t() == crow::json::type::String) {
            t.device_used = std::make_optional(data["device_used"].s());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "device_used is missing or it has invalid format"
            );
            t.device_used = std::nullopt;
        }

        if (data.has("is_fraud") &&
            (data["is_fraud"].t() == crow::json::type::True ||
             data["is_fraud"].t() == crow::json::type::False)
        ) {
            t.is_fraud = std::make_optional(data["is_fraud"].b());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "is_fraud is missing or it has invalid format"
            );
            t.is_fraud = std::nullopt;
        }

        if (data.has("fraud_type") &&
            data["fraud_type"].t() == crow::json::type::String) {
            t.fraud_type = std::make_optional(data["fraud_type"].s());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "fraud_type is missing or it has invalid format"
            );
            t.fraud_type = std::nullopt;
        }

        if (data.has("time_since_last_transaction") &&
            data["time_since_last_transaction"].t() == crow::json::type::Number) {
            t.time_since_last_transaction =
                std::make_optional(data["time_since_last_transaction"].d());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "time_since_last_transaction is missing or it has invalid format"
            );
            t.time_since_last_transaction = std::nullopt;
        }

        if (data.has("spending_deviation_score") &&
            data["spending_deviation_score"].t() == crow::json::type::Number) {
            t.spending_deviation_score = std::make_optional(data["spending_deviation_score"].d());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "spending_deviation_score is missing or it has invalid format"
            );
            t.spending_deviation_score = std::nullopt;
        }

        if (data.has("velocity_score") &&
            data["velocity_score"].t() == crow::json::type::Number) {
            t.velocity_score = std::make_optional(data["velocity_score"].d());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "velocity_score is missing or it has invalid format"
            );
            t.velocity_score = std::nullopt;
        }

        if (data.has("geo_anomaly_score") &&
            data["geo_anomaly_score"].t() == crow::json::type::Number) {
            t.geo_anomaly_score = std::make_optional(data["geo_anomaly_score"].d());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "geo_anomaly_scole is missing or it has invalid format"
            );
            t.geo_anomaly_score = std::nullopt;
        }

        if (data.has("payment_channel") &&
            data["payment_channel"].t() == crow::json::type::String) {
            t.payment_channel = std::make_optional(data["payment_channel"].s());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "payment_channel is missing or it has invalid format"
            );
            t.payment_channel = std::nullopt;
        }

        if (data.has("ip_address") &&
            data["ip_address"].t() == crow::json::type::String) {
            t.ip_address = std::make_optional(data["ip_address"].s());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "ip_address is missing or it has invalid format"
            );
            t.ip_address = std::nullopt;
        }

        if (data.has("device_hash") &&
            data["device_hash"].t() == crow::json::type::String) {
            t.device_hash = std::make_optional(data["device_hash"].s());
        } else {
            log_processing(
                spdlog::level::warn,
                app::core::Stage::INGEST,
                correlation_id,
                "device_hash is missing or it has invalid format"
            );
            t.device_hash = std::nullopt;
        }

        return crow::response{200};

    } catch (...) {
        // Unhandled exception - return internal error code
        return crow::response{500};
    }
}

/* /metrics handler */
crow::response handle_metrics() {
    auto& metrics {app::core::IngestMetricsSingleton::get_instance()};
    return crow::response{200, metrics.get_json()};
}

} // namespace app::webserver
