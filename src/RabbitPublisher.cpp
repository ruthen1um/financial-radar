#include "RabbitPublisher.h"
#include <spdlog/spdlog.h>
#include <amqpcpp.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <thread>
#include <sstream>
#include <cstdlib>

using namespace std::chrono_literals;

// Implement the TCP handler methods
void RabbitPublisherTcpHandler::onConnected(AMQP::TcpConnection* connection) {
    (void)connection; // silence unused parameter warning
    spdlog::info(R"({"component":"publisher","level":"info","msg":"amqp_connected"})");
}

void RabbitPublisherTcpHandler::onClosed(AMQP::TcpConnection* connection) {
    (void)connection;
    spdlog::warn(R"({"component":"publisher","level":"warn","msg":"amqp_closed"})");
}

void RabbitPublisherTcpHandler::onError(AMQP::TcpConnection* connection, const char* message) {
    (void)connection;
    spdlog::error(R"({"component":"publisher","level":"error","msg":"amqp_error","error":"%s"})", message ? message : "");
}

void RabbitPublisherTcpHandler::onLost(AMQP::TcpConnection* connection) {
    (void)connection;
    spdlog::warn(R"({"component":"publisher","level":"warn","msg":"amqp_lost"})");
}

void RabbitPublisherTcpHandler::onReady(AMQP::TcpConnection* connection) {
    (void)connection;
    spdlog::info(R"({"component":"publisher","level":"info","msg":"amqp_ready"})");
}

// Now matches AMQP-CPP: returns bool and accepts SSL*
bool RabbitPublisherTcpHandler::onSecured(AMQP::TcpConnection* connection, const SSL* ssl) {
    (void)connection;
    (void)ssl;
    spdlog::info(R"({"component":"publisher","level":"info","msg":"amqp_secured"})");
    return true; // indicate success/continue
}

// implement pure-virtual monitor() (no-op)
void RabbitPublisherTcpHandler::monitor(AMQP::TcpConnection* connection, int fd, int flags) {
    (void)connection;
    (void)fd;
    (void)flags;
    // no-op: the default AMQP-CPP examples sometimes require a platform-specific monitor implementation.
    // If you need custom file-descriptor monitoring (e.g., integrating with an event loop), implement it here.
}

RabbitPublisher::RabbitPublisher(const std::string& addr,
    size_t max_queue,
    int max_retries,
    int number_of_workers)
    : _address(addr),
    _max_queue(max_queue),
    _max_retries(max_retries),
    _workers_count(std::max(1, number_of_workers)),
    _tcpHandler(std::make_unique<RabbitPublisherTcpHandler>()) // Initialize the handler
{
}

RabbitPublisher::~RabbitPublisher() {
    stop();
}

void RabbitPublisher::start() {
    bool expected = false;
    if (!_running.compare_exchange_strong(expected, true)) return;
    spdlog::info(R"({"component":"publisher","level":"info","msg":"starting","addr":"%s","workers":%d})",
        _address.c_str(), _workers_count);

    // spawn worker threads
    _workers.reserve(_workers_count);
    for (int i = 0; i < _workers_count; ++i) {
        _workers.emplace_back(&RabbitPublisher::worker_loop, this, i);
    }
}

void RabbitPublisher::stop() {
    bool expected = true;
    if (!_running.compare_exchange_strong(expected, false)) return;

    // notify all workers and join
    {
        std::lock_guard<std::mutex> l(_mu);
        _cv.notify_all();
    }
    for (auto& t : _workers) {
        if (t.joinable()) t.join();
    }
    _workers.clear();

    // persist remaining queue to DLQ to avoid silent loss
    std::lock_guard<std::mutex> l(_mu);
    while (!_queue.empty()) {
        persist_dlq(_queue.front(), "shutdown");
        _dlq.fetch_add(1);
        _queue.pop_front();
    }

    spdlog::info(R"({"component":"publisher","level":"info","msg":"stopped"})");
}

bool RabbitPublisher::publish(const std::string& payload, const std::string& routingKey) {
    std::unique_lock<std::mutex> l(_mu);
    if (_queue.size() >= _max_queue) {
        _publish_failures.fetch_add(1); // record attempt failed due to queue full
        spdlog::warn(R"({"component":"publisher","level":"warn","msg":"enqueue_rejected","reason":"queue_full","max_queue":%zu,"current":%zu})",
            _max_queue, _queue.size());
        return false;
    }
    QueueItem it;
    it.payload = payload;
    it.routing = routingKey;
    it.attempts = 0;
    it.next_try = std::chrono::steady_clock::now();
    _queue.emplace_back(std::move(it));
    _enqueued.fetch_add(1);
    spdlog::info(R"({"component":"publisher","level":"debug","msg":"enqueued","routing":"%s","queue_size":%zu})",
        routingKey.c_str(), _queue.size());
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
    catch (...) {
        // never throw
    }
}

