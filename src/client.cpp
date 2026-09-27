#include "mellzi/client.h"
#include <iostream>
#include <thread>

namespace mellzi {

DetectorClient::DetectorClient() {
    acq_.set_frame_dimensions(settings_.frame_width, settings_.frame_height);
    calib::CalibrationParams cp;
    cp.width = settings_.frame_width;
    cp.height = settings_.frame_height;
    cp.cut = settings_.cut;
    calib_.set_params(cp);
}

bool DetectorClient::connect(std::string_view host, uint16_t ctrl_port, uint16_t frame_port) {
    if (!acq_.connect(host, ctrl_port, frame_port)) {
        return false;
    }
    settings_.ip_address = host;
    settings_.control_port = ctrl_port;
    settings_.frame_port = frame_port;
    return true;
}

void DetectorClient::disconnect() {
    acq_.disconnect();
}

bool DetectorClient::is_connected() const noexcept {
    return acq_.is_connected();
}

bool DetectorClient::acquire_raw_frame(image::Image& out_frame, std::chrono::milliseconds timeout) {
    return acq_.get_frame(out_frame, timeout);
}

bool DetectorClient::acquire_calibrated_frame(image::Image& out_frame, uint32_t calib_flags,
                                              std::chrono::milliseconds timeout) {
    image::Image raw;
    if (!acquire_raw_frame(raw, timeout)) {
        return false;
    }

    out_frame = calib_.process(raw, calib_flags);
    return true;
}

bool DetectorClient::perform_dark_calibration(int num_frames, int skip_frames) {
    std::vector<image::Image> darks;
    darks.reserve(num_frames);

    for (int i = 0; i < skip_frames + num_frames; ++i) {
        image::Image frame;
        if (!acquire_raw_frame(frame)) {
            return false;
        }
        if (i >= skip_frames) {
            darks.push_back(std::move(frame));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return calib_.generate_dark_from_frames(darks);
}

bool DetectorClient::perform_bright_calibration(int num_frames) {
    std::vector<image::Image> brights;
    brights.reserve(num_frames);

    for (int i = 0; i < num_frames; ++i) {
        image::Image frame;
        if (!acquire_raw_frame(frame)) {
            return false;
        }
        brights.push_back(std::move(frame));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (!calib_.generate_bright_from_frames(brights)) {
        return false;
    }

    calib_.generate_gain_map();
    calib_.generate_bpm_map();
    return true;
}

bool DetectorClient::load_calibration_directory(const std::filesystem::path& dir) {
    return calib_.load_all_from_directory(dir);
}

bool DetectorClient::save_calibration_directory(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    // Not strictly needed to save if read-only, but placeholder for persistence
    return true;
}

} // namespace mellzi
