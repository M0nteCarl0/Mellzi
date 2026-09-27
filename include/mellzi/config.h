#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <span>
#include <filesystem>

namespace mellzi::config {

#pragma pack(push, 1)

// Binary structure matching .initcfg (exactly 32 bytes)
struct InitCfg {
    uint8_t data[32]{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00
    };

    static constexpr size_t SIZE = 32;
};
static_assert(sizeof(InitCfg) == InitCfg::SIZE, ".initcfg structure must be 32 bytes");

// Binary structure matching .elsetcfg / .elsetcfg2 / .elsetcfg3 (exactly 128 bytes)
struct ElsetCfg {
    uint8_t data[128]{0};

    static constexpr size_t SIZE = 128;
};
static_assert(sizeof(ElsetCfg) == ElsetCfg::SIZE, ".elsetcfg structure must be 128 bytes");

#pragma pack(pop)

// Detector Initialization & Timing Parameters (Corresponds to VADav.INI [Init1] and DetectorInitParamets)
struct DetectorInitParameters {
    int32_t f_ctrl{1};
    int32_t int_b{0};
    int32_t om{0};
    int32_t scan{1};
    int32_t speed{0};
    int32_t eo_sel{1};
    int32_t ag0{1};
    int32_t ag1{0};
    int32_t ag2{1};
    int32_t da0{0};
    int32_t da1{0};
    int32_t da2{0};
    int32_t da3{1};
    int32_t da4{1};
    int32_t da5{0};
    int32_t da6{0};
    int32_t da7{0};
    int32_t da8{0};
    int32_t agi_auto{0};
    int32_t gate0{0};
    int32_t gate1{0};
    int32_t gate2{0};
    int32_t gate3{0};
    int32_t gate4{0};
    int32_t p_drv{0};
    int32_t s_drv{0};
    int32_t dark_comp{0};
    int32_t at_en{0};
    int32_t af_gate{0};
    int32_t af_el{0};
    int32_t ex_time{1333333};
    int32_t loop_cnt{1};
    int32_t frame_cnt{10};
    int32_t frame_delay{1};
    int32_t el_delay{1};
    int32_t el_high{0};
    int32_t gate_low{1};
    int32_t gate_high{160};
    int32_t readout_count{1};
    int32_t el_high2{400000};
    int32_t frame_cnt2{4};
    int32_t loop_cnt2{1};
    int32_t el_high3{0};
    int32_t frame_cnt3{0};
    int32_t loop_cnt3{0};
    int32_t sh0{214};
    int32_t sh1{214};
    int32_t gate{160};
    int32_t test_pattern{27};
    int32_t af_el_runtime{0};
    int32_t af_el_delay{0};
    int32_t af_el_loop{0};
    int32_t af_gate_rd_cnt{0};
    int32_t af_delay_cnt{0};
    int32_t af_gate_rd_loop{0};
    int32_t at_threshold{0};
    int32_t at_delay{0};
    int32_t ready_delay{0};
    int32_t agi_delay{0};

    // 128 Option fields (Option00 to Option7f)
    std::array<int32_t, 128> options{0};

    // 96 Control fields (Control00 to Control5f)
    std::array<int32_t, 96> controls{0};

    void init_defaults() noexcept;
    bool load_from_ini(const std::filesystem::path& iniPath);
    bool save_to_ini(const std::filesystem::path& iniPath) const;
};

struct ImageCutParams {
    int32_t left{30};
    int32_t top{10};
    int32_t right{30};
    int32_t bottom{50};
};

struct DetectorSettings {
    uint32_t frame_width{3328};
    uint32_t frame_height{3328};
    ImageCutParams cut{};
    std::string ip_address{"127.0.0.1"};
    uint16_t control_port{20000};
    uint16_t frame_port{20001};
    std::filesystem::path calib_dir{"./calib"};
    std::filesystem::path home_dir{"./"};
    int32_t dark_average_frames{5};
    int32_t bright_average_frames{4};
    int32_t skip_frames{1};
    int32_t bit_depth{16};

    bool load_from_ini(const std::filesystem::path& iniPath);
    bool save_to_ini(const std::filesystem::path& iniPath) const;
};

} // namespace mellzi::config
