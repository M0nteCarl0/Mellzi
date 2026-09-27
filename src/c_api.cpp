#define _CRT_SECURE_NO_WARNINGS
#include "mellzi/api.h"
#include "mellzi/client.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
    std::mutex g_api_mutex;
    std::unique_ptr<mellzi::DetectorClient> g_client;
    std::string g_calib_dir = "./calib";
    std::ofstream g_log_file;

    mellzi::DetectorClient& get_client() {
        if (!g_client) {
            g_client = std::make_unique<mellzi::DetectorClient>();
        }
        return *g_client;
    }
}

MELLZI_EXTERN_C {

int VDACQ_Connect(const char* ipAddr, int port) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!ipAddr) return MELLZI_ERROR_INVALID;
    return get_client().connect(ipAddr, static_cast<uint16_t>(port), static_cast<uint16_t>(port + 1)) ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDACQ_Connect2(const char* ipAddr, int ctrlPort, int framePort) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!ipAddr) return MELLZI_ERROR_INVALID;
    return get_client().connect(ipAddr, static_cast<uint16_t>(ctrlPort), static_cast<uint16_t>(framePort)) ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDACQ_Close(void) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (g_client) {
        g_client->disconnect();
    }
    return MELLZI_OK;
}

int VDACQ_CheckConnection(void) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    return (g_client && g_client->is_connected()) ? MELLZI_OK : MELLZI_ERROR_NOT_CONN;
}

int VDACQ_StartFrame(void) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    return get_client().acquisition().start_acquisition() ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDACQ_GetFrame(uint16_t* pBuffer, int timeoutMs) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pBuffer) return MELLZI_ERROR_INVALID;

    mellzi::image::Image frame;
    if (!get_client().acquire_raw_frame(frame, std::chrono::milliseconds(timeoutMs))) {
        return MELLZI_ERROR_FAILED;
    }

    std::memcpy(pBuffer, frame.data(), frame.byte_size());
    return MELLZI_OK;
}

int VDACQ_Abort(void) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    return get_client().acquisition().abort_acquisition() ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDACQ_SetFrameDim(int width, int height) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    get_client().acquisition().set_frame_dimensions(width, height);
    return MELLZI_OK;
}

int VDACQ_GetFrameDim(int* pWidth, int* pHeight) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pWidth || !pHeight) return MELLZI_ERROR_INVALID;
    uint32_t w = 0, h = 0;
    get_client().acquisition().get_frame_dimensions(w, h);
    *pWidth = static_cast<int>(w);
    *pHeight = static_cast<int>(h);
    return MELLZI_OK;
}

int VDACQ_GetDetectorIPAddr(char* pIpBuffer, int maxLen) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pIpBuffer || maxLen <= 0) return MELLZI_ERROR_INVALID;
    std::string ip;
    if (get_client().acquisition().query_device_ip(ip)) {
        strncpy(pIpBuffer, ip.c_str(), maxLen - 1);
        pIpBuffer[maxLen - 1] = 0;
        return MELLZI_OK;
    }
    return MELLZI_ERROR_FAILED;
}

int VDACQ_SetDetectorIPAddr(const char* pNewIp) {
    (void)pNewIp;
    return MELLZI_OK;
}

int VDACQ_GetDetectorInfo(char* pInfoBuffer, int maxLen) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pInfoBuffer || maxLen <= 0) return MELLZI_ERROR_INVALID;

    std::string fw, fpga, main_v;
    get_client().acquisition().query_version_firmware(fw);
    get_client().acquisition().query_version_fpga(fpga);
    get_client().acquisition().query_version_main(main_v);

    std::string info = "FW: " + fw + ", FPGA: " + fpga + ", Main: " + main_v;
    strncpy(pInfoBuffer, info.c_str(), maxLen - 1);
    pInfoBuffer[maxLen - 1] = 0;
    return MELLZI_OK;
}

int VDACQ_GetBatteryRemaining(int* pPercent, int* pMilliVolts) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    uint32_t pct = 0, mv = 0;
    if (get_client().acquisition().query_battery(pct, mv)) {
        if (pPercent) *pPercent = static_cast<int>(pct);
        if (pMilliVolts) *pMilliVolts = static_cast<int>(mv);
        return MELLZI_OK;
    }
    return MELLZI_ERROR_FAILED;
}

int VDACQ_GetQualityWireless(int* pRssi) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pRssi) return MELLZI_ERROR_INVALID;
    int32_t rssi = 0;
    if (get_client().acquisition().query_wireless(rssi)) {
        *pRssi = rssi;
        return MELLZI_OK;
    }
    return MELLZI_ERROR_FAILED;
}

int VDACQ_GetFrameNum(int* pCount) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pCount) return MELLZI_ERROR_INVALID;
    *pCount = 60;
    return MELLZI_OK;
}

int VDACQ_LoadFrame(int frameIndex, uint16_t* pBuffer, int timeoutMs) {
    (void)frameIndex;
    return VDACQ_GetFrame(pBuffer, timeoutMs);
}

