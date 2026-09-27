#include "mellzi/acquisition.h"
#include <iostream>
#include <cstring>
#include <thread>

namespace mellzi::client {

AcquisitionClient::AcquisitionClient() = default;

AcquisitionClient::~AcquisitionClient() {
    disconnect();
}

bool AcquisitionClient::connect(std::string_view host, uint16_t ctrl_port, uint16_t frame_port,
                                std::chrono::milliseconds timeout) {
    disconnect();

    host_ = host;
    ctrl_port_ = ctrl_port;
    frame_port_ = frame_port;

    ctrl_stream_ = net::TcpStream::connect(host_, ctrl_port_, timeout);
    if (!ctrl_stream_) {
        return false;
    }

    frame_stream_ = net::TcpStream::connect(host_, frame_port_, timeout);
    if (!frame_stream_) {
        ctrl_stream_.reset();
        return false;
    }

    return true;
}

void AcquisitionClient::disconnect() {
    if (ctrl_stream_) {
        ctrl_stream_->close();
        ctrl_stream_.reset();
    }
    if (frame_stream_) {
        frame_stream_->close();
        frame_stream_.reset();
    }
}

bool AcquisitionClient::is_connected() const noexcept {
    return ctrl_stream_ && ctrl_stream_->is_valid() &&
           frame_stream_ && frame_stream_->is_valid();
}

bool AcquisitionClient::send_command(uint32_t cmd_id, std::span<const uint8_t> payload) {
    if (!is_connected()) return false;

    protocol::Packet pkt{};
    pkt.set_command(cmd_id);
    if (!payload.empty()) {
        size_t n = std::min(payload.size(), protocol::PAYLOAD_SIZE);
        std::memcpy(pkt.payload, payload.data(), n);
    }

    std::span<const uint8_t> wire_data(reinterpret_cast<const uint8_t*>(&pkt), protocol::PACKET_SIZE);
    return ctrl_stream_->send_all(wire_data);
}

bool AcquisitionClient::send_command_wait_resp(uint32_t cmd_id, protocol::Packet& resp_out,
                                               std::chrono::milliseconds timeout) {
    if (!send_command(cmd_id)) {
        return false;
    }

    ctrl_stream_->set_timeout(timeout);
    std::span<uint8_t> in_data(reinterpret_cast<uint8_t*>(&resp_out), protocol::PACKET_SIZE);
    if (!ctrl_stream_->receive_exact(in_data)) {
        return false;
    }

    process_incoming_packet(resp_out);
    return true;
}

void AcquisitionClient::process_incoming_packet(const protocol::Packet& pkt) {
    uint32_t cmd = pkt.command_id;
    auto sec = pkt.get_section();

    if (sec == protocol::Section::CaptureOrConfig) {
        auto sub = static_cast<protocol::CaptureCmd>(cmd);
        std::lock_guard<std::mutex> lock(telemetry_mutex_);

        if (sub == protocol::CaptureCmd::BatteryRemain) {
            const auto* b = reinterpret_cast<const protocol::BatteryPayload*>(pkt.payload);
            telemetry_.battery_percent = b->percent;
            telemetry_.battery_voltage_mv = b->voltage_mv;
            if (telemetry_cb_) telemetry_cb_(telemetry_);
        } else if (sub == protocol::CaptureCmd::WlanRssi) {
            const auto* w = reinterpret_cast<const protocol::WirelessPayload*>(pkt.payload);
            telemetry_.wireless_rssi_dbm = w->rssi_dbm;
            telemetry_.wireless_quality = w->link_quality;
            if (telemetry_cb_) telemetry_cb_(telemetry_);
        } else if (sub == protocol::CaptureCmd::ThermalInfo) {
            const auto* t = reinterpret_cast<const protocol::ThermalPayload*>(pkt.payload);
            telemetry_.temp1_celsius = static_cast<float>(t->temp1_celsius_x10) / 10.0f;
            telemetry_.temp2_celsius = static_cast<float>(t->temp2_celsius_x10) / 10.0f;
            if (telemetry_cb_) telemetry_cb_(telemetry_);
        } else if (sub == protocol::CaptureCmd::AxisInfo) {
            const auto* a = reinterpret_cast<const protocol::AxisPayload*>(pkt.payload);
            telemetry_.accel_x = a->x_axis;
            telemetry_.accel_y = a->y_axis;
            telemetry_.accel_z = a->z_axis;
            telemetry_.shock_detected = a->shock_detected != 0;
            if (telemetry_cb_) telemetry_cb_(telemetry_);
        } else if (sub == protocol::CaptureCmd::RecvPercents) {
            uint32_t pct = *reinterpret_cast<const uint32_t*>(pkt.payload);
            if (progress_cb_) progress_cb_(pct);
        }
    }
}

Telemetry AcquisitionClient::get_latest_telemetry() const {
    std::lock_guard<std::mutex> lock(telemetry_mutex_);
    return telemetry_;
}

void AcquisitionClient::set_telemetry_callback(TelemetryCallback cb) {
    std::lock_guard<std::mutex> lock(telemetry_mutex_);
    telemetry_cb_ = std::move(cb);
}

void AcquisitionClient::set_progress_callback(ProgressCallback cb) {
    std::lock_guard<std::mutex> lock(telemetry_mutex_);
    progress_cb_ = std::move(cb);
}

bool AcquisitionClient::start_acquisition() {
    return send_command(static_cast<uint32_t>(protocol::CaptureCmd::Start));
}

bool AcquisitionClient::abort_acquisition() {
    return send_command(static_cast<uint32_t>(protocol::CaptureCmd::Abort));
}

bool AcquisitionClient::get_frame(image::Image& out_frame, std::chrono::milliseconds timeout) {
    if (!is_connected()) return false;

    // Trigger acquisition
    if (!start_acquisition()) {
        return false;
    }

    out_frame.resize(width_, height_);
    size_t total_bytes = out_frame.byte_size();
    std::span<uint8_t> frame_bytes(reinterpret_cast<uint8_t*>(out_frame.data()), total_bytes);

    frame_stream_->set_timeout(timeout);

    // Read full frame from frame socket (port 20001)
    if (!frame_stream_->receive_exact(frame_bytes)) {
        return false;
    }

    // Read FRAME_DONE packet (either on frame_stream or ctrl_stream)
    protocol::Packet done_pkt{};
    std::span<uint8_t> pkt_bytes(reinterpret_cast<uint8_t*>(&done_pkt), protocol::PACKET_SIZE);
    frame_stream_->receive_exact(pkt_bytes);

    return true;
}

bool AcquisitionClient::query_battery(uint32_t& percent, uint32_t& mv) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::AuxCmd::ReqBattery), resp)) {
        const auto* b = reinterpret_cast<const protocol::BatteryPayload*>(resp.payload);
        percent = b->percent;
        mv = b->voltage_mv;
        return true;
    }
    return false;
}

