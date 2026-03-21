gh release create v1.1.0 \
  --title "NapiWRT v1.1.0 — Napi 2 (RK3568) + Napi-C (RK3308)" \
  --notes "## NapiWRT v1.1.0

### New: NapiLab Napi 2 (RK3568)
- Rockchip RK3568, 4GB RAM, 32GB eMMC
- Dual Gigabit Ethernet: eth0 (LAN 192.168.1.1) + eth1 (WAN DHCP)
- Full router with NAT
- HDMI framebuffer console + USB keyboard
- Dual console output: serial (ttyS2) + HDMI (tty1)
- RS-485 on UART7 with hardware RTS
- CAN 2.0, RTC DS1338, EEPROM CAT24AA02
- Stable MAC from eMMC CID
- U-Boot from NanoPi R5S

### Updated: NapiLab Napi-C (RK3308)
- Same industrial stack as before
- Package updates from OpenWrt SNAPSHOT

### Images
| File | Platform | Type |
|------|----------|------|
| \`openwrt-*-napi2-rk3568-ext4-sysupgrade.img.gz\` | Napi 2 | ext4, expandable |
| \`openwrt-*-napi2-rk3568-squashfs-sysupgrade.img.gz\` | Napi 2 | squashfs |
| \`openwrt-*-napilab_napic-ext4-sysupgrade.img.gz\` | Napi-C | ext4, expandable |
| \`zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz\` | Zigbee2mtt | pack|

### Flashing
\`\`\`bash
gunzip openwrt-*-sysupgrade.img.gz
dd if=openwrt-*-sysupgrade.img of=/dev/sdX bs=4M status=progress
\`\`\`
" \
  ~/data2/opemwrt/2103/release/openwrt-rockchip-armv8-napi2-rk3568-ext4-sysupgrade.img.gz \
  ~/data2/opemwrt/2103/release/openwrt-rockchip-armv8-napi2-rk3568-squashfs-sysupgrade.img.gz \
  ~/data2/opemwrt/2103/release/openwrt-rockchip-armv8-napilab_napic-ext4-sysupgrade.img.gz \
  ~/data2/opemwrt/2103/release/zigbee2mqtt-2.9.1-openwrt-aarch64-musl.tar.gz
