#include "mellzi/server.h"
#include <iostream>
#include <cstring>
#include <random>
#include <fstream>
#include <chrono>

namespace mellzi::server {

JetsonServer::JetsonServer(const ServerConfig& config)
    : config_(config), current_frame_(config.frame_width, config.frame_height) {
    init_params_.init_defaults();
    load_persistent_configs();
}

JetsonServer::~JetsonServer() {
    stop();
}

bool JetsonServer::start() {
    if (is_running_.load()) return true;

    ctrl_listener_ = net::TcpListener::bind(config_.control_port, config_.listen_address);
    if (!ctrl_listener_) {
        std::cerr << "[Mellzi::Server] Failed to bind control port " << config_.control_port << "\n";
        return false;
    }

    frame_listener_ = net::TcpListener::bind(config_.frame_port, config_.listen_address);
    if (!frame_listener_) {
        std::cerr << "[Mellzi::Server] Failed to bind frame port " << config_.frame_port << "\n";
        ctrl_listener_.reset();
        return false;
    }

    is_running_.store(true);

    if (config_.verbose_log) {
        std::cout << "[Mellzi::Server] Daemon started successfully on "
                  << config_.listen_address << "\n"
                  << "  Board: " << hardware_.get_board_name() << "\n"
                  << "  Control port: " << config_.control_port << " (TCP)\n"
                  << "  Frame port:   " << config_.frame_port << " (TCP)\n"
                  << "  Geometry:     " << config_.frame_width << " x " << config_.frame_height << " (16-bit)\n";
    }

    ctrl_thread_ = std::jthread([this](std::stop_token st) { ctrl_task_run(st); });
    frame_thread_ = std::jthread([this](std::stop_token st) { frame_task_run(st); });
    if (config_.enable_telemetry_broadcast) {
        telemetry_thread_ = std::jthread([this](std::stop_token st) { telemetry_task_run(st); });
    }

    return true;
}

void JetsonServer::stop() {
    if (!is_running_.exchange(false)) return;

    if (ctrl_thread_.joinable()) ctrl_thread_.request_stop();
    if (frame_thread_.joinable()) frame_thread_.request_stop();
    if (telemetry_thread_.joinable()) telemetry_thread_.request_stop();

    if (ctrl_listener_) ctrl_listener_->close();
    if (frame_listener_) frame_listener_->close();

    save_persistent_configs();

    if (config_.verbose_log) {
        std::cout << "[Mellzi::Server] Server stopped.\n";
    }
}

void JetsonServer::wait_until_stopped() {
    if (ctrl_thread_.joinable()) ctrl_thread_.join();
    if (frame_thread_.joinable()) frame_thread_.join();
    if (telemetry_thread_.joinable()) telemetry_thread_.join();
}

void JetsonServer::load_persistent_configs() {
    std::filesystem::path ini_p = config_.config_dir / "VADav.INI";
    if (std::filesystem::exists(ini_p)) {
        init_params_.load_from_ini(ini_p);
    }

    std::filesystem::path init_p = config_.config_dir / ".initcfg";
    if (std::filesystem::exists(init_p)) {
        std::ifstream f(init_p, std::ios::binary);
        if (f.is_open()) {
            f.read(reinterpret_cast<char*>(init_cfg_.data), 32);
        }
    }

    for (int i = 0; i < 3; ++i) {
        std::string fname = i == 0 ? ".elsetcfg" : (".elsetcfg" + std::to_string(i + 1));
        std::filesystem::path elset_p = config_.config_dir / fname;
        if (std::filesystem::exists(elset_p)) {
            std::ifstream f(elset_p, std::ios::binary);
            if (f.is_open()) {
                f.read(reinterpret_cast<char*>(elset_cfgs_[i].data), 128);
            }
        }
    }
}

void JetsonServer::save_persistent_configs() {
    std::filesystem::create_directories(config_.config_dir);
    init_params_.save_to_ini(config_.config_dir / "VADav.INI");

    std::ofstream f_init(config_.config_dir / ".initcfg", std::ios::binary);
    if (f_init.is_open()) {
        f_init.write(reinterpret_cast<const char*>(init_cfg_.data), 32);
    }

    for (int i = 0; i < 3; ++i) {
        std::string fname = i == 0 ? ".elsetcfg" : (".elsetcfg" + std::to_string(i + 1));
        std::ofstream f_elset(config_.config_dir / fname, std::ios::binary);
        if (f_elset.is_open()) {
            f_elset.write(reinterpret_cast<const char*>(elset_cfgs_[i].data), 128);
        }
    }
}

void JetsonServer::set_next_frame(image::Image frame) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    current_frame_ = std::move(frame);
    has_custom_frame_ = true;
}

void JetsonServer::set_raw_template(const std::filesystem::path& raw_path) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (current_frame_.load_raw(raw_path, config_.frame_width, config_.frame_height)) {
        has_custom_frame_ = true;
    }
}