bool AcquisitionClient::query_wireless(int32_t& rssi) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::CaptureCmd::WlanRssi), resp)) {
        const auto* w = reinterpret_cast<const protocol::WirelessPayload*>(resp.payload);
        rssi = w->rssi_dbm;
        return true;
    }
    return false;
}

bool AcquisitionClient::query_version_firmware(std::string& version_str) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::AuxCmd::ReqVerFirmware), resp)) {
        version_str = resp.as_string();
        return true;
    }
    return false;
}

bool AcquisitionClient::query_version_fpga(std::string& version_str) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::AuxCmd::ReqVerFpga), resp)) {
        version_str = resp.as_string();
        return true;
    }
    return false;
}

bool AcquisitionClient::query_version_main(std::string& version_str) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::AuxCmd::ReqVerMain), resp)) {
        version_str = resp.as_string();
        return true;
    }
    return false;
}

bool AcquisitionClient::query_device_ip(std::string& ip_str) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::AuxCmd::ReqVerIp), resp)) {
        ip_str = resp.as_string();
        return true;
    }
    return false;
}

bool AcquisitionClient::query_device_mac(std::string& mac_str) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::AuxCmd::ReqVerMac), resp)) {
        mac_str = resp.as_string();
        return true;
    }
    return false;
}

bool AcquisitionClient::query_acquisition_count(uint32_t& count) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::AuxCmd::ReqGetCount), resp)) {
        const auto* cp = reinterpret_cast<const protocol::CountPayload*>(resp.payload);
        count = cp->total_acquisitions;
        return true;
    }
    return false;
}

bool AcquisitionClient::download_init_config(std::span<uint8_t, 32> init_cfg_out) {
    protocol::Packet resp{};
    if (send_command_wait_resp(static_cast<uint32_t>(protocol::ConfigCmd::InitUpload), resp)) {
        std::copy_n(resp.payload, 32, init_cfg_out.data());
        return true;
    }
    return false;
}

bool AcquisitionClient::download_elset_config(int index, std::span<uint8_t, 128> elset_cfg_out) {
    uint32_t cmd = static_cast<uint32_t>(protocol::ConfigCmd::ElsetUpload);
    if (index == 2) cmd = static_cast<uint32_t>(protocol::ConfigCmd::Elset2Upload);
    else if (index == 3) cmd = static_cast<uint32_t>(protocol::ConfigCmd::Elset3Upload);

    protocol::Packet resp{};
    if (send_command_wait_resp(cmd, resp)) {
        std::copy_n(resp.payload, 128, elset_cfg_out.data());
        return true;
    }
    return false;
}

bool AcquisitionClient::upload_init_config(std::span<const uint8_t, 32> init_cfg_in) {
    protocol::Packet resp{};
    protocol::Packet req{};
    req.set_command(static_cast<uint32_t>(protocol::ConfigCmd::InitSave));
    std::copy_n(init_cfg_in.data(), 32, req.payload);

    std::span<const uint8_t> req_span(reinterpret_cast<const uint8_t*>(&req), protocol::PACKET_SIZE);
    if (!ctrl_stream_->send_all(req_span)) return false;

    std::span<uint8_t> resp_span(reinterpret_cast<uint8_t*>(&resp), protocol::PACKET_SIZE);
    return ctrl_stream_->receive_exact(resp_span);
}

bool AcquisitionClient::upload_elset_config(int index, std::span<const uint8_t, 128> elset_cfg_in) {
    uint32_t cmd = static_cast<uint32_t>(protocol::ConfigCmd::ElsetSave);
    if (index == 2) cmd = static_cast<uint32_t>(protocol::ConfigCmd::Elset2Save);
    else if (index == 3) cmd = static_cast<uint32_t>(protocol::ConfigCmd::Elset3Save);

    protocol::Packet req{};
    req.set_command(cmd);
    std::copy_n(elset_cfg_in.data(), 128, req.payload);

    std::span<const uint8_t> req_span(reinterpret_cast<const uint8_t*>(&req), protocol::PACKET_SIZE);
    if (!ctrl_stream_->send_all(req_span)) return false;

    protocol::Packet resp{};
    std::span<uint8_t> resp_span(reinterpret_cast<uint8_t*>(&resp), protocol::PACKET_SIZE);
    return ctrl_stream_->receive_exact(resp_span);
}

} // namespace mellzi::client
