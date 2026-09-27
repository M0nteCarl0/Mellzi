#include "mellzi/config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <algorithm>
#include <iomanip>

namespace mellzi::config {

namespace {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

using IniMap = std::map<std::string, std::map<std::string, std::string>>;

IniMap parse_ini(const std::filesystem::path& path) {
    IniMap ini;
    std::ifstream file(path);
    if (!file.is_open()) return ini;

    std::string current_section;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(line.substr(0, eq_pos));
            std::string val = trim(line.substr(eq_pos + 1));
            ini[current_section][key] = val;
        }
    }
    return ini;
}

int32_t get_ini_int(const std::map<std::string, std::string>& sec, const std::string& key, int32_t def_val) {
    auto it = sec.find(key);
    if (it != sec.end()) {
        try {
            return std::stol(it->second);
        } catch (...) {
            return def_val;
        }
    }
    return def_val;
}

} // namespace

void DetectorInitParameters::init_defaults() noexcept {
    f_ctrl = 1;
    int_b = 0;
    om = 0;
    scan = 1;
    speed = 0;
    eo_sel = 1;
    ag0 = 1;
    ag1 = 0;
    ag2 = 1;
    da0 = 0; da1 = 0; da2 = 0; da3 = 1; da4 = 1; da5 = 0; da6 = 0; da7 = 0; da8 = 0;
    agi_auto = 0;
    gate0 = 0; gate1 = 0; gate2 = 0; gate3 = 0; gate4 = 0;
    p_drv = 0;
    s_drv = 0;
    dark_comp = 0;
    at_en = 0;
    af_gate = 0;
    af_el = 0;
    ex_time = 1333333;
    loop_cnt = 1;
    frame_cnt = 10;
    frame_delay = 1;
    el_delay = 1;
    el_high = 0;
    gate_low = 1;
    gate_high = 160;
    readout_count = 1;
    el_high2 = 400000;
    frame_cnt2 = 4;
    loop_cnt2 = 1;
    el_high3 = 0;
    frame_cnt3 = 0;
    loop_cnt3 = 0;
    sh0 = 214;
    sh1 = 214;
    gate = 160;
    test_pattern = 27;
    af_el_runtime = 0;
    af_el_delay = 0;
    af_el_loop = 0;
    af_gate_rd_cnt = 0;
    af_delay_cnt = 0;
    af_gate_rd_loop = 0;
    at_threshold = 0;
    at_delay = 0;
    ready_delay = 0;
    agi_delay = 0;

    options.fill(0);
    // Standard default options
    options[0x11] = 1;
    options[0x13] = 1;
    options[0x16] = 1;
    options[0x1c] = 1;
    options[0x1e] = 1;

    controls.fill(0);
    controls[0x00] = 19999999;
    controls[0x01] = 43;
    controls[0x02] = 4;
    controls[0x03] = 8;
    controls[0x04] = 4401;
    controls[0x06] = 256;
    controls[0x07] = 256;
    controls[0x08] = 2;
    controls[0x09] = 1;
    controls[0x0a] = 4;
    controls[0x0c] = 4899;
    controls[0x0d] = 3328;
    controls[0x0e] = 256;
    controls[0x0f] = 30;
    controls[0x10] = 1022;
    controls[0x11] = 1;
    controls[0x12] = 4;
    controls[0x13] = 5;
    controls[0x14] = 3600;
    controls[0x15] = 2;
    controls[0x16] = 10;
    controls[0x17] = 1044;
    controls[0x18] = 4372;
    controls[0x19] = 3000;
    controls[0x1c] = 4373;
    controls[0x1d] = 200;
    controls[0x1e] = 80;
    controls[0x1f] = 512;
}

