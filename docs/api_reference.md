# Mellzi SDK API Reference

This document provides a comprehensive technical reference for the Mellzi SDK, covering both the modern **C++20 Object-Oriented API** and the **Legacy C ABI** (`VDACQ_*`, `VDC_*`, `VDIP_*`, `VD_*`) designed for binary and source compatibility with existing Rayence DaVinci applications.

---

## 1. Overview & Header Organization

| Header | Description | Language Standard |
|:---|:---|:---|
| `mellzi/client.h` | Top-level unified client orchestrator (`mellzi::DetectorClient`) | C++20 |
| `mellzi/acquisition.h` | Low-level TCP acquisition client (`mellzi::client::AcquisitionClient`) | C++20 |
| `mellzi/calibration.h` | Dark, flat-field, and bad-pixel calibration engine (`mellzi::calib::CalibrationEngine`) | C++20 |
| `mellzi/image_process.h` | 16-bit image container, statistics, window/level LUT, despeckle | C++20 |
| `mellzi/server.h` | Embedded daemon server for NVIDIA Jetson (`mellzi::server::JetsonServer`) | C++20 |
| `mellzi/platform.h` | Jetson sysfs thermal, I2C battery, GPIO sync (`mellzi::platform::HardwareMonitor`) | C++20 |
| `mellzi/config.h` | Parameter mappings for `.initcfg`, `.elsetcfg`, and `VADav.INI` | C++20 |
| `mellzi/protocol.h` | 132-byte packet layout and command opcodes | C++20 / C |
| `mellzi/api.h` | Complete C ABI compatibility layer (`VDACQ_*`, `VDC_*`, `VDIP_*`, `VD_*`) | C99 / C++ |

---

## 2. Modern C++20 Core SDK API

### 2.1 `mellzi::DetectorClient`
Defined in `<mellzi/client.h>`.  
The primary client orchestrator for high-level image acquisition, automatic calibration application, and subsystem configuration.

#### Constructor & Lifecycle
```cpp
DetectorClient();
~DetectorClient() = default;
DetectorClient(const DetectorClient&) = delete;
DetectorClient& operator=(const DetectorClient&) = delete;
```

#### Connection Management
- `bool connect(std::string_view host = "127.0.0.1", uint16_t ctrl_port = 20000, uint16_t frame_port = 20001);`  
  Establishes concurrent TCP connections to the detector control and streaming endpoints. Returns `true` on success.
- `void disconnect();`  
  Terminates control and frame streams gracefully.
- `bool is_connected() const noexcept;`  
  Returns connection state across both sockets.

#### Subsystem Accessors
- `mellzi::client::AcquisitionClient& acquisition() noexcept;`
- `mellzi::calib::CalibrationEngine& calibration() noexcept;`
- `mellzi::config::DetectorSettings& settings() noexcept;`

#### Image Acquisition
- `bool acquire_raw_frame(image::Image& out_frame, std::chrono::milliseconds timeout = 10000ms);`  
  Triggers exposure (`CAPTURE_START`), awaits raw 16-bit frame transfer on port 20001, and returns uncalibrated image.
- `bool acquire_calibrated_frame(image::Image& out_frame, uint32_t calib_flags = calib::CALIB_ALL, std::chrono::milliseconds timeout = 10000ms);`  
  Acquires raw frame and executes offset subtraction, gain correction, BPM defect interpolation, and margin cropping.

#### Calibration Workflows
- `bool perform_dark_calibration(int num_frames = 5, int skip_frames = 1);`  
  Acquires multiple non-exposed dark frames, averages them, and updates the calibration engine offset map.
- `bool perform_bright_calibration(int num_frames = 4);`  
  Acquires flat-field exposed frames, generates gain correction coefficients, and recalculates bad pixel masks.
- `bool load_calibration_directory(const std::filesystem::path& dir);`  
  Loads `dark.raw`, `bright.raw`, `gain.bin`, and `bpm.bpm` from the designated folder.
- `bool save_calibration_directory(const std::filesystem::path& dir);`  
  Serializes active calibration maps to disk.

#### Telemetry
- `client::Telemetry get_telemetry() const;`  
  Returns the latest sampled battery, thermal, wireless RSSI, and accelerometer readings.

