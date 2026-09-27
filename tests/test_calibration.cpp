#include "mellzi/calibration.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
#include <vector>

void test_calibration_pipeline() {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    mellzi::calib::CalibrationParams params;
    params.width = W;
    params.height = H;
    params.cut = {2, 2, 2, 2};
    params.baseline_shift = 500;

    mellzi::calib::CalibrationEngine engine(params);

    // 1. Create dark frame (mean = 1000)
    mellzi::image::Image dark(W, H, 1000);
    engine.set_dark_frame(dark);

    // 2. Create raw frame with artificial bad pixel at (10, 10)
    mellzi::image::Image raw(W, H, 5000);
    raw.set_pixel(10, 10, 65000); // Bad saturated pixel

    // 3. Create BPM map flagging (10, 10)
    mellzi::image::Image bpm(W, H, 0);
    bpm.set_pixel(10, 10, 1);
    engine.set_bpm_map(bpm);

    // 4. Run calibration without crop first
    auto proc = engine.process(raw, mellzi::calib::CALIB_OFFSET | mellzi::calib::CALIB_BPM | mellzi::calib::CALIB_NEG_COMPENSATION);

    // Pixel at (10, 10) should have been repaired to around 4500 (5000 - 1000 + 500)
    uint16_t repaired_val = proc.get_pixel(10, 10);
    uint16_t normal_val = proc.get_pixel(5, 5);

    assert(normal_val == 4500);
    assert(std::abs(static_cast<int>(repaired_val) - 4500) < 50);

    // 5. Test cropping
    auto cropped = engine.process(raw, mellzi::calib::CALIB_CROP);
    assert(cropped.width() == W - 4);
    assert(cropped.height() == H - 4);

    std::cout << "[PASS] test_calibration_pipeline\n";
}

int main() {
    std::cout << "Running Calibration Unit Tests...\n";
    test_calibration_pipeline();
    std::cout << "All Calibration Tests Passed!\n";
    return 0;
}
