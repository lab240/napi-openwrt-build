# OpenWrt для NapiLab Napi

Готовые сборки OpenWrt для промышленных IoT-шлюзов **NapiLab Napi** на базе Rockchip SoC.

Репозиторий содержит все кастомизации, которые превращают ванильный снапшот OpenWrt в полноценный промышленный шлюз Modbus TCP с веб-интерфейсом.

---

## Поддерживаемое железо

![alt text](img/boards.jpeg)

### Платформа RK3308 (Napi-C / Napi-P / Napi-Slot)

| Плата | SoC | RAM | Хранилище | Ethernet | Тип |
|-------|-----|-----|-----------|----------|-----|
| [NapiLab Napi-C](https://napiworld.ru/docs/napi-intro/) | RK3308 | 512 МБ | 4 ГБ NAND — 32 ГБ eMMC | 100 Мбит/с | Промышленный SBC |
| [NapiLab Napi-P](https://napiworld.ru/docs/napi-intro/) | RK3308 | 512 МБ | 4 ГБ NAND — 32 ГБ eMMC | 100 Мбит/с | Промышленный SBC |
| [NapiLab Napi-Slot](https://napiworld.ru/docs/napi-som-intro) | RK3308 | 512 МБ | 4 ГБ NAND — 32 ГБ eMMC | 100 Мбит/с | SOM |

### Платформа RK3568 (Napi 2)

| Плата | SoC | RAM | Хранилище | Ethernet | Тип |
|-------|-----|-----|-----------|----------|-----|
| [NapiLab Napi 2](https://napiworld.ru/) | RK3568 | 4 ГБ DDR4 | 32 ГБ eMMC + SD | 2× Gigabit | Промышленный шлюз |

> Документация по платам, схемы и файлы проектирования: [napilab/napi-boards](https://github.com/napilab/napi-boards)

---

## Сравнение платформ

| Параметр | Napi-C (RK3308) | Napi 2 (RK3568) |
|----------|----------------|-----------------|
| CPU | Cortex-A35 × 4, 1.3 ГГц | Cortex-A55 × 4, 2.0 ГГц |
| RAM | 256/512 МБ DDR3 | 4 ГБ DDR4 |
| Ethernet | 1× 100 Мбит/с | 2× Gigabit (LAN + WAN) |
| USB | 2× USB 2.0 | USB 2.0 + USB 3.0 OTG |
| RS-485 | UART1 (`/dev/ttyS1`) | UART7 (`/dev/ttyS7`), аппаратный RTS |
| CAN | — | CAN 2.0 |
| RTC | — | DS1338 на I2C5 |
| EEPROM | — | CAT24AA02 (256 байт) на I2C5 |
| HDMI | — | HDMI 2.0, framebuffer-консоль |
| I2C | — | I2C4, I2C5 |
| Доп. UART | — | UART3, UART4, UART5 (PLD) |
| Wi-Fi | RTL8723DS (802.11b/g/n) | — |
| NAT / Маршрутизация | — | Полноценный роутер (LAN + WAN + NAT) |
| Консоль | Serial ttyS0, 1 500 000 бод | Serial ttyS2 + HDMI tty1 |
| MAC-адрес | Из OTP RK3308 | Из eMMC CID |

---

## Что внутри

### U-Boot

| Платформа | Источник U-Boot |
|-----------|----------------|
| RK3308 (Napi-C) | Кастомный `napic-rk3308_defconfig` на базе Radxa ROCK Pi S |
| RK3568 (Napi 2) | defconfig от NanoPi R5S (тот же RK3568) |

### Device Trees

| Платформа | DTS-файл | Основа |
|-----------|----------|--------|
| RK3308 | `rk3308-napi-c.dts` | Radxa ROCK Pi S |
| RK3568 | `rk3568-napi2-rk3568.dts` | Кастомный, на базе Armbian |

### Стабильный MAC-адрес

**RK3308** — генерируется из OTP-данных:
```sh
MAC=$(cat /sys/bus/nvmem/devices/rockchip-otp0/nvmem | md5sum | \
  sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
```

**RK3568** — генерируется из eMMC CID (OTP недоступна):
```sh
CID=$(cat /sys/class/block/mmcblk0/device/cid)
MAC=$(echo "$CID" | md5sum | sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
WAN_MAC=$(echo "${CID}wan" | md5sum | sed 's/\(..\)\(..\)\(..\)\(..\)\(..\)\(..\).*/02:\1:\2:\3:\4:\5/')
```

### Сетевая конфигурация

| Платформа | LAN | WAN | Режим |
|-----------|-----|-----|-------|
| RK3308 | eth0 (DHCP) | — | Один интерфейс |
| RK3568 | eth0 (192.168.1.1, статика) | eth1 (DHCP) | Полноценный роутер с NAT |

### HDMI-консоль (только Napi 2)

Napi 2 поддерживает HDMI-выход с framebuffer-консолью — лог ядра и приглашение входа отображаются на HDMI-мониторе. USB-клавиатура поддерживается для локального доступа.

Лог загрузки ядра выводится одновременно на serial и HDMI через кастомный bootscript (`console=tty1 console=ttyS2,1500000`).

Драйвер DRM Rockchip VOP2 встроен в ядро:
- `DRM_ROCKCHIP=y`, `ROCKCHIP_VOP2=y`, `ROCKCHIP_DW_HDMI=y`
- `FRAMEBUFFER_CONSOLE=y`, `VT=y`

### Настройка при первом старте (uci-defaults)

| Скрипт | RK3308 | RK3568 | Назначение |
|--------|--------|--------|------------|
| `70-rootpt-resize` | ✓ | ✓ | Расширение корневого раздела на весь носитель (перезагрузка) |
| `80-rootfs-resize` | ✓ | ✓ | Расширение файловой системы после resize раздела (перезагрузка) |
| `91-bash` | ✓ | ✓ | bash как оболочка по умолчанию для root |
| `92-timezone` | ✓ | ✓ | Часовой пояс MSK-3 |
| `93-console-password` | ✓ | ✓ | Запрос пароля на серийной консоли |
| `94-macaddr` | OTP | eMMC CID | Генерация стабильного MAC (LAN + WAN на RK3568) |
| `95-network` | только eth0 | eth0 LAN + eth1 WAN | Сетевая конфигурация |
| `96-hostname` | `napiwrt` | `napi2wrt` | Имя устройства |
| `97-luci-theme` | ✓ | ✓ | Тема LuCI openwrt-2020 |
| `98-usb-ethernet` | ✓ | — | USB Ethernet как WAN (только RK3308) |
| `99-dhcp` | ✓ | — | DHCP на LAN (только RK3308) |

### Предустановленные пакеты

**Промышленный стек**
- `mosquitto` + `mosquitto-client` — MQTT-брокер
- `mbusd` + `luci-app-mbusd` — шлюз Modbus TCP с веб-интерфейсом
- `mbpoll` + `luci-app-mbpoll` — CLI-инструмент Modbus с веб-интерфейсом
- `mbscan` — сканер Modbus-устройств

**1-Wire / DS18B20**
- `owfs`, `owserver`, `owfs-client` — поддержка шины 1-Wire и доступ к устройствам

**I2C / GPIO**
- `i2c-tools`, `libi2c` — диагностика шины I2C
- `gpiod-tools`, `libgpiod` — управление GPIO через libgpiod

**Сбор метрик**
- `collectd` + модули `mqtt`, `exec`, `network`, `rrdtool`, `modbus` — сбор и экспорт метрик

**USB-Serial адаптеры**
- `kmod-usb-serial-ch341`, `cp210x`, `ftdi`, `pl2303`

**LTE-модем**
- `kmod-usb-net-qmi-wwan` + `uqmi` + `luci-proto-qmi` — поддержка Quectel EP06

**USB Ethernet**
- `kmod-usb-net-rtl8152` — USB Ethernet адаптер RTL8152/8153

**Управление дисками и resize разделов**
- `parted`, `losetup`, `resize2fs`

**Сеть и администрирование**
- `openssh-sftp-server` — SFTP-доступ
- `luci-ssl-wolfssl` — HTTPS для LuCI
- `tcpdump`, `ethtool` — сетевая диагностика
- `bash`, `htop`, `nano`, `screen`, `tree`, `minicom` — утилиты администрирования
- `procps-ng`, `usbutils`, `lsblk` — системные утилиты

**C++ runtime**
- `libstdcpp` — необходим для нативных модулей Node.js (Zigbee2MQTT)

**Дополнительно для Napi 2**
- `kmod-usb-hid`, `kmod-hid-generic` — USB-клавиатура для HDMI-консоли
- `kmod-drm`, `kmod-fb` — DRM и framebuffer для HDMI-вывода

---

## Структура репозитория

```
napi-openwrt/
├── napic-files/                    # Кастомизации RK3308 (Napi-C/P/Slot)
│   ├── target/linux/rockchip/
│   │   ├── files/.../rk3308-napi-c.dts
│   │   └── image/armv8.mk
│   ├── package/
│   │   ├── luci-app-mbusd/
│   │   ├── luci-app-mbpoll/
│   │   └── mbscan/
│   ├── files/etc/uci-defaults/
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

Веб-интерфейс для Modbus-шлюза mbusd — главная фишка этой сборки.

- Запуск / остановка / перезапуск службы
- Включение / отключение автозапуска
- Live-статус процесса с реальными параметрами запуска
- Отображение IP:порт, на котором слушает шлюз
- Полная конфигурация последовательного порта и параметров Modbus

---

## Сборка

### Зависимости (Ubuntu/Debian)

```bash
sudo apt install build-essential clang flex bison g++ gawk gcc-multilib \
  gettext git libncurses-dev libssl-dev python3-distutils python3-setuptools \
  python3-dev python3-pyelftools rsync swig unzip zlib1g-dev
```

### Подготовка

```bash
git clone https://github.com/openwrt/openwrt.git
cd openwrt
./scripts/feeds update -a
./scripts/feeds install -a
```

### Сборка для Napi-C (RK3308)

```bash
# Накладываем кастомизации
cp -r /path/to/napi-openwrt/napic-files/* .

# Собираем
make defconfig
make download -j$(nproc)
make -j$(nproc)
```

Результат: `bin/targets/rockchip/armv8/openwrt-rockchip-armv8-napilab_napic-ext4-sysupgrade.img.gz`

### Сборка для Napi 2 (RK3568)

```bash
# Накладываем кастомизации
cp -r /path/to/napi-openwrt/napi2-files/* .

# Применяем конфиг ядра для HDMI-консоли
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

```bash
gunzip openwrt-*-sysupgrade.img.gz
dd if=openwrt-*-sysupgrade.img of=/dev/sdX bs=4M status=progress
```

---

## Доступ по умолчанию

| Параметр | Napi-C (RK3308) | Napi 2 (RK3568) |
|----------|----------------|-----------------|
| LAN IP | DHCP | 192.168.1.1 |
| WAN | — | eth1 (DHCP от провайдера) |
| Веб-интерфейс | `http://<IP>/` | `http://192.168.1.1/` |
| SSH | `root@<IP>` | `root@192.168.1.1` |
| Консоль | ttyS0, 1 500 000 бод | ttyS2, 1 500 000 бод + HDMI |

---

## Zigbee2MQTT

Сборка поддерживает запуск Zigbee2MQTT на OpenWrt. Поскольку OpenWrt использует **musl libc**, стандартные бинарники Node.js не работают — нужна специальная сборка musl/aarch64.

Готовый архив доступен в [Releases](https://github.com/lab240/napi-openwrt-build/releases):

```
zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz
```

### Быстрый старт

```bash
# Устанавливаем Node.js (musl/arm64)
wget https://unofficial-builds.nodejs.org/download/release/v22.22.0/node-v22.22.0-linux-arm64-musl.tar.gz
mkdir -p /opt/node
tar xzf node-v22.22.0-linux-arm64-musl.tar.gz -C /opt/node --strip-components=1

# Устанавливаем Zigbee2MQTT
mkdir /opt/zigbee2mqtt
tar xzf zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz -C /opt/zigbee2mqtt/

# Устанавливаем runtime-зависимость
apk add libstdcpp6

# Запускаем
export PATH=/opt/node/bin:$PATH
cd /opt/zigbee2mqtt && npm start
```

Веб-интерфейс доступен по адресу `http://<IP>:8080/`

> Требуется 512 МБ RAM и ~500 МБ свободного места на носителе. Mosquitto уже предустановлен в этой сборке. Для Zigbee2MQTT рекомендуется Napi 2 с 4 ГБ RAM.

---

## Changelog

### v2.0.0
- **Поддержка NapiLab Napi 2 (RK3568)** — новая платформа с двумя Gigabit Ethernet, 4 ГБ RAM
- Режим полноценного роутера: LAN (192.168.1.1) + WAN (DHCP) + NAT
- HDMI framebuffer-консоль с поддержкой USB-клавиатуры — лог ядра и вход на мониторе
- Двойной вывод консоли: serial (ttyS2) + HDMI (tty1) одновременно
- RS-485 на UART7 с аппаратным управлением направлением RTS
- Поддержка шины CAN 2.0
- Аппаратные RTC (DS1338) и EEPROM (CAT24AA02)
- Стабильный MAC-адрес из eMMC CID (LAN + WAN)
- U-Boot от NanoPi R5S (тот же SoC RK3568)
- Кастомный bootscript для двойного вывода консоли
- DRM Rockchip VOP2 + DW-HDMI встроены в ядро

### v1.0.2
- Готовый архив Zigbee2MQTT 2.9.1 для musl/aarch64 в Releases
- Автоматическое расширение корневого раздела и файловой системы при первом старте (скрипты 70/80-rootpt-resize, двойная перезагрузка)
- Управление дисками: `parted`, `fdisk`, `cfdisk`, `losetup`, `resize2fs`
- 1-Wire / DS18B20: `owfs`, `owserver`, `owfs-client`
- I2C: `i2c-tools`, `libi2c`
- GPIO: `gpiod-tools`, `libgpiod`
- Collectd: `collectd` + модули `mqtt`, `exec`, `network`, `rrdtool`, `modbus`
- C++ runtime: `libstdcpp`
- Утилиты: `tree`, `fuse-utils`

### v1.0.1
- Автоматическое расширение корневого раздела при первом старте (двойная перезагрузка)
- Добавлены пакеты: parted, fdisk, cfdisk, resize2fs, losetup

### v1.0.0
- Первый релиз для RK3308 (Napi-C/P/Slot)
- Кастомный U-Boot, DTS, uci-defaults
- mbusd + luci-app-mbusd
- Стабильный MAC из OTP

---

## Контакты

Заказ, интеграция, кастомные сборки: **dj.novikov@gmail.com**

---

## Лицензия

GPL-2.0 (следуем лицензии OpenWrt)
