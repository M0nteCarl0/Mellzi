#include "mellzi/image_process.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace mellzi::image {

Image::Image(uint32_t width, uint32_t height, uint16_t initial_val)
    : width_(width), height_(height), pixels_(static_cast<size_t>(width) * height, initial_val) {}

Image::Image(uint32_t width, uint32_t height, std::span<const uint16_t> data)
    : width_(width), height_(height), pixels_(data.begin(), data.end()) {}

void Image::resize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    pixels_.resize(static_cast<size_t>(width) * height, 0);
}

void Image::fill(uint16_t val) noexcept {
    std::fill(pixels_.begin(), pixels_.end(), val);
}

ImageStats Image::compute_stats() const {
    ImageStats stats{};
    if (pixels_.empty()) return stats;

    double sum = 0.0;
    double sq_sum = 0.0;

    for (uint16_t val : pixels_) {
        if (val < stats.min_val) stats.min_val = val;
        if (val > stats.max_val) stats.max_val = val;
        sum += val;
        sq_sum += static_cast<double>(val) * val;
    }

    double n = static_cast<double>(pixels_.size());
    stats.mean = sum / n;
    double variance = (sq_sum / n) - (stats.mean * stats.mean);
    stats.std_dev = variance > 0.0 ? std::sqrt(variance) : 0.0;

    return stats;
}

Image Image::crop(const config::ImageCutParams& cut) const {
    if (width_ <= static_cast<uint32_t>(cut.left + cut.right) ||
        height_ <= static_cast<uint32_t>(cut.top + cut.bottom)) {
        return *this;
    }

    uint32_t new_w = width_ - cut.left - cut.right;
    uint32_t new_h = height_ - cut.top - cut.bottom;

    Image out(new_w, new_h);

    for (uint32_t y = 0; y < new_h; ++y) {
        const uint16_t* src_row = &pixels_[(y + cut.top) * width_ + cut.left];
        uint16_t* dst_row = &out.pixels_[y * new_w];
        std::copy_n(src_row, new_w, dst_row);
    }

    return out;
}

void Image::apply_window_level(uint16_t window_center, uint16_t window_width, std::span<uint8_t> out_8bit) const {
    if (pixels_.empty() || out_8bit.size() < pixels_.size()) return;

    if (window_width == 0) {
        auto stats = compute_stats();
        window_center = static_cast<uint16_t>((stats.min_val + stats.max_val) / 2);
        window_width = static_cast<uint16_t>(std::max(1, stats.max_val - stats.min_val));
    }

    double low = static_cast<double>(window_center) - static_cast<double>(window_width) / 2.0;
    double high = static_cast<double>(window_center) + static_cast<double>(window_width) / 2.0;
    double range = high - low;
    if (range <= 0.0) range = 1.0;

    for (size_t i = 0; i < pixels_.size(); ++i) {
        double val = pixels_[i];
        if (val <= low) {
            out_8bit[i] = 0;
        } else if (val >= high) {
            out_8bit[i] = 255;
        } else {
            out_8bit[i] = static_cast<uint8_t>(((val - low) / range) * 255.0);
        }
    }
}

void Image::despeckle(int threshold) {
    if (width_ < 3 || height_ < 3) return;

    std::vector<uint16_t> temp = pixels_;

    for (uint32_t y = 1; y < height_ - 1; ++y) {
        for (uint32_t x = 1; x < width_ - 1; ++x) {
            uint16_t center = temp[y * width_ + x];

            // 8 neighbors
            uint16_t n[8] = {
                temp[(y - 1) * width_ + (x - 1)],
                temp[(y - 1) * width_ + x],
                temp[(y - 1) * width_ + (x + 1)],
                temp[y * width_ + (x - 1)],
                temp[y * width_ + (x + 1)],
                temp[(y + 1) * width_ + (x - 1)],
                temp[(y + 1) * width_ + x],
                temp[(y + 1) * width_ + (x + 1)]
            };

            std::sort(std::begin(n), std::end(n));
            uint16_t median = (n[3] + n[4]) / 2;

            if (std::abs(static_cast<int>(center) - static_cast<int>(median)) > threshold) {
                pixels_[y * width_ + x] = median;
            }
        }
    }
}

bool Image::load_raw(const std::filesystem::path& path, uint32_t width, uint32_t height) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    resize(width, height);
    f.read(reinterpret_cast<char*>(pixels_.data()), static_cast<std::streamsize>(byte_size()));
    return f.gcount() == static_cast<std::streamsize>(byte_size());
}

