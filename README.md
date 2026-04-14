# OpenWrt for NapiLab Napi family

Production-ready OpenWrt builds for **NapiLab Napi** industrial IoT gateways based on Rockchip SoCs.

## This repository contains all customizations needed to turn a vanilla OpenWrt snapshot into a fully functional industrial Modbus TCP gateway with a polished web interface.

## Supported hardware

![alt text](img/boards.jpeg)

### RK3308 platform (Napi-C / Napi-P / Napi-Slot)

| Board | SoC | RAM | Storage | Ethernet | Docs |
| --- | --- | --- | --- | --- | --- |
| [NapiLab Napi-C](https://napiworld.ru/docs/napi-intro/) | RK3308 | 256/512MB | 4GB NAND - 32GB eMMC | 100Mbps | Industrial SBC |
| [NapiLab Napi-P](https://napiworld.ru/docs/napi-intro/) | RK3308 | 256/512MB | 4GB NAND - 32GB eMMC | 100Mbps | Industrial SBC |
| [NapiLab Napi-Slot](https://napiworld.ru/docs/napi-som-intro) | RK3308 | 256/512MB | 4GB NAND - 32GB eMMC | 100Mbps | SOM |

### RK3568 platform (Napi 2)

| Board | SoC | RAM | Storage | Ethernet | Docs |
| --- | --- | --- | --- | --- | --- |
| [NapiLab Napi 2](https://napiworld.ru/) | RK3568 | 4GB DDR4 | 32GB eMMC + SD | 2x Gigabit | Industrial gateway |

> Board documentation, schematics, and design files: [napilab/napi-boards](https://github.com/napilab/napi-boards)

---

## Platform comparison

| Feature | Napi-C (RK3308) | Napi 2 (RK3568) |
| --- | --- | --- |
| CPU | Cortex-A35 x 4, 1.3GHz | Cortex-A55 x 4, 2.0GHz |
| RAM | 256/512MB DDR3 | 4GB DDR4 |
| Ethernet | 1x 100Mbps + USB Ethernet (optional) | 2x Gigabit (LAN + WAN) |
| USB | 2x USB 2.0 + USB OTG (host mode) | USB 2.0 + USB 3.0 OTG |
| RS-485 | UART1 (`/dev/ttyS1`) | UART7 (`/dev/ttyS7`), hardware RTS |
| CAN | - | CAN 2.0 |
| RTC | - | DS1338 on I2C5 |
| EEPROM | - | CAT24AA02 (256 bytes) on I2C5 |
| HDMI | - | HDMI 2.0 with framebuffer console |
| I2C | - | I2C4, I2C5 |
| Additional UART | UART2, UART3, UART4 | UART3, UART4, UART5 (PLD) |
| Wi-Fi | RTL8723DS (802.11b/g/n) | - |
| NAT / Routing | Full router when USB Ethernet present | Full router (LAN + WAN + NAT) |
| Console | Serial ttyS0, 1500000 baud | Serial ttyS2 + HDMI tty1 |
| MAC address | From RK3308 OTP | From eMMC CID |

---

## What's inside

### U-Boot

| Platform | U-Boot source |
| --- | --- |
| RK3308 (Napi-C) | Custom `napic-rk3308_defconfig` based on Radxa ROCK Pi S |
| RK3568 (Napi 2) | NanoPi R5S defconfig (same RK3568 SoC) |

### Device Trees

| Platform | DTS file | Based on |
| --- | --- | --- |
| RK3308 | `rk3308-napi-c.dts` | Radxa ROCK Pi S |
| RK3568 | `rk3568-napi2-rk3568.dts` | Custom, Armbian-derived |

### Stable MAC address

**RK3308** - generated from OTP data:

```
MAC=$(cat /sys/bus/nvmem/devices/rockchip-otp0/nvmem | md5sum | \
  sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
```

**RK3568** - generated from eMMC CID (OTP not available):

```
CID=$(cat /sys/class/block/mmcblk0/device/cid)
MAC=$(echo "$CID" | md5sum | sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
WAN_MAC=$(echo "${CID}wan" | md5sum | sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
```

### Network configuration

| Platform | LAN | WAN | Mode |
| --- | --- | --- | --- |
| RK3308 (1 NIC) | eth0 (DHCP) | - | Single interface |
| RK3308 (2 NICs) | eth0 (192.168.1.1, static) | eth1 / USB Ethernet (DHCP) | Full router with NAT |
| RK3308 (3+ NICs) | br-lan: eth0 + eth2+ (192.168.1.1) | eth1 / USB Ethernet (DHCP) | Router with bridged LAN |
| RK3568 | eth0 (192.168.1.1, static) | eth1 (DHCP) | Full router with NAT |

### USB OTG support (RK3308)

The DWC2 USB OTG controller is configured in **host mode** (`dr_mode = "host"` in DTS). This enables USB devices on the OTG port, including USB-Ethernet adapters (SMSC LAN9500/LAN9500A).

Required kernel modules: `kmod-usb-dwc2`, `kmod-usb-net-smsc95xx`, `kmod-usb-gadget`, `kmod-lib-crc16`, `kmod-net-selftests`, `kmod-phy-smsc`.

### HDMI console (Napi 2 only)

Napi 2 supports HDMI output with framebuffer console - kernel log and login prompt are displayed on an HDMI monitor. USB keyboard is supported for local access.

Kernel boot log is sent to both serial and HDMI simultaneously via custom bootscript (`console=tty1 console=ttyS2,1500000`).

DRM Rockchip VOP2 driver is built into the kernel with the following options:

* `DRM_ROCKCHIP=y`, `ROCKCHIP_VOP2=y`, `ROCKCHIP_DW_HDMI=y`
* `FRAMEBUFFER_CONSOLE=y`, `VT=y`

### First-boot configuration (uci-defaults)

| Script | RK3308 | RK3568 | Purpose |
| --- | --- | --- | --- |
| `70-rootpt-resize` | ✓ | ✓ | Resize root partition to fill storage (reboot) |
| `80-rootfs-resize` | ✓ | ✓ | Expand filesystem after partition resize (reboot) |
| `91-bash` | ✓ | ✓ | Set bash as default shell for root |
| `92-timezone` | ✓ | ✓ | Set timezone to MSK-3 |
| `93-console-password` | ✓ | ✓ | Enable password prompt on serial console |
| `94-macaddr` | OTP | eMMC CID | Generate stable MAC (LAN + WAN on RK3568) |
| `95-network` | auto-detect | eth0 LAN + eth1 WAN | Network configuration |
| `96-hostname` | `napiwrt` | `napi2wrt` | Set hostname |
| `97-luci-theme` | ✓ | ✓ | Set LuCI theme to openwrt-2020 |
| `98-firewall-wan` | ✓ | - | Enable masq + allow SSH/HTTP/HTTPS on WAN |
| `99-dhcp` | ✓ | - | Disable DHCP server if single NIC |

**RK3308 network auto-detection (`95-network`):**
- 1 NIC (eth0 only): DHCP client, no DHCP server
- 2 NICs (eth0 + eth1): eth0 = LAN (192.168.1.1), eth1 = WAN (DHCP), NAT enabled
- 3+ NICs: eth0 + eth2+ bridged into br-lan, eth1 = WAN

### Login banner and system info

Custom banner with dynamic network information displayed at login via `/etc/profile.d/10-sysinfo.sh`:

```
  _   _             _ _    _    _ _____  _______
 | \ | |           (_) |  | |  | |  __ \|__   __|
 |  \| | __ _ _ __  _| |  | |  | | |__) |  | |
 | . ` |/ _` | '_ \| | |  | |  | |  _  /   | |
 | |\  | (_| | |_) | | |__| |__| | | \ \   | |
 |_| \_|\__,_| .__/|_|_____\____/|_|  \_\  |_|
              | |
              |_|    Industrial Linux Gateway
 -----------------------------------------------------
 NapiWRT - OpenWrt for NapiLab Napi (RK3308)
 -----------------------------------------------------

 eth0: 192.168.1.1/24 (up)
 eth1: 192.168.30.91/24 (up)

root@napiwrt:~#
```

### Pre-installed packages

**Industrial stack**

* `mosquitto` + `mosquitto-client` - MQTT broker
* `mbusd` + `luci-app-mbusd` - Modbus TCP gateway with web UI
* `mbpoll` + `luci-app-mbpoll` - Modbus CLI tool with web UI
* `mbscan` - Modbus RTU bus scanner

**1-Wire / DS18B20**

* `owfs`, `owserver`, `owfs-client` - 1-Wire bus support and device access

**I2C / GPIO**

* `i2c-tools`, `libi2c` - I2C bus diagnostics
* `gpiod-tools`, `libgpiod` - GPIO control via libgpiod

**Metrics collection**

* `collectd` + modules `mqtt`, `exec`, `network`, `rrdtool`, `modbus` - metrics collection and export

**USB-Serial adapters**

* `kmod-usb-serial-ch341`, `cp210x`, `ftdi`, `pl2303`

**LTE modem**

* `kmod-usb-net-qmi-wwan` + `uqmi` + `luci-proto-qmi` - Quectel EP06 support

**USB Ethernet**

* `kmod-usb-dwc2` - DWC2 OTG controller in host mode
* `kmod-usb-net-smsc95xx` - SMSC LAN9500/LAN9500A USB Ethernet
* `kmod-usb-net-rtl8152` - RTL8152/8153 USB Ethernet adapter

**Disk management and partition resize**

* `parted`, `losetup`, `resize2fs`

**Networking and admin**

* `openssh-sftp-server` - SFTP access
* `luci-ssl-wolfssl` - HTTPS for LuCI
* `tcpdump`, `ethtool` - network diagnostics
* `bash`, `htop`, `nano`, `screen`, `tree`, `minicom` - admin tools
* `procps-ng`, `usbutils`, `lsblk` - system utilities

**C++ runtime**

* `libstdcpp` - required for native Node.js modules (Zigbee2MQTT)

**Napi 2 additional**

* `kmod-usb-hid`, `kmod-hid-generic` - USB keyboard for HDMI console
* `kmod-drm`, `kmod-fb` - DRM and framebuffer for HDMI output

---

## Repository structure

```
napi-openwrt/
├── napic-files/                    # RK3308 (Napi-C/P/Slot) customizations
│   ├── target/linux/rockchip/
│   │   ├── files/.../rk3308-napi-c.dts
│   │   └── image/armv8.mk
│   ├── package/
│   │   ├── boot/uboot-rockchip/
│   │   │   ├── Makefile            # napic-rk3308 block + limited UBOOT_TARGETS
│   │   │   └── patches/108-...patch
│   │   ├── luci-app-mbusd/
│   │   ├── luci-app-mbpoll/
│   │   └── mbscan/
│   ├── files/
│   │   ├── etc/banner
│   │   ├── etc/shadow
│   │   ├── etc/profile.d/10-sysinfo.sh
│   │   └── etc/uci-defaults/
│   └── .config
│
├── napi2-files/                    # RK3568 (Napi 2) customizations
│   ├── target/linux/rockchip/
│   │   ├── files/.../rk3568-napi2-rk3568.dts
│   │   ├── image/armv8.mk
│   │   └── image/napi2.bootscript
│   ├── package/
│   │   ├── luci-app-mbusd/
│   │   ├── luci-app-mbpoll/
│   │   └── mbscan/
│   ├── files/etc/
│   │   ├── uci-defaults/
│   │   ├── banner
│   │   ├── shadow
│   │   └── inittab
│   ├── apply-kernel-config.sh
│   └── .config
│
├── _arch/                          # Architecture docs
├── img/                            # Images for README
└── README.md
```

---

## luci-app-mbusd

Web interface for mbusd Modbus gateway - the crown jewel of this build.

* Start / Stop / Restart service
* Enable / Disable autostart
* Live process status with actual running parameters
* Listening IP:port display
* Full serial port and Modbus configuration

---

## Building

### Prerequisites (Ubuntu/Debian)

```
sudo apt install build-essential clang flex bison g++ gawk gcc-multilib \
  gettext git libncurses-dev libssl-dev python3-distutils python3-setuptools \
  python3-dev python3-pyelftools rsync swig unzip zlib1g-dev help2man
```

### Setup

```
git clone https://github.com/openwrt/openwrt.git
cd openwrt
./scripts/feeds update -a
./scripts/feeds install -a
```

### Build for Napi-C (RK3308)

```bash
# Apply customizations
cp -r /path/to/napi-openwrt/napic-files/* .

# Add required packages to .config
for pkg in kmod-usb-dwc2 kmod-usb-net-smsc95xx kmod-usb-gadget kmod-lib-crc16 \
  kmod-net-selftests kmod-phy-smsc parted losetup resize2fs; do
  sed -i "/$pkg/d" .config
  echo "CONFIG_PACKAGE_$pkg=y" >> .config
done

# Build (never run make menuconfig or make defconfig after editing .config!)
make package/boot/arm-trusted-firmware-rockchip/compile -j$(nproc)
make package/boot/uboot-rockchip/compile -j$(nproc)
make -j$(nproc) EXTRA_IMAGE_NAME=$(date +%d%b_%H%M)
```

Output: `bin/targets/rockchip/armv8/openwrt-ДАТА-rockchip-armv8-napilab_napic-ext4-sysupgrade.img.gz`

### Build for Napi 2 (RK3568)

```
# Apply customizations
cp -r /path/to/napi-openwrt/napi2-files/* .

# Apply kernel config for HDMI console
bash apply-kernel-config.sh

# Build NanoPi R5S first (for U-Boot)
echo 'CONFIG_TARGET_rockchip_armv8_DEVICE_friendlyarm_nanopi-r5s=y' >> .config
make defconfig
make -j$(nproc)

# Switch to Napi 2
sed -i '/DEVICE_friendlyarm_nanopi-r5s/d' .config
echo 'CONFIG_TARGET_rockchip_armv8_DEVICE_napi2-rk3568=y' >> .config
make defconfig
make -j$(nproc)
```

Output: `bin/targets/rockchip/armv8/openwrt-rockchip-armv8-napi2-rk3568-ext4-sysupgrade.img.gz`

---

## Flashing

```
gunzip openwrt-*-sysupgrade.img.gz
dd if=openwrt-*-sysupgrade.img of=/dev/sdX bs=4M status=progress
sync
```

> Check `/dev/sdX` with `lsblk` before writing!

---

## Default access

| Parameter | Napi-C (1 NIC) | Napi-C (2 NICs) | Napi 2 (RK3568) |
| --- | --- | --- | --- |
| LAN IP | DHCP | 192.168.1.1 | 192.168.1.1 |
| WAN | - | eth1 (DHCP) | eth1 (DHCP) |
| Web UI | `http://<IP>/` | `http://192.168.1.1/` | `http://192.168.1.1/` |
| SSH | `root@<IP>` | `root@<IP>` (lan + wan) | `root@192.168.1.1` |
| Console | ttyS0, 1500000 baud | ttyS0, 1500000 baud | ttyS2, 1500000 baud + HDMI |

---

## Transferring build to another machine

Only transfer customizations, never build_dir/staging_dir (they contain absolute paths):

```bash
cd ~/openwrt
tar czf ~/napi-custom-$(date +%d%b_%H%M).tar.gz \
  .config \
  files/ \
  target/linux/rockchip/files/ \
  target/linux/rockchip/image/armv8.mk \
  package/boot/uboot-rockchip/patches/108-board-rockchip-add-napilab-napic.patch \
  package/boot/uboot-rockchip/Makefile \
  package/luci-app-mbusd/ \
  package/luci-app-mbpoll/ \
  package/mbscan/
```

On the new machine:

```bash
git clone https://github.com/openwrt/openwrt.git ~/openwrt
cd ~/openwrt
./scripts/feeds update -a
./scripts/feeds install -a
tar xzf napi-custom-*.tar.gz
make -j$(nproc) EXTRA_IMAGE_NAME=$(date +%d%b_%H%M)
```

> **Important:** `armv8.mk` and `package/boot/uboot-rockchip/Makefile` from the archive will overwrite the originals. If the OpenWrt version differs significantly, merge the napic blocks into the new files manually.

---

## Known issues

- **Never run `make menuconfig` or `make defconfig`** after editing `.config` - they will overwrite custom entries
- **Adding packages**: use `sed -i '/<pkg>/d' .config && echo 'CONFIG_PACKAGE_<pkg>=y' >> .config`
- **build_dir/staging_dir contain absolute paths** - always `make distclean` (save `.config` first) when transferring between machines
- **UBOOT_TARGETS must be limited** to RK3308 only - without ATF for rk3399/rk3588 the U-Boot build fails
- **`dr_mode = "host"` is required** for USB OTG - without it dwc2 enters gadget mode and USB devices are invisible
- **`files/etc/shadow`** must contain a hash, not a plaintext password - use `openssl passwd -6`
- **USB-Ethernet (SMSC LAN9500)** requires: `kmod-usb-dwc2`, `kmod-usb-net-smsc95xx`, `kmod-usb-gadget`, `kmod-lib-crc16`, `kmod-net-selftests`, `kmod-phy-smsc`
- **Firewall wan zone**: do not create a separate `firewall.wan_zone` - OpenWrt already defines one; only add masq and allow rules

---

## Zigbee2MQTT

This build supports running Zigbee2MQTT on OpenWrt. Since OpenWrt uses **musl libc**, the standard Node.js binaries won't work - a special musl/aarch64 build is required.

A pre-built archive is available in [Releases](https://github.com/lab240/napi-openwrt-build/releases):

```
zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz
```

### Quick start

```
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

---

## Changelog

### v1.2.0

* **USB OTG host mode** - DWC2 controller configured as host in DTS (`dr_mode = "host"`), enabling USB devices on OTG port
* **USB-Ethernet support** - SMSC LAN9500/LAN9500A adapter works as second network interface (eth1 = WAN)
* **Automatic network configuration** - uci-defaults auto-detects number of NICs:
  - 1 NIC: eth0 = DHCP client
  - 2 NICs: eth0 = LAN (192.168.1.1), eth1 = WAN (DHCP), NAT enabled
  - 3+ NICs: eth0 + eth2+ bridged into br-lan, eth1 = WAN
* **Firewall with WAN access** - SSH, HTTP, HTTPS allowed on WAN interface
* **UART3 enabled** in DTS - additional serial port available
* **Dynamic login info** - `/etc/profile.d/10-sysinfo.sh` shows interface IPs at login
* **Custom banner** - NapiWRT branding
* **Fixed firewall duplicate zone** - uci-defaults no longer creates a second wan zone
* **Build transfer guide** - documented tar archive with all customizations for clean rebuild on another machine
* **Build instructions updated** - complete step-by-step guide with all patches, DTS, and uci-defaults

### v1.1.0

* **NapiLab Napi 2 (RK3568) support** - new platform with dual Gigabit Ethernet, 4GB RAM
* Full router mode: LAN (192.168.1.1) + WAN (DHCP) + NAT
* HDMI framebuffer console with USB keyboard support - kernel log and login on monitor
* Dual console output: serial (ttyS2) + HDMI (tty1) simultaneously
* RS-485 on UART7 with hardware RTS direction control
* CAN 2.0 bus support
* Hardware RTC (DS1338) and EEPROM (CAT24AA02)
* Stable MAC address from eMMC CID (LAN + WAN)
* U-Boot from NanoPi R5S (same RK3568 SoC)
* Custom bootscript for dual console output
* DRM Rockchip VOP2 + DW-HDMI built into kernel

### v1.0.2

* Zigbee2MQTT 2.9.1 pre-built archive for musl/aarch64 available in Releases
* Automatic root partition and filesystem resize on first boot (scripts 70/80-rootpt-resize, double reboot)
* Disk management: `parted`, `fdisk`, `cfdisk`, `losetup`, `resize2fs`
* 1-Wire / DS18B20: `owfs`, `owserver`, `owfs-client`
* I2C: `i2c-tools`, `libi2c`
* GPIO: `gpiod-tools`, `libgpiod`
* Collectd: `collectd` + modules `mqtt`, `exec`, `network`, `rrdtool`, `modbus`
* C++ runtime: `libstdcpp`
* Utilities: `tree`, `fuse-utils`

### v1.0.1

* Automatic root partition resize on first boot (double reboot)
* Added packages: parted, fdisk, cfdisk, resize2fs, losetup

### v1.0.0

* Initial release for RK3308 (Napi-C/P/Slot)
* Custom U-Boot, DTS, uci-defaults
* mbusd + luci-app-mbusd
* Stable MAC from OTP

---

## Contact

For orders, integration inquiries, and custom builds: **dj.novikov@gmail.com**

---

## License

GPL-2.0 (following OpenWrt)