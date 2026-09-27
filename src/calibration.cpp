#include "mellzi/calibration.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace mellzi::calib {

CalibrationEngine::CalibrationEngine()
    : params_() {}

CalibrationEngine::CalibrationEngine(const CalibrationParams& params)
    : params_(params) {}

void CalibrationEngine::set_calibration_directory(const std::filesystem::path& dir) {
    calib_dir_ = dir;
}

void CalibrationEngine::set_dark_frame(image::Image dark) {
    dark_frame_ = std::move(dark);
}

void CalibrationEngine::set_bright_frame(image::Image bright) {
    bright_frame_ = std::move(bright);
}

void CalibrationEngine::set_bpm_map(image::Image bpm) {
    bpm_map_ = std::move(bpm);
}

void CalibrationEngine::set_gain_map(std::vector<float> gain) {
    gain_map_ = std::move(gain);
}

bool CalibrationEngine::load_dark_file(const std::filesystem::path& path) {
    return dark_frame_.load_raw(path, params_.width, params_.height);
}

bool CalibrationEngine::load_bright_file(const std::filesystem::path& path) {
    return bright_frame_.load_raw(path, params_.width, params_.height);
}

bool CalibrationEngine::load_bpm_file(const std::filesystem::path& path) {
    return bpm_map_.load_raw(path, params_.width, params_.height);
}

bool CalibrationEngine::load_gain_file(const std::filesystem::path& path) {
    // Check if 16-bit raw or float raw
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    size_t count = static_cast<size_t>(params_.width) * params_.height;
    f.seekg(0, std::ios::end);
    size_t file_sz = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    gain_map_.resize(count, 1.0f);

    if (file_sz == count * sizeof(float)) {
        f.read(reinterpret_cast<char*>(gain_map_.data()), count * sizeof(float));
        return true;
    } else if (file_sz == count * sizeof(uint16_t)) {
        std::vector<uint16_t> u16_buf(count);
        f.read(reinterpret_cast<char*>(u16_buf.data()), count * sizeof(uint16_t));
        // Fixed point scaling (e.g. 1024 or 4096 = 1.0)
        double mean_raw = 0.0;
        for (auto v : u16_buf) mean_raw += v;
        mean_raw /= static_cast<double>(count);
        if (mean_raw <= 0.0) mean_raw = 1.0;

        for (size_t i = 0; i < count; ++i) {
            gain_map_[i] = static_cast<float>(static_cast<double>(u16_buf[i]) / mean_raw);
        }
        return true;
    }
    return false;
}

bool CalibrationEngine::load_all_from_directory(const std::filesystem::path& dir) {
    calib_dir_ = dir;

    // Standard Rayence filenames: dark.raw, bright.raw, bpm.raw, $c2_gain.raw
    load_dark_file(dir / "dark.raw") || load_dark_file(dir / "DARK.raw");
    load_bright_file(dir / "bright.raw") || load_bright_file(dir / "BRIGHT.raw") || load_bright_file(dir / "$a_rawbright.raw");
    load_bpm_file(dir / "bpm.raw") || load_bpm_file(dir / "BPM.raw");
    load_gain_file(dir / "gain.raw") || load_gain_file(dir / "$c2_gain.raw");

    if (has_dark() && has_bright() && (!has_gain() || !has_bpm())) {
        generate_gain_map();
        generate_bpm_map();
    }
    return has_dark();
}

bool CalibrationEngine::generate_dark_from_frames(std::span<const image::Image> dark_frames) {
    if (dark_frames.empty()) return false;

    uint32_t w = dark_frames[0].width();
    uint32_t h = dark_frames[0].height();
    size_t count = static_cast<size_t>(w) * h;

    std::vector<double> sum(count, 0.0);
    for (const auto& f : dark_frames) {
        if (f.width() != w || f.height() != h) continue;
        const auto sp = f.span();
        for (size_t i = 0; i < count; ++i) {
            sum[i] += sp[i];
        }
    }

    dark_frame_.resize(w, h);
    auto dark_sp = dark_frame_.span();
    double n = static_cast<double>(dark_frames.size());
    for (size_t i = 0; i < count; ++i) {
        dark_sp[i] = static_cast<uint16_t>(std::clamp(sum[i] / n, 0.0, 65535.0));
    }
    return true;
}