---

### 2.2 `mellzi::client::AcquisitionClient`
Defined in `<mellzi/acquisition.h>`.  
Handles packet framing, command dispatching, asynchronous telemetry packet parsing, and raw frame streaming over dual sockets.

#### Command Dispatch
- `bool send_command(uint32_t cmd_id, std::span<const uint8_t> payload = {});`  
  Sends a 132-byte packet to port 20000 without waiting for a reply.
- `bool send_command_wait_resp(uint32_t cmd_id, protocol::Packet& resp_out, std::chrono::milliseconds timeout = 3000ms);`  
  Sends a command and synchronously awaits the corresponding response code from the detector.

#### Telemetry & Callbacks
- `void set_telemetry_callback(std::function<void(const Telemetry&)> cb);`  
  Registers a callback invoked upon receipt of telemetry packets (`CAPTURE_BATTERY_REMAIN`, `CAPTURE_THERMAL_INFO`, `CAPTURE_WLAN_RSSI`, `CAPTURE_AXIS_INFO`).
- `void set_progress_callback(std::function<void(uint32_t percent)> cb);`  
  Monitors network stream transfer progress during 22.1 MB frame readout.

#### Low-Level Geometry & Config
- `void set_frame_dimensions(uint32_t width, uint32_t height);`  
  Configures expected image dimensions (defaults to 3328 x 3328).
- `bool download_init_config(std::span<uint8_t, 32> init_cfg_out);`  
  Reads 32-byte `/home/.initcfg` from the detector via `CONFIG_INIT_UP`.
- `bool upload_init_config(std::span<const uint8_t, 32> init_cfg_in);`  
  Writes 32-byte `/home/.initcfg` to detector storage via `CONFIG_INIT_SAVE`.
- `bool download_elset_config(int index, std::span<uint8_t, 128> elset_cfg_out);`  
  Downloads `.elsetcfg` (indices 1, 2, or 3) via `CONFIG_ELSET_UP[2|3]`.
- `bool upload_elset_config(int index, std::span<const uint8_t, 128> elset_cfg_in);`  
  Uploads `.elsetcfg` (indices 1, 2, or 3) via `CONFIG_ELSET[2|3]_SAVE`.

---

### 2.3 `mellzi::calib::CalibrationEngine`
Defined in `<mellzi/calibration.h>`.  
Executes SIMD-accelerated dark offset subtraction, flat-field gain normalization, 8-neighbor defect interpolation, and margin cropping.

#### Calibration Flags
```cpp
enum CalibrationFlags : uint32_t {
    CALIB_NONE             = 0x00,
    CALIB_OFFSET           = 0x01, // Subtract dark frame with baseline preservation
    CALIB_GAIN             = 0x02, // Apply flat-field gain normalization
    CALIB_BPM              = 0x04, // Correct defective pixels using Bad Pixel Map
    CALIB_DESPECKLE        = 0x08, // Remove single-pixel impulse radiation spikes
    CALIB_NEG_COMPENSATION = 0x10, // Clamp negative values to baseline offset
    CALIB_DEFECT_CORR      = 0x20, // 2-line sensor defect correction
    CALIB_CROP             = 0x40, // Crop physical non-active margins
    CALIB_ALL              = 0x7F
};
```

#### Processing Methods
- `image::Image process(const image::Image& raw_in, uint32_t flags = CALIB_ALL) const;`  
  Processes an input frame out-of-place and returns the calibrated image.
- `void process_in_place(image::Image& image, uint32_t flags = CALIB_ALL) const;`  
  Applies the calibration pipeline directly within the provided buffer to eliminate memory allocations.
- `void correct_bad_pixels(image::Image& img) const;`  
  Performs 8-neighbor spatial filtering across all coordinates marked defective in the BPM map.

---

### 2.4 `mellzi::image::Image`
Defined in `<mellzi/image_process.h>`.  
Lightweight container for 16-bit uncompressed medical radiography data.

#### Key Operations
- `uint16_t* data() noexcept;` / `const uint16_t* data() const noexcept;`  
  Pointer to continuous 16-bit Little-Endian pixel buffer.
