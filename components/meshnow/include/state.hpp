#pragma once

#include <condition_variable>
#include <mutex>

namespace meshnow {

class NodeState {
   public:
    enum class StateEnum { STOPPED, STARTED, CONNECTED, ROOT_REACHABLE };

    explicit NodeState(bool is_root) : state_{StateEnum::STOPPED}, is_root_{is_root} {}

    void setState(StateEnum s) {
        {
            std::scoped_lock lock{mtx_};
            state_ = s;
        }
        cv_.notify_all();
    }

    [[nodiscard]] std::unique_lock<std::mutex> waitForState(StateEnum s) {
        std::unique_lock lock{mtx_};
        cv_.wait(lock, [&] {return state_ == s;});
        return lock;
    }

    [[nodiscard]] std::unique_lock<std::mutex> waitForState(StateEnum s, uint timeout_ms) {
        std::unique_lock lock{mtx_};
        cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]{return state_ == s;});
        return lock;
    }

    StateEnum getState() const { return state_; }

    bool isRoot() const { return is_root_; }

   private:
    StateEnum state_{StateEnum::STOPPED};

    bool is_root_;

    std::mutex mtx_{};

    std::condition_variable cv_{};
};

}  // namespace meshnow