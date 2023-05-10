#include <unity.h>

#include "networking.hpp"

void test_dumb_payload(meshnow::packet::DumbPayload&& payload) {
    // serialize
    std::vector<uint8_t> buffer = meshnow::packet::Packet{payload}.serialize();
    // deserialize
    auto compare = meshnow::packet::Packet::deserialize(buffer);

    // check deserialization was successful and type matches
    TEST_ASSERT_TRUE(compare != nullptr);
    TEST_ASSERT_TRUE(compare->type() == payload.type());
}

TEST_CASE("still_alive", "[serialization matches]") { test_dumb_payload(meshnow::packet::StillAlivePayload{}); }

TEST_CASE("anyone_there", "[serialization matches]") { test_dumb_payload(meshnow::packet::AnyoneTherePayload{}); }

TEST_CASE("i_am_here", "[serialization matches]") { test_dumb_payload(meshnow::packet::IAmHerePayload{}); }

TEST_CASE("pls_connect", "[serialization matches]") { test_dumb_payload(meshnow::packet::PlsConnectPayload{}); }

TEST_CASE("welcome", "[serialization matches]") { test_dumb_payload(meshnow::packet::WelcomePayload{}); }

TEST_CASE("mesh_unreachable", "[serialization matches]") {
    test_dumb_payload(meshnow::packet::MeshUnreachablePayload{});
}

TEST_CASE("mesh_reachable", "[serialization matches]") { test_dumb_payload(meshnow::packet::MeshReachablePayload{}); }