#include "mellzi/config.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
#include <filesystem>

void test_config_structures() {
    assert(sizeof(mellzi::config::InitCfg) == 32);
    assert(sizeof(mellzi::config::ElsetCfg) == 128);

    mellzi::config::DetectorInitParameters p;
    p.init_defaults();

    assert(p.f_ctrl == 1);
    assert(p.sh0 == 214);
    assert(p.options[0x11] == 1);
    assert(p.controls[0x0d] == 3328);

    std::filesystem::path temp_ini = "test_params.ini";
    assert(p.save_to_ini(temp_ini));

    mellzi::config::DetectorInitParameters p2;
    assert(p2.load_from_ini(temp_ini));
    assert(p2.f_ctrl == p.f_ctrl);
    assert(p2.sh0 == p.sh0);
    assert(p2.options[0x11] == p.options[0x11]);
    assert(p2.controls[0x0d] == p.controls[0x0d]);

    std::filesystem::remove(temp_ini);
    std::cout << "[PASS] test_config_structures\n";
}

int main() {
    std::cout << "Running Config Unit Tests...\n";
    test_config_structures();
    std::cout << "All Config Tests Passed!\n";
    return 0;
}
