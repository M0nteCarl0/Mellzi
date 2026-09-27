# NVIDIA Jetson Integration & Deployment Guide

This document describes how to deploy, configure, and optimize the **Mellzi Server daemon** (`mellzi-server`) on **NVIDIA Jetson** embedded systems (Jetson Nano, TX2, Xavier NX, AGX Xavier, Orin Nano, Orin NX, AGX Orin) running Linux for Tegra (L4T / JetPack 5.x & 6.x).

---

## 1. Hardware Integration Architecture

When functioning as an embedded detector controller, the Mellzi daemon runs on the Jetson system-on-module (SoM) and orchestrates:

```
+-------------------------------------------------------+
|                 NVIDIA Jetson SoM                     |
|                                                       |
|  +--------------------+        +--------------------+ |
|  | Hardware Monitor   |        | JetsonServer       | |
|  | - Thermal Sysfs    |        | - Port 20000 (Ctrl)| |
|  | - I2C Smart Battery|        | - Port 20001 (Data)| |
|  | - 40-pin Sync GPIO |        |                    | |
|  +---------+----------+        +---------+----------+ |
+------------|-----------------------------|------------+
             | (GPIO)                      | (Ethernet)
             v                             v
   [ X-Ray Generator ]             [ Workstation PC ]
```

### 1.1 GPIO Generator Synchronization
The Jetson communicates with the high-voltage X-ray generator using opto-isolated industrial GPIO lines:
- **`PREP_IN` / `EXP_REQ` (Input)**: Signal from hand switch / generator requesting exposure readiness.
- **`EXP_OK` / `XRAY_EN` (Output)**: Signal from Jetson confirming detector panel is cleared and ready for X-ray integration window.

### 1.2 Thermal & Battery Monitoring
- **Thermal zones**: Read directly from `/sys/devices/virtual/thermal/thermal_zone[0-7]/temp` (CPU, GPU, PMIC, thermal diodes).
- **Smart Battery**: Interfaced via I2C bus (`/dev/i2c-1` or `/sys/class/power_supply/battery/`).
- **Telemetry packets**: Dispatched automatically every 1000 ms over port 20000 (`CAPTURE_THERMAL_INFO`, `CAPTURE_BATTERY_REMAIN`).

---

## 2. High-Performance Network Tuning

Each $3328 \times 3328$ 16-bit uncompressed radiographic frame contains **22,151,168 bytes (~22.15 MB)**. When streamed across TCP port 20001, bursting this payload in under 200 ms requires low-latency TCP buffer optimization.

Add the following configuration to `/etc/sysctl.d/99-mellzi.conf`:

```ini
# Increase maximum socket receive and transmit buffers
net.core.rmem_max = 67108864
net.core.wmem_max = 67108864
net.core.rmem_default = 33554432
net.core.wmem_default = 33554432

# TCP buffer sizing: min, default, max
net.ipv4.tcp_rmem = 4096 87380 67108864
net.ipv4.tcp_wmem = 4096 65536 67108864

# Increase backlog queue
net.core.netdev_max_backlog = 10000
```

Apply immediately without reboot:
```bash
sudo sysctl --system
```

---

## 3. Building for Jetson

### Option A: Cross-Compilation on x86_64 Host (Recommended)
You can cross-compile the entire Mellzi project on an Ubuntu 20.04/22.04 workstation using the GNU ARM64 toolchain:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

cmake -B build-jetson -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
  -DBUILD_TESTING=OFF

cmake --build build-jetson --parallel
cmake --install build-jetson --prefix install-jetson
```

### Option B: Native Compilation on Jetson
Directly on the Jetson device:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
sudo cmake --install build
```

---

## 4. Running as a Systemd Service

To automatically launch the Mellzi daemon at boot:

1. Create service descriptor `/etc/systemd/system/mellzi-server.service`:

```ini
[Unit]
Description=Mellzi Rayence DaVinci Detector Server Daemon
After=network.target

[Service]
Type=simple
User=root
ExecStart=/usr/local/bin/mellzi-server --ctrl-port 20000 --frame-port 20001
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal

# Real-time scheduling priority (optional)
CPUSchedulingPolicy=rr
CPUSchedulingPriority=50

[Install]
WantedBy=multi-user.target
```

2. Enable and start the service:

```bash
sudo systemctl daemon-reload
sudo systemctl enable mellzi-server
sudo systemctl start mellzi-server
```

3. Check service status:

```bash
systemctl status mellzi-server
journalctl -u mellzi-server -f
```

---

## 5. Verification & Testing

Verify that both ports are active and listening on the Jetson:

```bash
ss -tulpn | grep -E '20000|20001'
```

Output:
```
tcp   LISTEN 0 128 0.0.0.0:20000 0.0.0.0:* users:(("mellzi-server",pid=1234,fd=3))
tcp   LISTEN 0 128 0.0.0.0:20001 0.0.0.0:* users:(("mellzi-server",pid=1234,fd=4))
```

From a remote workstation on the same subnet:
```bash
mellzi-client --host <JETSON_IP> --ctrl-port 20000 --frame-port 20001
```
