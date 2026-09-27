#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <filesystem>
#include "mellzi/acquisition.h"
#include "mellzi/calibration.h"
#include "mellzi/config.h"

namespace mellzi {

class DetectorClient {
public:
    DetectorClient();
    ~DetectorClient() = default;

    DetectorClient(const DetectorClient&) = delete;
    DetectorClient& operator=(const DetectorClient&) = delete;

    // Connection
    bool connect(std::string_view host = "127.0.0.1",
                 uint16_t ctrl_port = protocol::DEFAULT_CONTROL_PORT,
                 uint16_t frame_port = protocol::DEFAULT_FRAME_PORT);
    void disconnect();
    [[nodiscard]] bool is_connected() const noexcept;

    // Direct access to subsystems
    [[nodiscard]] client::AcquisitionClient& acquisition() noexcept { return acq_; }
    [[nodiscard]] const client::AcquisitionClient& acquisition() const noexcept { return acq_; }
    [[nodiscard]] calib::CalibrationEngine& calibration() noexcept { return calib_; }
    [[nodiscard]] const calib::CalibrationEngine& calibration() const noexcept { return calib_; }
    [[nodiscard]] config::DetectorSettings& settings() noexcept { return settings_; }
    [[nodiscard]] const config::DetectorSettings& settings() const noexcept { return settings_; }

    // High-level Acquisition Workflows
    bool acquire_raw_frame(image::Image& out_frame, std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
    bool acquire_calibrated_frame(image::Image& out_frame, uint32_t calib_flags = calib::CALIB_ALL,
                                  std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));

    // Automated Calibration Workflows
    bool perform_dark_calibration(int num_frames = 5, int skip_frames = 1);
    bool perform_bright_calibration(int num_frames = 4);
    bool load_calibration_directory(const std::filesystem::path& dir);
    bool save_calibration_directory(const std::filesystem::path& dir);

    // Device telemetry and info
    [[nodiscard]] client::Telemetry get_telemetry() const { return acq_.get_latest_telemetry(); }

private:
    config::DetectorSettings settings_;
    client::AcquisitionClient acq_;
    calib::CalibrationEngine  calib_;
};

} // namespace mellzi
