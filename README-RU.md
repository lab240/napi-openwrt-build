# OpenWrt для NapiLab Napi

Готовые сборки OpenWrt для промышленных IoT-шлюзов **NapiLab Napi** на базе Rockchip SoC.

## Репозиторий содержит все кастомизации, превращающие ванильный OpenWrt snapshot в полностью функциональный промышленный шлюз Modbus TCP с веб-интерфейсом.

## Загрузки

Готовые образы прошивок:

| Платформа | Загрузка |
| --- | --- |
| Napi-C / Napi-P / Napi-Slot (RK3308) | [download.napilinux.ru/linuximg/napic/openwrt/](https://download.napilinux.ru/linuximg/napic/openwrt/) |
| Napi 2 (RK3568) | [download.napilinux.ru/linuximg/napi2/openwrt/](https://download.napilinux.ru/linuximg/napi2/openwrt/) |

---

## Поддерживаемое железо

![alt text](img/boards.jpeg)

### Платформа RK3308 (Napi-C / Napi-P / Napi-Slot)

| Плата | SoC | RAM | Хранилище | Ethernet | Описание |
| --- | --- | --- | --- | --- | --- |
| [NapiLab Napi-C](https://napiworld.ru/docs/napi-intro/) | RK3308 | 256/512 МБ | 4 ГБ NAND - 32 ГБ eMMC | 100 Мбит/с | Промышленный SBC |
| [NapiLab Napi-P](https://napiworld.ru/docs/napi-intro/) | RK3308 | 256/512 МБ | 4 ГБ NAND - 32 ГБ eMMC | 100 Мбит/с | Промышленный SBC |
| [NapiLab Napi-Slot](https://napiworld.ru/docs/napi-som-intro) | RK3308 | 256/512 МБ | 4 ГБ NAND - 32 ГБ eMMC | 100 Мбит/с | SOM |

### Платформа RK3568 (Napi 2)

| Плата | SoC | RAM | Хранилище | Ethernet | Описание |
| --- | --- | --- | --- | --- | --- |
| [NapiLab Napi 2](https://napiworld.ru/) | RK3568 | 4 ГБ DDR4 | 32 ГБ eMMC + SD | 2x Gigabit | Промышленный шлюз |

> Документация, схемы и файлы проектирования плат: [napilab/napi-boards](https://github.com/napilab/napi-boards)

---

## Сравнение платформ

| Характеристика | Napi-C (RK3308) | Napi 2 (RK3568) |
| --- | --- | --- |
| CPU | Cortex-A35 x 4, 1.3 ГГц | Cortex-A55 x 4, 2.0 ГГц |
| RAM | 256/512 МБ DDR3 | 4 ГБ DDR4 |
| Ethernet | 1x 100 Мбит/с + USB Ethernet (опционально) | 2x Gigabit (LAN + WAN) |
| USB | 2x USB 2.0 + USB OTG (режим host) | USB 2.0 + USB 3.0 OTG |
| RS-485 | UART1 (`/dev/ttyS1`) | UART7 (`/dev/ttyS7`), аппаратный RTS |
| CAN | - | CAN 2.0 |
| RTC | - | DS1338 на I2C5 |
| EEPROM | - | CAT24AA02 (256 байт) на I2C5 |
| HDMI | - | HDMI 2.0 с framebuffer-консолью |
| I2C | - | I2C4, I2C5 |
| Дополнительные UART | UART2, UART3, UART4 | UART3, UART4, UART5 (PLD) |
| Wi-Fi | RTL8723DS (802.11b/g/n) | - |
| NAT / Маршрутизация | Полноценный роутер при наличии USB Ethernet | Полноценный роутер (LAN + WAN + NAT) |
| Консоль | Serial ttyS0, 1500000 бод | Serial ttyS2 + HDMI tty1 |
| MAC-адрес | Из OTP RK3308 | Из eMMC CID |

---

## Что внутри

### U-Boot

| Платформа | Источник U-Boot |
| --- | --- |
| RK3308 (Napi-C) | Кастомный `napic-rk3308_defconfig` на базе Radxa ROCK Pi S |
| RK3568 (Napi 2) | Defconfig NanoPi R5S (тот же RK3568 SoC) |

### Device Trees

| Платформа | DTS-файл | Основан на |
| --- | --- | --- |
| RK3308 | `rk3308-napi-c.dts` | Radxa ROCK Pi S |
| RK3568 | `rk3568-napi2-rk3568.dts` | Кастомный, на базе Armbian |

### Стабильный MAC-адрес

**RK3308** - генерируется из OTP-данных:

```
MAC=$(cat /sys/bus/nvmem/devices/rockchip-otp0/nvmem | md5sum | \
  sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
```

**RK3568** - генерируется из eMMC CID (OTP недоступен):

```
CID=$(cat /sys/class/block/mmcblk0/device/cid)
MAC=$(echo "$CID" | md5sum | sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
WAN_MAC=$(echo "${CID}wan" | md5sum | sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
```

### Конфигурация сети

| Платформа | LAN | WAN | Режим |
| --- | --- | --- | --- |
| RK3308 (1 сетевуха) | eth0 (DHCP) | - | Одиночный интерфейс |
| RK3308 (2 сетевухи) | eth0 (192.168.1.1, static) | eth1 / USB Ethernet (DHCP) | Роутер с NAT |
| RK3308 (3+ сетевух) | br-lan: eth0 + eth2+ (192.168.1.1) | eth1 / USB Ethernet (DHCP) | Роутер с бриджем на LAN |
| RK3568 | eth0 (192.168.1.1, static) | eth1 (DHCP) | Роутер с NAT |

### Поддержка USB OTG (RK3308)

Контроллер DWC2 USB OTG настроен в **режиме host** (`dr_mode = "host"` в DTS). Это позволяет использовать USB-устройства на OTG-порте, включая USB-Ethernet адаптеры (SMSC LAN9500/LAN9500A).

Необходимые модули ядра: `kmod-usb-dwc2`, `kmod-usb-net-smsc95xx`, `kmod-usb-gadget`, `kmod-lib-crc16`, `kmod-net-selftests`, `kmod-phy-smsc`.

### HDMI-консоль (только Napi 2)

Napi 2 поддерживает HDMI-выход с framebuffer-консолью - лог ядра и приглашение логина отображаются на HDMI-мониторе. Поддерживается USB-клавиатура для локального доступа.

Лог загрузки ядра выводится одновременно на последовательный порт и HDMI через кастомный bootscript (`console=tty1 console=ttyS2,1500000`).

Драйвер DRM Rockchip VOP2 встроен в ядро:

* `DRM_ROCKCHIP=y`, `ROCKCHIP_VOP2=y`, `ROCKCHIP_DW_HDMI=y`
* `FRAMEBUFFER_CONSOLE=y`, `VT=y`

### Настройка при первом старте (uci-defaults)

| Скрипт | RK3308 | RK3568 | Назначение |
| --- | --- | --- | --- |
| `70-rootpt-resize` | ✓ | ✓ | Расширение раздела до конца носителя (перезагрузка) |
| `80-rootfs-resize` | ✓ | ✓ | Расширение ФС после изменения раздела (перезагрузка) |
| `91-bash` | ✓ | ✓ | Bash как оболочка по умолчанию |
| `92-timezone` | ✓ | ✓ | Часовой пояс MSK-3 |
| `93-console-password` | ✓ | ✓ | Пароль на серийной консоли |
| `94-macaddr` | OTP | eMMC CID | Стабильный MAC (LAN + WAN на RK3568) |
| `95-network` | автоопределение | eth0 LAN + eth1 WAN | Конфигурация сети |
| `96-hostname` | `napiwrt` | `napi2wrt` | Имя устройства |
| `97-luci-theme` | ✓ | ✓ | Тема LuCI openwrt-2020 |
| `98-firewall-wan` | ✓ | - | Masq + разрешение SSH/HTTP/HTTPS на WAN |
| `99-dhcp` | ✓ | - | Отключение DHCP-сервера при одной сетевухе |

**Автоопределение сети на RK3308 (`95-network`):**
- 1 сетевуха (только eth0): DHCP-клиент, без DHCP-сервера
- 2 сетевухи (eth0 + eth1): eth0 = LAN (192.168.1.1), eth1 = WAN (DHCP), NAT включён
- 3+ сетевух: eth0 + eth2+ объединяются в br-lan, eth1 = WAN

### Баннер и информация о системе

Кастомный баннер с динамическим выводом информации о сетевых интерфейсах при логине через `/etc/profile.d/10-sysinfo.sh`:

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

### Предустановленные пакеты

**Промышленный стек**

* `mosquitto` + `mosquitto-client` - MQTT-брокер
* `mbusd` + `luci-app-mbusd` - шлюз Modbus TCP с веб-интерфейсом
* `mbpoll` + `luci-app-mbpoll` - Modbus CLI-инструмент с веб-интерфейсом
* `mbscan` - сканер Modbus RTU шины

**1-Wire / DS18B20**

* `owfs`, `owserver`, `owfs-client` - поддержка шины 1-Wire и доступ к устройствам

**I2C / GPIO**

* `i2c-tools`, `libi2c` - диагностика шины I2C
* `gpiod-tools`, `libgpiod` - управление GPIO через libgpiod

**Сбор метрик**

* `collectd` + модули `mqtt`, `exec`, `network`, `rrdtool`, `modbus` - сбор и экспорт метрик

**USB-Serial адаптеры**

* `kmod-usb-serial-ch341`, `cp210x`, `ftdi`, `pl2303`

**LTE-модем**

* `kmod-usb-net-qmi-wwan` + `uqmi` + `luci-proto-qmi` - поддержка Quectel EP06

**USB Ethernet**

* `kmod-usb-dwc2` - контроллер DWC2 OTG в режиме host
* `kmod-usb-net-smsc95xx` - SMSC LAN9500/LAN9500A USB Ethernet
* `kmod-usb-net-rtl8152` - RTL8152/8153 USB Ethernet адаптер

**Управление дисками и расширение разделов**

* `parted`, `losetup`, `resize2fs`

**Сетевые и административные утилиты**

* `openssh-sftp-server` - SFTP-доступ
* `luci-ssl-wolfssl` - HTTPS для LuCI
* `tcpdump`, `ethtool` - сетевая диагностика
* `bash`, `htop`, `nano`, `screen`, `tree`, `minicom` - утилиты администрирования
* `procps-ng`, `usbutils`, `lsblk` - системные утилиты

**Среда выполнения C++**

* `libstdcpp` - необходима для нативных модулей Node.js (Zigbee2MQTT)

**Дополнительно для Napi 2**

* `kmod-usb-hid`, `kmod-hid-generic` - USB-клавиатура для HDMI-консоли
* `kmod-drm`, `kmod-fb` - DRM и framebuffer для HDMI-выхода

---

## Структура репозитория

```
napi-openwrt/
├── napic-files/                    # Кастомизации RK3308 (Napi-C/P/Slot)
│   ├── target/linux/rockchip/
│   │   ├── files/.../rk3308-napi-c.dts
│   │   └── image/armv8.mk
│   ├── package/
│   │   ├── boot/uboot-rockchip/
│   │   │   ├── Makefile            # Блок napic-rk3308 + ограниченный UBOOT_TARGETS
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
├── napi2-files/                    # Кастомизации RK3568 (Napi 2)
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
├── _arch/                          # Архитектурная документация
├── img/                            # Изображения для README
└── README.md
```

---

## luci-app-mbusd

Веб-интерфейс для Modbus-шлюза mbusd - жемчужина сборки.

* Запуск / остановка / перезапуск службы
* Включение / отключение автозапуска
* Live-статус процесса с реальными параметрами запуска
* Отображение IP:порт на котором слушает шлюз
* Полная конфигурация последовательного порта и параметров Modbus

---

## Сборка

### Зависимости (Ubuntu/Debian)

```
sudo apt install build-essential clang flex bison g++ gawk gcc-multilib \
  gettext git libncurses-dev libssl-dev python3-distutils python3-setuptools \
  python3-dev python3-pyelftools rsync swig unzip zlib1g-dev help2man
```

### Подготовка

```
git clone https://github.com/openwrt/openwrt.git
cd openwrt
./scripts/feeds update -a
./scripts/feeds install -a
```

### Сборка для Napi-C (RK3308)

```bash
# Применяем кастомизации
cp -r /path/to/napi-openwrt/napic-files/* .

# Добавляем необходимые пакеты в .config
for pkg in kmod-usb-dwc2 kmod-usb-net-smsc95xx kmod-usb-gadget kmod-lib-crc16 \
  kmod-net-selftests kmod-phy-smsc parted losetup resize2fs; do
  sed -i "/$pkg/d" .config
  echo "CONFIG_PACKAGE_$pkg=y" >> .config
done

# Собираем (никогда не запускать make menuconfig или make defconfig после правки .config!)
make package/boot/arm-trusted-firmware-rockchip/compile -j$(nproc)
make package/boot/uboot-rockchip/compile -j$(nproc)
make -j$(nproc) EXTRA_IMAGE_NAME=$(date +%d%b_%H%M)
```

Результат: `bin/targets/rockchip/armv8/openwrt-ДАТА-rockchip-armv8-napilab_napic-ext4-sysupgrade.img.gz`

### Сборка для Napi 2 (RK3568)

```
# Применяем кастомизации
cp -r /path/to/napi-openwrt/napi2-files/* .

# Применяем конфигурацию ядра для HDMI-консоли
bash apply-kernel-config.sh

# Сначала собираем NanoPi R5S (для U-Boot)
echo 'CONFIG_TARGET_rockchip_armv8_DEVICE_friendlyarm_nanopi-r5s=y' >> .config
make defconfig
make -j$(nproc)

# Переключаемся на Napi 2
sed -i '/DEVICE_friendlyarm_nanopi-r5s/d' .config
echo 'CONFIG_TARGET_rockchip_armv8_DEVICE_napi2-rk3568=y' >> .config
make defconfig
make -j$(nproc)
```

Результат: `bin/targets/rockchip/armv8/openwrt-rockchip-armv8-napi2-rk3568-ext4-sysupgrade.img.gz`

---

## Прошивка

```
gunzip openwrt-*-sysupgrade.img.gz
dd if=openwrt-*-sysupgrade.img of=/dev/sdX bs=4M status=progress
sync
```

> Внимательно проверьте `/dev/sdX` командой `lsblk` перед записью!

---

## Доступ по умолчанию

| Параметр | Napi-C (1 сетевуха) | Napi-C (2 сетевухи) | Napi 2 (RK3568) |
| --- | --- | --- | --- |
| IP LAN | DHCP | 192.168.1.1 | 192.168.1.1 |
| WAN | - | eth1 (DHCP) | eth1 (DHCP) |
| Веб-интерфейс | `http://<IP>/` | `http://192.168.1.1/` | `http://192.168.1.1/` |
| SSH | `root@<IP>` | `root@<IP>` (lan + wan) | `root@192.168.1.1` |
| Консоль | ttyS0, 1500000 бод | ttyS0, 1500000 бод | ttyS2, 1500000 бод + HDMI |

---

## Перенос сборки на другую машину

Переносить нужно только кастомизации, не build_dir/staging_dir (содержат абсолютные пути):

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

На новой машине:

```bash
git clone https://github.com/openwrt/openwrt.git ~/openwrt
cd ~/openwrt
./scripts/feeds update -a
./scripts/feeds install -a
tar xzf napi-custom-*.tar.gz
make -j$(nproc) EXTRA_IMAGE_NAME=$(date +%d%b_%H%M)
```

> **Важно:** `armv8.mk` и `package/boot/uboot-rockchip/Makefile` из архива перезапишут оригинальные файлы OpenWrt. Если версия OpenWrt сильно отличается, эти файлы нужно мержить вручную - добавлять блоки napic в актуальные файлы новой версии.

---

## Известные грабли

- **Никогда не запускать `make menuconfig` или `make defconfig`** после ручной правки `.config` - они перезапишут кастомные записи
- **Добавление пакетов**: только через `sed -i '/<pkg>/d' .config && echo 'CONFIG_PACKAGE_<pkg>=y' >> .config`
- **build_dir/staging_dir содержат абсолютные пути** - при переносе между машинами всегда `make distclean` (сохранив `.config`)
- **UBOOT_TARGETS нужно ограничивать** только RK3308 - без ATF для rk3399/rk3588 сборка U-Boot падает
- **`dr_mode = "host"` обязателен** для USB OTG - без него dwc2 уходит в gadget-режим и USB-устройства не определяются
- **`files/etc/shadow`** должен содержать хеш, не пароль в открытом виде - используйте `openssl passwd -6`
- **USB-Ethernet (SMSC LAN9500)** требует пакеты: `kmod-usb-dwc2`, `kmod-usb-net-smsc95xx`, `kmod-usb-gadget`, `kmod-lib-crc16`, `kmod-net-selftests`, `kmod-phy-smsc`
- **Зона wan в firewall**: не создавать отдельную `firewall.wan_zone` - OpenWrt уже определяет зону wan; добавлять только masq и правила доступа

---

## Zigbee2MQTT

Сборка поддерживает запуск Zigbee2MQTT на OpenWrt. Поскольку OpenWrt использует **musl libc**, стандартные бинарники Node.js не работают - нужна специальная сборка musl/aarch64.

Готовый архив доступен на [download.napilinux.ru/apps/zigbee2mqtt-arm-openwrt/](https://download.napilinux.ru/apps/zigbee2mqtt-arm-openwrt/):

```
zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz
```

### Быстрый старт

```
# Установка Node.js (musl/arm64)
wget https://unofficial-builds.nodejs.org/download/release/v22.22.0/node-v22.22.0-linux-arm64-musl.tar.gz
mkdir -p /opt/node
tar xzf node-v22.22.0-linux-arm64-musl.tar.gz -C /opt/node --strip-components=1

# Установка Zigbee2MQTT
mkdir /opt/zigbee2mqtt
tar xzf zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz -C /opt/zigbee2mqtt/

# Установка зависимости
apk add libstdcpp6

# Запуск
export PATH=/opt/node/bin:$PATH
cd /opt/zigbee2mqtt && npm start
```

Веб-интерфейс доступен по адресу `http://<IP>:8080/`

> Требуется 512 МБ RAM и ~500 МБ свободного места на диске. Mosquitto уже предустановлен в сборке.

---

## Changelog

### v1.2.0

* **USB OTG в режиме host** - контроллер DWC2 настроен как host в DTS (`dr_mode = "host"`), USB-устройства на OTG-порте определяются
* **Поддержка USB-Ethernet** - адаптер SMSC LAN9500/LAN9500A работает как второй сетевой интерфейс (eth1 = WAN)
* **Автоматическая настройка сети** - uci-defaults автоматически определяет количество сетевых интерфейсов:
  - 1 сетевуха: eth0 = DHCP-клиент
  - 2 сетевухи: eth0 = LAN (192.168.1.1), eth1 = WAN (DHCP), NAT включён
  - 3+ сетевух: eth0 + eth2+ объединяются в br-lan, eth1 = WAN
* **Firewall с доступом на WAN** - SSH, HTTP, HTTPS разрешены на WAN-интерфейсе
* **UART3 включён** в DTS - дополнительный последовательный порт доступен
* **Динамическая информация при логине** - `/etc/profile.d/10-sysinfo.sh` показывает IP интерфейсов
* **Кастомный баннер** - брендирование NapiWRT
* **Исправлена дублирующая зона firewall** - uci-defaults больше не создаёт вторую зону wan
* **Инструкция по переносу сборки** - документирован tar-архив со всеми кастомизациями для чистой пересборки на другой машине
* **Обновлена инструкция сборки** - полное пошаговое руководство со всеми патчами, DTS и uci-defaults

### v1.1.0

* **Поддержка NapiLab Napi 2 (RK3568)** - новая платформа с двумя Gigabit Ethernet и 4 ГБ RAM
* Полноценный роутер: LAN (192.168.1.1) + WAN (DHCP) + NAT
* HDMI framebuffer-консоль с поддержкой USB-клавиатуры - лог ядра и логин на мониторе
* Двойной вывод консоли: serial (ttyS2) + HDMI (tty1) одновременно
* RS-485 на UART7 с аппаратным управлением направлением RTS
* Поддержка шины CAN 2.0
* Аппаратный RTC (DS1338) и EEPROM (CAT24AA02)
* Стабильный MAC-адрес из eMMC CID (LAN + WAN)
* U-Boot от NanoPi R5S (тот же RK3568 SoC)
* Кастомный bootscript для двойного вывода консоли
* DRM Rockchip VOP2 + DW-HDMI встроены в ядро

### v1.0.2

* Готовый архив Zigbee2MQTT 2.9.1 для musl/aarch64 в Releases
* Автоматическое расширение раздела и ФС при первом старте (скрипты 70/80-rootpt-resize, двойная перезагрузка)
* Управление дисками: `parted`, `fdisk`, `cfdisk`, `losetup`, `resize2fs`
* 1-Wire / DS18B20: `owfs`, `owserver`, `owfs-client`
* I2C: `i2c-tools`, `libi2c`
* GPIO: `gpiod-tools`, `libgpiod`
* Collectd: `collectd` + модули `mqtt`, `exec`, `network`, `rrdtool`, `modbus`
* Среда выполнения C++: `libstdcpp`
* Утилиты: `tree`, `fuse-utils`

### v1.0.1

* Автоматическое расширение раздела при первом старте (двойная перезагрузка)
* Добавлены пакеты: parted, fdisk, cfdisk, resize2fs, losetup

### v1.0.0

* Первый релиз для RK3308 (Napi-C/P/Slot)
* Кастомный U-Boot, DTS, uci-defaults
* mbusd + luci-app-mbusd
* Стабильный MAC из OTP

---

## Контакты

Заказы, интеграция и кастомные сборки: **dj.novikov@gmail.com**

---

## Лицензия

GPL-2.0 (следуем лицензии OpenWrt)