bool DetectorInitParameters::load_from_ini(const std::filesystem::path& iniPath) {
    auto ini = parse_ini(iniPath);
    auto it = ini.find("Init1");
    if (it == ini.end()) {
        return false;
    }
    const auto& sec = it->second;

    f_ctrl = get_ini_int(sec, "FCtrl", f_ctrl);
    int_b = get_ini_int(sec, "IntB", int_b);
    om = get_ini_int(sec, "Om", om);
    scan = get_ini_int(sec, "Scan", scan);
    speed = get_ini_int(sec, "Speed", speed);
    eo_sel = get_ini_int(sec, "EOSel", eo_sel);
    ag0 = get_ini_int(sec, "Ag0", ag0);
    ag1 = get_ini_int(sec, "Ag1", ag1);
    ag2 = get_ini_int(sec, "Ag2", ag2);
    da0 = get_ini_int(sec, "DA0", da0);
    da1 = get_ini_int(sec, "DA1", da1);
    da2 = get_ini_int(sec, "DA2", da2);
    da3 = get_ini_int(sec, "DA3", da3);
    da4 = get_ini_int(sec, "DA4", da4);
    da5 = get_ini_int(sec, "DA5", da5);
    da6 = get_ini_int(sec, "DA6", da6);
    da7 = get_ini_int(sec, "DA7", da7);
    da8 = get_ini_int(sec, "DA8", da8);
    agi_auto = get_ini_int(sec, "AGI_AUTO", agi_auto);
    gate0 = get_ini_int(sec, "Gate0", gate0);
    gate1 = get_ini_int(sec, "Gate1", gate1);
    gate2 = get_ini_int(sec, "Gate2", gate2);
    gate3 = get_ini_int(sec, "Gate3", gate3);
    gate4 = get_ini_int(sec, "Gate4", gate4);
    p_drv = get_ini_int(sec, "P_DRV", p_drv);
    s_drv = get_ini_int(sec, "S_DRV", s_drv);
    dark_comp = get_ini_int(sec, "Dark_Comp", dark_comp);
    at_en = get_ini_int(sec, "AT_EN", at_en);
    af_gate = get_ini_int(sec, "AF_GATE", af_gate);
    af_el = get_ini_int(sec, "AF_EL", af_el);
    ex_time = get_ini_int(sec, "Ex_Time", ex_time);
    loop_cnt = get_ini_int(sec, "Loop_Cnt", loop_cnt);
    frame_cnt = get_ini_int(sec, "Frame_Cnt", frame_cnt);
    frame_delay = get_ini_int(sec, "Frame_Delay", frame_delay);
    el_delay = get_ini_int(sec, "EL_Delay", el_delay);
    el_high = get_ini_int(sec, "EL_High", el_high);
    gate_low = get_ini_int(sec, "Gate_Low", gate_low);
    gate_high = get_ini_int(sec, "Gate_High", gate_high);
    readout_count = get_ini_int(sec, "ReadOut_Count", readout_count);
    el_high2 = get_ini_int(sec, "EL_High2", el_high2);
    frame_cnt2 = get_ini_int(sec, "Frame_Cnt2", frame_cnt2);
    loop_cnt2 = get_ini_int(sec, "Loop_Cnt2", loop_cnt2);
    el_high3 = get_ini_int(sec, "EL_High3", el_high3);
    frame_cnt3 = get_ini_int(sec, "Frame_Cnt3", frame_cnt3);
    loop_cnt3 = get_ini_int(sec, "Loop_Cnt3", loop_cnt3);
    sh0 = get_ini_int(sec, "Sh0", sh0);
    sh1 = get_ini_int(sec, "Sh1", sh1);
    gate = get_ini_int(sec, "Gate", gate);
    test_pattern = get_ini_int(sec, "TestPattern", test_pattern);
    af_el_runtime = get_ini_int(sec, "AF_ELRunTime", af_el_runtime);
    af_el_delay = get_ini_int(sec, "AF_ELDelay", af_el_delay);
    af_el_loop = get_ini_int(sec, "AF_ELLoop", af_el_loop);
    af_gate_rd_cnt = get_ini_int(sec, "AF_GateRdCnt", af_gate_rd_cnt);
    af_delay_cnt = get_ini_int(sec, "AF_DelayCnt", af_delay_cnt);
    af_gate_rd_loop = get_ini_int(sec, "AF_GateRdLoop", af_gate_rd_loop);
    at_threshold = get_ini_int(sec, "AT_Threshold", at_threshold);
    at_delay = get_ini_int(sec, "AT_Delay", at_delay);
    ready_delay = get_ini_int(sec, "Ready_Delay", ready_delay);
    agi_delay = get_ini_int(sec, "AGI_Delay", agi_delay);

    for (size_t i = 0; i < options.size(); ++i) {
        std::ostringstream ss;
        ss << "Option" << std::hex << std::setw(2) << std::setfill('0') << i;
        options[i] = get_ini_int(sec, ss.str(), options[i]);
    }

    for (size_t i = 0; i < controls.size(); ++i) {
        std::ostringstream ss;
        ss << "Control" << std::hex << std::setw(2) << std::setfill('0') << i;
        controls[i] = get_ini_int(sec, ss.str(), controls[i]);
    }

    return true;
}

