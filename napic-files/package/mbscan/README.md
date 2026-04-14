# mbscan — Fast Modbus RTU Bus Scanner

A lightweight command-line utility that scans a Modbus RTU serial bus for connected slave devices. Opens the serial port once and probes a range of addresses by sending FC03 (Read Holding Registers) requests.

## Features

- **Fast** — scans 247 addresses in ~2.5 seconds at 115200 baud (10ms timeout)
- **Cross-platform** — Linux (x86_64, aarch64), Windows (x86_64), OpenWrt, Raspberry Pi, Armbian
- **Zero dependencies** — pure C, no libmodbus required
- **Own CRC16** — built-in Modbus CRC16 implementation
- **Configurable** — baud rate, data format, address range, timeout, register count
- **Multi-port scanning** — scan multiple ports in one run with wildcards or comma-separated list
- **Universal scan** — auto-detect baud rate and data format (`-T`, `-B`, `-D`)
- **Scan results report** — summary of all found devices at the end (`-R`)
- **Busy port detection** — detects non-Modbus traffic when write errors occur

## Quick Start

### Linux

```bash
# Scan all addresses on /dev/ttyUSB0 (default: 115200-8N1, timeout 100ms)
mbscan /dev/ttyUSB0

# Fast scan with 10ms timeout
mbscan /dev/ttyUSB0 -o 10

# Scan specific range, read 4 registers
mbscan /dev/ttyUSB0 -f 1 -t 30 -c 4

# 9600 baud, 8E1 parity
mbscan /dev/ttyS1 -b 9600 -d 8E1

# Scan multiple ports at once
mbscan /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyUSB2

# Wildcards — scan all USB serial ports (shell expands the glob)
mbscan /dev/ttyUSB*

# Scan all USB and ACM ports
mbscan /dev/ttyUSB* /dev/ttyACM*

# Comma-separated ports via -p
mbscan -p /dev/ttyUSB0,/dev/ttyUSB1,/dev/ttyS0

# Universal scan: try all baud rates x data formats
mbscan -T /dev/ttyUSB0

# Universal scan with summary report
mbscan -T -R /dev/ttyUSB0
```

### Windows

```cmd
rem Scan COM3 with default settings
mbscan.exe COM3

rem Fast scan, 10ms timeout
mbscan.exe COM3 -o 10

rem Scan addresses 1-30, read 4 registers, 9600 baud
mbscan.exe COM5 -b 9600 -d 8E1 -f 1 -t 30 -c 4

rem Scan multiple ports
mbscan.exe COM3 COM4 COM5

rem Comma-separated ports via -p
mbscan.exe -p COM3,COM4,COM5

rem Universal scan
mbscan.exe -T COM3
```

## Output Example

### Single port scan

```
mbscan: scanning for Modbus RTU slaves (addresses 1-247).
mbscan: use only on ports where Modbus sensors are expected.

mbscan: scanning /dev/ttyUSB0 115200-8N1, addresses 1-247, timeout 100ms
mbscan: reading 4 register(s) starting at 0

Found slave 125 at 115200-8N1: [0]=125 [1]=1 [2]=830 [3]=794

mbscan: done in 00:25. Found 1 device(s).
```

### Multi-port scan with report (`-R`)

```
mbscan: scanning for Modbus RTU slaves (addresses 1-247).
mbscan: use only on ports where Modbus sensors are expected.

=== Port /dev/ttyUSB0 [1/2] ===
...
Found slave   3 at 9600-8E1: [0]=100 [1]=200

=== Port /dev/ttyUSB1 [2/2] ===
...
Found slave  17 at 115200-8N1: [0]=50

=== Scan results ===
  /dev/ttyUSB0      Slave   3 at 9600-8E1: [0]=100 [1]=200
  /dev/ttyUSB1      Slave  17 at 115200-8N1: [0]=50

mbscan: done in 00:52. Found 2 device(s).
```

### Busy port detection

If write errors occur, mbscan stops probing that port, listens passively for 500ms, and tries to identify what is there:

```
Write error at address 1: Input/output error
Write error at address 2: Input/output error
Write error at address 3: Input/output error
mbscan: 3 consecutive write errors on /dev/ttyUSB0 at 9600-8N1
mbscan: port may be occupied by another device.
mbscan: listening for incoming data (500ms)...
mbscan: detected: GPS/NMEA stream
mbscan: skipping port /dev/ttyUSB0.
```

Detected protocols: GPS/NMEA, AT/LTE modem, ESP8266/ESP32 boot log, Modbus master traffic, unknown text or binary protocol.

## Usage

```
Usage: mbscan [options] PORT [PORT ...]
       mbscan -p PORT[,PORT,...] [options]

Modbus RTU bus scanner. Sends FC03 to each address and reports responses.
Use only on ports where Modbus sensors are expected.

Options:
  -p PORT    Serial port(s), comma-separated
             Linux: /dev/ttyUSB0,/dev/ttyUSB1
             Windows: COM3,COM4
  -b BAUD    Baud rate (default: 115200)
  -d PARAMS  Data format: 8N1, 8E1, 8O1, 7E1, etc. (default: 8N1)
  -f FROM    Start address (default: 1)
  -t TO      End address (default: 247)
  -o MS      Timeout per address in ms (default: 100)
  -r REG     Start register, 0-based (default: 0)
  -c COUNT   Number of registers to read (default: 1)
  -T         Total scan: try all baud rates x data formats
               Bauds: 9600, 19200, 38400, 115200
               Formats: 8N1, 8E1
  -B         Scan baud rates only, keep format from -d
  -D         Scan data formats only, keep baud from -b
  -S         Stop after first device found
  -R         Print summary report at the end
  -v         Verbose output
  -h         Show this help
```

### Multi-port usage

