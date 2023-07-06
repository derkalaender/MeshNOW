#pragma once

#include <esp_event.h>
#include <esp_random.h>

#include <optional>
#include <variant>
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

    /**
     * Searches for potential parents by performing an all-channel scan
     */
    class SearchPhase {
       public:
        TickType_t nextActionAt() const noexcept;
        void performAction(ConnectJob& job);

        void event_handler(ConnectJob& job, int32_t event_id, void* event_data);

       private:
        /**
         * Sends a beacon to search for nearby parents willing to accept this node as a child.
         */
        static void sendSearchProbe();

        /**
         * If any probe has been sent yet
         */
        bool started_{false};

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

    /**
     * Tries to connect to the best available parent.
     */
    class ConnectPhase {
       public:
        TickType_t nextActionAt() const noexcept;
        void performAction(ConnectJob& job);

        void event_handler(ConnectJob& job, int32_t event_id, void* event_data);

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

        /**
         * If currently awaiting a connect response.
         */
        bool awaiting_connect_response_{false};

        /**
         * The MAC address of the parent we are currently trying to connect to.
         */
        util::MacAddr current_parent_mac_;
    };

    /**
     * Sends a request request upstream so that all nodes to the root know of this node's presence.
     */
    class ResetPhase {
       public:
        TickType_t nextActionAt() const noexcept;
        void performAction(ConnectJob& job);

        void event_handler(ConnectJob& job, int32_t event_id, void* event_data) const;

       private:
        static void sendResetRequest(uint32_t id);

        bool reset_sent{false};
        TickType_t reset_sent_time_{0};
        uint32_t reset_id_{esp_random()};
    };

    /**
     * Idle phase, only reacts to state change when disconnecting from the parent.
     */
    class DonePhase {
       public:
        TickType_t nextActionAt() const noexcept;
        void performAction(ConnectJob& job);

        void event_handler(ConnectJob& job, int32_t event_id, void* event_data);
    };

    using Phase = std::variant<SearchPhase, ConnectPhase, ResetPhase, DonePhase>;

    static void event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    util::EventHandlerInstance event_handler_instance_{event::getEventHandle(), event::MESHNOW_INTERNAL,
                                                       ESP_EVENT_ANY_ID, &ConnectJob::event_handler, this};

    const ChannelConfig channel_config_;
    std::vector<ParentInfo> parent_infos_;
    // starts per default with SearchPhase
    Phase phase_{SearchPhase{}};
};

}  // namespace meshnow::job