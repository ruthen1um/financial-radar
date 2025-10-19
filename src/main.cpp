#include "time_parser.h"
#include <spdlog/spdlog.h>
#include <uuid.h>
#include <crow.h>
#include <atomic>
#include <regex>
#include <chrono>

// static std::atomic<uint64_t> api_requests{ 0 }, validation_errors{ 0 }, enqueued{ 0 }, enqueue_failures{ 0 };
// static std::vector<uint64_t> latency_samples;
// static std::mutex lat_mu;
//
// bool check_id_format(const std::string& s) {
//     static std::regex re(R"(^[A-Za-z0-9\-\._]{3,64}$)");
//     return std::regex_match(s, re);
// }
//
// std::string gen_correlation() {
//     return uuids::to_string(uuids::uuid_system_generator{}());
// }

int main() {
    // spdlog::set_level(spdlog::level::info);
    // spdlog::info(R"({"component":"startup","level":"info","msg":"starting"})");
    // crow::App<> app;
    //
    //this should be putted in Prometheus later
    // CROW_ROUTE(app, "/metrics").methods("GET"_method)([]() {
    //     std::ostringstream s;
    //     s << "api_requests_total " << api_requests.load() << "\n";
    //     s << "validation_errors_total " << validation_errors.load() << "\n";
    //     s << "enqueued_total " << enqueued.load() << "\n";
    //     s << "enqueue_failures_total " << enqueue_failures.load() << "\n";
    //     // p50/p95
    //     std::lock_guard<std::mutex> g(lat_mu);
    //     if (!latency_samples.empty()) {
    //         auto v = latency_samples;
    //         std::sort(v.begin(), v.end());
    //         if (!v.empty()) {
    //             size_t p50_idx = v.size() * 50 / 100;
    //             size_t p95_idx = std::min(v.size() - 1, v.size() * 95 / 100);
    //             size_t p50 = v[p50_idx];
    //             size_t p95 = v[p95_idx];
    //             s << "api_latency_p50_ms " << p50 << "\n";
    //             s << "api_latency_p95_ms " << p95 << "\n";
    //         }
    //         size_t p50 = v[v.size() * 50 / 100];
    //         size_t p95 = v[std::min(v.size() - 1, v.size() * 95 / 100)];
    //         s << "api_latency_p50_ms " << p50 << "\n";
    //         s << "api_latency_p95_ms " << p95 << "\n";
    //     }
    //     return crow::response(200, s.str());
    // });
    //
    // CROW_ROUTE(app, "/transactions").methods("POST"_method)([](const crow::request& req) {
    //     //time_metric
    //     auto start = std::chrono::steady_clock::now();
    //
    //     api_requests.fetch_add(1);
    //     crow::json::rvalue body;
    //
    //     // transaction_body
    //     try {
    //         body = crow::json::load(req.body);
    //     }
    //     catch (...) {
    //         validation_errors.fetch_add(1);
    //         spdlog::warn(R"({"component":"ingest","level":"warn","msg":"json_parse_failed"})");
    //         crow::json::wvalue out; out["error"] = "invalid_json";
    //         return crow::response(400, out);
    //     }
    //     if (req.body.empty()) {
    //         validation_errors.fetch_add(1);
    //         return crow::response(400, R"({"error":"empty_body"})"_json);
    //     }
    //
    //     //required_checks
    //     if (!body.has("amount") || !body["amount"].isnum()) {
    //         validation_errors.fetch_add(1);
    //         crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "amount required and must be number";
    //         return crow::response(400, out);
    //     }
    //     double amount = body["amount"].d();
    //
    //     if (!(amount > 0 && amount <= 1e9)) {
    //         validation_errors.fetch_add(1);
    //         crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "amount out of bounds";
    //         return crow::response(400, out);
    //     }
    //     if (!body.has("from") || !body["from"].is_string() || !check_id_format(body["from"].s())) {
    //         validation_errors.fetch_add(1);
    //         crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "invalid from";
    //         return crow::response(400, out);
    //     }
    //     if (!body.has("to") || !body["to"].is_string() || !check_id_format(body["to"].s())) {
    //         validation_errors.fetch_add(1);
    //         crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "invalid to";
    //         return crow::response(400, out);
    //     }
    //     if (!body.has("timestamp") || !body["timestamp"].is<std::string>()) {
    //         validation_errors.fetch_add(1);
    //         crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "invalid timestamp";
    //         return crow::response(400, out);
    //     }
    //     // timestamp validation
    //     std::string ts_str = body["timestamp"].s();
    //     try {
    //         auto parsed_time = parse8601_full(ts_str);
    //
    //         auto now = sys_time<milliseconds>{ duration_cast<milliseconds>(system_clock::now().time_since_epoch()) };
    //         auto diff = parsed_time - now;
    //         auto abs_diff = std::chrono::abs(diff);
    //
    //         auto two_days = days(2);
    //         if (abs_diff > two_days) {
    //             validation_errors.fetch_add(1);
    //             crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] =
    //             "timestamp out of range (+-2 days)";
    //             return crow::response(400, out);
    //         }
    //     }
    //     catch (const std::runtime_error&) {
    //         validation_errors.fetch_add(1);
    //         crow::json::wvalue out; out["error"] = "validation_failed"; out["details"] = "invalid timestamp format";
    //         return crow::response(400, out);
    //     }
    //
    //     // correlation id
    //     std::string correlation;
    //     correlation = gen_correlation();
    //     body["correlation_id"] = correlation;
    //
    //     // idempotency
    //     auto idem_key = req.get_header_value("Idempotency-Key");
    //     if (!idem_key.empty()) {
    //         auto prev = IdempotencyStore::get().get(idem_key);
    //         if (prev.has_value()) {
    //             spdlog::info(R"({"component":"ingest","level":"info","correlation_id":"%s","msg":"idempotent_hit"})", correlation.c_str());
    //             return crow::response(200, prev.value());
    //         }
    //     }
    //     // sanitize description
    //     if (body.has("description") && body["description"].is_string()) {
    //         std::string d = body["description"].s();
    //         if (d.size() > 512) d = d.substr(0, 512);
    //         body["description"] = d;
    //     }
    //     // sanitize device_hash
    //     if (body.has("device_hash") && body["device_hash"].is_string()) {
    //         std::string d = body["device_hash"].s();
    //         if (d.size() > 512) d = d.substr(0, 512);
    //         body["device_hash"] = d;
    //     }
    //
    //     // publish in RabbitMQ
    //     bool ok = true; //is_published
    //     if (!ok) {
    //         enqueue_failures.fetch_add(1);
    //         spdlog::error(R"({"component":"ingest","level":"error","correlation_id":"%s","msg":"enqueue_failed"})", correlation.c_str());
    //         crow::json::wvalue out; out["error"] = "enqueue_failed"; out["details"] = "broker_unavailable_or_queue_full";
    //         return crow::response(503, out);
    //     }
    //
    //
    //     enqueued.fetch_add(1);
    //     crow::json::wvalue accepted;
    //     accepted["correlation_id"] = correlation;
    //     accepted["status"] = "accepted";
    //     std::string acceptedStr = crow::json::dump(accepted);
    //     if (!idem_key.empty()) IdempotencyStore::get().put(idem_key, acceptedStr);
    //
    //     auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    //     {
    //         std::lock_guard<std::mutex> g(lat_mu);
    //         latency_samples.push_back((uint64_t)dur);
    //         if (latency_samples.size() > 2000) latency_samples.erase(latency_samples.begin()); // bounded storage
    //     }
    //
    //     spdlog::info(R"({"component":"ingest","level":"info","correlation_id":"%s","msg":"enqueued","enqueue_ms":%lld})", correlation.c_str(), (long long)dur);
    //     return crow::response(202, acceptedStr);
    // });
}
