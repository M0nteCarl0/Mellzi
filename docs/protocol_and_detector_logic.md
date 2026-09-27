# Rayence DaVinci Protocol & Reconstructed Detector Logic

This document details the reverse-engineered communication protocol, internal thread model, and hardware interaction layer of **Rayence Flat Panel Detectors (FPD)** running the **DaVinci** firmware architecture (e.g. 1717SCC).

---

## 1. Internal Firmware Thread Architecture

Analysis of the Rayence ARM Linux daemon (`davinci`) reveals the following multi-threaded architecture:

```
                          [ davinci Daemon ]
                                  |
        +-------------------------+-------------------------+
        |                         |                         |
   [ CtrlTask ]              [ SendTask ]              [ SignalTask ]
   (Port 20000)              (Port 20001)              (Sensor & Trigger)
        |                         |                         |
   +----+----+               +----+----+               +----+----+
   |         |               |         |               |         |
CtrlProc  MessageProc     SendProc  FlashSaveProc  TriggerProc  ReadoutProc
(Cmd/Rsp) (Telemetry)     (Frames)  (Raw Logs)     (Sync)       (ADC/DMA)
```

### Core Firmware Tasks
1. **`CtrlTask`**: Listens on TCP port `20000`. Spawns `CtrlProc` upon connection. Receives 132-byte command packets from the acquisition workstation, executes configuration/acquisition routines, and sends response packets.
2. **`SendTask`**: Listens on TCP port `20001`. Spawns `SendProc` to stream raw 16-bit pixel data across the network as soon as readout completes.
3. **`SignalTask` & `TriggerProc`**: Interfaces with the hardware sync signal from the X-ray generator, handles exposure windows, and triggers detector readout.
4. **`ReadoutProc`**: Reads sensor data from the FPGA / EIM bus into physical memory buffers.
5. **`MessageProc` / `Telemetry`**: Periodically samples battery state, temperature sensors, Wi-Fi link quality, and G-sensor data, transmitting telemetry packets to the workstation.

---

## 2. Network Transport & Framing

Communication uses two dedicated TCP sockets:
- **Control Socket (Port 20000)**: Command exchange, handshakes, status queries, configuration persistence, and periodic telemetry.
- **Frame Socket (Port 20001)**: Dedicated uncompressed 16-bit image streaming.

### Packet Structure (Fixed 132 Bytes)
All messages exchanged over port `20000` (and control frames on port `20001`) use a fixed 132-byte frame:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Command ID (4 bytes)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                      Payload (128 bytes)                      |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Command Section Multiplexing
The command identifier is split into a **Section Tag** and **Sub-Command**:
- `Section = CommandID >> 16`
- `SubCommand = CommandID & 0xFFFF`

| Section Tag | Hex Range | Meaning | Client Prefix | Server Reply Prefix |
|:---|:---|:---|:---|:---|
| `0` | `0x00000..0x000FF` | Capture & Configuration | Direct code | Code or specialized Ack |
| `1` | `0x10000..0x11082` | Auxiliary (AUX) | `0x10000 | Code` | `0x11000 | Code` |
| `2` | `0x20000..0x22024` | Backup / EEPROM (BAK) | `0x20000 | Code` | `0x22000 | Code` |

---

## 3. Reconstructed Command Dictionary

### 3.1 Auxiliary Commands (AUX: Section 1)
Used for version inspection, device identification, self-test, and power management:

