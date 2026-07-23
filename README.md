# ESP32-CYD-PC-Monitor-PoC

*This project was developed with AI-assisted code generation and human oversight.*

> **PROOF OF CONCEPT**: PC metrics displayed on an ESP32 Cheap Yellow Display (CYD).

This repository contains a proof-of-concept system where a PC acts as a server and an ESP32 with a CYD display acts as a client, receiving and displaying system metrics via Serial.

This is intentionally experimental, incomplete, and **not production-ready.**

## What this *IS*

- An ESP32 client (CYD + LVGL) that:
  - Receives metrics formatted as JSON over Serial
  - Displays them on a 2.8" ST7789 TFT
  - Times out and clears after 5 seconds of no data

The ESP32 does not parse arbitrary JSON schemas — it consumes a known, fixed JSON format.

## What this IS *NOT*

- Not a finished product
- Not a reusable library
- Not optimized
- Not hardened for security
- Not guaranteed to be stable

This repo exists as proof-of-existence and experimentation.

## Architecture (high level)

```
PC (Companion App)
  └─ Collects system metrics
  └─ Sends JSON over Serial (USB)

ESP32-CYD (Client)
  └─ Receives JSON over Serial
  └─ Parses with ArduinoJson
  └─ Displays values via LVGL
```

## Hardware

- **MCU**: ESP32-2432S028 (Cheap Yellow Display)
- **Display**: 2.8" ST7789 TFT (320×240)
- **Power**: USB-C (5V)

### Pin Configuration

| Component | Pins |
|-----------|------|
| **TFT** | CS: 15, DC: 2, RST: -1 (tied to ESP32 RST), LED: 21, MOSI: 13, MISO: 12, SCLK: 14 |

> **Note:** TFT_RST is set to `-1` — the display reset pin is connected to the ESP32's RST pin, so the display resets with the ESP32.

## Software

- **PC Server:** written in Go (companion app sends JSON over Serial)
- **ESP32 firmware:** written in Arduino-style C++ with LVGL v8.4

### Dependencies (PlatformIO)

- `bodmer/TFT_eSPI@^2.5.43`
- `bblanchon/ArduinoJson@^7.4.2`
- `lvgl/lvgl@8.4.0`

## Data Format

The ESP32 expects a single-line JSON payload ending with `\n` at 115200 baud:

```json
{
  "network": {
    "network_type": "Wi-Fi",
    "ping": 12,
    "jitter": 3,
    "packet_loss": 0
  },
  "cpu": {
    "load": 45,
    "temp": 65
  },
  "gpu": {
    "load": 78,
    "temp": 72
  },
  "ram": {
    "used_gb": 8.2,
    "total_gb": 16.0,
    "percent": 51
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `network.network_type` | string | `Wi-Fi` or `Ethernet` |
| `network.ping` | int | Ping in ms |
| `network.jitter` | int | Jitter in ms |
| `network.packet_loss` | int | Packet loss percentage |
| `cpu.load` | int | CPU usage percentage |
| `cpu.temp` | int | CPU temperature in °C |
| `gpu.load` | int | GPU usage percentage |
| `gpu.temp` | int | GPU temperature in °C |
| `ram.used_gb` | float | RAM used in GB |
| `ram.total_gb` | float | Total RAM in GB |
| `ram.percent` | int | RAM usage percentage |

## Quick Start

### 1. Install Dependencies

```bash
pio lib install
```

### 2. Build & Upload

```
pio run -t upload
```

### 3. Connect to PC

Connect the ESP32 to your PC via USB. The PC companion application sends JSON data over Serial at 115200 baud.

## Color Thresholds

Metrics are color-coded:

| Color | Meaning |
|-------|---------|
| 🟢 Green | Good/Healthy |
| 🟡 Orange | Warning |
| 🔴 Red | Critical/Error |
| ⚪ Gray | N/A or Inactive |

### Network

| Metric | Wi-Fi (Warn / Error) | Ethernet (Warn / Error) |
|--------|----------------------|-------------------------|
| **Ping** | >20ms / >50ms | >10ms / >20ms |
| **Jitter** | >5ms / >10ms | >2ms / >5ms |
| **Packet Loss** | >0.5% / >2% | >0.1% / >1% |

### CPU / GPU

| Metric | Good | Warn | Error |
|--------|------|------|-------|
| **CPU Load** | <50% | 50–75% | >75% |
| **CPU Temp** | <70°C | 70–85°C | ≥85°C |
| **GPU Load** | <50% | 50–75% | >75% |
| **GPU Temp** | <75°C | 75–85°C | ≥85°C |

### RAM

| Metric | Good | Warn | Error |
|--------|------|------|-------|
| **Usage** | <60% | 60–80% | >80% |

## Project Structure

```
ESP32-CYD-PC-Monitor-PoC/
├── src/
│   ├── Inconsolata_16px.c
│   ├── Inconsolata_16px.h
│   ├── Inconsolata_18px.c
│   ├── Inconsolata_18px.h
│   ├── Inconsolata_26px.c
│   ├── Inconsolata_26px.h
│   └── main.cpp
├── lv_conf.h
├── platformio.ini
├── User_Setup.h
├── README.md
└── PC-Resource-Monitor-CYD_ARCHITECTURE.md
```

## Troubleshooting

| Symptom | Likely Fix |
|---------|------------|
| Display stays white/black | Check TFT wiring; verify `User_Setup.h` pin match |
| No data shows | Ensure PC is sending JSON over Serial at 115200 baud |
| Data shows then clears | `DATA_TIMEOUT_MS` is 5 seconds; ensure PC sends updates continuously |
| Wrong colors | Check `TFT_RGB_ORDER` in `User_Setup.h` (TFT_RGB or TFT_BGR) |
| Display flickers | Adjust `BUF_HEIGHT` in main.cpp (currently 50px) |
| Backlight dim/off | Check `TFT_BL` pin (21) and `TFT_BACKLIGHT_ON` (HIGH) |

## Repository status

- Snapshot-style upload
- No git history
- No expectation of maintenance
- May contain rough code, hardcoded values, or hacks

## Notes

If you are looking for:
- A polished PC monitor: this is not it
- A reusable ESP32 framework: this is not it
- A reference PoC for ESP32 + PC metrics: **this is it**

Use, fork, or ignore as you please.

## For Developers

This project enforces a strict code layout standard documented in [`docs/Code_Layout_Standard.md`](docs/Code_Layout_Standard.md). Key rules:

- One logical module per file
- Headers declare public API only — no implementation
- Source files contain all implementation and internal state
- Comment hierarchy: T1 (file header) → T7 (inline notes)

When contributing, follow the visual hierarchy scale defined in the standard document.

## License

MIT