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

#include <amqpcpp.h>
#include <amqpcpp/linux_tcp.h>
#include <openssl/ssl.h> // for SSL*

// Define the TCP handler class that inherits from AMQP::TcpHandler
class RabbitPublisherTcpHandler : public AMQP::TcpHandler {
public:
    RabbitPublisherTcpHandler() = default;
    virtual ~RabbitPublisherTcpHandler() = default;

    // Implement the required virtual methods from AMQP::TcpHandler
    virtual void onConnected(AMQP::TcpConnection* connection) override;
    virtual void onClosed(AMQP::TcpConnection* connection) override;
    virtual void onError(AMQP::TcpConnection* connection, const char* message) override;
    virtual void onLost(AMQP::TcpConnection* connection) override;
    virtual void onReady(AMQP::TcpConnection* connection) override;

    // Match AMQP-CPP signature: returns bool and receives SSL*
    virtual bool onSecured(AMQP::TcpConnection* connection, const SSL* ssl) override;

    // implement pure virtual monitor()
    virtual void monitor(AMQP::TcpConnection* connection, int fd, int flags) override;
};

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

    // push payload; returns false when queue is full
    bool publish(const std::string& payload, const std::string& routingKey = "");

    // metrics getters
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

    std::string _component = "publisher";

    // Add a member for the TCP handler
    std::unique_ptr<RabbitPublisherTcpHandler> _tcpHandler;
};