| Request Code | Response Code | Name | Description |
|:---|:---|:---|:---|
| `0x10000` | `0x11000` | `AUX_DEF_STAT` | Query default detector operational status |
| `0x10001` | `0x11001` | `AUX_LED_STAT` | Query / set LED indicator state |
| `0x10002` | — | `AUX_SET_TIME` | Synchronize detector internal clock |
| `0x10003` | `0x11003` | `AUX_GCAL` | Request hardware gain calibration status |
| `0x10010` | `0x11010` | `AUX_VER_FIRM` | Query firmware version string |
| `0x10011` | `0x11011` | `AUX_VER_FPGA` | Query FPGA logic version string |
| `0x10012` | `0x11012` | `AUX_VER_MAIN` | Query main daemon executable version string |
| `0x10013` | `0x11013` | `AUX_VER_TFTP` | Query internal TFTP loader version |
| `0x10014` | `0x11014` | `AUX_VER_CSIS` | Query CSI sensor interface subsystem version |
| `0x10015` | `0x11015` | `AUX_VER_LICENSE` | Query software / firmware license |
| `0x10016` | `0x11016` | `AUX_VER_IP` | Query detector active IP address |
| `0x10017` | `0x11017` | `AUX_VER_MAC` | Query detector hardware MAC address |
| `0x10020` | `0x11020` | `AUX_SELF_ELST` | Self-test: ELST internal test |
| `0x10021` | `0x11021` | `AUX_SELF_ELAM` | Self-test: ELAM memory validation |
| `0x10023` | `0x11023` | `AUX_SELF_PLST` | Self-test: panel communication test |
| `0x10024` | `0x11024` | `AUX_SELF_XTST` | Self-test: X-ray interface test |
| `0x10025` | `0x11025` | `AUX_DETE_STAT` | Query detailed sensor diagnostic status |
| `0x10026` | `0x11026` | `AUX_FRAME_NUM` | Query number of frames stored in flash |
| `0x10027` | `0x11027` | `AUX_FRAME_LOAD` | Request stored frame retrieval |
| `0x10028` | `0x11028` | `AUX_REMAIN_FRAME_LOAD` | Retrieve remaining unread frames |
| `0x10029` | `0x11029` | `AUX_REMAIN_FRAME_REMOVE` | Erase stored frames from detector flash |
| `0x10030` | `0x11030` | `AUX_TEST_PTRN` | Enable hardware test pattern generation |
| `0x10040` | `0x11040` | `AUX_RST_HARD` | Trigger hardware reboot / reset |
| `0x10050` | `0x11050` | `AUX_XIMG_RSND` | Resend last acquired frame |
| `0x10060` | `0x11060` | `AUX_GET_COUNT` | Query lifetime acquisition counters |
| `0x10070` | `0x11070` | `AUX_CHECK_FIRM` | Check firmware integrity / checksum |
| `0x10080` | `0x10080` | `AUX_SLEEP` | Transition detector into low-power sleep mode |
| `0x10081` | `0x10081` | `AUX_WAKEUP` | Wake up detector from sleep mode |
| `0x10082` | `0x10082` | `AUX_BATTERY` | Query smart battery status |

### 3.2 Backup & Hardware EEPROM Commands (BAK: Section 2)
Used for reading and programming factory panel calibration constants into onboard EEPROM:

| Request Code | Response Code | Name | Description |
|:---|:---|:---|:---|
| `0x20000` | — | `BAK_ENTER_BACKUP_MODE` | Unlock backup configuration mode |
| `0x20012` | `0x22012` | `BAK_WR_WMBD` | Program motherboard identifier |
| `0x20014` | `0x22014` | `BAK_WR_VPNL` | Program panel manufacturing identifier |
| `0x20016` | `0x22016` | `BAK_WR_VCSI` | Program CSI sensor identifier |
| `0x20021` | `0x22021` | `BAK_RD_ELAD` | Read ELAD configuration block |
| `0x20022` | `0x22022` | `BAK_WR_ELAD` | Write ELAD configuration block |
| `0x20023` | `0x22023` | `BAK_RD_ELAF` | Read ELAF configuration block |
| `0x20024` | `0x22024` | `BAK_WR_ELAF` | Write ELAF configuration block |

### 3.3 Capture & Telemetry Commands (Section 0)
Controls exposure timing, dark frame updates, and sensor telemetry:

| Command ID | Direction | Name | Description |
|:---|:---|:---|:---|
| `0x00001` | Client $\to$ Srv | `CAPTURE_READYIN` | Trigger detector initialization |
| `0x00011` | Srv $\to$ Client | `CAPTURE_INITIALISATION_DONE` | Detector initialization completed |
| `0x00002` | Client $\to$ Srv | `CAPTURE_START` | Trigger acquisition / exposure window |
| `0x00012` | Srv $\to$ Client | `CAPTURE_START_DONE` | Exposure sequence initiated |
| `0x00003` | Client $\to$ Srv | `CAPTURE_ABORT` | Cancel active acquisition |
| `0x00013` | Srv $\to$ Client | `CAPTURE_ABORT_DONE` | Cancellation confirmed |
| `0x00007` | Client $\to$ Srv | `CAPTURE_READY_DONE_REQ` | Client ready notification |
| `0x00017` | Srv $\to$ Client | `CAPTURE_READY_DONE` | Detector ready for X-ray exposure |
| `0x0000B` | Client $\to$ Srv | `CAPTURE_SHUTDOWN` | Initiate graceful detector power-off |
| `0x0001B` | Srv $\to$ Client | `CAPTURE_SHUTDOWN_DONE` | Shutdown acknowledged |
| `0x00019` | Srv $\to$ Client | `CAPTURE_BATTERY_REMAIN` | Telemetry: battery capacity & voltage |
| `0x0001A` | Srv $\to$ Client | `CAPTURE_WLAN_RSSI` | Telemetry: wireless RSSI in dBm |
| `0x00020` | Srv $\to$ Client | `CAPTURE_REDARK_DONE` | Dynamic dark offset recalculation complete |
| `0x00021` | Srv $\to$ Client | `CAPTURE_XRAY_START` | Signal: X-ray window open |
| `0x00022` | Srv $\to$ Client | `CAPTURE_XRAY_STOP` | Signal: X-ray window closed |
| `0x00023` | Srv $\to$ Client | `CAPTURE_READY_ON` | Signal: generator exposure enable ON |
| `0x00024` | Srv $\to$ Client | `CAPTURE_READY_OFF` | Signal: generator exposure enable OFF |
| `0x00031` | Srv $\to$ Client | `CAPTURE_THERMAL_INFO` | Telemetry: temperature sensors T1 & T2 |
| `0x00051` | Srv $\to$ Client | `CAPTURE_AT_INFO` | Auto-trigger threshold telemetry |
| `0x00052` | Srv $\to$ Client | `CAPTURE_AT_READY` | Auto-trigger armed |
| `0x00055` | Srv $\to$ Client | `CAPTURE_DO_SHIFT` | Handshake: frame data streaming begins |
| `0x00056` | Client $\to$ Srv | `CAPTURE_REDARK` | Request dark offset refreshment |
| `0x00059` | Srv $\to$ Client | `CAPTURE_AXIS_INFO` | Telemetry: 3-axis accelerometer (X, Y, Z) |
| `0x000FF` | Srv $\to$ Client | `CAPTURE_FRAME_DONE` | Image transfer completed |

### 3.4 Configuration Commands (Section 0)
Handles detector operational registers:

