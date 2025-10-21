#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <cstdint>
#include <chrono>
#include <memory>
#include <SimpleAmqpClient/SimpleAmqpClient.h>

class RabbitPublisher {
public:
    RabbitPublisher(const std::string& addr,
        size_t max_queue = 50000,
        int max_retries = 5,
        int number_of_workers = 1);
    ~RabbitPublisher();

    RabbitPublisher(const RabbitPublisher&) = delete;
    RabbitPublisher& operator=(const RabbitPublisher&) = delete;

    void start();
    void stop();

    bool publish(const std::string& payload, const std::string& routingKey = "");

    uint64_t published_total() const noexcept;
    uint64_t publish_failures_total() const noexcept;
    uint64_t enqueued_total() const noexcept;
    uint64_t dlq_total() const noexcept;
    size_t queue_size() const noexcept;

private:
    struct QueueItem {
        std::string payload;
        std::string routing;
        int attempts = 0;
        std::chrono::steady_clock::time_point next_try = std::chrono::steady_clock::now();
    };

    void worker_loop(int worker_id);
    void persist_dlq(const QueueItem& it, const std::string& reason);

    // Parse AMQP URL
    struct ConnectionParams {
        std::string host = "localhost";
        int port = 5672;
        std::string vhost = "/";
        std::string username = "guest";
        std::string password = "guest";
    };
    ConnectionParams parse_url(const std::string& url);

    std::string _address;
    std::atomic<bool> _running{ false };
    mutable std::mutex _mu;
    std::condition_variable _cv;
    std::deque<QueueItem> _queue;
    size_t _max_queue;
    int _max_retries;
    int _workers_count;
    std::vector<std::thread> _workers;

    std::atomic<uint64_t> _published{ 0 };
    std::atomic<uint64_t> _publish_failures{ 0 };
    std::atomic<uint64_t> _enqueued{ 0 };
    std::atomic<uint64_t> _dlq{ 0 };

    std::mutex _reconnect_mu;
    std::chrono::steady_clock::time_point _last_reconnect{ std::chrono::steady_clock::now() };
};