bool CalibrationEngine::generate_bright_from_frames(std::span<const image::Image> bright_frames) {
    if (bright_frames.empty()) return false;

    uint32_t w = bright_frames[0].width();
    uint32_t h = bright_frames[0].height();
    size_t count = static_cast<size_t>(w) * h;

    std::vector<double> sum(count, 0.0);
    for (const auto& f : bright_frames) {
        if (f.width() != w || f.height() != h) continue;
        const auto sp = f.span();
        for (size_t i = 0; i < count; ++i) {
            sum[i] += sp[i];
        }
    }

    bright_frame_.resize(w, h);
    auto bright_sp = bright_frame_.span();
    double n = static_cast<double>(bright_frames.size());
    for (size_t i = 0; i < count; ++i) {
        bright_sp[i] = static_cast<uint16_t>(std::clamp(sum[i] / n, 0.0, 65535.0));
    }
    return true;
}

bool CalibrationEngine::generate_gain_map() {
    if (!has_dark() || !has_bright()) return false;

    size_t count = dark_frame_.pixel_count();
    gain_map_.resize(count, 1.0f);

    const auto dark_sp = dark_frame_.span();
    const auto bright_sp = bright_frame_.span();

    double sum_diff = 0.0;
    size_t valid_count = 0;

    std::vector<double> diffs(count);
    for (size_t i = 0; i < count; ++i) {
        double d = static_cast<double>(bright_sp[i]) - static_cast<double>(dark_sp[i]);
        diffs[i] = d;
        if (d > 10.0 && bright_sp[i] < params_.saturation_level) {
            sum_diff += d;
            valid_count++;
        }
    }

    if (valid_count == 0) return false;
    double mean_diff = sum_diff / static_cast<double>(valid_count);

    for (size_t i = 0; i < count; ++i) {
        if (diffs[i] > 10.0) {
            gain_map_[i] = static_cast<float>(mean_diff / diffs[i]);
        } else {
            gain_map_[i] = 1.0f; // Bad pixel, will be fixed by BPM
        }
    }
    return true;
}

bool CalibrationEngine::generate_bpm_map() {
    if (!has_dark() && !has_bright()) return false;

    uint32_t w = has_dark() ? dark_frame_.width() : bright_frame_.width();
    uint32_t h = has_dark() ? dark_frame_.height() : bright_frame_.height();
    size_t count = static_cast<size_t>(w) * h;

    bpm_map_.resize(w, h);
    bpm_map_.fill(0);
    auto bpm_sp = bpm_map_.span();

    if (has_dark()) {
        auto dark_stats = dark_frame_.compute_stats();
        const auto dark_sp = dark_frame_.span();
        double dark_thresh_hi = dark_stats.mean + params_.bpm_dark_dev_sigma * dark_stats.std_dev;
        double dark_thresh_lo = dark_stats.mean - params_.bpm_dark_dev_sigma * dark_stats.std_dev;

        for (size_t i = 0; i < count; ++i) {
            uint16_t d = dark_sp[i];
            if (d > dark_thresh_hi || d < dark_thresh_lo || d > params_.dark_max_lsb) {
                bpm_sp[i] = 1;
            }
        }
    }

    if (has_bright() && has_dark()) {
        const auto dark_sp = dark_frame_.span();
        const auto bright_sp = bright_frame_.span();

        double sum_diff = 0.0;
        size_t valid = 0;
        for (size_t i = 0; i < count; ++i) {
            if (bpm_sp[i] == 0) {
                double diff = static_cast<double>(bright_sp[i]) - static_cast<double>(dark_sp[i]);
                if (diff > 0.0) {
                    sum_diff += diff;
                    valid++;
                }
            }
        }

        if (valid > 0) {
            double mean_diff = sum_diff / static_cast<double>(valid);
            for (size_t i = 0; i < count; ++i) {
                double diff = static_cast<double>(bright_sp[i]) - static_cast<double>(dark_sp[i]);
                if (diff <= 10.0 || bright_sp[i] >= params_.saturation_level) {
                    bpm_sp[i] = 1;
                } else if (std::abs(diff - mean_diff) / mean_diff > params_.bpm_bright_dev_ratio) {
                    bpm_sp[i] = 1;
                }
            }
        }
    }

    return true;
}

