#include "mellzi/protocol.h"
#include <cassert>
#include <iostream>
#include <cstring>

void test_packet_structure() {
    mellzi::protocol::Packet pkt{};
    assert(sizeof(pkt) == 132);
    assert(sizeof(pkt.command_id) == 4);
    assert(sizeof(pkt.payload) == 128);

    pkt.set_command(0x11010);
    assert(pkt.get_section() == mellzi::protocol::Section::Aux);
    assert(pkt.get_sub_command() == 0x1010);

    pkt.set_command(0x22012);
    assert(pkt.get_section() == mellzi::protocol::Section::Bak);
    assert(pkt.get_sub_command() == 0x2012);

    pkt.set_command(0x00012);
    assert(pkt.get_section() == mellzi::protocol::Section::CaptureOrConfig);
    assert(pkt.get_sub_command() == 0x0012);

    pkt.set_string("Hello Jetson");
    assert(pkt.as_string() == "Hello Jetson");

    std::cout << "[PASS] test_packet_structure\n";
}

void test_endian_conversion() {
    uint32_t val = 0x12345678;
    uint32_t swapped = mellzi::protocol::swap32(val);
    assert(swapped == 0x78563412);
    assert(mellzi::protocol::swap32(swapped) == val);

    std::cout << "[PASS] test_endian_conversion\n";
}

int main() {
    std::cout << "Running Protocol Unit Tests...\n";
    test_packet_structure();
    test_endian_conversion();
    std::cout << "All Protocol Tests Passed!\n";
    return 0;
}