Multiple ports can be specified as positional arguments or via `-p` with a comma-separated list. Both can be combined:

```bash
# All equivalent:
mbscan /dev/ttyUSB0 /dev/ttyUSB1
mbscan -p /dev/ttyUSB0,/dev/ttyUSB1
mbscan -p /dev/ttyUSB0 /dev/ttyUSB1

# Shell wildcards — scans all matching ports:
mbscan /dev/ttyUSB*
mbscan /dev/ttyUSB* /dev/ttyACM*
mbscan /dev/ttyS0 /dev/ttyUSB*
```

> **Note:** Wildcards are expanded by the shell, not by mbscan. On Windows cmd/PowerShell, use comma-separated `-p` instead.

## Prebuilt Binaries

Prebuilt binaries are available on the [Releases](../../releases) page:

| File | Platform | Notes |
|---|---|---|
| `mbscan-linux-x86_64` | Linux x86_64 | Static binary, no dependencies |
| `mbscan-linux-aarch64` | Linux aarch64 (ARM64) | Static binary |
| `mbscan.exe` | Windows x86_64 | Static binary, no dependencies |

## Build from Source

### Linux — native

```bash
gcc -O2 -Wall -o mbscan mbscan.c
```

Static build (portable, no dependencies):

```bash
gcc -O2 -Wall -static -o mbscan mbscan.c
```

### Linux ARM64 — native on device

```bash
sudo apt install gcc
gcc -O2 -Wall -o mbscan mbscan.c
```

### Linux ARM64 — cross-compile from x86_64 host

```bash
sudo apt install gcc-aarch64-linux-gnu
aarch64-linux-gnu-gcc -O2 -Wall -static -o mbscan-linux-aarch64 mbscan.c
```

### Windows — cross-compile from Linux

```bash
sudo apt install gcc-mingw-w64-x86-64
x86_64-w64-mingw32-gcc -O2 -Wall -static -o mbscan.exe mbscan.c
```

### Windows — native with MinGW/MSYS2

```bash
pacman -S mingw-w64-x86_64-gcc
gcc -O2 -Wall -o mbscan.exe mbscan.c
```

### Windows — native with MSVC

```cmd
cl mbscan.c /Fe:mbscan.exe /O2
```

### OpenWrt Package

```bash
cp -r mbscan /path/to/openwrt/package/
cd /path/to/openwrt
echo "CONFIG_PACKAGE_mbscan=y" >> .config
make package/mbscan/compile -j$(nproc)
```

### Cross-compile for aarch64 via OpenWrt toolchain

```bash
/path/to/openwrt/staging_dir/toolchain-aarch64_generic_gcc-*/bin/aarch64-openwrt-linux-gcc \
  -O2 -Wall -static -o mbscan-linux-aarch64 mbscan.c
```

## How It Works

1. Opens each serial port and configures baud rate, parity, data/stop bits
2. For each address in the range:
   - Flushes stale data from the port
   - Sends an 8-byte Modbus RTU FC03 request with CRC16
   - Waits for response with configurable timeout
   - Validates response: CRC, slave address, function code
   - Reports found devices with register values
3. Observes Modbus inter-frame delay (3.5 character times) between requests
4. If 3 consecutive write errors occur, listens passively for 500ms, attempts to identify the occupying device, then skips the port
5. With `-T`/`-B`/`-D`, repeats the scan for each baud/format combination on the same open port
6. With multiple ports, scans each in sequence and collects all results into a unified report

## Performance

| Timeout | Full scan (1–247) | Notes |
|---|---|---|
| 10 ms | ~2.5 sec | Minimum, may miss devices |
| 20 ms | ~5 sec | Minimum recommended |
| 50 ms | ~12 sec | Recommended for most setups |
| 100 ms | ~25 sec | Default, reliable |
| 200 ms | ~50 sec | Long RS-485 lines |

> **Note on short timeouts:** With `-o 10` some devices may not be detected. The minimum recommended timeout is **20ms**. If a device is not found, try increasing the timeout before checking wiring or address.

## Windows Notes

- COM port names are passed as-is: `COM3`. The `\\.\` prefix is added automatically for high-numbered ports (COM10+).
- Find your COM port number in Device Manager → Ports (COM & LPT).
- Common USB-Serial adapters (CH341, CP2102, FTDI, PL2303) install their drivers automatically on Windows 10/11.
- Wildcard expansion (`COM*`) is not supported on Windows cmd/PowerShell. Use comma-separated `-p` instead: `mbscan.exe -p COM3,COM4,COM5`.

## Integration

`mbscan` is used as the backend for the **Scan Bus** tab in [luci-app-mbpoll](https://github.com/lab240/luci-app-mbpoll) — a LuCI web interface for Modbus device polling and bus scanning on OpenWrt.

## NAPI Boards

This project is developed and maintained by the [NAPI Lab](https://github.com/napilab) team and is primarily tested on NAPI industrial single-board computers based on Rockchip SoCs.

[github.com/napilab/napi-boards](https://github.com/napilab/napi-boards)

- **NAPI2** — RK3568J, RS-485 onboard, Armbian
- **NAPI-C** — RK3308, compact, industrial grade

## Hardware

Tested on:

- [NAPI boards](https://github.com/napilab/napi-boards) — industrial IoT gateways (RK3308, RK3568J, aarch64, OpenWrt/Armbian)
- Standard Linux x86_64 with USB-RS485 adapters (CH341, CP2102, FTDI)
- Windows 10/11 x86_64 with USB-RS485 adapters

## Authors

- **Dmitrii Novikov** ([@dmnovikov](https://github.com/dmnovikov)) — [NAPI Lab](https://github.com/napilab)
- **Claude** (Anthropic) — AI assistant and co-author — architecture, code, documentation

## License

MIT