void CalibrationEngine::correct_bad_pixels(image::Image& img) const {
    if (!has_bpm()) return;

    uint32_t w = img.width();
    uint32_t h = img.height();
    auto img_sp = img.span();
    const auto bpm_sp = bpm_map_.span();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = y * w + x;
            if (bpm_sp[idx] != 0) {
                // Average valid surrounding 8 neighbors
                uint32_t sum = 0;
                uint32_t count = 0;

                for (int dy = -1; dy <= 1; ++dy) {
                    int ny = static_cast<int>(y) + dy;
                    if (ny < 0 || ny >= static_cast<int>(h)) continue;

                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = static_cast<int>(x) + dx;
                        if (nx < 0 || nx >= static_cast<int>(w)) continue;

                        size_t n_idx = static_cast<size_t>(ny) * w + static_cast<size_t>(nx);
                        if (bpm_sp[n_idx] == 0) {
                            sum += img_sp[n_idx];
                            count++;
                        }
                    }
                }

                if (count > 0) {
                    img_sp[idx] = static_cast<uint16_t>(sum / count);
                }
            }
        }
    }
}

image::Image CalibrationEngine::process(const image::Image& raw_in, uint32_t flags) const {
    image::Image out = raw_in;
    process_in_place(out, flags);
    return out;
}

void CalibrationEngine::process_in_place(image::Image& image, uint32_t flags) const {
    if (image.span().empty()) return;

    uint32_t w = image.width();
    uint32_t h = image.height();
    (void)w; (void)h;
    size_t count = image.pixel_count();
    auto sp = image.span();

    // Step 1: Dark Offset subtraction
    if ((flags & CALIB_OFFSET) && has_dark() && dark_frame_.pixel_count() == count) {
        const auto dark_sp = dark_frame_.span();
        int32_t shift = (flags & CALIB_NEG_COMPENSATION) ? params_.baseline_shift : 0;

        for (size_t i = 0; i < count; ++i) {
            int32_t val = static_cast<int32_t>(sp[i]) - static_cast<int32_t>(dark_sp[i]) + shift;
            sp[i] = static_cast<uint16_t>(std::clamp(val, 0, 65535));
        }
    }

    // Step 2: Gain normalization
    if ((flags & CALIB_GAIN) && has_gain() && gain_map_.size() == count) {
        for (size_t i = 0; i < count; ++i) {
            double corrected = static_cast<double>(sp[i]) * static_cast<double>(gain_map_[i]);
            sp[i] = static_cast<uint16_t>(std::clamp(corrected, 0.0, 65535.0));
        }
    }

    // Step 3: Bad Pixel Map (BPM) defect interpolation
    if ((flags & CALIB_BPM) && has_bpm() && bpm_map_.pixel_count() == count) {
        correct_bad_pixels(image);
    }

    // Step 4: Despeckle filtering
    if (flags & CALIB_DESPECKLE) {
        image.despeckle();
    }

    // Step 5: Crop margins
    if (flags & CALIB_CROP) {
        image = image.crop(params_.cut);
    }
}

} // namespace mellzi::calib
