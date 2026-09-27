#pragma once

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(MELLZI_BUILDING_DLL)
        #define MELLZI_API __declspec(dllexport)
    #elif defined(MELLZI_USING_DLL)
        #define MELLZI_API __declspec(dllimport)
    #else
        #define MELLZI_API
    #endif
#else
    #if defined(MELLZI_BUILDING_DLL)
        #define MELLZI_API __attribute__((visibility("default")))
    #else
        #define MELLZI_API
    #endif
#endif

#ifdef __cplusplus
    #define MELLZI_EXTERN_C extern "C"
#else
    #define MELLZI_EXTERN_C
#endif

#define MELLZI_OK               0
#define MELLZI_ERROR_FAILED    -1
#define MELLZI_ERROR_TIMEOUT   -2
#define MELLZI_ERROR_INVALID   -3
#define MELLZI_ERROR_NOT_CONN  -4

MELLZI_EXTERN_C {

/* ========================================================================= */
/* VDACQ Acquisition API (Compatible with Rayence VADAV SDK)                */
/* ========================================================================= */

MELLZI_API int VDACQ_Connect(const char* ipAddr, int port);
MELLZI_API int VDACQ_Connect2(const char* ipAddr, int ctrlPort, int framePort);
MELLZI_API int VDACQ_Close(void);
MELLZI_API int VDACQ_CheckConnection(void);
MELLZI_API int VDACQ_StartFrame(void);
MELLZI_API int VDACQ_GetFrame(uint16_t* pBuffer, int timeoutMs);
MELLZI_API int VDACQ_Abort(void);

MELLZI_API int VDACQ_SetFrameDim(int width, int height);
MELLZI_API int VDACQ_GetFrameDim(int* pWidth, int* pHeight);

MELLZI_API int VDACQ_GetDetectorIPAddr(char* pIpBuffer, int maxLen);
MELLZI_API int VDACQ_SetDetectorIPAddr(const char* pNewIp);
MELLZI_API int VDACQ_GetDetectorInfo(char* pInfoBuffer, int maxLen);
MELLZI_API int VDACQ_GetBatteryRemaining(int* pPercent, int* pMilliVolts);
MELLZI_API int VDACQ_GetQualityWireless(int* pRssi);

MELLZI_API int VDACQ_GetFrameNum(int* pCount);
MELLZI_API int VDACQ_LoadFrame(int frameIndex, uint16_t* pBuffer, int timeoutMs);
MELLZI_API int VDACQ_LoadFrameDelete(int frameIndex);
MELLZI_API int VDACQ_LoadFrameWithName(const char* frameName, uint16_t* pBuffer, int timeoutMs);

MELLZI_API int VDACQ_ReDark(int count);
MELLZI_API int VDACQ_Shutdown(void);
MELLZI_API int VDACQ_SendCommand(int cmdId, const void* pPayload, int payloadSize);
MELLZI_API int VDACQ_SendCommandParam(int cmdId, int param1, int param2);

/* ========================================================================= */
/* VDC Calibration API (Compatible with Rayence VADAV SDK)                  */
/* ========================================================================= */

MELLZI_API int VDC_SetCalibrationDirectory(const char* dirPath);
MELLZI_API int VDC_GetCalibrationDirectory(char* pDirBuffer, int maxLen);
MELLZI_API int VDC_SetCalibrationInit(int initMode);
MELLZI_API int VDC_GetCalibrationInit(int* pInitMode);

MELLZI_API int VDC_GetImageDim(int* pWidth, int* pHeight);
MELLZI_API int VDC_SetImgCutParams(int left, int top, int right, int bottom);
MELLZI_API int VDC_GetImgCutParams(int* pLeft, int* pTop, int* pRight, int* pBottom);
MELLZI_API int VDC_CutImage(const uint16_t* pSrc, int srcW, int srcH,
                           uint16_t* pDst, int* pDstW, int* pDstH);

MELLZI_API int VDC_GetDark(int numFrames, uint16_t* pDarkBuffer);
MELLZI_API int VDC_GetDarkAllInitModes(void);
MELLZI_API int VDC_GetBright(int numFrames, uint16_t* pBrightBuffer);
MELLZI_API int VDC_GenerateBright(const char* brightFilePath);
MELLZI_API int VDC_GenerateAuto(void);

MELLZI_API int VDC_Process(const uint16_t* pRawIn, uint16_t* pProcessedOut,
                           int width, int height, int flags);
MELLZI_API int VDC_ProcessEx(const uint16_t* pRawIn, uint16_t* pProcessedOut,
                             int width, int height, int flags, void* extraParams);
MELLZI_API int VDC_Process_Preview(const uint16_t* pRawIn, uint16_t* pProcessedOut,
                                   int width, int height);
MELLZI_API int VDC_Abort(void);
MELLZI_API int VDC_Close(void);

/* ========================================================================= */
/* VDIP Image Processing API                                                */
/* ========================================================================= */

MELLZI_API int VDIP_Process(const uint16_t* pSrc, uint16_t* pDst, int w, int h, int filterMode);
MELLZI_API int VDIP_Abort(void);
MELLZI_API int VDIP_Close(void);

/* ========================================================================= */
/* VD High-level Orchestration API                                          */
/* ========================================================================= */

MELLZI_API int VD_GetImage(uint16_t* pBuffer, int timeoutMs);
MELLZI_API int VD_GetImageCancel(void);
MELLZI_API int VD_GetImageSP(uint16_t* pBuffer, int timeoutMs, void* specParams);
MELLZI_API int VD_Calibration(int flags);
MELLZI_API int VD_ConnectRestore(void);
MELLZI_API int VD_GetShockLog(char* pLogBuffer, int maxLen);
MELLZI_API int VD_GetHomeDirectory(char* pDirBuffer, int maxLen);

MELLZI_API int VD_Set_Acquisition(void* acqParams);
MELLZI_API int VD_Set_Calibration(void* calParams);
MELLZI_API int VD_Set_ImgProcess(void* imgParams);
MELLZI_API int VD_Set_Preview(void* previewParams);

MELLZI_API int VD_LogOpen(const char* logFile);
MELLZI_API int VD_LogMsg(const char* format, ...);
MELLZI_API int VD_LogFlush(void);
MELLZI_API int VD_LogClose(void);

MELLZI_API int VDDBG_SendCommand(int cmd, const void* data, int size);
MELLZI_API int VDINFO_Connect(const char* ip, int port);
MELLZI_API int VDSHARE_Connect(const char* ip, int port);

} // MELLZI_EXTERN_C
