#include "mellzi/server.h"
#include "mellzi/acquisition.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

void test_server_client_roundtrip() {
    constexpr uint16_t CTRL_PORT = 21000;
    constexpr uint16_t FRAME_PORT = 21001;
    constexpr uint32_t W = 256;
    constexpr uint32_t H = 256;

    mellzi::server::ServerConfig srv_cfg{};
    srv_cfg.control_port = CTRL_PORT;
    srv_cfg.frame_port = FRAME_PORT;
    srv_cfg.frame_width = W;
    srv_cfg.frame_height = H;
    srv_cfg.verbose_log = false;

    mellzi::server::JetsonServer server(srv_cfg);
    assert(server.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    mellzi::client::AcquisitionClient client;
    client.set_frame_dimensions(W, H);
    assert(client.connect("127.0.0.1", CTRL_PORT, FRAME_PORT));
    assert(client.is_connected());

    // 1. Query versions
    std::string fw, fpga, main_v;
    assert(client.query_version_firmware(fw));
    assert(!fw.empty());
    assert(client.query_version_fpga(fpga));
    assert(!fpga.empty());
    assert(client.query_version_main(main_v));
    assert(!main_v.empty());

    // 2. Query battery
    uint32_t batt_pct = 0, batt_mv = 0;
    assert(client.query_battery(batt_pct, batt_mv));
    assert(batt_pct > 0 && batt_mv > 0);

    // 3. Test Config download
    uint8_t init_cfg[32]{0};
    assert(client.download_init_config(init_cfg));
    assert(init_cfg[15] == 0x01); // matches standard .initcfg signature

    // 4. Test frame acquisition
    mellzi::image::Image frame;
    assert(client.get_frame(frame));
    assert(frame.width() == W);
    assert(frame.height() == H);
    assert(frame.byte_size() == W * H * 2);

    client.disconnect();
    server.stop();

    std::cout << "[PASS] test_server_client_roundtrip\n";
}

int main() {
    std::cout << "Running Network Integration Tests...\n";
    test_server_client_roundtrip();
    std::cout << "All Network Tests Passed!\n";
    return 0;
}
