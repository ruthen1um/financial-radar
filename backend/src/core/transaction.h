#ifndef CORE_TRANSACTION_H
#define CORE_TRANSACTION_H

#include <string>
#include <optional>

namespace app::core {

    struct Transaction {
        std::string correlation_id;

        std::string transaction_id;
        std::string timestamp; // ISO 8601 with microseconds
        std::string sender_account;
        std::string receiver_account;
        double amount;
        std::string transaction_type;
        std::optional<std::string> merchant_category;
        std::optional<std::string> location;
        std::optional<std::string> device_used;
        std::optional<bool> is_fraud;
        std::optional<std::string> fraud_type;
        std::optional<double> time_since_last_transaction;
        std::optional<double> spending_deviation_score;
        std::optional<double> velocity_score;
        std::optional<double> geo_anomaly_score;
        std::optional<std::string> payment_channel;
        std::optional<std::string> ip_address;
        std::optional<std::string> device_hash;
    };

    std::string generate_correlation_id();

    class Validator {
    public:
        static bool is_valid_transaction_id(const std::string& transaction_id);
        static bool is_valid_timestamp(const std::string& timestamp);
        static bool is_valid_sender_account(const std::string& sender_account);
        static bool is_valid_receiver_account(const std::string& receiver_account);
        static bool is_valid_amount(double amount);
        static bool is_valid_transaction_type(const std::string& transaction_type);
    };

} // namespace app::core

#endif // CORE_TRANSACTION_H
