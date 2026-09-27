# Mellzi: OpenDaVinci C++20 SDK & NVIDIA Jetson Server

Mellzi is an open-source, cross-platform C++20 software development kit (SDK) and server daemon for **Rayence / DaVinci** Flat Panel X-ray Detectors (FPD), specifically designed for embedded deployment on **NVIDIA Jetson** platforms (Orin, Xavier, Nano, TX2) and general Linux/Windows/macOS hosts.

The project provides a binary- and source-compatible drop-in replacement for the proprietary Rayence DaVinci SDK (`VADAV.dll`), removing vendor platform restrictions while offering a modern C++20 interface.

---

## Architecture Overview

The system consists of two primary components:

1. **Jetson Server Daemon (`mellzi_server`)**: Implements the detector-side network protocol, hardware abstraction layer (HAL) for NVIDIA Jetson, and frame streaming engine.
2. **Client SDK (`mellzi_static`, `mellzi.dll` / `libmellzi.so`)**: Implements network communication, telemetry parsing, an automated calibration pipeline, and image post-processing with a dual API (modern C++20 and legacy C ABI).

---

## Technical Specifications

### Network Protocol
- **Transport**: TCP/IP
- **Control Port (20000)**: Bidirectional command exchange (`CtrlTask`), configuration upload/download, and asynchronous telemetry streaming.
- **Frame Port (20001)**: Dedicated uncompressed 16-bit raw pixel streaming (`SendTask`), terminated by a `0xFF` (`CAPTURE_FRAME_DONE`) message.
- **Packet Structure**: Fixed 132-byte frames (`uint32_t command_id` followed by a 128-byte payload).
- **Default Geometry**: $3328 \times 3328$ pixels, 16 bits per pixel (22,151,168 bytes per raw frame).

### Protocol Command Sets
- **AUX (`0x10000..0x10082`)**: Version inspection (Firmware, FPGA, Main, TFTP, CSIS), network parameters (IP, MAC), serial numbers, exposure counters, and power states.
- **BAK (`0x20000..0x20024`)**: Panel configuration registers and EEPROM access (`ELAD`, `ELAF`, `WMBD`, `VPNL`, `VCSI`).
- **CAPTURE (`0x00001..0x000FF`)**: Acquisition control (`READYIN`, `START`, `ABORT`, `READY_DONE`) and cyclic telemetry delivery (Battery `0x19`, Wi-Fi RSSI `0x1A`, Thermals `0x31`, 3-axis Accelerometer `0x59`).
- **CONFIG (`0x00005..0x00045`)**: Upload and persistence of `.initcfg` (32 bytes) and `.elsetcfg`, `.elsetcfg2`, `.elsetcfg3` (128 bytes each).

### NVIDIA Jetson Hardware Integration
- **Platform Detection**: Automatic identification of Jetson architecture via `/proc/device-tree/model` and `/etc/nv_tegra_release`.
- **Thermal Monitoring**: Hardware temperature acquisition from `/sys/devices/virtual/thermal/thermal_zone*` (CPU, GPU, and board thermals).
- **Smart Battery Support**: Native SMBus/I2C interfacing with BQ20Z95 battery management ICs via `/dev/i2c-*` or Linux power supply subsystem.
- **Hardware Synchronization**: Real-time generator exposure synchronization via Jetson GPIOs (`sysfs` / `libgpiod`).

### Calibration Pipeline
- **Offset (Dark) Correction**: Multi-frame averaging and pixel-wise dark current subtraction with baseline shift compensation to prevent negative underflow.
- **Gain (Flat-Field) Normalization**: High-precision sensitivity correction:
  $$Gain(x,y) = \frac{\langle Bright - Dark \rangle}{Bright(x,y) - Dark(x,y)}$$
- **Bad Pixel Map (BPM)**: Statistical defect detection (dead, saturated, and out-of-distribution noise pixels) with spatial 8-connected neighbor reconstruction.
- **Overscan Trimming**: Configurable margin cropping (`ImgCutLeft`, `ImgCutTop`, `ImgCutRight`, `ImgCutBottom`).

---

## Build Instructions

### Prerequisites
- C++20 compliant compiler (Clang $\ge$ 14, GCC $\ge$ 11, or MSVC $\ge$ 19.29)
- CMake $\ge$ 3.20
- Ninja or Make

### Linux (NVIDIA Jetson / x86_64)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

### Windows (MSVC / Clang)
```powershell
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Deployment on NVIDIA Jetson

### Service Installation
Install the systemd unit for background execution with real-time priority:
```bash
sudo cp build/mellzi_server /opt/mellzi/bin/mellzi_server
sudo cp config/VADav.INI /opt/mellzi/config/VADav.INI
sudo cp systemd/mellzi-server.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now mellzi-server.service
```

### Standalone Execution
```bash
./build/mellzi_server --port-ctrl 20000 --port-frame 20001 --width 3328 --height 3328 --pattern gradient --verbose
```

Available arguments:
- `--port-ctrl <port>`: Control command port (default: `20000`)
- `--port-frame <port>`: Image data port (default: `20001`)
- `--width <px>`: Frame width (default: `3328`)
- `--height <px>`: Frame height (default: `3328`)
- `--pattern <gradient|checker|flat>`: Synthetic pattern generator mode
- `--raw-template <path>`: Source file for frame replay
- `--config-dir <path>`: Configuration directory for `VADav.INI` and `.initcfg`

---

## Command-Line Interface (CLI)

The `mellzi_cli` utility allows operational verification and offline acquisition:
```bash
# Query detector identity and telemetry
./build/mellzi_cli info --host 192.168.1.80

# Capture raw detector frame
./build/mellzi_cli capture --host 192.168.1.80 --out frame.bmp

# Acquire and calibrate against a calibration directory
./build/mellzi_cli calibrate --host 192.168.1.80 --calib-dir /path/to/calib --out frame_calibrated.bmp
```

---

## Programming Interfaces

### Modern C++20 Interface
```cpp
#include "mellzi/client.h"
#include <iostream>

int main() {
    mellzi::DetectorClient client;

    if (!client.connect("192.168.1.80", 20000, 20001)) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    client.load_calibration_directory("/opt/mellzi/calib");

    mellzi::image::Image frame;
    if (client.acquire_calibrated_frame(frame, mellzi::calib::CALIB_ALL)) {
        frame.save_bmp("radiograph.bmp");
    }

    client.disconnect();
    return 0;
}
```

### Legacy C ABI Interface (Rayence `VADAV.dll` Replacement)
```c
#include "mellzi/api.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    if (VDACQ_Connect("192.168.1.80", 20000) != MELLZI_OK) {
        return 1;
    }

    int width = 0, height = 0;
    VDACQ_GetFrameDim(&width, &height);

    uint16_t* buffer = (uint16_t*)malloc(width * height * sizeof(uint16_t));
    if (VD_GetImage(buffer, 10000) == MELLZI_OK) {
        printf("Acquired %dx%d image successfully\n", width, height);
    }

    free(buffer);
    VDACQ_Close();
    return 0;
}
```

---

## Verification & Performance

- **Automated Tests**: Complete test suite passed (`ProtocolTest`, `ConfigTest`, `CalibrationTest`, `NetworkTest`).
- **Transfer Latency**: Full $3328 \times 3328$ 16-bit frame transfer (~22.1 MB) achieved in **~160 ms** over local network.
- **Processing Overhead**: Complete calibration pipeline (Dark subtraction, Gain normalization, BPM interpolation, and Cropping) executes in **~25 ms**.

---

## License

MIT License.