int VDACQ_LoadFrameDelete(int frameIndex) {
    (void)frameIndex;
    return MELLZI_OK;
}

int VDACQ_LoadFrameWithName(const char* frameName, uint16_t* pBuffer, int timeoutMs) {
    (void)frameName;
    return VDACQ_GetFrame(pBuffer, timeoutMs);
}

int VDACQ_ReDark(int count) {
    (void)count;
    std::lock_guard<std::mutex> lock(g_api_mutex);
    return get_client().acquisition().send_command(static_cast<uint32_t>(mellzi::protocol::CaptureCmd::ReDark)) ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDACQ_Shutdown(void) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    return get_client().acquisition().send_command(static_cast<uint32_t>(mellzi::protocol::CaptureCmd::Shutdown)) ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDACQ_SendCommand(int cmdId, const void* pPayload, int payloadSize) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    std::span<const uint8_t> payload_span;
    if (pPayload && payloadSize > 0) {
        payload_span = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pPayload), payloadSize);
    }
    return get_client().acquisition().send_command(static_cast<uint32_t>(cmdId), payload_span) ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDACQ_SendCommandParam(int cmdId, int param1, int param2) {
    int params[2] = {param1, param2};
    return VDACQ_SendCommand(cmdId, params, sizeof(params));
}

int VDC_SetCalibrationDirectory(const char* dirPath) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!dirPath) return MELLZI_ERROR_INVALID;
    g_calib_dir = dirPath;
    get_client().calibration().load_all_from_directory(g_calib_dir);
    return MELLZI_OK;
}

int VDC_GetCalibrationDirectory(char* pDirBuffer, int maxLen) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pDirBuffer || maxLen <= 0) return MELLZI_ERROR_INVALID;
    strncpy(pDirBuffer, g_calib_dir.c_str(), maxLen - 1);
    pDirBuffer[maxLen - 1] = 0;
    return MELLZI_OK;
}

int VDC_SetCalibrationInit(int initMode) {
    (void)initMode;
    return MELLZI_OK;
}

int VDC_GetCalibrationInit(int* pInitMode) {
    if (!pInitMode) return MELLZI_ERROR_INVALID;
    *pInitMode = 1;
    return MELLZI_OK;
}

int VDC_GetImageDim(int* pWidth, int* pHeight) {
    return VDACQ_GetFrameDim(pWidth, pHeight);
}

int VDC_SetImgCutParams(int left, int top, int right, int bottom) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    auto p = get_client().calibration().get_params();
    p.cut.left = left;
    p.cut.top = top;
    p.cut.right = right;
    p.cut.bottom = bottom;
    get_client().calibration().set_params(p);
    return MELLZI_OK;
}

int VDC_GetImgCutParams(int* pLeft, int* pTop, int* pRight, int* pBottom) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    const auto& c = get_client().calibration().get_params().cut;
    if (pLeft) *pLeft = c.left;
    if (pTop) *pTop = c.top;
    if (pRight) *pRight = c.right;
    if (pBottom) *pBottom = c.bottom;
    return MELLZI_OK;
}

int VDC_CutImage(const uint16_t* pSrc, int srcW, int srcH,
                 uint16_t* pDst, int* pDstW, int* pDstH) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pSrc || !pDst || !pDstW || !pDstH) return MELLZI_ERROR_INVALID;

    mellzi::image::Image img(srcW, srcH, std::span<const uint16_t>(pSrc, static_cast<size_t>(srcW) * srcH));
    auto cropped = img.crop(get_client().calibration().get_params().cut);

    *pDstW = static_cast<int>(cropped.width());
    *pDstH = static_cast<int>(cropped.height());
    std::memcpy(pDst, cropped.data(), cropped.byte_size());
    return MELLZI_OK;
}

int VDC_GetDark(int numFrames, uint16_t* pDarkBuffer) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pDarkBuffer) return MELLZI_ERROR_INVALID;
    if (!get_client().perform_dark_calibration(numFrames)) return MELLZI_ERROR_FAILED;
    return MELLZI_OK;
}

int VDC_GetDarkAllInitModes(void) {
    return VDC_GetDark(5, nullptr);
}

int VDC_GetBright(int numFrames, uint16_t* pBrightBuffer) {
    (void)pBrightBuffer;
    std::lock_guard<std::mutex> lock(g_api_mutex);
    return get_client().perform_bright_calibration(numFrames) ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDC_GenerateBright(const char* brightFilePath) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!brightFilePath) return MELLZI_ERROR_INVALID;
    return get_client().calibration().load_bright_file(brightFilePath) ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VDC_GenerateAuto(void) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    get_client().calibration().generate_gain_map();
    get_client().calibration().generate_bpm_map();
    return MELLZI_OK;
}

