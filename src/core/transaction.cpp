#include "transaction.h"

#include <date/date.h>
#include <uuid.h>

#include <chrono>
#include <sstream>
#include <algorithm>
#include <functional>
#include <random>
#include <string>
#include <array>

namespace app::core {

    namespace {
        const std::size_t MAX_TRANSACTION_ID_SIZE = 15;
        const std::size_t MAX_PARTICIPANT_SIZE = 15;
        const std::size_t MAX_TIMESTAMP_ABS_DIFF_SECONDS = 2 * 24 * 60 * 60;

        bool is_alnum_string(const std::string& s) {
            for (auto c : s) {
                if (!std::isalnum(c)) {
                    return false;
                }
            }
            return true;
        }

        bool is_valid_participant_account(const std::string& participant) {
            return is_alnum_string(participant) && participant.size() < MAX_PARTICIPANT_SIZE;
        }
    }

    std::string generate_correlation_id() {
        // TODO: set seed with random device only on program start
        // and not every time correlation_id is generated as it is slow

        std::random_device rd;
        auto seed_data = std::array<int, std::mt19937::state_size>{};
        std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
        std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
        std::mt19937 generator(seq);

        uuids::uuid_random_generator uuid_generator{generator};
        uuids::uuid id = uuid_generator();

        return uuids::to_string(id);
    }

    bool Validator::is_valid_transaction_id(const std::string& transaction_id) {
        return is_alnum_string(transaction_id) &&
               transaction_id.size() < MAX_TRANSACTION_ID_SIZE;
    }

    bool Validator::is_valid_timestamp(const std::string& timestamp) {
        std::istringstream iss;
        date::sys_time<std::chrono::microseconds> tp;
        iss.str(timestamp);
        if ((iss >> date::parse("%Y-%m-%dT%H:%M:%S", tp)).fail()) {
            return false;
        }
        auto now {std::chrono::system_clock::now()};
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(tp - now);
        return static_cast<std::size_t>(std::abs(duration.count())) < MAX_TIMESTAMP_ABS_DIFF_SECONDS;
    }

    bool Validator::is_valid_sender_account(const std::string& sender_account) {
        return is_valid_participant_account(sender_account);
    }

    bool Validator::is_valid_receiver_account(const std::string& receiver_account) {
        return is_valid_participant_account(receiver_account);
    }

    bool Validator::is_valid_amount(double amount) {
        return amount > 0 && amount < 1e9;
    }

    bool Validator::is_valid_transaction_type(const std::string& transaction_type) {
        return transaction_type == "withdrawal" ||
               transaction_type == "deposit" ||
               transaction_type == "transfer" ||
               transaction_type == "payment";
    }

} // namespace app::core
