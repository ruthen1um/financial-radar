#include "crow.h"
#include "time_parser.h" 
#include <spdlog/spdlog.h>
#include <uuid.h>

#include <atomic>
#include <regex>
#include <string>
#include <random>
#include <chrono>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <optional>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <array>
#include <unordered_set>
#include <cmath>

static std::atomic<uint64_t> api_requests{ 0 }, validation_errors{ 0 }, enqueued{ 0 }, enqueue_failures{ 0 };
static std::vector<uint64_t> latency_samples;
static std::mutex lat_mu;

bool check_id_format(const std::string& s) {
    static std::regex re(R"(^[A-Za-z0-9\-\._]{3,64}$)");
    return std::regex_match(s, re);
}

std::string gen_correlation() {

    std::random_device rd;
    auto seed_data = std::array<int, std::mt19937::state_size>{};
    std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
    std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
    std::mt19937 generator(seq);

    uuids::uuid_random_generator gen{ generator };
    uuids::uuid id = gen();

    return uuids::to_string(id);
}

static std::string now_iso_utc() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    std::tm* tmp = std::gmtime(&t);
    if (tmp) tm = *tmp;
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

static std::string sanitize_text(const std::string& s, size_t max_len) {
    std::string out;
    out.reserve(std::min(s.size(), max_len));
    for (char c : s) {
        if (std::iscntrl(static_cast<unsigned char>(c))) continue;
        out.push_back(c);
        if (out.size() >= max_len) break;
    }
    return out;
}

bool is_ip_address(const std::string& s) {
    static std::regex re4(R"(^((25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(25[0-5]|2[0-4]\d|[01]?\d\d?)$)");
    static std::regex re6(R"(^([0-9a-fA-F]{1,4}:){1,7}[0-9a-fA-F]{1,4}$)");
    return std::regex_match(s, re4) || std::regex_match(s, re6);
}

static std::string build_log_json(const std::string& component,
    const std::string& level,
    const std::string& correlation,
    const std::string& msg,
    const std::vector<std::pair<std::string, std::string>>& kv = {})
{
    std::ostringstream o;
    o << "{\"component\":\"" << component << "\","
        << "\"level\":\"" << level << "\","
        << "\"correlation_id\":\"" << correlation << "\","
        << "\"msg\":\"" << msg << "\"";
    for (auto& p : kv) {
        o << ",\"" << p.first << "\":\"" << p.second << "\"";
    }
    o << "}";
    return o.str();
}


void log_junk(const std::string& raw_body, const std::string& reason, const std::string& correlation) {
    try {
        std::ofstream f("junk.log", std::ios::app);
        if (!f) return;
        f << "{"
            << "\"ts\":\"" << now_iso_utc() << "\","
            << "\"correlation_id\":\"" << correlation << "\","
            << "\"reason\":\"" << reason << "\","
            << "\"body\":";
        // safely quote raw_body as JSON string-like
        std::string esc;
        for (char c : raw_body) {
            switch (c) {
            case '\\': esc += "\\\\"; break;
            case '"': esc += "\\\""; break;
            case '\n': esc += "\\n"; break;
            case '\r': esc += "\\r"; break;
            case '\t': esc += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // control -> hex
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
                    esc += oss.str();
                }
                else esc += c;
            }
        }
        f << "\"" << esc << "\"";
        f << "}\n";
        f.close();
    }
    catch (...) {
        // never throw from logging
    }
}

