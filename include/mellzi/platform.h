#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <filesystem>

namespace mellzi::platform {

enum class BoardType {
    Unknown,
    JetsonNano,
    JetsonTX2,
    JetsonXavier,
    JetsonOrin,
    GenericLinux,
    WindowsHost,
    MacOSHost
};

struct ThermalStatus {
    float cpu_temp_celsius{35.0f};
    float gpu_temp_celsius{36.0f};
    float board_temp_celsius{34.0f};
    float sensor_temp_celsius{32.0f};
};

struct BatteryStatus {
    uint32_t percent{95};
    uint32_t voltage_mv{11800};
    int32_t  current_ma{-450};
    bool     is_charging{false};
    bool     is_present{true};
};

struct WirelessStatus {
    int32_t  rssi_dbm{-42};
    uint32_t link_quality{92};
    std::string ssid{"Rayence_AP"};
};

struct AccelStatus {
    int16_t x{10};
    int16_t y{-5};
    int16_t z{981}; // ~1G
    bool shock_detected{false};
};

class HardwareMonitor {
public:
    HardwareMonitor();
    ~HardwareMonitor() = default;

    [[nodiscard]] BoardType get_board_type() const noexcept { return board_type_; }
    [[nodiscard]] std::string_view get_board_name() const noexcept;

    // Telemetry getters (reads real Jetson sysfs/i2c or provides accurate simulation)
    [[nodiscard]] ThermalStatus read_thermal();
    [[nodiscard]] BatteryStatus read_battery();
    [[nodiscard]] WirelessStatus read_wireless();
    [[nodiscard]] AccelStatus read_accel();

    // Jetson GPIO controls for X-Ray Triggering
    bool init_gpio(int pin_in, int pin_out);
    bool set_gpio_output(bool active);
    [[nodiscard]] bool read_gpio_input();

    // Simulation overrides
    void set_simulated_temp(float temp_celsius);
    void set_simulated_battery(uint32_t percent, uint32_t voltage_mv);
    void set_simulated_rssi(int32_t rssi_dbm);

private:
    BoardType board_type_{BoardType::Unknown};
    std::string model_name_{"Standard System"};
    int gpio_in_pin_{-1};
    int gpio_out_pin_{-1};

    float sim_temp_{36.0f};
    uint32_t sim_batt_pct_{95};
    uint32_t sim_batt_mv_{11800};
    int32_t sim_rssi_{-45};

    void detect_hardware();
};

} // namespace mellzi::platform
