#include "mellzi/client.h"
#include "mellzi/server.h"
#include "mellzi/api.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "========================================================\n";
    std::cout << " Mellzi SDK & Jetson Server End-to-End C++20 Demo      \n";
    std::cout << "========================================================\n\n";

    // 1. Launch Server in background
    std::cout << "[Step 1] Starting internal Mellzi Jetson Server daemon...\n";
    mellzi::server::ServerConfig srv_cfg{};
    srv_cfg.control_port = 20000;
    srv_cfg.frame_port = 20001;
    srv_cfg.frame_width = 3328;
    srv_cfg.frame_height = 3328;
    srv_cfg.verbose_log = false; // keep demo output clean

    mellzi::server::JetsonServer server(srv_cfg);
    if (!server.start()) {
        std::cerr << "Failed to start server.\n";
        return 1;
    }
    std::cout << "  Server is active on ports 20000 (control) and 20001 (frame).\n\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 2. High-Level C++20 SDK Connection
    std::cout << "[Step 2] Connecting via C++20 DetectorClient...\n";
    mellzi::DetectorClient client;
    if (!client.connect("127.0.0.1", 20000, 20001)) {
        std::cerr << "Connection failed!\n";
        return 1;
    }
    std::cout << "  Connected successfully!\n\n";

    // 3. Query Device Versions & Telemetry
    std::cout << "[Step 3] Querying Detector Status and Telemetry...\n";
    std::string fw, fpga, main_v;
    client.acquisition().query_version_firmware(fw);
    client.acquisition().query_version_fpga(fpga);
    client.acquisition().query_version_main(main_v);

    uint32_t batt_pct = 0, batt_mv = 0;
    client.acquisition().query_battery(batt_pct, batt_mv);

    int32_t rssi = 0;
    client.acquisition().query_wireless(rssi);

    std::cout << "  Firmware: " << fw << "\n"
              << "  FPGA:     " << fpga << "\n"
              << "  Main:     " << main_v << "\n"
              << "  Battery:  " << batt_pct << "% (" << batt_mv << " mV)\n"
              << "  RSSI:     " << rssi << " dBm\n\n";

    // 4. Acquire Raw Frame
    std::cout << "[Step 4] Acquiring raw frame from detector...\n";
    auto t_start = std::chrono::high_resolution_clock::now();
    mellzi::image::Image raw_frame;
    if (!client.acquire_raw_frame(raw_frame)) {
        std::cerr << "Frame acquisition failed!\n";
        return 1;
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

    auto stats = raw_frame.compute_stats();
    std::cout << "  Frame acquired in " << ms << " ms (" << raw_frame.byte_size() << " bytes)\n"
              << "  Dimensions: " << raw_frame.width() << " x " << raw_frame.height() << "\n"
              << "  Min LSB:    " << stats.min_val << "\n"
              << "  Max LSB:    " << stats.max_val << "\n"
              << "  Mean:       " << stats.mean << "\n\n";

    // 5. Test Calibration Pipeline
    std::cout << "[Step 5] Running Calibration Pipeline...\n";
    mellzi::image::Image dark_frame(3328, 3328, 1200);
    client.calibration().set_dark_frame(dark_frame);

    auto calib_frame = client.calibration().process(raw_frame, mellzi::calib::CALIB_ALL);
    auto calib_stats = calib_frame.compute_stats();
    std::cout << "  Calibrated image dimensions: " << calib_frame.width() << " x " << calib_frame.height() << "\n"
              << "  Calibrated Min: " << calib_stats.min_val << ", Max: " << calib_stats.max_val
              << ", Mean: " << calib_stats.mean << "\n\n";

    // Save demo output
    calib_frame.save_bmp("demo_calibrated.bmp");
    std::cout << "  Saved calibrated output to 'demo_calibrated.bmp'\n\n";

    // 6. Test Legacy Rayence C ABI Compatibility
    std::cout << "[Step 6] Testing Legacy Rayence VADAV C ABI drop-in functions...\n";
    VDACQ_Connect("127.0.0.1", 20000);
    int check_conn = VDACQ_CheckConnection();
    std::cout << "  VDACQ_CheckConnection(): " << (check_conn == MELLZI_OK ? "OK" : "ERROR") << "\n";

    int w = 0, h = 0;
    VDACQ_GetFrameDim(&w, &h);
    std::cout << "  VDACQ_GetFrameDim(): " << w << " x " << h << "\n";

    char info[256]{0};
    VDACQ_GetDetectorInfo(info, sizeof(info));
    std::cout << "  VDACQ_GetDetectorInfo(): " << info << "\n\n";

    VDACQ_Close();

    // 7. Cleanup
    std::cout << "[Step 7] Disconnecting and stopping server...\n";
    client.disconnect();
    server.stop();
    std::cout << "Demo finished successfully!\n";

    return 0;
}