| Command ID | Direction | Name | Payload Size |
|:---|:---|:---|:---|
| `0x00005` | Client $\to$ Srv | `CONFIG_INIT_UP` | Request `.initcfg` table |
| `0x00015` | Srv $\to$ Client | `CONFIG_INIT_UP_DONE` | Returns 32 bytes of `.initcfg` |
| `0x00006` | Client $\to$ Srv | `CONFIG_ELSET_UP` | Request `.elsetcfg` table |
| `0x00016` | Srv $\to$ Client | `CONFIG_ELSET_UP_DONE` | Returns 128 bytes of `.elsetcfg` |
| `0x00034` | Client $\to$ Srv | `CONFIG_ELSET_UP2` | Request `.elsetcfg2` table |
| `0x00044` | Srv $\to$ Client | `CONFIG_ELSET_UP2_DONE` | Returns 128 bytes of `.elsetcfg2` |
| `0x00035` | Client $\to$ Srv | `CONFIG_ELSET_UP3` | Request `.elsetcfg3` table |
| `0x00045` | Srv $\to$ Client | `CONFIG_ELSET_UP3_DONE` | Returns 128 bytes of `.elsetcfg3` |
| `0x0000C` | Client $\to$ Srv | `CONFIG_INIT_SAVE` | Uploads and saves 32 bytes to `/home/.initcfg` |
| `0x0000D` | Client $\to$ Srv | `CONFIG_ELSET_SAVE` | Uploads and saves 128 bytes to `/home/.elsetcfg` |
| `0x00032` | Client $\to$ Srv | `CONFIG_ELSET2_SAVE` | Uploads and saves 128 bytes to `/home/.elsetcfg2` |
| `0x00033` | Client $\to$ Srv | `CONFIG_ELSET3_SAVE` | Uploads and saves 128 bytes to `/home/.elsetcfg3` |

---

## 4. Acquisition & Readout Timing Sequence

The exact frame acquisition workflow executed between workstation and detector is:

```
Workstation                                            Detector
    |                                                     |
    |---- Connect TCP 20000 (Control) ------------------->|
    |---- Connect TCP 20001 (Frame Data) ---------------->|
    |                                                     |
    |---- CMD 0x01 (CAPTURE_READYIN) -------------------->|
    |<--- CMD 0x11 (CAPTURE_INITIALISATION_DONE) ---------|
    |                                                     |
    |---- CMD 0x02 (CAPTURE_START) ---------------------->|
    |<--- CMD 0x12 (CAPTURE_START_DONE) ------------------|
    |                                                     |
    |      [ Generator Exposure & Panel Readout ]         |
    |                                                     |
    |<=== CMD 0x55 (CAPTURE_DO_SHIFT) [Port 20001] =======|
    |<=== Stream 22,151,168 bytes Raw Pixels [Port 20001] |
    |<=== CMD 0xFF (CAPTURE_FRAME_DONE) [Port 20001] =====|
    |                                                     |
    |   [ Frame Received: Dark/Gain/BPM Calibration ]     |
```

---

## 5. Configuration File Structures

### 5.1 `.initcfg` Binary Structure (32 Bytes)
Stored at `/home/.initcfg` on the detector. Represents low-level analog ASIC timing flags:
```cpp
struct InitCfg {
    uint8_t data[32]; // 32 bytes binary table
};
```
Default signature: `00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 01 00 00 01 01 01 00 00 00 00 00 00 00 01 00`.

### 5.2 `.elsetcfg` Binary Structure (128 Bytes)
Stored at `/home/.elsetcfg`, `/home/.elsetcfg2`, and `/home/.elsetcfg3`. Represents gate driver and charge readout IC (ROIC) bias voltages and clock timings. Exactly 128 bytes.

### 5.3 `VADav.INI` Configuration Map (281 Parameters)
Located in the configuration directory. Contains `[Init1]` section mapping directly to `DetectorInitParameters`:
- **Core Timing (57 fields)**: `FCtrl`, `IntB`, `Om`, `Scan`, `Speed`, `EOSel`, `Ag0..Ag2`, `DA0..DA8`, `Gate0..Gate4`, `Ex_Time`, `Loop_Cnt`, `Frame_Cnt`, `Frame_Delay`, `EL_Delay`, `EL_High`, `Gate_Low`, `Gate_High`, `ReadOut_Count`, `Sh0`, `Sh1`, `Gate`, `TestPattern`, etc.
- **Options (128 fields)**: `Option00` through `Option7f`.
- **Controls (96 fields)**: `Control00` through `Control5f`.