- `std::span<uint16_t> span() noexcept;`  
  Standard C++20 span over image pixels.
- `ImageStats compute_stats() const;`  
  Computes minimum, maximum, mean, and standard deviation across active pixels.
- `void apply_window_level(uint16_t center, uint16_t width, std::span<uint8_t> out_8bit) const;`  
  Maps 16-bit dynamic range to 8-bit grayscale for display following DICOM Window/Level rules.
- `void despeckle(int threshold = 350);`  
  Applies adaptive median filtering on high-energy impulse artifacts.
- `bool load_raw(const std::filesystem::path& path, uint32_t width, uint32_t height);`
- `bool save_raw(const std::filesystem::path& path) const;`
- `bool save_pgm(const std::filesystem::path& path) const;`
- `bool save_bmp(const std::filesystem::path& path, uint16_t center = 0, uint16_t width = 0) const;`

---

## 3. C ABI Compatibility Layer (`mellzi/api.h`)

The C ABI provides 100% binary and source compatibility with existing legacy Rayence `VADAV` integrations.

### 3.1 Status Codes
```c
#define MELLZI_OK               0
#define MELLZI_ERROR_FAILED    -1
#define MELLZI_ERROR_TIMEOUT   -2
#define MELLZI_ERROR_INVALID   -3
#define MELLZI_ERROR_NOT_CONN  -4
```

---

### 3.2 VDACQ Acquisition API

#### Connection
```c
int VDACQ_Connect(const char* ipAddr, int port);
```
Connects to the detector. `port` specifies the control port (default 20000); frame streaming automatically targets `port + 1`.

```c
int VDACQ_Connect2(const char* ipAddr, int ctrlPort, int framePort);
```
Connects to arbitrary control and frame streaming ports.

```c
int VDACQ_Close(void);
```
Gracefully terminates active sessions and closes sockets.

```c
int VDACQ_CheckConnection(void);
```
Returns `MELLZI_OK` if both control and streaming sockets are active.

#### Acquisition & Frame Transfer
```c
int VDACQ_StartFrame(void);
```
Sends `CAPTURE_START` (opcode `0x00002`) to begin integration and trigger readout.

```c
int VDACQ_GetFrame(uint16_t* pBuffer, int timeoutMs);
```
Waits up to `timeoutMs` milliseconds for frame streaming to complete and copies raw 16-bit pixels into `pBuffer`.

```c
int VDACQ_Abort(void);
```
Sends `CAPTURE_ABORT` (opcode `0x00003`) to interrupt ongoing exposure or streaming.

#### Geometry & Detector Information
```c
int VDACQ_SetFrameDim(int width, int height);
int VDACQ_GetFrameDim(int* pWidth, int* pHeight);
int VDACQ_GetDetectorIPAddr(char* pIpBuffer, int maxLen);
int VDACQ_SetDetectorIPAddr(const char* pNewIp);
int VDACQ_GetDetectorInfo(char* pInfoBuffer, int maxLen);
int VDACQ_GetBatteryRemaining(int* pPercent, int* pMilliVolts);
int VDACQ_GetQualityWireless(int* pRssi);
```

#### Flash Frame Storage
```c
int VDACQ_GetFrameNum(int* pCount);
int VDACQ_LoadFrame(int frameIndex, uint16_t* pBuffer, int timeoutMs);
int VDACQ_LoadFrameDelete(int frameIndex);
int VDACQ_LoadFrameWithName(const char* frameName, uint16_t* pBuffer, int timeoutMs);
```

#### Hardware Control & Direct Commands
```c
int VDACQ_ReDark(int count);
int VDACQ_Shutdown(void);
int VDACQ_SendCommand(int cmdId, const void* pPayload, int payloadSize);
int VDACQ_SendCommandParam(int cmdId, int param1, int param2);
```

---

### 3.3 VDC Calibration API

#### Directory & Mode Configuration
```c
int VDC_SetCalibrationDirectory(const char* dirPath);
int VDC_GetCalibrationDirectory(char* pDirBuffer, int maxLen);
int VDC_SetCalibrationInit(int initMode);
int VDC_GetCalibrationInit(int* pInitMode);
```

