#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <filesystem>
#include "mellzi/protocol.h"
#include "mellzi/socket.h"
#include "mellzi/config.h"
#include "mellzi/platform.h"
#include "mellzi/image_process.h"

namespace mellzi::server {

enum class SimulationPattern {
    Gradient,
    Checkerboard,
    FlatFieldNoise,
    LoadFromDisk
};

struct ServerConfig {
    uint16_t control_port{protocol::DEFAULT_CONTROL_PORT};
    uint16_t frame_port{protocol::DEFAULT_FRAME_PORT};
    std::string listen_address{"0.0.0.0"};
    uint32_t frame_width{protocol::DEFAULT_FRAME_WIDTH};
    uint32_t frame_height{protocol::DEFAULT_FRAME_HEIGHT};
    std::filesystem::path config_dir{"./"};
    std::filesystem::path raw_frames_dir{"./frames"};
    SimulationPattern pattern{SimulationPattern::Gradient};
    bool enable_telemetry_broadcast{true};
    uint32_t telemetry_interval_ms{1000};
    bool verbose_log{true};
};

class JetsonServer {
public:
    explicit JetsonServer(const ServerConfig& config = {});
    ~JetsonServer();

    JetsonServer(const JetsonServer&) = delete;
    JetsonServer& operator=(const JetsonServer&) = delete;

    bool start();
    void stop();
    [[nodiscard]] bool is_running() const noexcept { return is_running_.load(); }

    void wait_until_stopped();

    // Hardware & Platform
    [[nodiscard]] platform::HardwareMonitor& hardware() noexcept { return hardware_; }
    [[nodiscard]] const platform::HardwareMonitor& hardware() const noexcept { return hardware_; }

    // Configuration management
    [[nodiscard]] config::DetectorInitParameters& init_params() noexcept { return init_params_; }
    [[nodiscard]] config::InitCfg& init_cfg() noexcept { return init_cfg_; }
    [[nodiscard]] config::ElsetCfg& elset_cfg(size_t index) noexcept {
        return index < elset_cfgs_.size() ? elset_cfgs_[index] : elset_cfgs_[0];
    }

    // Set custom frame for next transmission
    void set_next_frame(image::Image frame);
    void set_raw_template(const std::filesystem::path& raw_path);

private:
    ServerConfig config_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_acquiring_{false};

    platform::HardwareMonitor hardware_;
    config::DetectorInitParameters init_params_{};
    config::InitCfg init_cfg_{};
    std::array<config::ElsetCfg, 3> elset_cfgs_{};

    std::unique_ptr<net::TcpListener> ctrl_listener_;
    std::unique_ptr<net::TcpListener> frame_listener_;

    std::jthread ctrl_thread_;
    std::jthread frame_thread_;
    std::jthread telemetry_thread_;

    mutable std::mutex frame_mutex_;
    bool has_custom_frame_{false};
    image::Image current_frame_;
    uint32_t acquisition_count_{0};

    // Internal task routines
    void ctrl_task_run(std::stop_token stop_token);
    void frame_task_run(std::stop_token stop_token);
    void telemetry_task_run(std::stop_token stop_token);

    // Client connection handlers
    void handle_ctrl_client(std::unique_ptr<net::TcpStream> stream, std::stop_token stop_token);
    void handle_frame_client(std::unique_ptr<net::TcpStream> stream, std::stop_token stop_token);

    // Protocol dispatch
    void dispatch_command(const protocol::Packet& req, protocol::Packet& resp,
                          net::TcpStream& client_stream);
    void handle_capture_cmd(uint32_t sub_cmd, const protocol::Packet& req,
                            protocol::Packet& resp, net::TcpStream& client_stream);
    void handle_config_cmd(uint32_t sub_cmd, const protocol::Packet& req,
                           protocol::Packet& resp, net::TcpStream& client_stream);
    void handle_aux_cmd(uint32_t sub_cmd, const protocol::Packet& req,
                        protocol::Packet& resp, net::TcpStream& client_stream);
    void handle_bak_cmd(uint32_t sub_cmd, const protocol::Packet& req,
                        protocol::Packet& resp, net::TcpStream& client_stream);

    // Image generation
    void generate_frame_data(image::Image& frame);
    void load_persistent_configs();
    void save_persistent_configs();
};

} // namespace mellzi::server
