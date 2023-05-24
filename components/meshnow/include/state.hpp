#pragma once

#include <condition_variable>
#include <mutex>

namespace meshnow {

class NodeState {
   public:
    explicit NodeState(bool is_root) : root_{is_root} {}

    [[nodiscard]] std::unique_lock<std::mutex> acquireLock() { return std::unique_lock{mtx_}; }

    void setStarted(bool started) {
        started_ = started;
        if (!started_) {
            connected_ = false;
            root_reachable_ = false;
        }
        cv_.notify_all();
    }

    bool isStarted() const { return started_; }

    void waitForStarted(std::unique_lock<std::mutex>& lock) {
        cv_.wait(lock, [&] { return started_; });
    }

    void setConnected(bool connected) {
        connected_ = connected;
        if (connected_) {
            started_ = true;
        } else {
            root_reachable_ = false;
        }
        cv_.notify_all();
    }

    bool isConnected() const { return connected_; }

    void waitForConnected(std::unique_lock<std::mutex>& lock) {
        cv_.wait(lock, [&] { return connected_; });
    }

    void waitForDisconnected(std::unique_lock<std::mutex>& lock) {
        cv_.wait(lock, [&] { return started_ && !connected_; });
    }

    void setRootReachable(bool reachable) {
        root_reachable_ = reachable;
        if (root_reachable_) {
            started_ = true;
            connected_ = true;
        }
        cv_.notify_all();
    }

    bool isRootReachable() const { return root_reachable_; }

    void waitForRootReachable(std::unique_lock<std::mutex>& lock) {
        cv_.wait(lock, [&] { return root_reachable_; });
    }

    bool isRoot() const { return root_; }

   private:
    bool root_;
    bool started_{false};
    bool connected_{false};
    bool root_reachable_{false};

    std::mutex mtx_{};

    std::condition_variable cv_{};
};

}  // namespace meshnow