#### Margin Cropping (`ImgCut`)
```c
int VDC_SetImgCutParams(int left, int top, int right, int bottom);
int VDC_GetImgCutParams(int* pLeft, int* pTop, int* pRight, int* pBottom);
int VDC_CutImage(const uint16_t* pSrc, int srcW, int srcH, uint16_t* pDst, int* pDstW, int* pDstH);
```

#### Calibration Generation
```c
int VDC_GetDark(int numFrames, uint16_t* pDarkBuffer);
int VDC_GetDarkAllInitModes(void);
int VDC_GetBright(int numFrames, uint16_t* pBrightBuffer);
int VDC_GenerateBright(const char* brightFilePath);
int VDC_GenerateAuto(void);
```

#### Image Processing Pipeline
```c
int VDC_Process(const uint16_t* pRawIn, uint16_t* pProcessedOut, int width, int height, int flags);
int VDC_ProcessEx(const uint16_t* pRawIn, uint16_t* pProcessedOut, int width, int height, int flags, void* extraParams);
int VDC_Process_Preview(const uint16_t* pRawIn, uint16_t* pProcessedOut, int width, int height);
int VDC_Abort(void);
int VDC_Close(void);
```

---

### 3.4 VD High-Level Orchestration API

```c
int VD_GetImage(uint16_t* pBuffer, int timeoutMs);
int VD_GetImageCancel(void);
int VD_Calibration(int flags);
int VD_ConnectRestore(void);
int VD_GetShockLog(char* pLogBuffer, int maxLen);
int VD_GetHomeDirectory(char* pDirBuffer, int maxLen);

int VD_Set_Acquisition(void* acqParams);
int VD_Set_Calibration(void* calParams);
int VD_Set_ImgProcess(void* imgParams);
int VD_Set_Preview(void* previewParams);

int VD_LogOpen(const char* logFile);
int VD_LogMsg(const char* format, ...);
int VD_LogFlush(void);
int VD_LogClose(void);
```

---

## 4. Usage Examples

### 4.1 Modern C++20 Acquisition & Calibration
```cpp
#include <iostream>
#include "mellzi/client.h"

int main() {
    mellzi::DetectorClient client;

    // Connect to Rayence detector or Mellzi Jetson daemon
    if (!client.connect("192.168.1.100", 20000, 20001)) {
        std::cerr << "Failed to connect to detector\n";
        return -1;
    }

    // Load calibration assets
    client.load_calibration_directory("./calibration_1717scc");

    // Acquire fully calibrated 16-bit frame
    mellzi::image::Image frame;
    if (client.acquire_calibrated_frame(frame, mellzi::calib::CALIB_ALL)) {
        auto stats = frame.compute_stats();
        std::cout << "Acquired image: " << frame.width() << "x" << frame.height()
                  << " Mean: " << stats.mean << " StdDev: " << stats.std_dev << "\n";

        frame.save_raw("output_calibrated.raw");
        frame.save_bmp("output_preview.bmp", 2048, 4096);
    }

    client.disconnect();
    return 0;
}
```

### 4.2 Legacy C ABI Acquisition
```c
#include <stdio.h>
#include <stdlib.h>
#include "mellzi/api.h"

int main() {
    if (VDACQ_Connect("192.168.1.100", 20000) != MELLZI_OK) {
        printf("Failed to connect\n");
        return -1;
    }

    int width = 3328, height = 3328;
    VDACQ_SetFrameDim(width, height);

    uint16_t* pRawBuffer = (uint16_t*)malloc(width * height * sizeof(uint16_t));
    uint16_t* pCalBuffer = (uint16_t*)malloc(width * height * sizeof(uint16_t));

    if (VDACQ_StartFrame() == MELLZI_OK) {
        if (VDACQ_GetFrame(pRawBuffer, 10000) == MELLZI_OK) {
            printf("Frame streaming completed successfully\n");

            VDC_SetCalibrationDirectory("./calibration");
            VDC_Process(pRawBuffer, pCalBuffer, width, height, 0x7F);
        }
    }

    free(pRawBuffer);
    free(pCalBuffer);
    VDACQ_Close();
    return 0;
}
```
