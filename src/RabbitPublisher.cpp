#include "RabbitPublisher.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <regex>

using namespace std::chrono_literals;
using namespace AmqpClient;

static std::string build_local_log(const std::string& component,
    const std::string& level,
    const std::string& msg,
    const std::vector<std::pair<std::string, std::string>>& kv = {})
{
    std::ostringstream o;
    o << "{\"component\":\"" << component << "\",";
    o << "\"level\":\"" << level << "\",";
    o << "\"msg\":\"" << msg << "\"";
    for (auto& p : kv) {
        o << ",\"" << p.first << "\":\"" << p.second << "\"";
    }
    o << "}";
    return o.str();
}

RabbitPublisher::RabbitPublisher(const std::string& addr,
    size_t max_queue,
    int max_retries,
    int number_of_workers)
    : _address(addr),
    _max_queue(max_queue),
    _max_retries(max_retries),
    _workers_count(std::max(1, number_of_workers))
{
}

RabbitPublisher::~RabbitPublisher() {
    stop();
}

void RabbitPublisher::start() {
    bool expected = false;
    if (!_running.compare_exchange_strong(expected, true)) return;

    auto start_msg = build_local_log("publisher", "info", "starting",
        { {"addr", _address}, {"workers", std::to_string(_workers_count)} });
    spdlog::info("{}", start_msg);

    _workers.reserve(_workers_count);
    for (int i = 0; i < _workers_count; ++i) {
        _workers.emplace_back(&RabbitPublisher::worker_loop, this, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + (i * 50)));
    }
}

void RabbitPublisher::stop() {
    bool expected = true;
    if (!_running.compare_exchange_strong(expected, false)) return;

    {
        std::lock_guard<std::mutex> l(_mu);
        _cv.notify_all();
    }
    for (auto& t : _workers) {
        if (t.joinable()) t.join();
    }
    _workers.clear();

    std::lock_guard<std::mutex> l(_mu);
    while (!_queue.empty()) {
        persist_dlq(_queue.front(), "shutdown");
        _dlq.fetch_add(1);
        _queue.pop_front();
    }

    spdlog::info("{}", build_local_log("publisher", "info", "stopped"));
}

bool RabbitPublisher::publish(const std::string& payload, const std::string& routingKey) {
    if (_publish_failures.load() > _published.load() * 0.5 && _published.load() > 100) {
        _publish_failures.fetch_add(1);
        spdlog::warn("Circuit breaker: too many failures, rejecting");
        return false;
    }

    std::unique_lock<std::mutex> l(_mu);
    if (_queue.size() >= _max_queue) {
        _publish_failures.fetch_add(1);
        auto warn_msg = build_local_log("publisher", "warn", "enqueue_rejected",
            { {"reason","queue_full"}, {"max_queue", std::to_string(_max_queue)} });
        spdlog::warn("{}", warn_msg);
        return false;
    }

    QueueItem it;
    it.payload = payload;
    it.routing = routingKey;
    it.attempts = 0;
    it.next_try = std::chrono::steady_clock::now();
    _queue.emplace_back(std::move(it));
    _enqueued.fetch_add(1);
    _cv.notify_one();
    return true;
}

uint64_t RabbitPublisher::published_total() const noexcept { return _published.load(); }
uint64_t RabbitPublisher::publish_failures_total() const noexcept { return _publish_failures.load(); }
uint64_t RabbitPublisher::enqueued_total() const noexcept { return _enqueued.load(); }
uint64_t RabbitPublisher::dlq_total() const noexcept { return _dlq.load(); }
size_t RabbitPublisher::queue_size() const noexcept {
    std::lock_guard<std::mutex> l(_mu);
    return _queue.size();
}

void RabbitPublisher::persist_dlq(const QueueItem& it, const std::string& reason) {
    try {
        std::string dir = std::getenv("DLQ_DIR") ? std::getenv("DLQ_DIR") : "logs";
        std::filesystem::create_directories(dir);
        std::string file = dir + "/dlq.log";
        std::ofstream f(file, std::ios::app);
        if (!f) return;
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        f << t << "," << it.routing << "," << it.attempts << "," << reason << ",";
        for (char c : it.payload) {
            if (c == '\n') f << "\\n";
            else if (c == '\r') f << "\\r";
            else f << c;
        }
        f << "\n";
    }
    catch (...) {}
}