void JetsonServer::generate_frame_data(image::Image& frame) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (has_custom_frame_ && !current_frame_.span().empty() &&
        current_frame_.width() == config_.frame_width &&
        current_frame_.height() == config_.frame_height) {
        frame = current_frame_;
        return;
    }

    frame.resize(config_.frame_width, config_.frame_height);
    auto sp = frame.span();

    static std::mt19937 rng(12345);
    std::normal_distribution<double> noise(0.0, 15.0);

    switch (config_.pattern) {
        case SimulationPattern::Gradient: {
            for (uint32_t y = 0; y < config_.frame_height; ++y) {
                for (uint32_t x = 0; x < config_.frame_width; ++x) {
                    double norm = static_cast<double>(x + y) / (config_.frame_width + config_.frame_height);
                    double val = 2000.0 + norm * 35000.0 + noise(rng);
                    sp[y * config_.frame_width + x] = static_cast<uint16_t>(std::clamp(val, 0.0, 65535.0));
                }
            }
            break;
        }
        case SimulationPattern::Checkerboard: {
            for (uint32_t y = 0; y < config_.frame_height; ++y) {
                for (uint32_t x = 0; x < config_.frame_width; ++x) {
                    bool white = ((x / 128) ^ (y / 128)) & 1;
                    double val = (white ? 30000.0 : 5000.0) + noise(rng);
                    sp[y * config_.frame_width + x] = static_cast<uint16_t>(std::clamp(val, 0.0, 65535.0));
                }
            }
            break;
        }
        case SimulationPattern::FlatFieldNoise:
        default: {
            for (size_t i = 0; i < sp.size(); ++i) {
                double val = 12000.0 + noise(rng);
                sp[i] = static_cast<uint16_t>(std::clamp(val, 0.0, 65535.0));
            }
            break;
        }
    }
}

void JetsonServer::ctrl_task_run(std::stop_token stop_token) {
    while (!stop_token.stop_requested() && is_running_.load()) {
        auto client = ctrl_listener_->accept(std::chrono::milliseconds(200));
        if (client) {
            if (config_.verbose_log) {
                std::cout << "[Mellzi::Server] Control connection accepted from "
                          << client->peer_address() << ":" << client->peer_port() << "\n";
            }
            std::thread([this, c = std::move(client), stop_token]() mutable {
                handle_ctrl_client(std::move(c), stop_token);
            }).detach();
        }
    }
}

void JetsonServer::handle_ctrl_client(std::unique_ptr<net::TcpStream> stream, std::stop_token stop_token) {
    stream->set_timeout(std::chrono::milliseconds(1000));
    stream->set_nodelay(true);

    while (!stop_token.stop_requested() && is_running_.load()) {
        protocol::Packet req{};
        std::span<uint8_t> req_span(reinterpret_cast<uint8_t*>(&req), protocol::PACKET_SIZE);

        if (!stream->receive_exact(req_span)) {
            break; // Client disconnected or error
        }

        protocol::Packet resp{};
        dispatch_command(req, resp, *stream);

        // If a response packet was populated, transmit it back
        if (resp.command_id != 0) {
            std::span<const uint8_t> resp_span(reinterpret_cast<const uint8_t*>(&resp), protocol::PACKET_SIZE);
            if (!stream->send_all(resp_span)) {
                break;
            }
        }
    }

    if (config_.verbose_log) {
        std::cout << "[Mellzi::Server] Control client disconnected.\n";
    }
}

void JetsonServer::dispatch_command(const protocol::Packet& req, protocol::Packet& resp,
                                   net::TcpStream& client_stream) {
    uint32_t cmd = req.command_id;
    auto sec = req.get_section();
    uint16_t sub = req.get_sub_command();
    (void)sub;

    switch (sec) {
        case protocol::Section::CaptureOrConfig:
            if (cmd <= 0x03 || cmd == 0x07 || cmd == 0x0B) {
                handle_capture_cmd(cmd, req, resp, client_stream);
            } else {
                handle_config_cmd(cmd, req, resp, client_stream);
            }
            break;
        case protocol::Section::Aux:
            handle_aux_cmd(cmd, req, resp, client_stream);
            break;
        case protocol::Section::Bak:
            handle_bak_cmd(cmd, req, resp, client_stream);
            break;
    }
}

