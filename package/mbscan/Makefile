include $(TOPDIR)/rules.mk

PKG_NAME:=mbscan
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_MAINTAINER:=NapiLab <info@napiworld.ru>
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/mbscan
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Modbus RTU bus scanner
  DEPENDS:=
endef

define Package/mbscan/description
  Fast Modbus RTU bus scanner. Opens serial port once and probes
  a range of slave addresses using FC03 (Read Holding Registers).
  No dependencies — pure POSIX implementation with own CRC16.
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $(PKG_BUILD_DIR)/mbscan \
		$(PKG_BUILD_DIR)/src/mbscan.c
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/src
	$(CP) ./src/mbscan.c $(PKG_BUILD_DIR)/src/
endef

define Package/mbscan/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mbscan $(1)/usr/bin/
endef

$(eval $(call BuildPackage,mbscan))
