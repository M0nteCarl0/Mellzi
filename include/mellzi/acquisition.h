#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <span>
#include "mellzi/protocol.h"
#include "mellzi/socket.h"
#include "mellzi/image_process.h"

namespace mellzi::client {

struct Telemetry {
    uint32_t battery_percent{0};
    uint32_t battery_voltage_mv{0};
    int32_t  wireless_rssi_dbm{0};
    uint32_t wireless_quality{0};
    float    temp1_celsius{0.0f};
    float    temp2_celsius{0.0f};
    int16_t  accel_x{0};
    int16_t  accel_y{0};
    int16_t  accel_z{0};
    bool     shock_detected{false};
};

using TelemetryCallback = std::function<void(const Telemetry&)>;
using ProgressCallback  = std::function<void(uint32_t percent)>;

class AcquisitionClient {
public:
    AcquisitionClient();
    ~AcquisitionClient();

    AcquisitionClient(const AcquisitionClient&) = delete;
    AcquisitionClient& operator=(const AcquisitionClient&) = delete;

    bool connect(std::string_view host, uint16_t ctrl_port = protocol::DEFAULT_CONTROL_PORT,
                 uint16_t frame_port = protocol::DEFAULT_FRAME_PORT,
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    void disconnect();
    [[nodiscard]] bool is_connected() const noexcept;

    // Command transmission
    bool send_command(uint32_t cmd_id, std::span<const uint8_t> payload = {});
    bool send_command_wait_resp(uint32_t cmd_id, protocol::Packet& resp_out,
                                std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    // Telemetry and status queries
    [[nodiscard]] Telemetry get_latest_telemetry() const;
    void set_telemetry_callback(TelemetryCallback cb);
    void set_progress_callback(ProgressCallback cb);

    // Frame acquisition
    bool start_acquisition();
    bool abort_acquisition();
    bool get_frame(image::Image& out_frame, std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));

    // Telemetry getters
    bool query_battery(uint32_t& percent, uint32_t& mv);
    bool query_wireless(int32_t& rssi);
    bool query_version_firmware(std::string& version_str);
    bool query_version_fpga(std::string& version_str);
    bool query_version_main(std::string& version_str);
    bool query_device_ip(std::string& ip_str);
    bool query_device_mac(std::string& mac_str);
    bool query_acquisition_count(uint32_t& count);

    // Config upload / download
    bool download_init_config(std::span<uint8_t, 32> init_cfg_out);
    bool download_elset_config(int index, std::span<uint8_t, 128> elset_cfg_out);
    bool upload_init_config(std::span<const uint8_t, 32> init_cfg_in);
    bool upload_elset_config(int index, std::span<const uint8_t, 128> elset_cfg_in);

    // Geometry
    void set_frame_dimensions(uint32_t width, uint32_t height) {
        width_ = width;
        height_ = height;
    }
    void get_frame_dimensions(uint32_t& width, uint32_t& height) const {
        width = width_;
        height = height_;
    }

private:
    std::string host_{"127.0.0.1"};
    uint16_t ctrl_port_{protocol::DEFAULT_CONTROL_PORT};
    uint16_t frame_port_{protocol::DEFAULT_FRAME_PORT};

    std::unique_ptr<net::TcpStream> ctrl_stream_;
    std::unique_ptr<net::TcpStream> frame_stream_;

    uint32_t width_{protocol::DEFAULT_FRAME_WIDTH};
    uint32_t height_{protocol::DEFAULT_FRAME_HEIGHT};

    mutable std::mutex telemetry_mutex_;
    Telemetry telemetry_{};
    TelemetryCallback telemetry_cb_;
    ProgressCallback progress_cb_;

    void process_incoming_packet(const protocol::Packet& pkt);
};

} // namespace mellzi::client