void JetsonServer::handle_capture_cmd(uint32_t sub_cmd, const protocol::Packet& req,
                                     protocol::Packet& resp, net::TcpStream& client_stream) {
    (void)req;
    (void)client_stream;
    switch (static_cast<protocol::CaptureCmd>(sub_cmd)) {
        case protocol::CaptureCmd::ReadyIn:
            resp.set_command(static_cast<uint32_t>(protocol::CaptureCmd::InitialisationDone));
            break;
        case protocol::CaptureCmd::Start:
            is_acquiring_.store(true);
            resp.set_command(static_cast<uint32_t>(protocol::CaptureCmd::StartDone));
            break;
        case protocol::CaptureCmd::Abort:
            is_acquiring_.store(false);
            resp.set_command(static_cast<uint32_t>(protocol::CaptureCmd::AbortDone));
            break;
        case protocol::CaptureCmd::ReadyDoneClient:
            resp.set_command(static_cast<uint32_t>(protocol::CaptureCmd::ReadyDone));
            break;
        case protocol::CaptureCmd::Shutdown:
            resp.set_command(static_cast<uint32_t>(protocol::CaptureCmd::ShutdownDone));
            break;
        default:
            resp.set_command(sub_cmd);
            break;
    }
}

void JetsonServer::handle_config_cmd(uint32_t sub_cmd, const protocol::Packet& req,
                                    protocol::Packet& resp, net::TcpStream& client_stream) {
    (void)client_stream;
    switch (static_cast<protocol::ConfigCmd>(sub_cmd)) {
        case protocol::ConfigCmd::InitUpload:
            resp.set_command(static_cast<uint32_t>(protocol::ConfigCmd::InitUploadDone));
            std::copy_n(init_cfg_.data, 32, resp.payload);
            break;
        case protocol::ConfigCmd::ElsetUpload:
            resp.set_command(static_cast<uint32_t>(protocol::ConfigCmd::ElsetUploadDone));
            std::copy_n(elset_cfgs_[0].data, 128, resp.payload);
            break;
        case protocol::ConfigCmd::Elset2Upload:
            resp.set_command(static_cast<uint32_t>(protocol::ConfigCmd::Elset2UploadDone));
            std::copy_n(elset_cfgs_[1].data, 128, resp.payload);
            break;
        case protocol::ConfigCmd::Elset3Upload:
            resp.set_command(static_cast<uint32_t>(protocol::ConfigCmd::Elset3UploadDone));
            std::copy_n(elset_cfgs_[2].data, 128, resp.payload);
            break;
        case protocol::ConfigCmd::InitSave:
            std::copy_n(req.payload, 32, init_cfg_.data);
            resp.set_command(static_cast<uint32_t>(protocol::ConfigCmd::SaveSuccess));
            break;
        case protocol::ConfigCmd::ElsetSave:
            std::copy_n(req.payload, 128, elset_cfgs_[0].data);
            resp.set_command(static_cast<uint32_t>(protocol::ConfigCmd::SaveSuccess));
            break;
        case protocol::ConfigCmd::Elset2Save:
            std::copy_n(req.payload, 128, elset_cfgs_[1].data);
            resp.set_command(static_cast<uint32_t>(protocol::ConfigCmd::SaveSuccess));
            break;
        case protocol::ConfigCmd::Elset3Save:
            std::copy_n(req.payload, 128, elset_cfgs_[2].data);
            resp.set_command(static_cast<uint32_t>(protocol::ConfigCmd::SaveSuccess));
            break;
        default:
            resp.set_command(sub_cmd);
            break;
    }
}

void JetsonServer::handle_aux_cmd(uint32_t sub_cmd, const protocol::Packet& req,
                                 protocol::Packet& resp, net::TcpStream& client_stream) {
    (void)req;
    (void)client_stream;
    switch (static_cast<protocol::AuxCmd>(sub_cmd)) {
        case protocol::AuxCmd::ReqDefStat:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespDefStat));
            break;
        case protocol::AuxCmd::ReqLedStat:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespLedStat));
            break;
        case protocol::AuxCmd::ReqVerFirmware:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespVerFirmware));
            resp.set_string("Mellzi Jetson FW v3.6.2026-ARM64");
            break;
        case protocol::AuxCmd::ReqVerFpga:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespVerFpga));
            resp.set_string("FPGA-JETSON-1717SCC-v1.4");
            break;
        case protocol::AuxCmd::ReqVerMain:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespVerMain));
            resp.set_string("Mellzi-Server-C++20-Daemon");
            break;
        case protocol::AuxCmd::ReqVerTftp:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespVerTftp));
            resp.set_string("TFTP-v1.0");
            break;
        case protocol::AuxCmd::ReqVerCsis:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespVerCsis));
            resp.set_string("CSIS-v1.0");
            break;
        case protocol::AuxCmd::ReqVerLicense:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespVerLicense));
            resp.set_string("OpenDavinchy-GPL/MIT");
            break;
        case protocol::AuxCmd::ReqVerIp:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespVerIp));
            resp.set_string("192.168.1.80");
            break;
        case protocol::AuxCmd::ReqVerMac:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespVerMac));
            resp.set_string("00:04:4B:88:99:AA");
            break;
        case protocol::AuxCmd::ReqDetectorStat:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespDetectorStat));
            resp.payload[0] = 0; // Status OK
            break;
        case protocol::AuxCmd::ReqFrameNum: {
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespFrameNum));
            uint32_t frames_avail = 60;
            std::memcpy(resp.payload, &frames_avail, sizeof(frames_avail));
            break;
        }
        case protocol::AuxCmd::ReqGetCount: {
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespGetCount));
            protocol::CountPayload cp{};
            cp.total_acquisitions = acquisition_count_;
            cp.dark_acquisitions = acquisition_count_ / 2;
            cp.bright_acquisitions = acquisition_count_ - cp.dark_acquisitions;
            std::memcpy(resp.payload, &cp, sizeof(cp));
            break;
        }
        case protocol::AuxCmd::ReqSleep:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespSleep));
            break;
        case protocol::AuxCmd::ReqWakeup:
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespWakeup));
            break;
        case protocol::AuxCmd::ReqBattery: {
            resp.set_command(static_cast<uint32_t>(protocol::AuxCmd::RespBattery));
            auto b = hardware_.read_battery();
            protocol::BatteryPayload bp{};
            bp.percent = b.percent;
            bp.voltage_mv = b.voltage_mv;
            bp.current_ma = b.current_ma;
            std::memcpy(resp.payload, &bp, sizeof(bp));
            break;
        }
        default:
            resp.set_command(sub_cmd ^ 0x01000); // Standard reply mask
            break;
    }
}