int VDC_Process(const uint16_t* pRawIn, uint16_t* pProcessedOut,
                int width, int height, int flags) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pRawIn || !pProcessedOut) return MELLZI_ERROR_INVALID;

    mellzi::image::Image raw(width, height, std::span<const uint16_t>(pRawIn, static_cast<size_t>(width) * height));
    auto proc = get_client().calibration().process(raw, static_cast<uint32_t>(flags));
    std::memcpy(pProcessedOut, proc.data(), proc.byte_size());
    return MELLZI_OK;
}

int VDC_ProcessEx(const uint16_t* pRawIn, uint16_t* pProcessedOut,
                  int width, int height, int flags, void* extraParams) {
    (void)extraParams;
    return VDC_Process(pRawIn, pProcessedOut, width, height, flags);
}

int VDC_Process_Preview(const uint16_t* pRawIn, uint16_t* pProcessedOut, int width, int height) {
    return VDC_Process(pRawIn, pProcessedOut, width, height, mellzi::calib::CALIB_OFFSET | mellzi::calib::CALIB_BPM);
}

int VDC_Abort(void) {
    return MELLZI_OK;
}

int VDC_Close(void) {
    return MELLZI_OK;
}

int VDIP_Process(const uint16_t* pSrc, uint16_t* pDst, int w, int h, int filterMode) {
    if (!pSrc || !pDst) return MELLZI_ERROR_INVALID;
    mellzi::image::Image img(w, h, std::span<const uint16_t>(pSrc, static_cast<size_t>(w) * h));
    if (filterMode != 0) {
        img.despeckle();
    }
    std::memcpy(pDst, img.data(), img.byte_size());
    return MELLZI_OK;
}

int VDIP_Abort(void) { return MELLZI_OK; }
int VDIP_Close(void) { return MELLZI_OK; }

int VD_GetImage(uint16_t* pBuffer, int timeoutMs) {
    std::lock_guard<std::mutex> lock(g_api_mutex);
    if (!pBuffer) return MELLZI_ERROR_INVALID;
    mellzi::image::Image frame;
    if (!get_client().acquire_calibrated_frame(frame, mellzi::calib::CALIB_ALL, std::chrono::milliseconds(timeoutMs))) {
        return MELLZI_ERROR_FAILED;
    }
    std::memcpy(pBuffer, frame.data(), frame.byte_size());
    return MELLZI_OK;
}

int VD_GetImageCancel(void) {
    return VDACQ_Abort();
}

int VD_GetImageSP(uint16_t* pBuffer, int timeoutMs, void* specParams) {
    (void)specParams;
    return VD_GetImage(pBuffer, timeoutMs);
}

int VD_Calibration(int flags) {
    (void)flags;
    return VDC_GenerateAuto();
}

int VD_ConnectRestore(void) {
    return VDACQ_CheckConnection();
}

int VD_GetShockLog(char* pLogBuffer, int maxLen) {
    if (!pLogBuffer || maxLen <= 0) return MELLZI_ERROR_INVALID;
    strncpy(pLogBuffer, "No shock recorded", maxLen - 1);
    pLogBuffer[maxLen - 1] = 0;
    return MELLZI_OK;
}

int VD_GetHomeDirectory(char* pDirBuffer, int maxLen) {
    if (!pDirBuffer || maxLen <= 0) return MELLZI_ERROR_INVALID;
    strncpy(pDirBuffer, "./", maxLen - 1);
    pDirBuffer[maxLen - 1] = 0;
    return MELLZI_OK;
}

int VD_Set_Acquisition(void* acqParams) { (void)acqParams; return MELLZI_OK; }
int VD_Set_Calibration(void* calParams) { (void)calParams; return MELLZI_OK; }
int VD_Set_ImgProcess(void* imgParams) { (void)imgParams; return MELLZI_OK; }
int VD_Set_Preview(void* previewParams) { (void)previewParams; return MELLZI_OK; }

int VD_LogOpen(const char* logFile) {
    if (!logFile) return MELLZI_ERROR_INVALID;
    g_log_file.open(logFile, std::ios::app);
    return g_log_file.is_open() ? MELLZI_OK : MELLZI_ERROR_FAILED;
}

int VD_LogMsg(const char* format, ...) {
    if (!format) return MELLZI_ERROR_INVALID;
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (g_log_file.is_open()) {
        g_log_file << buffer << "\n";
        g_log_file.flush();
    } else {
        std::cout << "[Mellzi Log] " << buffer << "\n";
    }
    return MELLZI_OK;
}

int VD_LogFlush(void) {
    if (g_log_file.is_open()) g_log_file.flush();
    return MELLZI_OK;
}

int VD_LogClose(void) {
    if (g_log_file.is_open()) g_log_file.close();
    return MELLZI_OK;
}

int VDDBG_SendCommand(int cmd, const void* data, int size) {
    return VDACQ_SendCommand(cmd, data, size);
}

int VDINFO_Connect(const char* ip, int port) {
    return VDACQ_Connect(ip, port);
}

int VDSHARE_Connect(const char* ip, int port) {
    return VDACQ_Connect(ip, port);
}

} // MELLZI_EXTERN_C
