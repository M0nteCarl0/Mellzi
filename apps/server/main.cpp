#include "mellzi/server.h"
#include <iostream>
#include <csignal>
#include <string>
#include <thread>
#include <chrono>

namespace {
    mellzi::server::JetsonServer* g_server = nullptr;

    void signal_handler(int sig) {
        std::cout << "\n[Mellzi::Server] Received termination signal (" << sig << "), shutting down...\n";
        if (g_server) {
            g_server->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=====================================================\n";
    std::cout << " Mellzi DaVinci Detector Server for NVIDIA Jetson   \n";
    std::cout << " C++20 Cross-Platform Open Source Rayence Emulator  \n";
    std::cout << "=====================================================\n";

    mellzi::server::ServerConfig config{};

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port-ctrl" && i + 1 < argc) {
            config.control_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--port-frame" && i + 1 < argc) {
            config.frame_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--width" && i + 1 < argc) {
            config.frame_width = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            config.frame_height = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--pattern" && i + 1 < argc) {
            std::string pat = argv[++i];
            if (pat == "gradient") config.pattern = mellzi::server::SimulationPattern::Gradient;
            else if (pat == "checker") config.pattern = mellzi::server::SimulationPattern::Checkerboard;
            else if (pat == "flat") config.pattern = mellzi::server::SimulationPattern::FlatFieldNoise;
        } else if (arg == "--raw-template" && i + 1 < argc) {
            config.pattern = mellzi::server::SimulationPattern::LoadFromDisk;
            config.raw_frames_dir = argv[++i];
        } else if (arg == "--config-dir" && i + 1 < argc) {
            config.config_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: mellzi_server [options]\n"
                      << "  --port-ctrl <port>       Control port (default: 20000)\n"
                      << "  --port-frame <port>      Frame streaming port (default: 20001)\n"
                      << "  --width <pixels>         Frame width (default: 3328)\n"
                      << "  --height <pixels>        Frame height (default: 3328)\n"
                      << "  --pattern <name>         gradient | checker | flat (default: gradient)\n"
                      << "  --raw-template <file>    Use a real .raw frame as template\n"
                      << "  --config-dir <path>      Path to directory with VADav.INI / .initcfg\n"
                      << "  --help                   Display this help message\n";
            return 0;
        }
    }

    mellzi::server::JetsonServer server(config);
    g_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!server.start()) {
        std::cerr << "[Mellzi::Server] Error: Failed to start server daemon.\n";
        return 1;
    }

    std::cout << "[Mellzi::Server] Daemon is running. Press Ctrl+C to terminate.\n";
    server.wait_until_stopped();

    return 0;
}
