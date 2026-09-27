#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <span>
#include <string_view>
#include <bit>

namespace mellzi::protocol {

inline constexpr uint16_t DEFAULT_CONTROL_PORT = 20000;
inline constexpr uint16_t DEFAULT_FRAME_PORT   = 20001;

inline constexpr size_t PACKET_SIZE        = 132;
inline constexpr size_t PAYLOAD_SIZE       = 128;
inline constexpr size_t CMD_SIZE           = 4;

inline constexpr uint32_t DEFAULT_FRAME_WIDTH  = 3328;
inline constexpr uint32_t DEFAULT_FRAME_HEIGHT = 3328;
inline constexpr uint32_t DEFAULT_PIXEL_BYTES  = 2;
inline constexpr size_t   DEFAULT_FRAME_BYTES  = static_cast<size_t>(DEFAULT_FRAME_WIDTH) * DEFAULT_FRAME_HEIGHT * DEFAULT_PIXEL_BYTES; // 22,151,168 bytes

// Section tags (cmd >> 16)
enum class Section : uint16_t {
    CaptureOrConfig = 0x0000,
    Aux             = 0x0001, // 0x10000
    Bak             = 0x0002  // 0x20000
};

// Capture / General commands (Section 0)
enum class CaptureCmd : uint32_t {
    ReadyIn           = 0x01, // Client -> Server (Init trigger)
    Start             = 0x02, // Client -> Server (Start acquisition)
    Abort             = 0x03, // Client -> Server (Abort acquisition)
    ReadyDoneClient   = 0x07, // Client -> Server
    Shutdown          = 0x0B, // Client -> Server

    // Responses from Server (Section 0)
    InitialisationDone = 0x11, // Server -> Client (Init completed)
    StartDone          = 0x12, // Server -> Client (Start confirmed)
    AbortDone          = 0x13, // Server -> Client (Abort confirmed)
    ElsetDone          = 0x14, // Server -> Client (EL parameters set)
    ReadyDone          = 0x17, // Server -> Client (Detector ready for X-ray)
    BatteryRemain      = 0x19, // Server -> Client (Telemetry)
    WlanRssi           = 0x1A, // Server -> Client (Telemetry)
    ShutdownDone       = 0x1B, // Server -> Client
    ReDarkDone         = 0x20, // Server -> Client (Dynamic dark offset updated)
    XRayStart          = 0x21, // Server -> Client
    XRayStop           = 0x22, // Server -> Client
    ReadyOn            = 0x23, // Server -> Client
    ReadyOff           = 0x24, // Server -> Client
    ThermalInfo        = 0x31, // Server -> Client (Telemetry: T1, T2)
    AutoTriggerInfo    = 0x51, // Server -> Client
    AutoTriggerReady   = 0x52, // Server -> Client
    MsgAtMode          = 0x53, // Server -> Client
    DoSaturation       = 0x54, // Server -> Client
    DoShift            = 0x55, // Server -> Client (Pre-frame transfer signal)
    ReDark             = 0x56, // Server -> Client
    AutoTriggerWait    = 0x57, // Server -> Client
    NoRemainFrame      = 0x58, // Server -> Client
    AxisInfo           = 0x59, // Server -> Client (Telemetry: Accelerometer X, Y, Z)
    GainBinning        = 0x60, // Server -> Client
    RecvPercents       = 0x05, // Server -> Client (Progress indicator)
    FrameDone          = 0xFF  // Server -> Client (Frame transmission complete)
};

// Configuration Commands (Section 0)
enum class ConfigCmd : uint32_t {
    InitUpload         = 0x05, // Client requests .initcfg (32 bytes)
    ElsetUpload        = 0x06, // Client requests .elsetcfg (128 bytes)
    InitSave           = 0x0C, // Client sends .initcfg to save
    ElsetSave          = 0x0D, // Client sends .elsetcfg to save
    Elset2Save         = 0x32, // Client sends .elsetcfg2 to save
    Elset3Save         = 0x33, // Client sends .elsetcfg3 to save
    Elset2Upload       = 0x34, // Client requests .elsetcfg2 (128 bytes)
    Elset3Upload       = 0x35, // Client requests .elsetcfg3 (128 bytes)

    // Config responses
    InitUploadDone     = 0x15, // Server sends 32 bytes init config
    ElsetUploadDone    = 0x16, // Server sends 128 bytes elset config
    Elset2UploadDone   = 0x44, // Server sends 128 bytes elset2 config
    Elset3UploadDone   = 0x45, // Server sends 128 bytes elset3 config
    SaveSuccess        = 0x01  // Server confirms save
};

// AUX Commands (Section 1: 0x10000 range)
// Client requests using 0x10000 | Code, Server replies using 0x11000 | Code
enum class AuxCmd : uint32_t {
    ReqDefStat         = 0x10000,
    RespDefStat        = 0x11000,
    ReqLedStat         = 0x10001,
    RespLedStat        = 0x11001,
    ReqSetTime         = 0x10002,
    ReqGainCal         = 0x10003,
    RespGainCal        = 0x11003,

    ReqVerFirmware     = 0x10010,
    RespVerFirmware    = 0x11010,
    ReqVerFpga         = 0x10011,
    RespVerFpga        = 0x11011,
    ReqVerMain         = 0x10012,
    RespVerMain        = 0x11012,
    ReqVerTftp         = 0x10013,
    RespVerTftp        = 0x11013,
    ReqVerCsis         = 0x10014,
    RespVerCsis        = 0x11014,
    ReqVerLicense      = 0x10015,
    RespVerLicense     = 0x11015,
    ReqVerIp           = 0x10016,
    RespVerIp          = 0x11016,
    ReqVerMac          = 0x10017,
    RespVerMac         = 0x11017,