bool DetectorInitParameters::save_to_ini(const std::filesystem::path& iniPath) const {
    std::ofstream file(iniPath);
    if (!file.is_open()) return false;

    file << "[Init1]\n";
    file << "Caption=Default standard mode\n";
    file << "FCtrl=" << f_ctrl << "\n";
    file << "IntB=" << int_b << "\n";
    file << "Om=" << om << "\n";
    file << "Scan=" << scan << "\n";
    file << "Speed=" << speed << "\n";
    file << "EOSel=" << eo_sel << "\n";
    file << "Ag0=" << ag0 << "\n";
    file << "Ag1=" << ag1 << "\n";
    file << "Ag2=" << ag2 << "\n";
    file << "DA0=" << da0 << "\n";
    file << "DA1=" << da1 << "\n";
    file << "DA2=" << da2 << "\n";
    file << "DA3=" << da3 << "\n";
    file << "DA4=" << da4 << "\n";
    file << "DA5=" << da5 << "\n";
    file << "DA6=" << da6 << "\n";
    file << "DA7=" << da7 << "\n";
    file << "DA8=" << da8 << "\n";
    file << "Gate0=" << gate0 << "\n";
    file << "Gate1=" << gate1 << "\n";
    file << "Gate2=" << gate2 << "\n";
    file << "Gate3=" << gate3 << "\n";
    file << "Gate4=" << gate4 << "\n";
    file << "P_DRV=" << p_drv << "\n";
    file << "S_DRV=" << s_drv << "\n";
    file << "Dark_Comp=" << dark_comp << "\n";
    file << "AT_EN=" << at_en << "\n";
    file << "AF_GATE=" << af_gate << "\n";
    file << "AF_EL=" << af_el << "\n";
    file << "AGI_AUTO=" << agi_auto << "\n";
    file << "Ex_Time=" << ex_time << "\n";
    file << "Loop_Cnt=" << loop_cnt << "\n";
    file << "Frame_Cnt=" << frame_cnt << "\n";
    file << "Frame_Delay=" << frame_delay << "\n";
    file << "EL_Delay=" << el_delay << "\n";
    file << "EL_High=" << el_high << "\n";
    file << "Gate_Low=" << gate_low << "\n";
    file << "Gate_High=" << gate_high << "\n";
    file << "ReadOut_Count=" << readout_count << "\n";
    file << "EL_High2=" << el_high2 << "\n";
    file << "Frame_Cnt2=" << frame_cnt2 << "\n";
    file << "Loop_Cnt2=" << loop_cnt2 << "\n";
    file << "Sh0=" << sh0 << "\n";
    file << "Sh1=" << sh1 << "\n";
    file << "Gate=" << gate << "\n";
    file << "TestPattern=" << test_pattern << "\n";
    file << "AF_ELRunTime=" << af_el_runtime << "\n";
    file << "AF_ELDelay=" << af_el_delay << "\n";
    file << "AF_ELLoop=" << af_el_loop << "\n";
    file << "AF_GateRdCnt=" << af_gate_rd_cnt << "\n";
    file << "AF_DelayCnt=" << af_delay_cnt << "\n";
    file << "AF_GateRdLoop=" << af_gate_rd_loop << "\n";
    file << "AT_Threshold=" << at_threshold << "\n";
    file << "AT_Delay=" << at_delay << "\n";
    file << "Ready_Delay=" << ready_delay << "\n";
    file << "EL_High3=" << el_high3 << "\n";
    file << "Frame_Cnt3=" << frame_cnt3 << "\n";
    file << "Loop_Cnt3=" << loop_cnt3 << "\n";
    file << "AGI_Delay=" << agi_delay << "\n";

    for (size_t i = 0; i < options.size(); ++i) {
        std::ostringstream ss;
        ss << "Option" << std::hex << std::setw(2) << std::setfill('0') << i;
        file << ss.str() << "=" << options[i] << "\n";
    }

    for (size_t i = 0; i < controls.size(); ++i) {
        std::ostringstream ss;
        ss << "Control" << std::hex << std::setw(2) << std::setfill('0') << i;
        file << ss.str() << "=" << controls[i] << "\n";
    }

    return true;
}