// helper to extract numeric values
std::optional<double> opt_double(const crow::json::rvalue& v) {
    using jt = crow::json::type;
    if (v.t() == jt::Number) {
        return v.d();
    }
    if (v.t() == jt::String) {
        try {
            return std::stod(std::string(v.s()));
        }
        catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

struct TransactionRow {
    std::string transaction_id;
    std::string timestamp; // ISO
    std::string sender_account;
    std::string receiver_account;
    double amount = 0.0;
    std::string transaction_type;
    std::string merchant_category;
    std::string location;
    std::string device_used;
    bool is_fraud = false;
    std::string fraud_type;
    double time_since_last_transaction = 0.0;
    double spending_deviation_score = 0.0;
    double velocity_score = 0.0;
    double geo_anomaly_score = 0.0;
    std::string payment_channel;
    std::string ip_address;
    std::string device_hash;
    std::string description; // any other fields
};


std::string row_to_line(const TransactionRow& r) {
    std::ostringstream o;
    auto esc = [](const std::string& s) {
        std::string out; out.reserve(s.size());
        for (char c : s) {
            if (c == ';') out += "\\;";
            else if (c == '\n') out += ' ';
            else out += c;
        }
        return out;
        };
    o << esc(r.transaction_id) << ";" << esc(r.timestamp) << ";" << esc(r.sender_account) << ";" << esc(r.receiver_account)
        << ";" << r.amount << ";" << esc(r.transaction_type) << ";" << esc(r.merchant_category) << ";" << esc(r.location)
        << ";" << esc(r.device_used) << ";" << (r.is_fraud ? "1" : "0") << ";" << esc(r.fraud_type)
        << ";" << r.time_since_last_transaction << ";" << r.spending_deviation_score << ";" << r.velocity_score
        << ";" << r.geo_anomaly_score << ";" << esc(r.payment_channel) << ";" << esc(r.ip_address) << ";" << esc(r.device_hash)
        << ";" << esc(r.description);
    return o.str();
}


int main() {
    spdlog::set_level(spdlog::level::info);
    auto logmsg = build_log_json("startup", "info", "", "starting");
    spdlog::info(logmsg);
    crow::App<> app;

    // Metrics endpoint
    CROW_ROUTE(app, "/metrics").methods("GET"_method)([]() {
        std::ostringstream s;
        s << "api_requests_total " << api_requests.load() << "\n";
        s << "validation_errors_total " << validation_errors.load() << "\n";
        s << "enqueued_total " << enqueued.load() << "\n";
        s << "enqueue_failures_total " << enqueue_failures.load() << "\n";
        // p50/p95
        std::lock_guard<std::mutex> g(lat_mu);
        if (!latency_samples.empty()) {
            auto v = latency_samples;
            std::sort(v.begin(), v.end());
            if (!v.empty()) {
                size_t p50_idx = v.size() * 50 / 100;
                size_t p95_idx = std::min(v.size() - 1, v.size() * 95 / 100);
                size_t p50 = v[p50_idx];
                size_t p95 = v[p95_idx];
                s << "api_latency_p50_ms " << p50 << "\n";
                s << "api_latency_p95_ms " << p95 << "\n";
            }
        }
        return crow::response(200, s.str());
        });

    // Ingest endpoint with verbose logging
    CROW_ROUTE(app, "/transactions").methods("POST"_method)([&](const crow::request& req) {
        auto start = std::chrono::steady_clock::now();
        api_requests.fetch_add(1);

        // generate correlation at entry for tracing
        std::string correlation = gen_correlation();
        logmsg = build_log_json("ingest", "info", correlation, "received_request",
            { {"content_length", std::to_string(req.body.size())} });
        //spdlog::info(logmsg);

        const std::string raw = req.body;
        if (raw.empty()) {
            validation_errors.fetch_add(1);
            log_junk(raw, "empty_body", correlation);
            logmsg = build_log_json("ingest", "warn", correlation, "empty_body");
            spdlog::warn(logmsg);
            crow::json::wvalue out; out["error"] = "empty_body"; out["details"] = "no transaction";
            return crow::response(400, out);
        }

        // protective outer try/catch to avoid uncaught exception -> 500
        try {
            logmsg = build_log_json("ingest", "info", correlation, "parsing_json");
            //spdlog::info(logmsg);
            crow::json::rvalue body;
            try {
                body = crow::json::load(raw);
            }
            catch (...) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_json_parse", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "json_parse_failed");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "invalid_json"; out["details"] = "cannot parse JSON";
                return crow::response(400, out);
            }
            logmsg = build_log_json("ingest", "info", correlation, "json_parsed");
            //spdlog::info(logmsg);

            TransactionRow row;

            // transaction_id
            if (body.has("transaction_id") && body["transaction_id"].t() == crow::json::type::String) {
                row.transaction_id = sanitize_text(body["transaction_id"].s(), 128);
                logmsg = build_log_json("ingest", "info", correlation, "transaction_id_provided",
                    { {"transaction_id", row.transaction_id} });
                //spdlog::info(logmsg);
            }
            else {
                row.transaction_id = "tx-" + correlation;
                logmsg = build_log_json("ingest", "info", correlation, "transaction_id_generated",
                    { {"transaction_id", row.transaction_id} });
                //spdlog::info(logmsg);
            }

            // timestamp
            logmsg = build_log_json("ingest", "info", correlation, "validating_timestamp");
            //spdlog::info(logmsg);
            if (!body.has("timestamp") || body["timestamp"].t() != crow::json::type::String) {
                validation_errors.fetch_add(1);
                log_junk(raw, "missing_or_invalid_timestamp", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "missing_or_invalid_timestamp");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "timestamp required and must be ISO string";
                return crow::response(400, out);
            }
            row.timestamp = sanitize_text(body["timestamp"].s(), 64);
            try {
                auto parsed_time = parse8601_full(row.timestamp);
                auto now = sys_time<milliseconds>{ std::chrono::duration_cast<milliseconds>(std::chrono::system_clock::now().time_since_epoch()) };
                auto diff = parsed_time - now;
                auto abs_diff = std::chrono::duration_cast<hours>(diff >= milliseconds(0) ? diff : -diff);
                if (abs_diff > days(2)) {
                    validation_errors.fetch_add(1);
                    log_junk(raw, "timestamp_out_of_range", correlation);
                    logmsg = build_log_json("ingest", "warn", correlation, "timestamp_out_of_range");
                    spdlog::warn(logmsg);
                    crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "timestamp out of allowed window (+/-2 days)";
                    return crow::response(400, out);
                }
            }
            catch (const std::runtime_error&) {
                validation_errors.fetch_add(1);
                log_junk(raw, "timestamp_parse_error", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "timestamp_parse_error");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "timestamp parse error";
                return crow::response(400, out);
            }
            logmsg = build_log_json("ingest", "info", correlation, "timestamp_valid");
            //spdlog::info(logmsg);

            // sender_account (from)
            logmsg = build_log_json("ingest", "info", correlation, "validating_sender");
            //spdlog::info(logmsg);
            if (body.has("sender_account") && body["sender_account"].t() == crow::json::type::String) {
                row.sender_account = sanitize_text(body["sender_account"].s(), 64);
            }
            else if (body.has("from") && body["from"].t() == crow::json::type::String) {
                row.sender_account = sanitize_text(body["from"].s(), 64);
            }
            else {
                validation_errors.fetch_add(1);
                log_junk(raw, "missing_sender", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "missing_sender");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "sender_account/from required";
                return crow::response(400, out);
            }
            if (!check_id_format(row.sender_account)) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_sender_account_format", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "invalid_sender_account_format",
                    { {"sender_account", row.sender_account} });
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "invalid sender_account format";
                return crow::response(400, out);
            }
            logmsg = build_log_json("ingest", "info", correlation, "sender_valid",
                { {"sender_account", row.sender_account} });
            //spdlog::info(logmsg);

            // receiver_account (to)
            logmsg = build_log_json("ingest", "info", correlation, "validating_receiver");
            //spdlog::info(logmsg);
            if (body.has("receiver_account") && body["receiver_account"].t() == crow::json::type::String) {
                row.receiver_account = sanitize_text(body["receiver_account"].s(), 64);
            }
            else if (body.has("to") && body["to"].t() == crow::json::type::String) {
                row.receiver_account = sanitize_text(body["to"].s(), 64);
            }
            else {
                validation_errors.fetch_add(1);
                log_junk(raw, "missing_receiver", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "missing_receiver");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "receiver_account/to required";
                return crow::response(400, out);
            }
            if (!check_id_format(row.receiver_account)) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_receiver_account_format", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "invalid_receiver_account_format",
                    { {"receiver_account", row.receiver_account} });
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "invalid receiver_account format";
                return crow::response(400, out);
            }
            logmsg = build_log_json("ingest", "info", correlation, "receiver_valid",
                { {"receiver_account", row.receiver_account} });
            //spdlog::info(logmsg);

            // amount
            logmsg = build_log_json("ingest", "info", correlation, "validating_amount");
            //spdlog::info(logmsg);
            if (!body.has("amount")) {
                validation_errors.fetch_add(1);
                log_junk(raw, "missing_amount", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "missing_amount");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "amount required";
                return crow::response(400, out);
            }
            auto optAmt = opt_double(body["amount"]);
            if (!optAmt.has_value()) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_amount_type", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "invalid_amount_type");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "amount must be numeric";
                return crow::response(400, out);
            }
            row.amount = optAmt.value();
            if (!(row.amount > 0 && row.amount <= 1e9)) {
                validation_errors.fetch_add(1);
                log_junk(raw, "amount_out_of_bounds", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "amount_out_of_bounds",
                    { {"amount", std::to_string(row.amount)} });
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "amount out of acceptable range";
                return crow::response(400, out);
            }
            logmsg = build_log_json("ingest", "info", correlation, "amount_valid",
                { {"amount", std::to_string(row.amount)} });
            //spdlog::info(logmsg);

            // optional string fields (sanitization)
            auto get_str_limited = [](const crow::json::rvalue& v, size_t limit)->std::string {
                if (v.t() != crow::json::type::String) return std::string();
                return sanitize_text(v.s(), limit);
                };
            row.transaction_type = body.has("transaction_type") ? get_str_limited(body["transaction_type"], 64) : "";
            row.merchant_category = body.has("merchant_category") ? get_str_limited(body["merchant_category"], 64) : "";
            row.location = body.has("location") ? get_str_limited(body["location"], 128) : "";
            row.device_used = body.has("device_used") ? get_str_limited(body["device_used"], 128) : "";
            row.fraud_type = body.has("fraud_type") ? get_str_limited(body["fraud_type"], 128) : "";
            row.payment_channel = body.has("payment_channel") ? get_str_limited(body["payment_channel"], 64) : "";
            row.device_hash = body.has("device_hash") ? get_str_limited(body["device_hash"], 128) : "";
            row.ip_address = body.has("ip_address") ? get_str_limited(body["ip_address"], 64) : "";
            //spdlog::info(R"({"component":"ingest","level":"info","correlation_id":"{}","msg":"optional_fields_sanitized"})", correlation);
            // is_fraud handling
            if (body.has("is_fraud")) {
                logmsg = build_log_json("ingest", "info", correlation, "validating_is_fraud");
                //spdlog::info(logmsg);
                auto v = body["is_fraud"];
                if (v.t() == crow::json::type::True || v.t() == crow::json::type::False) {
                    row.is_fraud = v.b();
                }
                else if (v.t() == crow::json::type::Number) {
                    double vv = v.d();
                    row.is_fraud = (vv != 0.0);
                }
                else if (v.t() == crow::json::type::String) {
                    std::string s = v.s();
                    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                    row.is_fraud = (s == "true" || s == "1" || s == "yes");
                }
                else {
                    validation_errors.fetch_add(1);
                    log_junk(raw, "invalid_is_fraud_type", correlation);
                    logmsg = build_log_json("ingest", "warn", correlation, "invalid_is_fraud_type");
                    spdlog::warn(logmsg);
                    crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "is_fraud must be boolean/0/1";
                    return crow::response(400, out);
                }
                logmsg = build_log_json("ingest", "info", correlation, "is_fraud_parsed",
                    { {"is_fraud", std::to_string(row.is_fraud ? 1 : 0)} });
                //spdlog::info(logmsg);
            }

            // numeric optional scores
            auto get_score = [&](const std::string& key)->std::optional<double> {
                if (!body.has(key)) return std::nullopt;
                auto v = opt_double(body[key]);
                if (!v.has_value()) return std::nullopt;
                double val = v.value();
                if (!std::isfinite(val) || val < 0) return std::nullopt;
                return val;
                };

            if (auto v = get_score("time_since_last_transaction")) row.time_since_last_transaction = *v;
            else if (body.has("time_since_last_transaction") && body["time_since_last_transaction"].t() != crow::json::type::Null) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_time_since_last_transaction", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "invalid_time_since_last_transaction");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "time_since_last_transaction must be non-negative number";
                return crow::response(400, out);
            }

            if (auto v = get_score("spending_deviation_score")) row.spending_deviation_score = *v;
            else if (body.has("spending_deviation_score") && body["spending_deviation_score"].t() != crow::json::type::Null) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_spending_deviation_score", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "invalid_spending_deviation_score");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "spending_deviation_score invalid";
                return crow::response(400, out);
            }

            if (auto v = get_score("velocity_score")) row.velocity_score = *v;
            else if (body.has("velocity_score") && body["velocity_score"].t() != crow::json::type::Null) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_velocity_score", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "invalid_velocity_score");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "velocity_score invalid";
                return crow::response(400, out);
            }

            if (auto v = get_score("geo_anomaly_score")) row.geo_anomaly_score = *v;
            else if (body.has("geo_anomaly_score") && body["geo_anomaly_score"].t() != crow::json::type::Null) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_geo_anomaly_score", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "invalid_geo_anomaly_score");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "geo_anomaly_score invalid";
                return crow::response(400, out);
            }

            // ip_address check
            if (!row.ip_address.empty() && !is_ip_address(row.ip_address)) {
                validation_errors.fetch_add(1);
                log_junk(raw, "invalid_ip_address", correlation);
                logmsg = build_log_json("ingest", "warn", correlation, "invalid_ip_address");
                spdlog::warn(logmsg);
                crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "ip_address invalid";
                return crow::response(400, out);
            }

            // Build description: store sanitized raw payload for safety
            row.description = sanitize_text(raw, 512);
            logmsg = build_log_json("ingest", "info", correlation, "description_built",
                { {"desc_len", std::to_string(row.description.size())} });
            //spdlog::info(logmsg);

            // Prepare payload (fast CSV-like)
            std::string row_line = row_to_line(row);
            logmsg = build_log_json("ingest", "info", correlation, "payload_prepared",
                { {"payload_len", std::to_string(row_line.size())} });
            //spdlog::info(logmsg);

            // TODO: publish to RabbitMQ
            bool ok = true;
            logmsg = build_log_json("ingest", "info", correlation, "publishing_to_queue_attempt");
            //spdlog::info(logmsg);
            if (!ok) {
                enqueue_failures.fetch_add(1);
                logmsg = build_log_json("ingest", "error", correlation, "enqueue_failed");
                spdlog::error(logmsg);
                log_junk(raw, "enqueue_failed", correlation);
                crow::json::wvalue out; out["error"] = "enqueue_failed"; out["details"] = "broker_unavailable_or_queue_full";
                return crow::response(503, out);
            }
            logmsg = build_log_json("ingest", "info", correlation, "published_to_queue");
            //spdlog::info(logmsg);

            // mark enqueued and respond
            enqueued.fetch_add(1);
            crow::json::wvalue accepted;
            accepted["correlation_id"] = correlation;
            accepted["transaction_id"] = row.transaction_id;
            accepted["status"] = "accepted";
            std::string acceptedStr = accepted.dump();

            // metrics latency
            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            {
                std::lock_guard<std::mutex> g(lat_mu);
                latency_samples.push_back((uint64_t)dur);
                if (latency_samples.size() > 2000) latency_samples.erase(latency_samples.begin());
            }

            logmsg = build_log_json("ingest", "info", correlation, "completed",
                { {"transaction_id", row.transaction_id}, {"enqueue_ms", std::to_string(dur)} });
            //spdlog::info(logmsg);

            return crow::response(202, acceptedStr);
        }
        catch (const std::exception& e) {
            // catch any unexpected runtime errors, log raw payload + message
            validation_errors.fetch_add(1);
            logmsg = build_log_json("ingest", "error", correlation, "exception_in_handler",
                { {"error", e.what()} });
            spdlog::error(logmsg);
            log_junk(raw, std::string("exception: ") + e.what(), correlation);
            crow::json::wvalue out; out["error"] = "internal_error"; out["details"] = "invalid json object or processing error";
            return crow::response(400, out);
        }
        catch (...) {
            validation_errors.fetch_add(1);
            logmsg = build_log_json("ingest", "error", correlation, "unknown_exception_in_handler");
            spdlog::error(logmsg);
            log_junk(raw, "unknown_exception", correlation);
            crow::json::wvalue out; out["error"] = "internal_error"; out["details"] = "unknown error";
            return crow::response(400, out);
        }
        });

    CROW_ROUTE(app, "/")([]() {
        return "Hello world";
        });

    app.port(80).multithreaded().run();
}