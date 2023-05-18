#include <esp_log.h>
#include <unity.h>

#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include "constants.hpp"
#include "networking.hpp"
#include "packets.hpp"

const char* TAG = "test_packets";

/**
 * Serializes and deserializes the given payload, performs type checks and returns the deserialized payload for further
 * testing.
 */
std::unique_ptr<meshnow::packets::BasePayload> basic_check(const meshnow::packets::BasePayload& payload) {
    // serialize
    std::vector<uint8_t> buffer = meshnow::packets::Packet{payload}.serialize();
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer.data(), buffer.size(), ESP_LOG_INFO);
    // deserialize
    auto compare = meshnow::packets::Packet::deserialize(buffer);

    // check deserialization was successful and type matches
    TEST_ASSERT_NOT_NULL(compare);
    TEST_ASSERT_MESSAGE(compare->type() == payload.type(), "Type mismatch");

    return compare;
}

TEST_CASE("still_alive", "[serialization matches]") { basic_check(meshnow::packets::StillAlivePayload{}); }

TEST_CASE("anyone_there", "[serialization matches]") { basic_check(meshnow::packets::AnyoneTherePayload{}); }

TEST_CASE("i_am_here", "[serialization matches]") { basic_check(meshnow::packets::IAmHerePayload{}); }

TEST_CASE("pls_connect", "[serialization matches]") { basic_check(meshnow::packets::PlsConnectPayload{}); }

TEST_CASE("welcome", "[serialization matches]") { basic_check(meshnow::packets::WelcomePayload{}); }

TEST_CASE("node_connected", "[serialization matches]") {
    meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};

    meshnow::packets::NodeConnectedPayload payload{mac_addr};

    auto compare = static_cast<meshnow::packets::NodeConnectedPayload&>(*basic_check(payload));

    TEST_ASSERT(compare.connected_to_ == mac_addr);
}

TEST_CASE("node_disconnected", "[serialization matches]") {
    meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};

    meshnow::packets::NodeConnectedPayload payload{mac_addr};

    auto compare = static_cast<meshnow::packets::NodeDisconnectedPayload&>(*basic_check(payload));

    TEST_ASSERT(compare.disconnected_from_ == mac_addr);
}

TEST_CASE("data_ack", "[serialization matches]") {
    meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};
    uint16_t seq_num = 1234;

    meshnow::packets::DataAckPayload payload{mac_addr, seq_num};
    auto compare = static_cast<meshnow::packets::DataAckPayload&>(*basic_check(payload));

    TEST_ASSERT(compare.target_ == mac_addr);
    TEST_ASSERT(compare.seq_num_ == seq_num);
}

TEST_CASE("data_nack", "[serialization matches]") {
    meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};
    uint16_t seq_num = 1234;

    meshnow::packets::DataNackPayload payload{mac_addr, seq_num};
    auto compare = static_cast<meshnow::packets::DataNackPayload&>(*basic_check(payload));

    TEST_ASSERT(compare.target_ == mac_addr);
    TEST_ASSERT(compare.seq_num_ == seq_num);
}

TEST_CASE("data_first", "[serialization matches]") {
    // test both custom and lwip data
    bool custom = false;

    do {
        meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};
        uint16_t seq_num = 1234;
        uint16_t len = meshnow::MAX_DATA_TOTAL_SIZE;
        // data vector consists of increasing numbers
        std::vector<uint8_t> data(meshnow::MAX_DATA_FIRST_SIZE);
        std::iota(data.begin(), data.end(), 0);

        meshnow::packets::DataFirstPayload payload{mac_addr, seq_num, len, custom, data};
        auto compare = static_cast<meshnow::packets::DataFirstPayload&>(*basic_check(payload));

        TEST_ASSERT(compare.target_ == mac_addr);
        TEST_ASSERT(compare.seq_num_ == seq_num);
        TEST_ASSERT(compare.len_ == len);
        TEST_ASSERT(compare.custom_ == custom);
        TEST_ASSERT(compare.data_ == data);

        custom = !custom;
    } while (custom);
}

TEST_CASE("data_next", "[serialization matches]") {
    // test both custom and lwip data
    bool custom = false;

    do {
        meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};
        uint16_t seq_num = 1234;
        uint8_t frag_num = 5;
        // data vector consists of increasing numbers
        std::vector<uint8_t> data(meshnow::MAX_DATA_NEXT_SIZE);
        std::iota(data.begin(), data.end(), 0);

        meshnow::packets::DataNextPayload payload{mac_addr, seq_num, frag_num, custom, data};
        auto compare = static_cast<meshnow::packets::DataNextPayload&>(*basic_check(payload));

        TEST_ASSERT(compare.target_ == mac_addr);
        TEST_ASSERT(compare.seq_num_ == seq_num);
        TEST_ASSERT(compare.frag_num_ == frag_num);
        TEST_ASSERT(compare.custom_ == custom);
        TEST_ASSERT(compare.data_ == data);

        custom = !custom;
    } while (custom);
}

TEST_CASE("mesh_unreachable", "[serialization matches]") { basic_check(meshnow::packets::MeshUnreachablePayload{}); }

TEST_CASE("mesh_reachable", "[serialization matches]") { basic_check(meshnow::packets::MeshReachablePayload{}); }
