# OpenWrt for NapiLab Napi family (RK3308)

Production-ready OpenWrt build for **NapiLab Napi** industrial IoT SBC and SOM based on Rockchip RK3308 SoC.

This repository contains all customizations needed to turn a vanilla OpenWrt snapshot into a fully functional industrial Modbus TCP gateway with a polished web interface.

---

![](img/srn.jpeg)

---

## Supported hardware

| Board | Storage | Docs |
|-------|---------|------|
| [NapiLab Napi-C](https://napiworld.ru/docs/napi-intro/) | 4GB NAND — 32GB eMMC | Industrial SBC |
| [NapiLab Napi-P](https://napiworld.ru/docs/napi-intro/) | 4GB NAND — 32GB eMMC | Industrial SBC |
| [NapiLab Napi-Slot](https://napiworld.ru/docs/napi-som-intro) | 4GB NAND — 32GB eMMC | SOM |
| [Radxa ROCK Pi S](https://wiki.radxa.com/RockpiS) | — | Community board, same RK3308 SoC |

> All boards share the same Rockchip RK3308 SoC — one firmware to rule them all.

---
## Using with other RK3308 boards
 
This build is not limited to NapiLab Napi boards. Any board based on **Rockchip RK3308** can use the same customizations — just change the target profile in `.config`.
 
To build for a different board, replace the target device:
 
```bash
# List available RK3308 boards
grep "CONFIG_TARGET_rockchip_armv8_DEVICE" .config
 
# Example: build for Radxa ROCK Pi S
sed -i 's/CONFIG_TARGET_PROFILE="DEVICE_napilab_napic"/CONFIG_TARGET_PROFILE="DEVICE_radxa_rock-pi-s"/' .config
```
 
To switch to a different board, edit `.config` directly — never use `make menuconfig` as it will overwrite your configuration:
```bash
# Switch target profile to Radxa ROCK Pi S
sed -i 's/DEVICE_napilab_napic/DEVICE_radxa_rock-pi-s/g' .config
```
 
All uci-defaults scripts, packages and customizations will work on any RK3308 board. The only board-specific parts are:
- Device Tree (`rk3308-napi-c.dts`) — may need adjustment for different pin mux
- U-Boot defconfig (`napic-rk3308`) — based on ROCK Pi S, works on similar hardware
- MAC generation from OTP — works on all RK3308 boards
 
> The same approach works for any OpenWrt-supported Rockchip board (RK3328, RK3566, RK3568, RK3588) — just select the appropriate target profile and adapt the DTS if needed.

---

## SoC features

| Component | Details |
|-----------|---------|
| SoC | Rockchip RK3308 (quad-core ARM Cortex-A35, 1.3GHz) |
| RAM | 256MB / 512MB DDR3 |
| Storage | 4GB NAND up to 32GB eMMC |
| Ethernet | 100Mbps (GMAC + RTL8201F PHY) |
| USB | 2x USB 2.0 Host |
| UART | 3x UART (ttyS0 console, ttyS1, ttyS2) |
| Wi-Fi | RTL8723DS (802.11b/g/n) |

---

## What's inside

### U-Boot
- Added `napic-rk3308` U-Boot variant based on Radxa ROCK Pi S (same RK3308 hardware)
- Custom `napic-rk3308_defconfig` patch

### Device Tree
- Custom DTS `rk3308-napi-c.dts` based on ROCK Pi S
- UART1 → `/dev/ttyS1`
- UART2 → `/dev/ttyS2`


### Stable MAC address
Generated deterministically from RK3308 OTP data — same MAC across reboots on every board:

```sh
MAC=$(cat /sys/bus/nvmem/devices/rockchip-otp0/nvmem | md5sum | \
  sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
```

### First-boot configuration (uci-defaults)

| Script | Purpose |
|--------|---------|
| `70-rootpt-resize` | Resize root partition to fill storage (reboot) |
| `80-rootfs-resize` | Expand filesystem after partition resize (reboot) |
| `91-bash` | Set bash as default shell for root |
| `92-timezone` | Set timezone to MSK-3 |
| `93-console-password` | Enable password prompt on serial console |
| `94-macaddr` | Generate stable MAC from OTP |
| `95-network` | Configure eth0 without bridge |
| `96-hostname` | Set hostname to `napiwrt` |
| `97-luci-theme` | Set LuCI theme to openwrt-2020 |
| `99-dhcp` | DHCP configuration |

### Pre-installed packages
- `mosquitto` + `mosquitto-client` — MQTT broker
- `mbusd` + `luci-app-mbusd` — Modbus TCP gateway with web UI
- `mbpoll` — Modbus CLI tool
- `kmod-usb-serial-ch341`, `cp210x`, `ftdi`, `pl2303` — USB-Serial adapters
- `kmod-usb-net-qmi-wwan` + `uqmi` — LTE modem support (Quectel EP06)
- `openssh-sftp-server` — SFTP access
- `bash`, `htop`, `nano`, `screen`, `tcpdump`, `ethtool` — admin tools
- `parted`, `fdisk`, `cfdisk`, `resize2fs`, `losetup` — disk management and partition resize
- `luci-ssl-wolfssl` — HTTPS for LuCI
---

## luci-app-mbusd

Web interface for mbusd Modbus gateway — the crown jewel of this build.

- Start / Stop / Restart service
- Enable / Disable autostart
- Live process status with actual running parameters
- Listening IP:port display
- Full serial port and Modbus configuration

→ See [luci-app-mbusd](https://github.com/YOUR_USERNAME/luci-app-mbusd) repository

---

## Building

### Prerequisites (Ubuntu/Debian)

```bash
sudo apt install build-essential clang flex bison g++ gawk gcc-multilib \
  gettext git libncurses-dev libssl-dev python3-distutils rsync unzip zlib1g-dev
```

### Setup

```bash
git clone https://github.com/openwrt/openwrt.git
cd openwrt
./scripts/feeds update -a
./scripts/feeds install -a
```

### Apply customizations

```bash
tar xzf napic-openwrt-YYYYMMDD-HHMM-v1.0.tar.gz -C /path/to/openwrt/
```

### Build U-Boot

```bash
make package/boot/uboot-rockchip/compile VARIANT=napic-rk3308 -j$(nproc)
```

### Build image

```bash
make -j$(nproc)
```

Output: `bin/targets/rockchip/armv8/openwrt-rockchip-armv8-napilab_napic-ext4-sysupgrade.img.gz`

---

## Flashing

```bash
gunzip openwrt-rockchip-armv8-napilab_napic-ext4-sysupgrade.img.gz
dd if=openwrt-rockchip-armv8-napilab_napic-ext4-sysupgrade.img of=/dev/sdX bs=4M status=progress
```

---

## Default access

| Parameter | Value |
|-----------|-------|
| IP | DHCP (stable MAC ensures consistent lease) |
| Web UI | `http://<IP>/` → LuCI |
| SSH | `root@<IP>` |
| Console | ttyS0, 1500000 baud |

---

## Zigbee2MQTT
 
This build supports running Zigbee2MQTT on OpenWrt. Since OpenWrt uses **musl libc**, the standard Node.js binaries won't work — a special musl/aarch64 build is required.
 
A pre-built archive is available in [Releases](https://github.com/lab240/napi-openwrt-build/releases):
 
```
zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz
```
 
### Quick start
 
```bash
# Install Node.js (musl/arm64)
wget https://unofficial-builds.nodejs.org/download/release/v22.22.0/node-v22.22.0-linux-arm64-musl.tar.gz
mkdir -p /opt/node
tar xzf node-v22.22.0-linux-arm64-musl.tar.gz -C /opt/node --strip-components=1
 
# Install Zigbee2MQTT
mkdir /opt/zigbee2mqtt
tar xzf zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz -C /opt/zigbee2mqtt/
 
# Install runtime dependency
apk add libstdcpp6
 
# Run
export PATH=/opt/node/bin:$PATH
cd /opt/zigbee2mqtt && npm start
```
 
Web UI available at `http://<IP>:8080/`
 
> Requires 512 MB RAM and ~500 MB free disk space. Mosquitto is already pre-installed in this build.
 

## Changelog

### v1.0.2
- Zigbee2MQTT 2.9.1 pre-built archive for musl/aarch64 available in Releases
- Automatic root partition and filesystem resize on first boot (scripts 70/80-rootpt-resize, double reboot)
- **Disk management**: `parted`, `fdisk`, `cfdisk`, `losetup`, `resize2fs`
- **1-Wire / DS18B20**: `owfs`, `owserver`, `owfs-client` — 1-Wire device support via OWFS
- **I2C**: `i2c-tools`, `libi2c` — I2C bus diagnostics and device access
- **GPIO**: `gpiod-tools`, `libgpiod` — GPIO control via libgpiod
- **Collectd**: `collectd` + modules `mqtt`, `exec`, `network`, `rrdtool`, `modbus` — metrics collection and export
- **C++ runtime**: `libstdcpp` — required for native Node.js modules (Zigbee2MQTT)
- **Utilities**: `tree`, `fuse-utils`

### v1.0.1
- Automatic root partition resize on first boot (double reboot)
- Added packages: parted, fdisk, cfdisk, resize2fs, losetup

### v1.0.0
- Initial release
- Custom U-Boot, DTS, uci-defaults
- mbusd + luci-app-mbusd
- Stable MAC from OTP

---

## License

GPL-2.0 (following OpenWrt)