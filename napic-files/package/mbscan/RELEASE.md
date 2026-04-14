# mbscan v1.1.1 — Multi-port scan & diagnostics

## What's New

- **Multi-port scanning** — pass multiple ports as arguments or via `-p` with comma-separated list:
  ```bash
  mbscan /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyUSB2
  mbscan /dev/ttyUSB*
  mbscan -p /dev/ttyUSB0,/dev/ttyUSB1
  ```
- **Scan results report** (`-R`) — summary at the end showing port, address, baud/format and register values. Enabled automatically on multi-port or universal scans (`-T`/`-B`/`-D`):
  ```
  === Scan results ===
    /dev/ttyUSB0      Slave   3 at 9600-8E1: [0]=100 [1]=200
    /dev/ttyUSB1      Slave  17 at 115200-8N1: [0]=50
  ```
- **Write error diagnostics** — if 3 consecutive write errors occur, mbscan stops scanning that port, listens passively for 500ms and attempts to identify what is connected:
  ```
  mbscan: 3 consecutive write errors on /dev/ttyUSB0 at 9600-8N1
  mbscan: port may be occupied by another device.
  mbscan: listening for incoming data (500ms)...
  mbscan: detected: GPS/NMEA stream
  mbscan: skipping port /dev/ttyUSB0.
  ```
  Detected protocols: GPS/NMEA, AT/LTE modem, ESP8266/ESP32 boot log, Modbus master, unknown text or binary protocol.
- **Startup warning** — mbscan reminds you to use it only on ports where Modbus sensors are expected.

## Downloads

| File | Platform | How to build |
|---|---|---|
| `mbscan-linux-x86_64` | Linux x86_64 | `gcc -O2 -Wall -static -o mbscan-linux-x86_64 mbscan.c` |
| `mbscan-linux-aarch64` | Linux ARM64 | `aarch64-linux-gnu-gcc -O2 -Wall -static -o mbscan-linux-aarch64 mbscan.c` |
| `mbscan.exe` | Windows x86_64 | `x86_64-w64-mingw32-gcc -O2 -Wall -static -o mbscan.exe mbscan.c` |

All binaries are statically linked — no runtime dependencies, just copy and run.

## Usage

```bash
# Linux
mbscan /dev/ttyUSB0
mbscan -T -R /dev/ttyUSB0 /dev/ttyUSB1
mbscan -b 9600 -d 8E1 -f 1 -t 30 -c 4 /dev/ttyUSB0

# Windows
mbscan.exe COM3
mbscan.exe -p COM3,COM4 -T -R
mbscan.exe -b 9600 -d 8E1 -f 1 -t 30 -c 4 COM5
```

## Note on timeouts

Default timeout is 100ms per address (~25 sec full scan). For faster scans use `-o 20` (minimum recommended). Timeout `-o 10` may miss devices, especially on Windows due to USB-Serial driver latency.

---

Full documentation: [README](https://github.com/lab240/mbscan#readme)