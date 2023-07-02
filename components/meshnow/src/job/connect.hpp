#pragma once

#include <esp_event.h>

#include <memory>
#include <vector>

#include "event_internal.hpp"
#include "job.hpp"
#include "util/event.hpp"
#include "util/mac.hpp"

namespace meshnow::job {

class ConnectJob : public Job {
   public:
    ConnectJob();

    TickType_t nextActionAt() const noexcept override;

    void performAction() override;

   private:
    struct ChannelConfig {
        uint8_t min_channel;
        uint8_t max_channel;
    };

    struct ParentInfo {
        util::MacAddr mac_addr;
        int rssi{0};
    };

    class Phase {
       public:
        explicit Phase(ConnectJob& job) : job_(job) {}

        virtual ~Phase() = default;

        virtual TickType_t nextActionAt() const noexcept = 0;
        virtual void performAction() = 0;

       protected:
        ConnectJob& job_;
    };

    class SearchPhase : public Phase {
       public:
        static void event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                  void* event_data);

        using Phase::Phase;

        TickType_t nextActionAt() const noexcept override;
        void performAction() override;

       private:
        /**
         * Sends a beacon to search for nearby parents willing to accept this node as a child.
         */
        static void sendSearchProbe();

        util::EventHandlerInstance event_handler_instance_{event::getEventHandle(), event::MESHNOW_INTERNAL,
                                                           event::InternalEvent::PARENT_FOUND,
                                                           &SearchPhase::event_handler, this};

        /**
         * Ticks since the first parent was found while searching.
         */
        TickType_t first_parent_found_time_{0};

        /**
         * Ticks since the last search probe was sent.
         */
        TickType_t last_search_probe_time_{0};

        /**
         * How many search probes have been sent on the current channel.
         */
        uint search_probes_sent_{0};

        /**
         * The current channel we are searching on.
         */
        uint8_t current_channel_{0};
    };

    class ConnectPhase : public Phase {
       public:
        using Phase::Phase;

        TickType_t nextActionAt() const noexcept override;
        void performAction() override;

       private:
        /**
         * Sends a connect request to a potential parent.
         * @param to_mac MAC address of the parent
         */
        static void sendConnectRequest(const util::MacAddr& to_mac);

        /**
         * Ticks since the last connect request was sent.
         */
        TickType_t last_connect_request_time_{0};
    };

    const ChannelConfig channel_config_;
    std::vector<ParentInfo> parent_infos_;
    std::unique_ptr<Phase> phase_;
};

}  // namespace meshnow::job