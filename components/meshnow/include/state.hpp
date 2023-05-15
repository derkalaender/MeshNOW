#pragma once

namespace meshnow {

class NodeState {
   public:
    explicit NodeState(bool is_root) : state_{StateEnum::STOPPED}, is_root_{is_root} {}

    void setStarted() {
        assert(state_ == StateEnum::STOPPED);
        state_ = StateEnum::STARTED;
    }

    void setStopped() {
        assert(state_ == StateEnum::STARTED);
        state_ = StateEnum::STOPPED;
    }

    void setConnected() {
        assert(state_ == StateEnum::STARTED);
        state_ = StateEnum::CONNECTED;
    }

    void setDisconnected() {
        assert(state_ == StateEnum::CONNECTED);
        state_ = StateEnum::STARTED;
    }

    void setRootReachable() {
        assert(state_ == StateEnum::CONNECTED);
        state_ = StateEnum::ROOT_REACHABLE;
    }

    void setRootUnreachable() {
        assert(state_ == StateEnum::ROOT_REACHABLE);
        state_ = StateEnum::CONNECTED;
    }

    bool isStarted() { return state_ != StateEnum::STOPPED; }

    bool isConnected() { return state_ == StateEnum::CONNECTED || state_ == StateEnum::ROOT_REACHABLE; }

    bool isRootReachable() { return state_ == StateEnum::ROOT_REACHABLE; }

    bool isRoot() const { return is_root_; }

   private:
    enum class StateEnum { STOPPED, STARTED, CONNECTED, ROOT_REACHABLE };

    StateEnum state_;

    bool is_root_;
};

}  // namespace meshnow