void JetsonServer::handle_bak_cmd(uint32_t sub_cmd, const protocol::Packet& req,
                                 protocol::Packet& resp, net::TcpStream& client_stream) {
    (void)req;
    (void)client_stream;
    resp.set_command(sub_cmd ^ 0x02000); // BAK reply mask
}

void JetsonServer::frame_task_run(std::stop_token stop_token) {
    while (!stop_token.stop_requested() && is_running_.load()) {
        auto client = frame_listener_->accept(std::chrono::milliseconds(200));
        if (client) {
            if (config_.verbose_log) {
                std::cout << "[Mellzi::Server] Frame streaming connection accepted from "
                          << client->peer_address() << ":" << client->peer_port() << "\n";
            }
            std::thread([this, c = std::move(client), stop_token]() mutable {
                handle_frame_client(std::move(c), stop_token);
            }).detach();
        }
    }
}

void JetsonServer::handle_frame_client(std::unique_ptr<net::TcpStream> stream, std::stop_token stop_token) {
    stream->set_timeout(std::chrono::milliseconds(10000));
    stream->set_nodelay(true);
    stream->set_buffer_sizes(16 * 1024 * 1024, 16 * 1024 * 1024);

    while (!stop_token.stop_requested() && is_running_.load()) {
        if (is_acquiring_.load()) {
            if (config_.verbose_log) {
                std::cout << "[Mellzi::Server] Acquisition triggered -> streaming frame...\n";
            }

            image::Image frame;
            generate_frame_data(frame);

            // Stream raw 16-bit frame bytes
            std::span<const uint8_t> frame_bytes(
                reinterpret_cast<const uint8_t*>(frame.data()), frame.byte_size()
            );

            if (!stream->send_all(frame_bytes)) {
                std::cerr << "[Mellzi::Server] Frame data send error\n";
                break;
            }

            // Send FRAME_DONE packet (0xFF)
            protocol::Packet done_pkt{};
            done_pkt.set_command(static_cast<uint32_t>(protocol::CaptureCmd::FrameDone));
            std::span<const uint8_t> done_bytes(
                reinterpret_cast<const uint8_t*>(&done_pkt), protocol::PACKET_SIZE
            );
            stream->send_all(done_bytes);

            acquisition_count_++;
            is_acquiring_.store(false);

            if (config_.verbose_log) {
                std::cout << "[Mellzi::Server] Frame #" << acquisition_count_ << " successfully sent ("
                          << frame.byte_size() << " bytes)\n";
            }
        } else {
            // Check if client has disconnected while waiting for acquisition
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(stream->native_handle(), &rfds);
            timeval tv{0, 20000}; // 20ms
            int sel = ::select(static_cast<int>(stream->native_handle() + 1), &rfds, nullptr, nullptr, &tv);
            if (sel > 0 && FD_ISSET(stream->native_handle(), &rfds)) {
                char peek_byte = 0;
                int n = ::recv(stream->native_handle(), &peek_byte, 1, MSG_PEEK);
                if (n <= 0) {
                    break; // peer closed connection
                }
            }
        }
    }

    if (config_.verbose_log) {
        std::cout << "[Mellzi::Server] Frame client disconnected.\n";
    }
}

void JetsonServer::telemetry_task_run(std::stop_token stop_token) {
    while (!stop_token.stop_requested() && is_running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.telemetry_interval_ms));
    }
}

} // namespace mellzi::server