    ReqSelfElst        = 0x10020,
    RespSelfElst       = 0x11020,
    ReqSelfElam        = 0x10021,
    RespSelfElam       = 0x11021,
    ReqSelfPlst        = 0x10023,
    RespSelfPlst       = 0x11023,
    ReqSelfXtst        = 0x10024,
    RespSelfXtst       = 0x11024,

    ReqDetectorStat    = 0x10025,
    RespDetectorStat   = 0x11025,
    ReqFrameNum        = 0x10026,
    RespFrameNum       = 0x11026,
    ReqFrameLoad       = 0x10027,
    RespFrameLoad      = 0x11027,
    ReqRemainFrameLoad = 0x10028,
    RespRemainFrameLoad= 0x11028,
    ReqRemainFrameRemove = 0x10029,
    RespRemainFrameRemove= 0x11029,

    ReqTestPatternStart= 0x10030,
    RespTestPatternStart= 0x11030,
    ReqResetHardware   = 0x10040,
    RespResetHardware  = 0x11040,
    ReqImageResend     = 0x10050,
    RespImageResend    = 0x11050,
    ReqGetCount        = 0x10060,
    RespGetCount       = 0x11060,
    ReqCheckFirmware   = 0x10070,
    RespCheckFirmware  = 0x11070,

    ReqSleep           = 0x10080,
    RespSleep          = 0x10080,
    ReqWakeup          = 0x10081,
    RespWakeup         = 0x10081,
    ReqBattery         = 0x10082,
    RespBattery        = 0x10082
};

// BAK Commands (Section 2: 0x20000 range)
// Client sends 0x20000 | Code, Server replies 0x22000 | Code
enum class BakCmd : uint32_t {
    ReqEnterBackupMode = 0x20000,
    ReqWriteWmbd       = 0x20012,
    RespWriteWmbd      = 0x22012,
    ReqWriteVpnl       = 0x20014,
    RespWriteVpnl      = 0x22014,
    ReqWriteVcsi       = 0x20016,
    RespWriteVcsi      = 0x22016,

    ReqReadElad        = 0x20021,
    RespReadElad       = 0x22021,
    ReqWriteElad       = 0x20022,
    RespWriteElad      = 0x22022,

    ReqReadElaf        = 0x20023,
    RespReadElaf       = 0x22023,
    ReqWriteElaf       = 0x20024,
    RespWriteElaf      = 0x22024
};

#pragma pack(push, 1)

// Raw wire representation of standard DaVinci 132-byte packet
struct Packet {
    uint32_t command_id{0};
    uint8_t  payload[PAYLOAD_SIZE]{0};

    [[nodiscard]] constexpr Section get_section() const noexcept {
        return static_cast<Section>(command_id >> 16);
    }

    [[nodiscard]] constexpr uint16_t get_sub_command() const noexcept {
        return static_cast<uint16_t>(command_id & 0xFFFF);
    }

    void set_command(uint32_t id) noexcept {
        command_id = id;
    }

    void clear() noexcept {
        command_id = 0;
        for (auto& b : payload) {
            b = 0;
        }
    }

    void set_string(std::string_view str) noexcept {
        size_t n = str.size() < PAYLOAD_SIZE - 1 ? str.size() : PAYLOAD_SIZE - 1;
        for (size_t i = 0; i < n; ++i) {
            payload[i] = static_cast<uint8_t>(str[i]);
        }
        payload[n] = 0;
    }

    [[nodiscard]] std::string_view as_string() const noexcept {
        size_t len = 0;
        while (len < PAYLOAD_SIZE && payload[len] != 0) {
            ++len;
        }
        return std::string_view(reinterpret_cast<const char*>(payload), len);
    }
};

static_assert(sizeof(Packet) == PACKET_SIZE, "Protocol Packet must be exactly 132 bytes");

// Telemetry payloads
struct BatteryPayload {
    uint32_t percent;       // 0..100 %
    uint32_t voltage_mv;    // e.g. 11100 mV
    int32_t  current_ma;    // charge / discharge current
    uint32_t flags;         // AC connected, charging, etc.
};

struct ThermalPayload {
    int16_t  temp1_celsius_x10; // T1 * 10 (e.g. 365 = 36.5 C)
    int16_t  temp2_celsius_x10; // T2 * 10
    uint32_t reserved[31];
};

struct WirelessPayload {
    int32_t  rssi_dbm;      // e.g. -45 dBm
    uint32_t link_quality;  // 0..100%
    uint32_t reserved[30];
};

struct AxisPayload {
    int16_t  x_axis;        // Accelerometer X
    int16_t  y_axis;        // Accelerometer Y
    int16_t  z_axis;        // Accelerometer Z
    int16_t  shock_detected;// 1 if shock threshold exceeded
    uint32_t reserved[30];
};

struct CountPayload {
    uint32_t total_acquisitions;
    uint32_t dark_acquisitions;
    uint32_t bright_acquisitions;
    uint32_t error_count;
};

#pragma pack(pop)

// Endian conversion
[[nodiscard]] constexpr uint32_t swap32(uint32_t v) noexcept {
    return ((v & 0x000000FFU) << 24) |
           ((v & 0x0000FF00U) << 8)  |
           ((v & 0x00FF0000U) >> 8)  |
           ((v & 0xFF000000U) >> 24);
}

[[nodiscard]] constexpr int32_t swap32_signed(int32_t v) noexcept {
    return static_cast<int32_t>(swap32(static_cast<uint32_t>(v)));
}

} // namespace mellzi::protocol