bool Image::save_raw(const std::filesystem::path& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    f.write(reinterpret_cast<const char*>(pixels_.data()), static_cast<std::streamsize>(byte_size()));
    return f.good();
}

bool Image::save_pgm(const std::filesystem::path& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    // PGM Header (P5 = binary grayscale, maxval 65535)
    f << "P5\n" << width_ << " " << height_ << "\n65535\n";

    // PGM requires big-endian 16-bit values
    std::vector<uint8_t> be_buf(byte_size());
    for (size_t i = 0; i < pixels_.size(); ++i) {
        uint16_t p = pixels_[i];
        be_buf[i * 2 + 0] = static_cast<uint8_t>((p >> 8) & 0xFF);
        be_buf[i * 2 + 1] = static_cast<uint8_t>(p & 0xFF);
    }

    f.write(reinterpret_cast<const char*>(be_buf.data()), static_cast<std::streamsize>(be_buf.size()));
    return f.good();
}

bool Image::save_bmp(const std::filesystem::path& path, uint16_t window_center, uint16_t window_width) const {
    if (pixels_.empty()) return false;

    std::vector<uint8_t> gray8(pixel_count());
    apply_window_level(window_center, window_width, gray8);

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    uint32_t row_stride = (width_ + 3) & ~3;
    uint32_t palette_size = 256 * 4;
    uint32_t data_size = row_stride * height_;
    uint32_t file_size = 14 + 40 + palette_size + data_size;
    uint32_t data_offset = 14 + 40 + palette_size;

    // BMP Header (14 bytes)
    uint8_t file_hdr[14]{
        'B', 'M',
        static_cast<uint8_t>(file_size & 0xFF),
        static_cast<uint8_t>((file_size >> 8) & 0xFF),
        static_cast<uint8_t>((file_size >> 16) & 0xFF),
        static_cast<uint8_t>((file_size >> 24) & 0xFF),
        0, 0, 0, 0, // reserved
        static_cast<uint8_t>(data_offset & 0xFF),
        static_cast<uint8_t>((data_offset >> 8) & 0xFF),
        static_cast<uint8_t>((data_offset >> 16) & 0xFF),
        static_cast<uint8_t>((data_offset >> 24) & 0xFF)
    };
    f.write(reinterpret_cast<const char*>(file_hdr), sizeof(file_hdr));

    // DIB Header (40 bytes, BITMAPINFOHEADER)
    uint8_t dib_hdr[40]{
        40, 0, 0, 0, // header size
        static_cast<uint8_t>(width_ & 0xFF), static_cast<uint8_t>((width_ >> 8) & 0xFF),
        static_cast<uint8_t>((width_ >> 16) & 0xFF), static_cast<uint8_t>((width_ >> 24) & 0xFF),
        static_cast<uint8_t>(height_ & 0xFF), static_cast<uint8_t>((height_ >> 8) & 0xFF),
        static_cast<uint8_t>((height_ >> 16) & 0xFF), static_cast<uint8_t>((height_ >> 24) & 0xFF),
        1, 0, // planes = 1
        8, 0, // bpp = 8
        0, 0, 0, 0, // compression = 0 (BI_RGB)
        static_cast<uint8_t>(data_size & 0xFF), static_cast<uint8_t>((data_size >> 8) & 0xFF),
        static_cast<uint8_t>((data_size >> 16) & 0xFF), static_cast<uint8_t>((data_size >> 24) & 0xFF),
        0x13, 0x0B, 0, 0, // 2835 ppm x
        0x13, 0x0B, 0, 0, // 2835 ppm y
        0, 1, 0, 0, // 256 colors
        0, 1, 0, 0
    };
    f.write(reinterpret_cast<const char*>(dib_hdr), sizeof(dib_hdr));

    // Palette (256 grayscale entries: B, G, R, 0)
    uint8_t palette[1024];
    for (int i = 0; i < 256; ++i) {
        palette[i * 4 + 0] = static_cast<uint8_t>(i);
        palette[i * 4 + 1] = static_cast<uint8_t>(i);
        palette[i * 4 + 2] = static_cast<uint8_t>(i);
        palette[i * 4 + 3] = 0;
    }
    f.write(reinterpret_cast<const char*>(palette), sizeof(palette));

    // BMP stores rows bottom-to-top
    std::vector<uint8_t> row_buf(row_stride, 0);
    for (int32_t y = static_cast<int32_t>(height_) - 1; y >= 0; --y) {
        std::copy_n(&gray8[static_cast<size_t>(y) * width_], width_, row_buf.data());
        f.write(reinterpret_cast<const char*>(row_buf.data()), row_stride);
    }

    return f.good();
}

} // namespace mellzi::image
