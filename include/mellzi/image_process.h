#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>
#include <filesystem>
#include <string_view>
#include "mellzi/config.h"

namespace mellzi::image {

struct ImageStats {
    uint16_t min_val{65535};
    uint16_t max_val{0};
    double   mean{0.0};
    double   std_dev{0.0};
};

class Image {
public:
    Image() = default;
    Image(uint32_t width, uint32_t height, uint16_t initial_val = 0);
    explicit Image(uint32_t width, uint32_t height, std::span<const uint16_t> data);

    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] uint32_t height() const noexcept { return height_; }
    [[nodiscard]] size_t pixel_count() const noexcept { return static_cast<size_t>(width_) * height_; }
    [[nodiscard]] size_t byte_size() const noexcept { return pixel_count() * sizeof(uint16_t); }

    [[nodiscard]] uint16_t* data() noexcept { return pixels_.data(); }
    [[nodiscard]] const uint16_t* data() const noexcept { return pixels_.data(); }
    [[nodiscard]] std::span<uint16_t> span() noexcept { return pixels_; }
    [[nodiscard]] std::span<const uint16_t> span() const noexcept { return pixels_; }

    [[nodiscard]] uint16_t get_pixel(uint32_t x, uint32_t y) const noexcept {
        return pixels_[y * width_ + x];
    }

    void set_pixel(uint32_t x, uint32_t y, uint16_t val) noexcept {
        pixels_[y * width_ + x] = val;
    }

    void resize(uint32_t width, uint32_t height);
    void fill(uint16_t val) noexcept;

    // Statistics
    [[nodiscard]] ImageStats compute_stats() const;

    // Operations
    [[nodiscard]] Image crop(const config::ImageCutParams& cut) const;
    void apply_window_level(uint16_t window_center, uint16_t window_width, std::span<uint8_t> out_8bit) const;
    void despeckle(int threshold = 350);

    // File I/O
    bool load_raw(const std::filesystem::path& path, uint32_t width, uint32_t height);
    bool save_raw(const std::filesystem::path& path) const;
    bool save_pgm(const std::filesystem::path& path) const;
    bool save_bmp(const std::filesystem::path& path, uint16_t window_center = 0, uint16_t window_width = 0) const;

private:
    uint32_t width_{0};
    uint32_t height_{0};
    std::vector<uint16_t> pixels_;
};

} // namespace mellzi::image
