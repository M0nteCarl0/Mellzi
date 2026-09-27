#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <filesystem>
#include <memory>
#include "mellzi/image_process.h"
#include "mellzi/config.h"

namespace mellzi::calib {

// Calibration flags matching Rayence SDK VDC_Process
enum CalibrationFlags : uint32_t {
    CALIB_NONE              = 0x00,
    CALIB_OFFSET            = 0x01, // Subtract dark offset frame
    CALIB_GAIN              = 0x02, // Apply bright gain map
    CALIB_BPM               = 0x04, // Correct bad pixels using BPM map
    CALIB_DESPECKLE         = 0x08, // Remove impulse spikes
    CALIB_NEG_COMPENSATION  = 0x10, // Compensate negative pixels by baseline shift
    CALIB_DEFECT_CORR       = 0x20, // 2-line defect correction
    CALIB_CROP              = 0x40, // Crop margins (ImgCut)
    CALIB_ALL               = 0x7F
};

struct CalibrationParams {
    uint32_t width{3328};
    uint32_t height{3328};
    config::ImageCutParams cut{};
    int32_t baseline_shift{1000};
    uint16_t saturation_level{54000};
    uint16_t dark_max_lsb{24000};
    double gain_target_mean{10000.0};
    double bpm_dark_dev_sigma{3.0};
    double bpm_bright_dev_ratio{0.20};
};

class CalibrationEngine {
public:
    CalibrationEngine();
    explicit CalibrationEngine(const CalibrationParams& params);
    ~CalibrationEngine() = default;

    void set_params(const CalibrationParams& params) { params_ = params; }
    [[nodiscard]] const CalibrationParams& get_params() const noexcept { return params_; }

    void set_calibration_directory(const std::filesystem::path& dir);
    [[nodiscard]] const std::filesystem::path& get_calibration_directory() const noexcept { return calib_dir_; }

    // Calibration data loaders
    bool load_dark_file(const std::filesystem::path& path);
    bool load_bright_file(const std::filesystem::path& path);
    bool load_gain_file(const std::filesystem::path& path);
    bool load_bpm_file(const std::filesystem::path& path);
    bool load_all_from_directory(const std::filesystem::path& dir);

    // Calibration data setters directly
    void set_dark_frame(image::Image dark);
    void set_bright_frame(image::Image bright);
    void set_bpm_map(image::Image bpm);
    void set_gain_map(std::vector<float> gain);

    [[nodiscard]] bool has_dark() const noexcept { return !dark_frame_.span().empty(); }
    [[nodiscard]] bool has_bright() const noexcept { return !bright_frame_.span().empty(); }
    [[nodiscard]] bool has_bpm() const noexcept { return !bpm_map_.span().empty(); }
    [[nodiscard]] bool has_gain() const noexcept { return !gain_map_.empty(); }

    // Generation routines
    bool generate_dark_from_frames(std::span<const image::Image> dark_frames);
    bool generate_bright_from_frames(std::span<const image::Image> bright_frames);
    bool generate_gain_map();
    bool generate_bpm_map();

    // Process a raw frame through calibration pipeline
    [[nodiscard]] image::Image process(const image::Image& raw_in, uint32_t flags = CALIB_ALL) const;
    void process_in_place(image::Image& image, uint32_t flags = CALIB_ALL) const;

    // Bad pixel correction on an image using internal or external BPM
    void correct_bad_pixels(image::Image& img) const;

private:
    CalibrationParams params_;
    std::filesystem::path calib_dir_{"./calib"};

    image::Image dark_frame_;
    image::Image bright_frame_;
    image::Image bpm_map_;       // 0 = good pixel, 1 = defective
    std::vector<float> gain_map_; // Multiplier per pixel
};

} // namespace mellzi::calib