void RabbitPublisher::worker_loop(int worker_id) {
    // Each worker keeps its own connection+channel to avoid channel thread-safety issues
    std::unique_ptr<AMQP::TcpConnection> connection;
    std::unique_ptr<AMQP::TcpChannel> channel;

    auto workerTcpHandler = std::make_unique<RabbitPublisherTcpHandler>();

    int backoff_ms = 1000;

    while (_running.load()) {
        // Ensure connection for this worker
        if (!channel || !connection) {
            try {
                AMQP::Address addr(_address);
                // Use the handler instance instead of 'this'
                connection.reset(new AMQP::TcpConnection(workerTcpHandler.get(), addr));
                channel.reset(new AMQP::TcpChannel(connection.get()));
                try { channel->confirmSelect(); }
                catch (...) {}
                spdlog::info(R"({"component":"publisher","level":"info","msg":"worker_connected","id":%d})", worker_id);
                backoff_ms = 1000;
            }
            catch (const std::exception& e) {
                spdlog::error(R"({"component":"publisher","level":"error","msg":"worker_connect_failed","id":%d,"error":"%s"})", worker_id, e.what());
                connection.reset(); channel.reset();
                // backoff sleep with early exit if shutting down
                for (int slept = 0; slept < backoff_ms && _running.load(); slept += 200) std::this_thread::sleep_for(200ms);
                backoff_ms = std::min(60000, backoff_ms * 2);
                continue;
            }
        }

        // Pop next ready item
        QueueItem item;
        {
            std::unique_lock<std::mutex> l(_mu);
            // wait until item available or shutdown
            while (_queue.empty() && _running.load()) {
                _cv.wait_for(l, 500ms);
            }
            if (!_running.load()) break;
            // find first ready item (next_try <= now)
            auto now = std::chrono::steady_clock::now();
            bool found = false;
            for (auto it = _queue.begin(); it != _queue.end(); ++it) {
                if (it->next_try <= now) {
                    item = std::move(*it);
                    _queue.erase(it);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // nothing ready yet -> continue to wait
                continue;
            }
        } // unlock

        // try publish
        try {
            if (!channel) throw std::runtime_error("channel_not_ready");
            channel->publish("", item.routing.empty() ? "transactions" : item.routing, item.payload);
            _published.fetch_add(1);
            spdlog::info(R"({"component":"publisher","level":"info","msg":"published","worker":%d,"routing":"%s","attempts":%d})",
                worker_id, item.routing.c_str(), item.attempts);
        }
        catch (const std::exception& e) {
            item.attempts += 1;
            _publish_failures.fetch_add(1);
            spdlog::error(R"({"component":"publisher","level":"error","msg":"publish_failed","worker":%d,"error":"%s","attempts":%d})",
                worker_id, e.what(), item.attempts);

            if (item.attempts > _max_retries) {
                _dlq.fetch_add(1);
                persist_dlq(item, e.what());
                spdlog::error(R"({"component":"publisher","level":"error","msg":"moved_to_dlq","worker":%d,"routing":"%s","attempts":%d})",
                    worker_id, item.routing.c_str(), item.attempts);
            }
            else {
                // exponential backoff for this item
                int base_ms = 100;
                int delay_ms = base_ms * (1 << (std::min(item.attempts - 1, 10)));
                item.next_try = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                // push to tail so other items can proceed
                {
                    std::lock_guard<std::mutex> l(_mu);
                    _queue.emplace_back(std::move(item));
                }
                // small sleep to avoid hot loop
                std::this_thread::sleep_for(10ms);
            }
            // If the channel/connection is broken, drop them so reconnect happens next loop

            // For safety, reset channel/connection on error
            try { channel.reset(); connection.reset(); }
            catch (...) {}
        }
        catch (...) {
            item.attempts += 1;
            _publish_failures.fetch_add(1);
            spdlog::error(R"({"component":"publisher","level":"error","msg":"publish_unknown_failure","worker":%d,"attempts":%d})", worker_id, item.attempts);
            if (item.attempts > _max_retries) {
                _dlq.fetch_add(1);
                persist_dlq(item, "unknown");
            }
            else {
                int base_ms = 100;
                int delay_ms = base_ms * (1 << (std::min(item.attempts - 1, 10)));
                item.next_try = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
                std::lock_guard<std::mutex> l(_mu);
                _queue.emplace_back(std::move(item));
            }
            try { channel.reset(); connection.reset(); }
            catch (...) {}
        }
    } // while running

    // worker shutting down: cleanup
    try { channel.reset(); connection.reset(); }
    catch (...) {}
}
