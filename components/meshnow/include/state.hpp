#pragma once

#include <condition_variable>
#include <mutex>

namespace meshnow {

class NodeState {
   public:
    enum class ConnectionStatus { DISCONNECTED, CONNECTED, ROOT_REACHABLE };

    explicit NodeState(bool is_root) : is_root_{is_root} {}

    [[nodiscard]] std::unique_lock<std::mutex> acquireLock() { return std::unique_lock{mtx_}; }

    void setStarted(bool started) {
        is_started_ = started;
        cv_.notify_all();
    }

    bool isStarted() const { return is_started_; }

    void waitForStarted(std::unique_lock<std::mutex>& lock) {
        cv_.wait(lock, [&] { return is_started_; });
    }

    void setConnectionStatus(ConnectionStatus s) {
        connection_status = s;
        cv_.notify_all();
    }

    ConnectionStatus getConnectionStatus() const { return connection_status; }

    void waitForDisconnected(std::unique_lock<std::mutex>& lock) {
        cv_.wait(lock, [&] { return is_started_ && connection_status == ConnectionStatus::DISCONNECTED; });
    }

    void waitForConnected(std::unique_lock<std::mutex>& lock) {
        cv_.wait(lock, [&] {
            return is_started_ && (connection_status == ConnectionStatus::CONNECTED ||
                                   connection_status == ConnectionStatus::ROOT_REACHABLE);
        });
    }

    void waitForRootReachable(std::unique_lock<std::mutex>& lock) {
        cv_.wait(lock, [&] { return is_started_ && connection_status == ConnectionStatus::ROOT_REACHABLE; });
    }

    bool isRoot() const { return is_root_; }

   private:
    ConnectionStatus connection_status{ConnectionStatus::DISCONNECTED};

    bool is_root_;
    bool is_started_{false};

    std::mutex mtx_{};

    std::condition_variable cv_{};
};

}  // namespace meshnow