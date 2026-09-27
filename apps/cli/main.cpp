#include "mellzi/client.h"
#include <iostream>
#include <string>
#include <filesystem>

void print_usage() {
    std::cout << "Mellzi Command Line Tool (CLI) for Rayence / DaVinci FPD\n\n"
              << "Usage: mellzi_cli <command> [options]\n\n"
              << "Commands:\n"
              << "  info       Query detector device info, versions, and telemetry\n"
              << "  capture    Trigger and download single frame\n"
              << "  calibrate  Run full calibration cycle or apply existing calibration\n"
              << "\nOptions:\n"
              << "  --host <ip>          Detector IP (default: 127.0.0.1)\n"
              << "  --port-ctrl <port>   Control port (default: 20000)\n"
              << "  --port-frame <port>  Frame port (default: 20001)\n"
              << "  --out <file>         Output image file path (.raw, .pgm, .bmp)\n"
              << "  --calib-dir <path>   Calibration directory path (default: ./calib)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        print_usage();
        return 0;
    }

    std::string command = argv[1];
    std::string host = "127.0.0.1";
    uint16_t ctrl_port = 20000;
    uint16_t frame_port = 20001;
    std::string out_path = "output.bmp";
    std::string calib_dir = "./calib";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port-ctrl" && i + 1 < argc) ctrl_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--port-frame" && i + 1 < argc) frame_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--out" && i + 1 < argc) out_path = argv[++i];
        else if (arg == "--calib-dir" && i + 1 < argc) calib_dir = argv[++i];
    }

    mellzi::DetectorClient client;
    std::cout << "[CLI] Connecting to detector at " << host << ":" << ctrl_port << " / " << frame_port << "...\n";

    if (!client.connect(host, ctrl_port, frame_port)) {
        std::cerr << "[CLI] Error: Could not connect to detector!\n";
        return 1;
    }

    std::cout << "[CLI] Connected successfully.\n";

    if (command == "info") {
        std::string fw, fpga, main_v, dev_ip, dev_mac;
        uint32_t count = 0, batt_pct = 0, batt_mv = 0;
        int32_t rssi = 0;

        client.acquisition().query_version_firmware(fw);
        client.acquisition().query_version_fpga(fpga);
        client.acquisition().query_version_main(main_v);
        client.acquisition().query_device_ip(dev_ip);
        client.acquisition().query_device_mac(dev_mac);
        client.acquisition().query_acquisition_count(count);
        client.acquisition().query_battery(batt_pct, batt_mv);
        client.acquisition().query_wireless(rssi);

        std::cout << "--- Detector Information ---\n"
                  << "  Firmware Version:   " << fw << "\n"
                  << "  FPGA Version:       " << fpga << "\n"
                  << "  Main Daemon:        " << main_v << "\n"
                  << "  Detector IP:        " << dev_ip << "\n"
                  << "  Detector MAC:       " << dev_mac << "\n"
                  << "  Acquisition Count:  " << count << "\n"
                  << "  Battery:            " << batt_pct << "% (" << batt_mv << " mV)\n"
                  << "  WiFi Signal (RSSI): " << rssi << " dBm\n";
    } else if (command == "capture") {
        std::cout << "[CLI] Acquiring raw frame (" << client.settings().frame_width
                  << "x" << client.settings().frame_height << ")...\n";

        mellzi::image::Image frame;
        if (!client.acquire_raw_frame(frame)) {
            std::cerr << "[CLI] Failed to acquire frame from detector.\n";
            return 1;
        }

        auto stats = frame.compute_stats();
        std::cout << "[CLI] Frame acquired! Min: " << stats.min_val << ", Max: " << stats.max_val
                  << ", Mean: " << stats.mean << "\n";

        std::filesystem::path p(out_path);
        if (p.extension() == ".raw") {
            frame.save_raw(p);
        } else if (p.extension() == ".pgm") {
            frame.save_pgm(p);
        } else {
            frame.save_bmp(p);
        }
        std::cout << "[CLI] Image saved to " << out_path << "\n";
    } else if (command == "calibrate") {
        std::cout << "[CLI] Loading calibration files from " << calib_dir << "...\n";
        client.load_calibration_directory(calib_dir);

        std::cout << "[CLI] Acquiring calibrated frame...\n";
        mellzi::image::Image calib_frame;
        if (!client.acquire_calibrated_frame(calib_frame)) {
            std::cerr << "[CLI] Acquisition failed.\n";
            return 1;
        }

        calib_frame.save_bmp(out_path);
        std::cout << "[CLI] Calibrated frame saved to " << out_path << "\n";
    } else {
        std::cerr << "[CLI] Unknown command: " << command << "\n";
        print_usage();
        return 1;
    }

    return 0;
}