bool DetectorSettings::load_from_ini(const std::filesystem::path& iniPath) {
    auto ini = parse_ini(iniPath);
    auto it = ini.find("Settings");
    if (it == ini.end()) {
        return false;
    }
    const auto& sec = it->second;

    frame_width = static_cast<uint32_t>(get_ini_int(sec, "FrameWidth", static_cast<int32_t>(frame_width)));
    frame_height = static_cast<uint32_t>(get_ini_int(sec, "FrameHeight", static_cast<int32_t>(frame_height)));
    cut.left = get_ini_int(sec, "ImgCutLeft", cut.left);
    cut.top = get_ini_int(sec, "ImgCutTop", cut.top);
    cut.right = get_ini_int(sec, "ImgCutRight", cut.right);
    cut.bottom = get_ini_int(sec, "ImgCutBottom", cut.bottom);

    auto it_ip = sec.find("IPAddr");
    if (it_ip != sec.end()) {
        ip_address = it_ip->second;
    }

    auto it_home = sec.find("HomeDir");
    if (it_home != sec.end()) {
        home_dir = it_home->second;
    }

    dark_average_frames = get_ini_int(sec, "EZAveDarkFrames", dark_average_frames);
    bright_average_frames = get_ini_int(sec, "EZAveBrightFrames", bright_average_frames);
    skip_frames = get_ini_int(sec, "EZSkipFrames", skip_frames);
    bit_depth = get_ini_int(sec, "Bit16", 1) == 1 ? 16 : 14;

    return true;
}

bool DetectorSettings::save_to_ini(const std::filesystem::path& iniPath) const {
    std::ofstream file(iniPath);
    if (!file.is_open()) return false;

    file << "[Settings]\n";
    file << "FrameWidth=" << frame_width << "\n";
    file << "FrameHeight=" << frame_height << "\n";
    file << "ImgCutLeft=" << cut.left << "\n";
    file << "ImgCutTop=" << cut.top << "\n";
    file << "ImgCutRight=" << cut.right << "\n";
    file << "ImgCutBottom=" << cut.bottom << "\n";
    file << "IPAddr=" << ip_address << "\n";
    file << "HomeDir=" << home_dir.string() << "\n";
    file << "EZAveDarkFrames=" << dark_average_frames << "\n";
    file << "EZAveBrightFrames=" << bright_average_frames << "\n";
    file << "EZSkipFrames=" << skip_frames << "\n";
    file << "Bit16=" << (bit_depth == 16 ? 1 : 0) << "\n";

    return true;
}

} // namespace mellzi::config
