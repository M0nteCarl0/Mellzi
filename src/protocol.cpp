#include "mellzi/protocol.h"
#include <string>
#include <sstream>
#include <iomanip>

namespace mellzi::protocol {

std::string get_command_name(uint32_t cmd_id) {
    Section sec = static_cast<Section>(cmd_id >> 16);
    uint16_t sub = static_cast<uint16_t>(cmd_id & 0xFFFF);

    switch (sec) {
        case Section::CaptureOrConfig: {
            switch (static_cast<CaptureCmd>(sub)) {
                case CaptureCmd::ReadyIn:           return "CAPTURE_READYIN (0x01)";
                case CaptureCmd::Start:             return "CAPTURE_START (0x02)";
                case CaptureCmd::Abort:             return "CAPTURE_ABORT (0x03)";
                case CaptureCmd::InitialisationDone:return "CAPTURE_INITIALISATION_DONE (0x11)";
                case CaptureCmd::StartDone:         return "CAPTURE_START_DONE (0x12)";
                case CaptureCmd::AbortDone:         return "CAPTURE_ABORT_DONE (0x13)";
                case CaptureCmd::ElsetDone:         return "CAPTURE_ELSET_DONE (0x14)";
                case CaptureCmd::ReadyDone:         return "CAPTURE_READY_DONE (0x17)";
                case CaptureCmd::BatteryRemain:     return "CAPTURE_BATTERY_REMAIN (0x19)";
                case CaptureCmd::WlanRssi:          return "CAPTURE_WLAN_RSSI (0x1A)";
                case CaptureCmd::Shutdown:          return "CAPTURE_SHUTDOWN (0x0B)";
                case CaptureCmd::ReDarkDone:        return "CAPTURE_REDARK_DONE (0x20)";
                case CaptureCmd::ThermalInfo:       return "CAPTURE_THERMAL_INFO (0x31)";
                case CaptureCmd::AxisInfo:          return "CAPTURE_AXIS_INFO (0x59)";
                case CaptureCmd::DoShift:           return "CAPTURE_DO_SHIFT (0x55)";
                case CaptureCmd::FrameDone:         return "CAPTURE_FRAME_DONE (0xFF)";
                default: break;
            }
            switch (static_cast<ConfigCmd>(sub)) {
                case ConfigCmd::InitUpload:         return "CONFIG_INIT_UP (0x05)";
                case ConfigCmd::ElsetUpload:        return "CONFIG_ELSET_UP (0x06)";
                case ConfigCmd::InitUploadDone:     return "CONFIG_INIT_UP_DONE (0x15)";
                case ConfigCmd::ElsetUploadDone:    return "CONFIG_ELSET_UP_DONE (0x16)";
                case ConfigCmd::InitSave:           return "CONFIG_INIT_SAVE (0x0C)";
                case ConfigCmd::ElsetSave:          return "CONFIG_ELSET_SAVE (0x0D)";
                case ConfigCmd::Elset2Save:         return "CONFIG_ELSET2_SAVE (0x32)";
                case ConfigCmd::Elset3Save:         return "CONFIG_ELSET3_SAVE (0x33)";
                case ConfigCmd::Elset2Upload:       return "CONFIG_ELSET_UP2 (0x34)";
                case ConfigCmd::Elset3Upload:       return "CONFIG_ELSET_UP3 (0x35)";
                case ConfigCmd::Elset2UploadDone:   return "CONFIG_ELSET_UP2_DONE (0x44)";
                case ConfigCmd::Elset3UploadDone:   return "CONFIG_ELSET_UP3_DONE (0x45)";
                default: break;
            }
            break;
        }
        case Section::Aux: {
            switch (static_cast<AuxCmd>(cmd_id)) {
                case AuxCmd::ReqDefStat:            return "AUX_REQ_DEF_STAT (0x10000)";
                case AuxCmd::RespDefStat:           return "AUX_RESP_DEF_STAT (0x11000)";
                case AuxCmd::ReqLedStat:            return "AUX_REQ_LED_STAT (0x10001)";
                case AuxCmd::RespLedStat:           return "AUX_RESP_LED_STAT (0x11001)";
                case AuxCmd::ReqSetTime:            return "AUX_REQ_SET_TIME (0x10002)";
                case AuxCmd::ReqGainCal:            return "AUX_REQ_GCAL (0x10003)";
                case AuxCmd::RespGainCal:           return "AUX_RESP_GCAL (0x11003)";
                case AuxCmd::ReqVerFirmware:        return "AUX_REQ_VER_FIRM (0x10010)";
                case AuxCmd::RespVerFirmware:       return "AUX_RESP_VER_FIRM (0x11010)";
                case AuxCmd::ReqVerFpga:            return "AUX_REQ_VER_FPGA (0x10011)";
                case AuxCmd::RespVerFpga:           return "AUX_RESP_VER_FPGA (0x11011)";
                case AuxCmd::ReqVerMain:            return "AUX_REQ_VER_MAIN (0x10012)";
                case AuxCmd::RespVerMain:           return "AUX_RESP_VER_MAIN (0x11012)";
                case AuxCmd::ReqVerTftp:            return "AUX_REQ_VER_TFTP (0x10013)";
                case AuxCmd::RespVerTftp:           return "AUX_RESP_VER_TFTP (0x11013)";
                case AuxCmd::ReqVerCsis:            return "AUX_REQ_VER_CSIS (0x10014)";
                case AuxCmd::RespVerCsis:           return "AUX_RESP_VER_CSIS (0x11014)";
                case AuxCmd::ReqVerLicense:         return "AUX_REQ_VER_LICENSE (0x10015)";
                case AuxCmd::RespVerLicense:        return "AUX_RESP_VER_LICENSE (0x11015)";
                case AuxCmd::ReqVerIp:              return "AUX_REQ_VER_IP (0x10016)";
                case AuxCmd::RespVerIp:             return "AUX_RESP_VER_IP (0x11016)";
                case AuxCmd::ReqVerMac:             return "AUX_REQ_VER_MAC (0x10017)";
                case AuxCmd::RespVerMac:            return "AUX_RESP_VER_MAC (0x11017)";
                case AuxCmd::ReqDetectorStat:       return "AUX_REQ_DETE_STAT (0x10025)";
                case AuxCmd::RespDetectorStat:      return "AUX_RESP_DETE_STAT (0x11025)";
                case AuxCmd::ReqFrameNum:           return "AUX_REQ_FRAME_NUM (0x10026)";
                case AuxCmd::RespFrameNum:          return "AUX_RESP_FRAME_NUM (0x11026)";
                case AuxCmd::ReqFrameLoad:          return "AUX_REQ_FRAME_LOAD (0x10027)";
                case AuxCmd::RespFrameLoad:         return "AUX_RESP_FRAME_LOAD (0x11027)";
                case AuxCmd::ReqGetCount:           return "AUX_REQ_GET_COUNT (0x10060)";
                case AuxCmd::RespGetCount:          return "AUX_RESP_GET_COUNT (0x11060)";
                case AuxCmd::ReqSleep:              return "AUX_REQ_SLEEP (0x10080)";
                case AuxCmd::ReqWakeup:             return "AUX_REQ_WAKEUP (0x10081)";
                case AuxCmd::ReqBattery:            return "AUX_REQ_BATTERY (0x10082)";
                default: break;
            }
            break;
        }
        case Section::Bak: {
            switch (static_cast<BakCmd>(cmd_id)) {
                case BakCmd::ReqEnterBackupMode:    return "BAK_ENTER_BACKUP_MODE (0x20000)";
                case BakCmd::ReqWriteWmbd:          return "BAK_REQ_WR_WMBD (0x20012)";
                case BakCmd::RespWriteWmbd:         return "BAK_RESP_WR_WMBD (0x22012)";
                case BakCmd::ReqWriteVpnl:          return "BAK_REQ_WR_VPNL (0x20014)";
                case BakCmd::RespWriteVpnl:         return "BAK_RESP_WR_VPNL (0x22014)";
                case BakCmd::ReqWriteVcsi:          return "BAK_REQ_WR_VCSI (0x20016)";
                case BakCmd::RespWriteVcsi:         return "BAK_RESP_WR_VCSI (0x22016)";
                case BakCmd::ReqReadElad:           return "BAK_REQ_RD_ELAD (0x20021)";
                case BakCmd::RespReadElad:          return "BAK_RESP_RD_ELAD (0x22021)";
                case BakCmd::ReqWriteElad:          return "BAK_REQ_WR_ELAD (0x20022)";
                case BakCmd::RespWriteElad:         return "BAK_RESP_WR_ELAD (0x22022)";
                case BakCmd::ReqReadElaf:           return "BAK_REQ_RD_ELAF (0x20023)";
                case BakCmd::RespReadElaf:          return "BAK_RESP_RD_ELAF (0x22023)";
                case BakCmd::ReqWriteElaf:          return "BAK_REQ_WR_ELAF (0x20024)";
                case BakCmd::RespWriteElaf:         return "BAK_RESP_WR_ELAF (0x22024)";
                default: break;
            }
            break;
        }
    }

    std::ostringstream ss;
    ss << "CMD_0x" << std::hex << std::uppercase << cmd_id;
    return ss.str();
}

} // namespace mellzi::protocol