RabbitPublisher::ConnectionParams RabbitPublisher::parse_url(const std::string& url) {
    ConnectionParams params;

    // amqp://user:pass@host:port/vhost
    std::regex re(R"(amqps?://([^:]+):([^@]+)@([^:]+):?(\d+)?(/.*)?)");;
    std::smatch match;

    if (std::regex_match(url, match, re)) {
        params.username = match[1].str();
        params.password = match[2].str();
        params.host = match[3].str();
        if (match[4].matched) {
            params.port = std::stoi(match[4].str());
        }
        if (match[5].matched) {
            params.vhost = match[5].str();
            if (params.vhost[0] == '/') params.vhost = params.vhost.substr(1);
            if (params.vhost.empty()) params.vhost = "/";
        }
    }

    return params;
}

void RabbitPublisher::worker_loop(int worker_id) {
    Channel::ptr_t channel;
    int backoff_ms = 1000;
    std::vector<QueueItem> batch;
    const size_t BATCH_SIZE = 10;

    while (_running.load()) {
        // Ensure connection
        if (!channel) {
            {
                std::lock_guard<std::mutex> rlock(_reconnect_mu);
                auto now = std::chrono::steady_clock::now();
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_reconnect);
                if (diff.count() < 100) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 - diff.count()));
                }
                _last_reconnect = now;
            }

            try {
                auto params = parse_url(_address);
                // Build OpenOpts using the library's API
                AmqpClient::Channel::OpenOpts opts;
                opts.host = params.host;
                opts.port = params.port;
                opts.vhost = params.vhost;
                opts.frame_max = 131072; // optional, typical default

                // pass username/password via the auth variant
                opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth{ params.username, params.password };

                channel = AmqpClient::Channel::Open(opts);
                // Declare queue
                channel->DeclareQueue("transactions", false, true, false, false);

                auto msg = build_local_log("publisher", "info", "worker_connected",
                    { {"id", std::to_string(worker_id)} });
                spdlog::info("{}", msg);
                backoff_ms = 1000;
            }
            catch (const std::exception& e) {
                auto err = build_local_log("publisher", "error", "worker_connect_failed",
                    { {"id", std::to_string(worker_id)}, {"error", e.what()} });
                spdlog::error("{}", err);

                channel.reset();
                for (int slept = 0; slept < backoff_ms && _running.load(); slept += 200)
                    std::this_thread::sleep_for(200ms);
                backoff_ms = std::min(60000, backoff_ms * 2);
                continue;
            }
        }

        // Collect batch
        {
            std::unique_lock<std::mutex> l(_mu);
            while (_queue.empty() && _running.load()) {
                _cv.wait_for(l, 100ms);
            }
            if (!_running.load()) break;

            auto now = std::chrono::steady_clock::now();
            for (auto it = _queue.begin(); it != _queue.end() && batch.size() < BATCH_SIZE;) {
                if (it->next_try <= now) {
                    batch.push_back(std::move(*it));
                    it = _queue.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        if (batch.empty()) {
            continue;
        }

        // Publish batch
        int failed_count = 0;
        for (auto& item : batch) {
            try {
                if (!channel) throw std::runtime_error("channel_not_ready");

                auto message = BasicMessage::Create(item.payload);
                message->DeliveryMode(BasicMessage::dm_persistent);

                std::string routing = item.routing.empty() ? "transactions" : item.routing;
                channel->BasicPublish("", routing, message);

                _published.fetch_add(1);
                auto okm = build_local_log("publisher", "info", "published", {
                    {"worker", std::to_string(worker_id)},
                    {"routing", item.routing},
                    {"attempts", std::to_string(item.attempts)}
                    });
                spdlog::info("{}", okm);
            }
            catch (const std::exception& e) {
                failed_count++;
                item.attempts += 1;
                _publish_failures.fetch_add(1);

                auto pf = build_local_log("publisher", "error", "publish_failed", {
                    {"worker", std::to_string(worker_id)},
                    {"error", e.what()},
                    {"attempts", std::to_string(item.attempts)}
                    });
                spdlog::error("{}", pf);

                if (item.attempts > _max_retries) {
                    _dlq.fetch_add(1);
                    persist_dlq(item, e.what());
                }
                else {
                    int base_ms = 100;
                    int delay_ms = base_ms * (1 << (std::min(item.attempts - 1, 10)));
                    item.next_try = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                    {
                        std::lock_guard<std::mutex> l(_mu);
                        _queue.emplace_back(std::move(item));
                    }
                }
            }
        }

        // Reset connection if too many failures
        if (static_cast<size_t>(failed_count) > batch.size() / 2) {
            try {
                channel.reset();
                auto msg = build_local_log("publisher", "warn", "connection_reset_due_to_failures", {
                    {"worker", std::to_string(worker_id)},
                    {"failed", std::to_string(failed_count)},
                    {"total", std::to_string(batch.size())}
                    });
                spdlog::warn("{}", msg);
            }
            catch (...) {}
        }

        batch.clear();
    }

    try { channel.reset(); }
    catch (...) {}
}