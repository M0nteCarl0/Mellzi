#include "mellzi/platform.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include <algorithm>

namespace mellzi::platform {

HardwareMonitor::HardwareMonitor() {
    detect_hardware();
}

std::string_view HardwareMonitor::get_board_name() const noexcept {
    return model_name_;
}

void HardwareMonitor::detect_hardware() {
#if defined(_WIN32)
    board_type_ = BoardType::WindowsHost;
    model_name_ = "Windows Host PC (Emulation Mode)";
#elif defined(__APPLE__)
    board_type_ = BoardType::MacOSHost;
    model_name_ = "macOS Host (Emulation Mode)";
#else
    // Check Linux Device Tree model for NVIDIA Jetson
    std::ifstream dt_model("/proc/device-tree/model");
    if (dt_model.is_open()) {
        std::string line;
        std::getline(dt_model, line);
        model_name_ = line;

        if (line.find("Orin") != std::string::npos) {
            board_type_ = BoardType::JetsonOrin;
        } else if (line.find("Xavier") != std::string::npos) {
            board_type_ = BoardType::JetsonXavier;
        } else if (line.find("Nano") != std::string::npos) {
            board_type_ = BoardType::JetsonNano;
        } else if (line.find("TX2") != std::string::npos) {
            board_type_ = BoardType::JetsonTX2;
        } else if (line.find("Jetson") != std::string::npos) {
            board_type_ = BoardType::JetsonNano;
        } else {
            board_type_ = BoardType::GenericLinux;
        }
        return;
    }

    // Check Tegra release
    std::ifstream tegra("/etc/nv_tegra_release");
    if (tegra.is_open()) {
        board_type_ = BoardType::JetsonNano;
        model_name_ = "NVIDIA Jetson Tegra Linux";
        return;
    }

    board_type_ = BoardType::GenericLinux;
    model_name_ = "Generic Linux SBC / PC";
#endif
}

ThermalStatus HardwareMonitor::read_thermal() {
    ThermalStatus status{};

#if !defined(_WIN32) && !defined(__APPLE__)
    // Try reading Jetson sysfs thermal zones
    bool found_real = false;
    for (int zone = 0; zone < 6; ++zone) {
        std::string type_path = "/sys/devices/virtual/thermal/thermal_zone" + std::to_string(zone) + "/type";
        std::string temp_path = "/sys/devices/virtual/thermal/thermal_zone" + std::to_string(zone) + "/temp";

        std::ifstream ftype(type_path);
        std::ifstream ftemp(temp_path);
        if (ftype.is_open() && ftemp.is_open()) {
            std::string type_name;
            float temp_raw = 0.0f;
            std::getline(ftype, type_name);
            ftemp >> temp_raw;
            float temp_c = temp_raw / 1000.0f;

            if (type_name.find("CPU") != std::string::npos || type_name.find("cpu") != std::string::npos) {
                status.cpu_temp_celsius = temp_c;
                found_real = true;
            } else if (type_name.find("GPU") != std::string::npos || type_name.find("gpu") != std::string::npos) {
                status.gpu_temp_celsius = temp_c;
                found_real = true;
            } else if (type_name.find("Tboard") != std::string::npos || type_name.find("thermal") != std::string::npos) {
                status.board_temp_celsius = temp_c;
                found_real = true;
            } else if (type_name.find("Tdiode") != std::string::npos) {
                status.sensor_temp_celsius = temp_c;
                found_real = true;
            }
        }
    }
    if (found_real) {
        return status;
    }
#endif

    // Simulation / Fallback with natural jitter
    static std::mt19937 rng(42);
    std::uniform_real_distribution<float> jitter(-0.2f, 0.2f);
    status.cpu_temp_celsius    = sim_temp_ + jitter(rng);
    status.gpu_temp_celsius    = sim_temp_ + 1.2f + jitter(rng);
    status.board_temp_celsius  = sim_temp_ - 1.5f + jitter(rng);
    status.sensor_temp_celsius = sim_temp_ - 2.0f + jitter(rng);
    return status;
}

BatteryStatus HardwareMonitor::read_battery() {
    BatteryStatus status{};

#if !defined(_WIN32) && !defined(__APPLE__)
    // Try reading Linux sysfs power supply
    std::ifstream fcap("/sys/class/power_supply/BAT0/capacity");
    std::ifstream fvolt("/sys/class/power_supply/BAT0/voltage_now");
    if (fcap.is_open() && fvolt.is_open()) {
        uint32_t cap = 0;
        uint32_t volt_uv = 0;
        fcap >> cap;
        fvolt >> volt_uv;
        status.percent = cap;
        status.voltage_mv = volt_uv / 1000;
        status.is_present = true;
        return status;
    }
#endif

    status.percent    = sim_batt_pct_;
    status.voltage_mv = sim_batt_mv_;
    status.current_ma = -420;
    status.is_present = true;
    status.is_charging = false;
    return status;
}

WirelessStatus HardwareMonitor::read_wireless() {
    WirelessStatus status{};

#if !defined(_WIN32) && !defined(__APPLE__)
    std::ifstream f_wireless("/proc/net/wireless");
    if (f_wireless.is_open()) {
        std::string line;
        // Skip 2 header lines
        std::getline(f_wireless, line);
        std::getline(f_wireless, line);
        if (std::getline(f_wireless, line)) {
            std::istringstream iss(line);
            std::string iface;
            int status_code, link, level, noise;
            if (iss >> iface >> status_code >> link >> level >> noise) {
                status.link_quality = static_cast<uint32_t>(link);
                status.rssi_dbm = level;
                return status;
            }
        }
    }
#endif

    status.rssi_dbm = sim_rssi_;
    status.link_quality = 90;
    status.ssid = "Rayence_Jetson_AP";
    return status;
}

AccelStatus HardwareMonitor::read_accel() {
    static std::mt19937 rng(1337);
    std::uniform_int_distribution<int16_t> noise(-4, 4);

    AccelStatus s{};
    s.x = noise(rng);
    s.y = noise(rng);
    s.z = static_cast<int16_t>(980 + noise(rng)); // ~1G
    s.shock_detected = false;
    return s;
}

bool HardwareMonitor::init_gpio(int pin_in, int pin_out) {
    gpio_in_pin_ = pin_in;
    gpio_out_pin_ = pin_out;

#if !defined(_WIN32) && !defined(__APPLE__)
    // Export pins if running on Linux / Jetson
    auto export_pin = [](int pin, const char* dir) {
        if (pin < 0) return;
        std::ofstream exp_f("/sys/class/gpio/export");
        if (exp_f.is_open()) {
            exp_f << pin;
        }
        std::string dir_path = "/sys/class/gpio/gpio" + std::to_string(pin) + "/direction";
        std::ofstream dir_f(dir_path);
        if (dir_f.is_open()) {
            dir_f << dir;
        }
    };
    export_pin(pin_in, "in");
    export_pin(pin_out, "out");
#endif
    return true;
}

bool HardwareMonitor::set_gpio_output(bool active) {
#if !defined(_WIN32) && !defined(__APPLE__)
    if (gpio_out_pin_ >= 0) {
        std::string val_path = "/sys/class/gpio/gpio" + std::to_string(gpio_out_pin_) + "/value";
        std::ofstream val_f(val_path);
        if (val_f.is_open()) {
            val_f << (active ? 1 : 0);
            return true;
        }
    }
#else
    (void)active;
#endif
    return true;
}

bool HardwareMonitor::read_gpio_input() {
#if !defined(_WIN32) && !defined(__APPLE__)
    if (gpio_in_pin_ >= 0) {
        std::string val_path = "/sys/class/gpio/gpio" + std::to_string(gpio_in_pin_) + "/value";
        std::ifstream val_f(val_path);
        if (val_f.is_open()) {
            int val = 0;
            val_f >> val;
            return val != 0;
        }
    }
#endif
    return false;
}

void HardwareMonitor::set_simulated_temp(float temp_celsius) {
    sim_temp_ = temp_celsius;
}

void HardwareMonitor::set_simulated_battery(uint32_t percent, uint32_t voltage_mv) {
    sim_batt_pct_ = percent;
    sim_batt_mv_ = voltage_mv;
}

void HardwareMonitor::set_simulated_rssi(int32_t rssi_dbm) {
    sim_rssi_ = rssi_dbm;
}

} // namespace mellzi::platform
